// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/PantheliaEquipmentComponent.h"

#include "Combat/PantheliaWeapon.h"
#include "Combat/PantheliaWeaponDefinition.h"
#include "Combat/WeaponTraceComponent.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "PantheliaLogChannels.h"

UPantheliaEquipmentComponent::UPantheliaEquipmentComponent()
{
	// El equipo no necesita Tick: solo reacciona a equip/unequip.
	PrimaryComponentTick.bCanEverTick = false;
}

void UPantheliaEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	// Si hay un arma por defecto configurada, la equipamos automáticamente.
	// Permite dar un arma inicial al jugador sin lógica extra en el Blueprint.
	if (DefaultWeaponClass && DefaultWeaponDefinition)
	{
		TryEquipWeapon(DefaultWeaponClass, DefaultWeaponDefinition);
	}
}

void UPantheliaEquipmentComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// No emitimos broadcasts durante teardown. Los consumidores también pueden estar
	// destruyéndose, pero el WeaponTrace debe perder la referencia antes que el arma.
	DestroyEquippedWeapon(false);

	Super::EndPlay(EndPlayReason);
}

void UPantheliaEquipmentComponent::EquipWeapon(
	TSubclassOf<APantheliaWeapon> WeaponClass, UPantheliaWeaponDefinition* Definition)
{
	TryEquipWeapon(WeaponClass, Definition);
}

bool UPantheliaEquipmentComponent::TryEquipWeapon(
	TSubclassOf<APantheliaWeapon> WeaponClass, UPantheliaWeaponDefinition* Definition)
{
	if (bPreparedForDeath)
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("[DEATH] Equip ignorado en %s: Equipment ya fue preparado para muerte."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	if (!WeaponClass || !IsValid(Definition))
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[EQUIPMENT] Equip fallido en %s: WeaponClass o WeaponDefinition inválido."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[EQUIPMENT] Equip fallido en %s: World inválido."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	ACharacter* OwnerCharacter = nullptr;
	USkeletalMeshComponent* OwnerMesh = nullptr;
	if (!ResolveOwnerAndHandSocket(OwnerCharacter, OwnerMesh))
	{
		return false;
	}

	// La nueva arma se crea y valida mientras la anterior sigue intacta. Solo hacemos
	// el swap cuando el candidato ya está inicializado y correctamente attacheado.
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerCharacter;
	SpawnParams.Instigator = OwnerCharacter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	const FTransform SpawnTransform = OwnerMesh->GetSocketTransform(
		HandSocketName, RTS_World);

	APantheliaWeapon* CandidateWeapon = World->SpawnActor<APantheliaWeapon>(
		WeaponClass, SpawnTransform, SpawnParams);
	if (!IsValid(CandidateWeapon))
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[EQUIPMENT] Equip fallido en %s: no se pudo spawnear %s. Se conserva el arma anterior %s."),
			*GetNameSafe(OwnerCharacter),
			*GetNameSafe(WeaponClass.Get()),
			*GetNameSafe(EquippedWeapon));
		return false;
	}

	CandidateWeapon->WeaponDefinition = Definition;
	CandidateWeapon->InitializeFromDefinition();

	UMeshComponent* CandidateMesh = nullptr;
	if (!ValidateCandidateWeapon(CandidateWeapon, Definition, CandidateMesh))
	{
		CandidateWeapon->Destroy();
		return false;
	}

	const FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, true);
	const bool bAttached = CandidateWeapon->AttachToComponent(
		OwnerMesh, AttachRules, HandSocketName);

	const bool bAttachmentMatches = bAttached &&
		CandidateWeapon->GetRootComponent() &&
		CandidateWeapon->GetRootComponent()->GetAttachParent() == OwnerMesh &&
		CandidateWeapon->GetRootComponent()->GetAttachSocketName() == HandSocketName;

	if (!bAttachmentMatches)
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[EQUIPMENT] Equip fallido en %s: %s no pudo adjuntarse al socket '%s'. Se conserva %s."),
			*GetNameSafe(OwnerCharacter),
			*GetNameSafe(CandidateWeapon),
			*HandSocketName.ToString(),
			*GetNameSafe(EquippedWeapon));
		CandidateWeapon->Destroy();
		return false;
	}

	APantheliaWeapon* PreviousWeapon = EquippedWeapon;

	// Ningún trace debe conservar el mesh/spec del arma anterior durante el swap.
	ClearOwnerWeaponTraceSource();
	EquippedWeapon = CandidateWeapon;

	if (IsValid(PreviousWeapon) && PreviousWeapon != CandidateWeapon)
	{
		PreviousWeapon->Destroy();
	}

	OnWeaponEquipped.Broadcast(EquippedWeapon);

	UE_LOG(LogPanthelia, Log,
		TEXT("[EQUIPMENT] %s equipó %s con Definition=%s Mesh=%s HandSocket=%s."),
		*GetNameSafe(OwnerCharacter),
		*GetNameSafe(EquippedWeapon),
		*GetNameSafe(Definition),
		*GetNameSafe(CandidateMesh),
		*HandSocketName.ToString());

	return true;
}

void UPantheliaEquipmentComponent::UnequipWeapon()
{
	DestroyEquippedWeapon(true);
}

APantheliaWeapon* UPantheliaEquipmentComponent::GetEquippedWeapon() const
{
	return IsValid(EquippedWeapon) ? EquippedWeapon.Get() : nullptr;
}

UPantheliaWeaponDefinition* UPantheliaEquipmentComponent::GetEquippedWeaponDefinition() const
{
	const APantheliaWeapon* Weapon = GetEquippedWeapon();
	return Weapon && IsValid(Weapon->WeaponDefinition)
		? Weapon->WeaponDefinition.Get()
		: nullptr;
}

FPantheliaEquippedWeaponDeathHandoff
UPantheliaEquipmentComponent::PrepareEquippedWeaponForDeath()
{
	if (bPreparedForDeath)
	{
		return CachedDeathHandoff;
	}

	bPreparedForDeath = true;
	CachedDeathHandoff.Reset();

	// El trace debe quedar cerrado antes de cambiar attachment o exponer meshes.
	if (AActor* OwnerActor = GetOwner())
	{
		if (UWeaponTraceComponent* TraceComponent =
			OwnerActor->FindComponentByClass<UWeaponTraceComponent>())
		{
			TraceComponent->ShutdownForDeath();
		}
	}

	APantheliaWeapon* Weapon = GetEquippedWeapon();
	if (!IsValid(Weapon))
	{
		UE_LOG(LogPanthelia, Log,
			TEXT("[DEATH] Handoff de arma en %s completado sin arma equipada."),
			*GetNameSafe(GetOwner()));
		return CachedDeathHandoff;
	}

	CachedDeathHandoff.WeaponActor = Weapon;
	TArray<UPrimitiveComponent*> VisualParts;
	GetEquippedWeaponVisualParts(VisualParts);
	for (UPrimitiveComponent* VisualPart : VisualParts)
	{
		CachedDeathHandoff.VisualParts.Add(VisualPart);
	}

	const USceneComponent* RootComponent = Weapon->GetRootComponent();
	const bool bWasAttached = RootComponent && RootComponent->GetAttachParent();
	if (bWasAttached)
	{
		Weapon->DetachFromActor(FDetachmentTransformRules::KeepWorldTransform);
	}
	CachedDeathHandoff.bWasDetached = bWasAttached;

	UE_LOG(LogPanthelia, Log,
		TEXT("[DEATH] Handoff de arma: Owner=%s Weapon=%s Parts=%d Detached=%s. Equipment conserva ownership."),
		*GetNameSafe(GetOwner()),
		*GetNameSafe(Weapon),
		CachedDeathHandoff.VisualParts.Num(),
		bWasAttached ? TEXT("true") : TEXT("false"));

	return CachedDeathHandoff;
}

void UPantheliaEquipmentComponent::GetEquippedWeaponVisualParts(
	TArray<UPrimitiveComponent*>& OutVisualParts) const
{
	OutVisualParts.Reset();
	APantheliaWeapon* Weapon = GetEquippedWeapon();
	if (!IsValid(Weapon))
	{
		return;
	}

	if (UMeshComponent* ActiveMesh = Weapon->GetActiveMeshComponent())
	{
		OutVisualParts.Add(ActiveMesh);
	}

	TArray<UMeshComponent*> MeshComponents;
	Weapon->GetComponents<UMeshComponent>(MeshComponents);
	for (UMeshComponent* MeshComponent : MeshComponents)
	{
		if (IsValid(MeshComponent) && MeshComponent->IsVisible())
		{
			OutVisualParts.AddUnique(MeshComponent);
		}
	}
}

bool UPantheliaEquipmentComponent::ResolveOwnerAndHandSocket(
	ACharacter*& OutOwnerCharacter,
	USkeletalMeshComponent*& OutOwnerMesh) const
{
	OutOwnerCharacter = Cast<ACharacter>(GetOwner());
	OutOwnerMesh = OutOwnerCharacter ? OutOwnerCharacter->GetMesh() : nullptr;

	if (!IsValid(OutOwnerCharacter) || !IsValid(OutOwnerMesh))
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[EQUIPMENT] Equip fallido: owner %s no es ACharacter o no tiene SkeletalMesh válido."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	if (HandSocketName.IsNone() || !OutOwnerMesh->DoesSocketExist(HandSocketName))
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[EQUIPMENT] Equip fallido en %s: HandSocket '%s' no existe en mesh %s."),
			*GetNameSafe(OutOwnerCharacter),
			*HandSocketName.ToString(),
			*GetNameSafe(OutOwnerMesh->GetSkeletalMeshAsset()));
		return false;
	}

	return true;
}

bool UPantheliaEquipmentComponent::ValidateCandidateWeapon(
	APantheliaWeapon* CandidateWeapon,
	UPantheliaWeaponDefinition* Definition,
	UMeshComponent*& OutWeaponMesh) const
{
	OutWeaponMesh = IsValid(CandidateWeapon)
		? CandidateWeapon->GetActiveMeshComponent()
		: nullptr;

	if (!IsValid(CandidateWeapon) || !IsValid(Definition) || !IsValid(OutWeaponMesh))
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[EQUIPMENT] Candidato inválido: Weapon=%s Definition=%s ActiveMesh=%s."),
			*GetNameSafe(CandidateWeapon),
			*GetNameSafe(Definition),
			*GetNameSafe(OutWeaponMesh));
		return false;
	}

	if (Definition->WeaponBaseSocketName.IsNone() ||
		Definition->WeaponTipSocketName.IsNone() ||
		Definition->WeaponBaseSocketName == Definition->WeaponTipSocketName)
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[EQUIPMENT] %s tiene sockets de trace inválidos: Base='%s' Tip='%s'."),
			*GetNameSafe(Definition),
			*Definition->WeaponBaseSocketName.ToString(),
			*Definition->WeaponTipSocketName.ToString());
		return false;
	}

	const bool bHasBaseSocket = OutWeaponMesh->DoesSocketExist(Definition->WeaponBaseSocketName);
	const bool bHasTipSocket = OutWeaponMesh->DoesSocketExist(Definition->WeaponTipSocketName);
	if (!bHasBaseSocket || !bHasTipSocket)
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[EQUIPMENT] %s no puede equiparse: Mesh=%s BaseSocket=%s(%s) TipSocket=%s(%s)."),
			*GetNameSafe(CandidateWeapon),
			*GetNameSafe(OutWeaponMesh),
			*Definition->WeaponBaseSocketName.ToString(),
			bHasBaseSocket ? TEXT("existe") : TEXT("falta"),
			*Definition->WeaponTipSocketName.ToString(),
			bHasTipSocket ? TEXT("existe") : TEXT("falta"));
		return false;
	}

	return true;
}

void UPantheliaEquipmentComponent::ClearOwnerWeaponTraceSource() const
{
	if (AActor* OwnerActor = GetOwner())
	{
		if (UWeaponTraceComponent* TraceComponent =
			OwnerActor->FindComponentByClass<UWeaponTraceComponent>())
		{
			TraceComponent->ClearExternalWeaponTraceSource();
		}
	}
}

void UPantheliaEquipmentComponent::DestroyEquippedWeapon(bool bBroadcastChange)
{
	if (!IsValid(EquippedWeapon))
	{
		EquippedWeapon = nullptr;
		return;
	}

	// El arma del jugador es la fuente externa del WeaponTraceComponent. Limpiamos
	// TODA la referencia de trace antes de destruir el Actor del arma para que un
	// notify tardío no pueda reutilizar el spec, sonido o mesh del ataque anterior.
	ClearOwnerWeaponTraceSource();

	APantheliaWeapon* WeaponToDestroy = EquippedWeapon;
	EquippedWeapon = nullptr;
	WeaponToDestroy->Destroy();

	if (bBroadcastChange)
	{
		OnWeaponEquipped.Broadcast(nullptr);
	}
}
