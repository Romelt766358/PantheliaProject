// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/PantheliaProjectileSpell.h"

#include "Actor/PantheliaProjectile.h"
#include "Combat/LockonComponent.h"
#include "Components/SceneComponent.h"
#include "Interfaces/CombatInterface.h"
#include "PantheliaGameplayTags.h"
#include "PantheliaLogChannels.h"

#if WITH_EDITOR
#include "AbilitySystem/Abilities/PantheliaSpellValidation.h"
#include "Misc/DataValidation.h"
#endif

#if WITH_EDITOR
EDataValidationResult UPantheliaProjectileSpell::IsDataValid(
	FDataValidationContext& Context) const
{
	if (GetClass()->HasAnyClassFlags(CLASS_Abstract))
	{
		return EDataValidationResult::NotValidated;
	}

	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	const EDataValidationResult SpellResult =
		PantheliaSpellValidation::ValidateProjectileSpell(*this, Context);

	return SuperResult == EDataValidationResult::Invalid
		|| SpellResult == EDataValidationResult::Invalid
		? EDataValidationResult::Invalid
		: EDataValidationResult::Valid;
}
#endif

UPantheliaProjectileSpell::UPantheliaProjectileSpell()
{
	// Los hechizos del jugador pueden activar el pipeline común desde sus Class Defaults.
	// Los hechizos de enemigos conservan bUsePantheliaResourceCost=false y no pagan recursos.
	// El coste principal es Mana y la entrada adicional de Stamina se cobra dentro del
	// mismo CommitAbility. Sus valores concretos siguen siendo datos de cada hechizo.
	ResourceCostType = EPantheliaResourceCostType::Mana;

	FPantheliaAdditionalResourceCost StaminaCost;
	StaminaCost.ResourceType = EPantheliaResourceCostType::Stamina;
	AdditionalResourceCosts.Add(StaminaCost);
}

void UPantheliaProjectileSpell::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

FGameplayTag UPantheliaProjectileSpell::GetResolvedSocketTag() const
{
	const FPantheliaGameplayTags& GameplayTags = FPantheliaGameplayTags::Get();

	return SocketTag.IsValid()
		? SocketTag
		: GameplayTags.Montage_Attack_Weapon;
}

AActor* UPantheliaProjectileSpell::GetFacingTargetActor() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return nullptr;
	}

	if (ULockonComponent* LockonComp =
		AvatarActor->FindComponentByClass<ULockonComponent>())
	{
		AActor* TargetActor = LockonComp->CurrentTargetActor.Get();
		if (!IsValid(TargetActor))
		{
			return nullptr;
		}

		if (TargetActor->GetClass()->ImplementsInterface(UCombatInterface::StaticClass())
			&& ICombatInterface::Execute_IsDead(TargetActor))
		{
			return nullptr;
		}

		return TargetActor;
	}

	return nullptr;
}

FVector UPantheliaProjectileSpell::GetFacingTargetLocation() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return FVector::ZeroVector;
	}

	FVector TargetLocation = FVector::ZeroVector;
	if (TryResolveTargetLocation(GetFacingTargetActor(), TargetLocation))
	{
		return TargetLocation;
	}

	return AvatarActor->GetActorLocation() +
		AvatarActor->GetActorForwardVector() * 2000.f;
}

FVector UPantheliaProjectileSpell::GetProjectileSocketLocation() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return FVector::ZeroVector;
	}

	if (!AvatarActor->GetClass()->ImplementsInterface(UCombatInterface::StaticClass()))
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("PantheliaProjectileSpell::GetProjectileSocketLocation — %s no implementa CombatInterface."),
			*GetNameSafe(AvatarActor));
		return AvatarActor->GetActorLocation();
	}

	// BlueprintNativeEvent: llamar siempre via Execute_ en C++.
	return ICombatInterface::Execute_GetCombatSocketLocation(
		AvatarActor,
		GetResolvedSocketTag());
}

bool UPantheliaProjectileSpell::TryResolveTargetLocation(
	AActor* TargetActor,
	FVector& OutTargetLocation) const
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	if (TargetActor->GetClass()->ImplementsInterface(UCombatInterface::StaticClass())
		&& ICombatInterface::Execute_IsDead(TargetActor))
	{
		return false;
	}

	if (AActor* AvatarActor = GetAvatarActorFromActorInfo())
	{
		if (ULockonComponent* LockonComp =
			AvatarActor->FindComponentByClass<ULockonComponent>())
		{
			// El punto lógico puede estar en torso, cabeza u otra zona configurada.
			// No usar GetActorLocation(): normalmente apunta a los pies/origen.
			OutTargetLocation = LockonComp->GetLockonLocation(TargetActor);
			return true;
		}
	}

	if (const USceneComponent* TargetRoot = TargetActor->GetRootComponent())
	{
		OutTargetLocation = TargetRoot->GetComponentLocation();
		return true;
	}

	OutTargetLocation = TargetActor->GetActorLocation();
	return true;
}

void UPantheliaProjectileSpell::SpawnProjectile()
{
	const FVector SocketLocation = GetProjectileSocketLocation();
	const FVector TargetLocation = GetFacingTargetLocation();
	const FVector Direction = (TargetLocation - SocketLocation).GetSafeNormal();

	if (Direction.IsNearlyZero())
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("PantheliaProjectileSpell::SpawnProjectile — dirección inválida en %s."),
			*GetName());
		return;
	}

	FRotator ProjectileRotation = Direction.Rotation();
	ProjectileRotation.Pitch = 0.f;
	ProjectileRotation.Roll = 0.f;

	SpawnProjectileWithRotation(ProjectileRotation);
}


bool UPantheliaProjectileSpell::ConfigureProjectileBeforeFinishSpawning(
	APantheliaProjectile* Projectile)
{
	if (!IsValid(Projectile))
	{
		return false;
	}

	if (DamageEffectClass)
	{
		// Un spec nuevo por proyectil. APantheliaProjectile escribe dirección de muerte,
		// knockback y launch en su EffectContext al impactar; compartir el mismo spec entre
		// hermanos permitiría que dos impactos simultáneos contaminaran ese contexto.
		Projectile->DamageEffectSpecHandle = MakeDamageSpec();
	}

	return !DamageEffectClass || Projectile->DamageEffectSpecHandle.IsValid();
}

APantheliaProjectile* UPantheliaProjectileSpell::SpawnProjectileWithRotation(
	const FRotator& ProjectileRotation,
	AActor* HomingTargetActor,
	const FPantheliaProjectileHomingSettings* HomingSettings,
	const float ProjectileSpeedOverride)
{
	FTransform SpawnTransform;
	SpawnTransform.SetLocation(GetProjectileSocketLocation());
	SpawnTransform.SetRotation(ProjectileRotation.Quaternion());

	return SpawnProjectileAtTransform(
		SpawnTransform,
		HomingTargetActor,
		HomingSettings,
		ProjectileSpeedOverride,
		false);
}

APantheliaProjectile* UPantheliaProjectileSpell::SpawnProjectileAtTransform(
	const FTransform& SpawnTransform,
	AActor* HomingTargetActor,
	const FPantheliaProjectileHomingSettings* HomingSettings,
	const float ProjectileSpeedOverride,
	const bool bPrepareForDelayedLaunch)
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!IsValid(AvatarActor) || !IsValid(World) || !AvatarActor->HasAuthority())
	{
		return nullptr;
	}

	// Si no se asignó ProjectileClass en el Blueprint del ability, no hay nada que spawnear.
	// Sin este guard, SpawnActorDeferred devuelve null y FinishSpawning crashea.
	if (!ProjectileClass)
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("PantheliaProjectileSpell::SpawnProjectileAtTransform — ProjectileClass no asignado en %s"),
			*GetName());
		return nullptr;
	}

	APantheliaProjectile* Projectile = World->SpawnActorDeferred<APantheliaProjectile>(
		ProjectileClass,
		SpawnTransform,
		GetOwningActorFromActorInfo(),
		Cast<APawn>(AvatarActor),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	// SpawnActorDeferred puede devolver null si la clase no es válida o hay un
	// conflicto de colisión irrecuperable. Siempre verificar antes de usar.
	if (!Projectile)
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("PantheliaProjectileSpell::SpawnProjectileAtTransform — SpawnActorDeferred devolvió null para %s"),
			*ProjectileClass->GetName());
		return nullptr;
	}

	if (!ConfigureProjectileBeforeFinishSpawning(Projectile))
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("PantheliaProjectileSpell::SpawnProjectileAtTransform — configuración pre-spawn falló para %s en %s."),
			*GetNameSafe(Projectile),
			*GetName());
		Projectile->Destroy();
		return nullptr;
	}

	if (bPrepareForDelayedLaunch)
	{
		Projectile->PrepareForDelayedLaunch();
	}
	else if (HomingSettings && HomingSettings->bEnabled && IsValid(HomingTargetActor))
	{
		Projectile->ConfigureSoftHoming(HomingTargetActor, *HomingSettings);
	}

	Projectile->FinishSpawning(SpawnTransform);

	if (!bPrepareForDelayedLaunch)
	{
		Projectile->SetProjectileSpeed(ProjectileSpeedOverride);
	}

	return Projectile;
}
