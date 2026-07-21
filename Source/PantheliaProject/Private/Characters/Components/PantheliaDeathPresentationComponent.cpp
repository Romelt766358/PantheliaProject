#include "Characters/Components/PantheliaDeathPresentationComponent.h"

#include "Characters/PantheliaCharacterBase.h"
#include "Components/CapsuleComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GroomComponent.h"
#include "PantheliaProject.h"
#include "PantheliaLogChannels.h"
#include "TimerManager.h"

UPantheliaDeathPresentationComponent::UPantheliaDeathPresentationComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

bool UPantheliaDeathPresentationComponent::RequestDeathPresentation()
{
	if (PresentationState != EPantheliaDeathPresentationState::Alive)
	{
		return false;
	}

	bFinishBroadcast = false;
	bDeathImpulseApplied = false;
	PresentationState = EPantheliaDeathPresentationState::DeathRequested;
	return true;
}

void UPantheliaDeathPresentationComponent::NotifyGameplayShutdownComplete()
{
	if (PresentationState == EPantheliaDeathPresentationState::DeathRequested)
	{
		PresentationState = EPantheliaDeathPresentationState::GameplayShutdown;
	}
}

bool UPantheliaDeathPresentationComponent::BeginDeathPresentation(const FVector& DeathImpulse)
{
	if (PresentationState == EPantheliaDeathPresentationState::PresentationStarted ||
		PresentationState == EPantheliaDeathPresentationState::PresentationFinished ||
		PresentationState == EPantheliaDeathPresentationState::Aborted)
	{
		return false;
	}

	// Llamadas directas desde Blueprint siguen una ruta cerrada y segura, aunque el
	// flujo normal siempre pasa antes por Request + GameplayShutdown desde CharacterBase.
	if (PresentationState == EPantheliaDeathPresentationState::Alive)
	{
		PresentationState = EPantheliaDeathPresentationState::DeathRequested;
	}
	if (PresentationState == EPantheliaDeathPresentationState::DeathRequested)
	{
		PresentationState = EPantheliaDeathPresentationState::GameplayShutdown;
	}

	if (!EnsureAuthoritativeBodyFallback() ||
		!RegisterAutomaticVisualParts() ||
		!ValidateRegisteredVisualParts())
	{
		AbortDeathPresentation();
		return false;
	}

	APantheliaCharacterBase* CharacterOwner = Cast<APantheliaCharacterBase>(GetOwner());
	if (!IsValid(CharacterOwner))
	{
		AbortDeathPresentation();
		return false;
	}

	PresentationState = EPantheliaDeathPresentationState::PresentationStarted;
	UE_LOG(LogPanthelia, Log,
		TEXT("[DEATH] Presentacion iniciada para %s con %d partes registradas."),
		*GetNameSafe(CharacterOwner), RegisteredVisualParts.Num());

	if (!ConfigureDeathPresentation(DeathImpulse))
	{
		AbortDeathPresentation();
		return false;
	}

	// El componente ya dejo preparado el estado visual. CharacterBase conserva aqui
	// solamente el sonido y el evento de muerte; el fallback legacy se activa solo si
	// esta presentacion no pudo comenzar.
	CharacterOwner->MulticastHandleDeath(DeathImpulse);
	SchedulePresentationFinish();
	return true;
}

void UPantheliaDeathPresentationComponent::SchedulePresentationFinish()
{
	if (PresentationState != EPantheliaDeathPresentationState::PresentationStarted)
	{
		return;
	}

	ClearPresentationFinishTimer();
	FinalizationSettings.PresentationDuration =
		FMath::Max(0.0f, FinalizationSettings.PresentationDuration);
	if (!FinalizationSettings.bAutoFinishPresentation)
	{
		return;
	}

	if (FinalizationSettings.PresentationDuration <= 0.0f)
	{
		FinishDeathPresentation();
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("[DEATH] World invalido; finalizacion inmediata para %s."),
			*GetNameSafe(GetOwner()));
		FinishDeathPresentation();
		return;
	}

	const uint32 CallbackGeneration = ++PresentationCallbackGeneration;
	FTimerDelegate FinishDelegate;
	FinishDelegate.BindUObject(
		this, &UPantheliaDeathPresentationComponent::HandleScheduledPresentationFinish,
		CallbackGeneration);
	World->GetTimerManager().SetTimer(
		PresentationTimerHandle, FinishDelegate, FinalizationSettings.PresentationDuration, false);

	UE_LOG(LogPanthelia, Log,
		TEXT("[DEATH] Finalizacion programada para %s en %.2f s."),
		*GetNameSafe(GetOwner()), FinalizationSettings.PresentationDuration);
}

void UPantheliaDeathPresentationComponent::FinishDeathPresentation()
{
	if (PresentationState != EPantheliaDeathPresentationState::PresentationStarted || bFinishBroadcast)
	{
		UE_LOG(LogPanthelia, Verbose,
			TEXT("[DEATH] Finalizacion duplicada ignorada para %s."),
			*GetNameSafe(GetOwner()));
		return;
	}

	ClearPresentationFinishTimer();
	PresentationState = EPantheliaDeathPresentationState::PresentationFinished;
	bFinishBroadcast = true;

	UE_LOG(LogPanthelia, Log, TEXT("[DEATH] Presentacion finalizada para %s."),
		*GetNameSafe(GetOwner()));
	OnPresentationFinished.Broadcast(GetOwner());
	RegisteredVisualParts.Reset();
}

void UPantheliaDeathPresentationComponent::AbortDeathPresentation()
{
	if (PresentationState == EPantheliaDeathPresentationState::Alive ||
		PresentationState == EPantheliaDeathPresentationState::PresentationFinished ||
		PresentationState == EPantheliaDeathPresentationState::Aborted)
	{
		ClearPresentationFinishTimer();
		return;
	}

	ClearPresentationFinishTimer();
	PresentationState = EPantheliaDeathPresentationState::Aborted;
	bFinishBroadcast = true;
	RegisteredVisualParts.Reset();
	OnPresentationFinished.Clear();

	UE_LOG(LogPanthelia, Log, TEXT("[DEATH] Presentacion abortada para %s."),
		*GetNameSafe(GetOwner()));
}

bool UPantheliaDeathPresentationComponent::IsPresentationActive() const
{
	return PresentationState == EPantheliaDeathPresentationState::DeathRequested ||
		PresentationState == EPantheliaDeathPresentationState::GameplayShutdown ||
		PresentationState == EPantheliaDeathPresentationState::PresentationStarted;
}

bool UPantheliaDeathPresentationComponent::HasPresentationFinished() const
{
	return PresentationState == EPantheliaDeathPresentationState::PresentationFinished;
}

bool UPantheliaDeathPresentationComponent::RegisterVisualPart(
	const FPantheliaDeathVisualPart& VisualPart)
{
	if (PresentationState != EPantheliaDeathPresentationState::Alive &&
		PresentationState != EPantheliaDeathPresentationState::DeathRequested &&
		PresentationState != EPantheliaDeathPresentationState::GameplayShutdown)
	{
		return false;
	}

	if (!IsValid(VisualPart.Component))
	{
		if (VisualPart.bRequired)
		{
			UE_LOG(LogPanthelia, Error,
				TEXT("[DEATH] Fallo de registro obligatorio en %s: componente nulo o invalido."),
				*GetNameSafe(GetOwner()));
			return false;
		}

		// Las partes opcionales vacias son validas por contrato y simplemente se omiten.
		return true;
	}

	AActor* PresentationOwner = GetOwner();
	AActor* PartOwner = VisualPart.Component->GetOwner();
	const bool bBelongsToPresentationOwner =
		PartOwner == PresentationOwner ||
		(IsValid(PartOwner) && PartOwner->GetOwner() == PresentationOwner);
	if (!bBelongsToPresentationOwner)
	{
		if (VisualPart.bRequired)
		{
			UE_LOG(LogPanthelia, Error,
				TEXT("[DEATH] Fallo de registro obligatorio en %s: %s pertenece a otro Actor."),
				*GetNameSafe(PresentationOwner), *GetNameSafe(VisualPart.Component));
		}
		return !VisualPart.bRequired;
	}

	for (FPantheliaDeathVisualPart& ExistingPart : RegisteredVisualParts)
	{
		if (ExistingPart.Component == VisualPart.Component)
		{
			ExistingPart = VisualPart;
			return true;
		}
	}

	RegisteredVisualParts.Add(VisualPart);
	return true;
}

void UPantheliaDeathPresentationComponent::ClearRegisteredVisualParts()
{
	if (PresentationState == EPantheliaDeathPresentationState::Alive)
	{
		RegisteredVisualParts.Reset();
	}
}

bool UPantheliaDeathPresentationComponent::RegisterAutomaticVisualParts()
{
	APantheliaCharacterBase* CharacterOwner = Cast<APantheliaCharacterBase>(GetOwner());
	if (!IsValid(CharacterOwner))
	{
		return false;
	}

	TArray<USkeletalMeshComponent*> SkeletalMeshes;
	CharacterOwner->GetComponents<USkeletalMeshComponent>(SkeletalMeshes);
	for (USkeletalMeshComponent* SkeletalMesh : SkeletalMeshes)
	{
		if (!IsValid(SkeletalMesh) || SkeletalMesh == CharacterOwner->GetMesh() ||
			SkeletalMesh == CharacterOwner->FinalWeaponMesh)
		{
			continue;
		}

		FPantheliaDeathVisualPart PoseFollowerPart;
		PoseFollowerPart.Component = SkeletalMesh;
		PoseFollowerPart.Role = EPantheliaDeathVisualPartRole::PoseFollower;
		PoseFollowerPart.RagdollPolicy = EPantheliaDeathRagdollPolicy::None;
		PoseFollowerPart.DissolvePolicy = EPantheliaDeathDissolvePolicy::None;
		PoseFollowerPart.VisibilityPolicy = EPantheliaDeathVisibilityPolicy::Preserve;
		PoseFollowerPart.bRequired = false;
		RegisterVisualPart(PoseFollowerPart);
	}

	TArray<UGroomComponent*> GroomComponents;
	CharacterOwner->GetComponents<UGroomComponent>(GroomComponents);
	for (UGroomComponent* GroomComponent : GroomComponents)
	{
		if (!IsValid(GroomComponent))
		{
			continue;
		}

		FPantheliaDeathVisualPart GroomPart;
		GroomPart.Component = GroomComponent;
		GroomPart.Role = EPantheliaDeathVisualPartRole::Groom;
		GroomPart.RagdollPolicy = EPantheliaDeathRagdollPolicy::None;
		GroomPart.DissolvePolicy = EPantheliaDeathDissolvePolicy::None;
		GroomPart.VisibilityPolicy = EPantheliaDeathVisibilityPolicy::Preserve;
		GroomPart.bRequired = false;
		RegisterVisualPart(GroomPart);
	}

	if (CharacterOwner->PreparedDeathWeaponMesh.IsValid())
	{
		FPantheliaDeathVisualPart WeaponPart;
		WeaponPart.Component = CharacterOwner->PreparedDeathWeaponMesh.Get();
		WeaponPart.Role = EPantheliaDeathVisualPartRole::WeaponPart;
		WeaponPart.RagdollPolicy = EPantheliaDeathRagdollPolicy::None;
		WeaponPart.DissolvePolicy = EPantheliaDeathDissolvePolicy::None;
		WeaponPart.VisibilityPolicy = EPantheliaDeathVisibilityPolicy::Preserve;
		WeaponPart.bRequired = false;
		RegisterVisualPart(WeaponPart);
	}

	return true;
}

bool UPantheliaDeathPresentationComponent::ConfigureDeathPresentation(
	const FVector& DeathImpulse)
{
	APantheliaCharacterBase* CharacterOwner = Cast<APantheliaCharacterBase>(GetOwner());
	USkeletalMeshComponent* BodyMesh = IsValid(CharacterOwner) ? CharacterOwner->GetMesh() : nullptr;
	if (!IsValid(CharacterOwner) || !IsValid(BodyMesh))
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[DEATH] Referencia obligatoria invalida en %s: Body autoritativo ausente."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	ConfigureCapsulePolicy(*CharacterOwner);
	ConfigureAuthoritativeBody(*BodyMesh, DeathImpulse);
	ConfigurePoseFollowers();
	ConfigureGrooms();
	ConfigureWeaponParts(*CharacterOwner, DeathImpulse);
	bDeathImpulseApplied = true;

	int32 PoseFollowerCount = 0;
	int32 GroomCount = 0;
	int32 WeaponPartCount = 0;
	for (const FPantheliaDeathVisualPart& Part : RegisteredVisualParts)
	{
		PoseFollowerCount += Part.Role == EPantheliaDeathVisualPartRole::PoseFollower ? 1 : 0;
		GroomCount += Part.Role == EPantheliaDeathVisualPartRole::Groom ? 1 : 0;
		WeaponPartCount += Part.Role == EPantheliaDeathVisualPartRole::WeaponPart ? 1 : 0;
	}
	UE_LOG(LogPanthelia, Log,
		TEXT("[DEATH] Body configurado para ragdoll en %s; seguidores=%d, grooms=%d, armas=%d."),
		*GetNameSafe(CharacterOwner), PoseFollowerCount, GroomCount, WeaponPartCount);
	return true;
}

void UPantheliaDeathPresentationComponent::ConfigureCapsulePolicy(
	APantheliaCharacterBase& CharacterOwner) const
{
	if (UCapsuleComponent* Capsule = CharacterOwner.GetCapsuleComponent())
	{
		Capsule->SetGenerateOverlapEvents(false);
		Capsule->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		UE_LOG(LogPanthelia, Log, TEXT("[DEATH] Capsule desactivada antes de la fisica en %s."),
			*GetNameSafe(&CharacterOwner));
	}
}

void UPantheliaDeathPresentationComponent::ConfigureAuthoritativeBody(
	USkeletalMeshComponent& BodyMesh, const FVector& DeathImpulse)
{
	BodyMesh.SetGenerateOverlapEvents(false);
	BodyMesh.SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	BodyMesh.SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	BodyMesh.SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
	BodyMesh.SetCollisionResponseToChannel(ECC_Fighter, ECR_Ignore);
	BodyMesh.SetCollisionResponseToChannel(ECC_Projectile, ECR_Ignore);
	BodyMesh.SetEnableGravity(true);
	BodyMesh.SetSimulatePhysics(true);
	BodyMesh.WakeAllRigidBodies();
	if (!bDeathImpulseApplied)
	{
		BodyMesh.AddImpulse(DeathImpulse, NAME_None, true);
	}
}

void UPantheliaDeathPresentationComponent::ConfigurePoseFollowers()
{
	int32 PoseFollowerCount = 0;
	for (const FPantheliaDeathVisualPart& Part : RegisteredVisualParts)
	{
		if (Part.Role != EPantheliaDeathVisualPartRole::PoseFollower ||
			!IsValid(Part.Component))
		{
			continue;
		}

		USkeletalMeshComponent* PoseFollower = Cast<USkeletalMeshComponent>(Part.Component);
		if (!IsValid(PoseFollower))
		{
			continue;
		}

		PoseFollower->SetGenerateOverlapEvents(false);
		PoseFollower->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		PoseFollower->SetSimulatePhysics(false);
		if (PoseFollowerTickPolicy == EPantheliaDeathPoseFollowerTickPolicy::PostPhysics)
		{
			PoseFollower->SetTickGroup(TG_PostPhysics);
		}
		++PoseFollowerCount;
	}

	if (PoseFollowerCount > 0)
	{
		UE_LOG(LogPanthelia, Log,
			TEXT("[DEATH] Pose followers configurados: %d (%s)."), PoseFollowerCount,
			PoseFollowerTickPolicy == EPantheliaDeathPoseFollowerTickPolicy::PostPhysics
				? TEXT("PostPhysics")
				: TEXT("grupo original"));
	}
}

void UPantheliaDeathPresentationComponent::ConfigureGrooms()
{
	int32 GroomCount = 0;
	int32 SimulationDisabledCount = 0;
	for (const FPantheliaDeathVisualPart& Part : RegisteredVisualParts)
	{
		if (Part.Role != EPantheliaDeathVisualPartRole::Groom ||
			!IsValid(Part.Component))
		{
			continue;
		}

		UGroomComponent* Groom = Cast<UGroomComponent>(Part.Component);
		if (!IsValid(Groom))
		{
			continue;
		}

		Groom->SetGenerateOverlapEvents(false);
		Groom->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		if (Groom->GroomAsset && Groom->SimulationSettings.SolverSettings.bEnableSimulation)
		{
			Groom->SetEnableSimulation(false);
			++SimulationDisabledCount;
		}
		++GroomCount;
	}

	if (GroomCount > 0)
	{
		UE_LOG(LogPanthelia, Log,
			TEXT("[DEATH] Grooms conservan binding/visibilidad: %d; simulacion desactivada: %d."),
			GroomCount, SimulationDisabledCount);
	}
}

void UPantheliaDeathPresentationComponent::ConfigureWeaponParts(
	APantheliaCharacterBase& CharacterOwner, const FVector& DeathImpulse)
{
	int32 SimulatedWeaponPartCount = 0;
	if (bDeathImpulseApplied)
	{
		return;
	}

	for (const FPantheliaDeathVisualPart& Part : RegisteredVisualParts)
	{
		if (Part.Role != EPantheliaDeathVisualPartRole::WeaponPart ||
			!IsValid(Part.Component))
		{
			continue;
		}

		UPrimitiveComponent* WeaponPart = Part.Component;
		WeaponPart->SetGenerateOverlapEvents(false);
		WeaponPart->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
		WeaponPart->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
		WeaponPart->SetCollisionResponseToChannel(ECC_Pawn, ECR_Ignore);
		WeaponPart->SetCollisionResponseToChannel(ECC_Fighter, ECR_Ignore);
		WeaponPart->SetCollisionResponseToChannel(ECC_Projectile, ECR_Ignore);
		WeaponPart->SetEnableGravity(true);
		WeaponPart->SetSimulatePhysics(true);
		WeaponPart->WakeAllRigidBodies();
		WeaponPart->AddImpulse(
			DeathImpulse * CharacterOwner.WeaponDeathImpulseScale, NAME_None, true);
		++SimulatedWeaponPartCount;
	}

	if (SimulatedWeaponPartCount > 0)
	{
		UE_LOG(LogPanthelia, Log, TEXT("[DEATH] Arma entregada a fisica: %d partes."),
			SimulatedWeaponPartCount);
	}
}

bool UPantheliaDeathPresentationComponent::EnsureAuthoritativeBodyFallback()
{
	for (const FPantheliaDeathVisualPart& Part : RegisteredVisualParts)
	{
		if (Part.Role == EPantheliaDeathVisualPartRole::AuthoritativeBody &&
			IsValid(Part.Component))
		{
			return true;
		}
	}

	APantheliaCharacterBase* CharacterOwner = Cast<APantheliaCharacterBase>(GetOwner());
	USkeletalMeshComponent* AuthoritativeBody =
		IsValid(CharacterOwner) ? CharacterOwner->GetMesh() : nullptr;
	if (!IsValid(AuthoritativeBody))
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[DEATH] Fallo de registro obligatorio en %s: GetMesh() no es valido."),
			*GetNameSafe(GetOwner()));
		return false;
	}

	FPantheliaDeathVisualPart BodyPart;
	BodyPart.Component = AuthoritativeBody;
	BodyPart.Role = EPantheliaDeathVisualPartRole::AuthoritativeBody;
	BodyPart.RagdollPolicy = EPantheliaDeathRagdollPolicy::LegacyAuthoritativeBody;
	BodyPart.DissolvePolicy = EPantheliaDeathDissolvePolicy::None;
	BodyPart.VisibilityPolicy = EPantheliaDeathVisibilityPolicy::Preserve;
	BodyPart.bRequired = true;
	return RegisterVisualPart(BodyPart);
}

bool UPantheliaDeathPresentationComponent::ValidateRegisteredVisualParts()
{
	for (int32 Index = RegisteredVisualParts.Num() - 1; Index >= 0; --Index)
	{
		const FPantheliaDeathVisualPart& Part = RegisteredVisualParts[Index];
		if (IsValid(Part.Component))
		{
			continue;
		}

		if (Part.bRequired)
		{
			UE_LOG(LogPanthelia, Error,
				TEXT("[DEATH] Fallo de registro obligatorio en %s: parte requerida invalida."),
				*GetNameSafe(GetOwner()));
			return false;
		}

		RegisteredVisualParts.RemoveAtSwap(Index, 1, EAllowShrinking::No);
	}

	return true;
}

void UPantheliaDeathPresentationComponent::ClearPresentationFinishTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(PresentationTimerHandle);
	}
	PresentationTimerHandle.Invalidate();
	++PresentationCallbackGeneration;
}

void UPantheliaDeathPresentationComponent::HandleScheduledPresentationFinish(
	uint32 CallbackGeneration)
{
	if (CallbackGeneration != PresentationCallbackGeneration)
	{
		return;
	}

	FinishDeathPresentation();
}

void UPantheliaDeathPresentationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	AbortDeathPresentation();
	Super::EndPlay(EndPlayReason);
}
