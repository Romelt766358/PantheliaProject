// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/PantheliaAreaImpactProjectileSpell.h"

#include "AbilitySystem/PantheliaProjectileTargeting.h"
#include "Actor/PantheliaAreaImpactProjectile.h"
#include "GameFramework/Actor.h"
#include "PantheliaLogChannels.h"

#if !UE_BUILD_SHIPPING
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "HAL/IConsoleManager.h"
#endif


#if !UE_BUILD_SHIPPING
namespace
{
	bool IsAreaImpactDebugEnabled()
	{
		const IConsoleVariable* DebugCVar = IConsoleManager::Get().FindConsoleVariable(
			TEXT("panthelia.Debug.AreaImpact"));
		return DebugCVar && DebugCVar->GetInt() > 0;
	}

	float GetAreaImpactDebugDuration()
	{
		const IConsoleVariable* DurationCVar = IConsoleManager::Get().FindConsoleVariable(
			TEXT("panthelia.Debug.AreaImpactDuration"));
		return DurationCVar
			? FMath::Max(0.05f, DurationCVar->GetFloat())
			: 3.f;
	}
}
#endif

#if WITH_EDITOR
#include "AbilitySystem/Abilities/PantheliaSpellValidation.h"
#include "Misc/DataValidation.h"
#endif

UPantheliaAreaImpactProjectileSpell::UPantheliaAreaImpactProjectileSpell()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

#if WITH_EDITOR
EDataValidationResult UPantheliaAreaImpactProjectileSpell::IsDataValid(
	FDataValidationContext& Context) const
{
	if (GetClass()->HasAnyClassFlags(CLASS_Abstract))
	{
		return EDataValidationResult::NotValidated;
	}

	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	const EDataValidationResult AreaResult =
		PantheliaSpellValidation::ValidateAreaImpactProjectileSpell(*this, Context);

	return SuperResult == EDataValidationResult::Invalid
		|| AreaResult == EDataValidationResult::Invalid
		? EDataValidationResult::Invalid
		: EDataValidationResult::Valid;
}
#endif

void UPantheliaAreaImpactProjectileSpell::SpawnAreaImpactProjectile()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return;
	}

	const float AbilityLevel = GetAbilityLevel();
	const FVector SpawnLocation = GetProjectileSocketLocation();
	AActor* TargetActor = GetFacingTargetActor();

	FVector TargetPoint = FVector::ZeroVector;
	bool bHasExplicitTargetPoint = false;
	if (IsValid(TargetActor))
	{
		bHasExplicitTargetPoint = PantheliaProjectileTargeting::TryResolveTargetPoint(
			this,
			AvatarActor,
			TargetActor,
			AimPointMode,
			GroundTraceUpDistance.GetValueAtLevel(AbilityLevel),
			GroundTraceDownDistance.GetValueAtLevel(AbilityLevel),
			GroundTraceChannel,
			GroundSurfaceOffset.GetValueAtLevel(AbilityLevel),
			TargetPoint);
	}

	// Sin lock-on no se adquiere target implícito: conserva el contrato solicitado
	// para Fireburst y viaja recto siguiendo el forward del personaje.
	FVector LaunchDirection = bHasExplicitTargetPoint
		? (TargetPoint - SpawnLocation).GetSafeNormal()
		: AvatarActor->GetActorForwardVector().GetSafeNormal();

	if (LaunchDirection.IsNearlyZero())
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("PantheliaAreaImpactProjectileSpell::SpawnAreaImpactProjectile — dirección inválida en %s."),
			*GetName());
		return;
	}

#if !UE_BUILD_SHIPPING
	if (IsAreaImpactDebugEnabled())
	{
		if (UWorld* DebugWorld = GetWorld())
		{
			const float DebugDuration = GetAreaImpactDebugDuration();
			const FVector DebugTargetPoint = bHasExplicitTargetPoint
				? TargetPoint
				: SpawnLocation + LaunchDirection * 2000.f;

			// Blanco: origen real del proyectil. Amarillo: punto objetivo. Cian: trayectoria inicial.
			DrawDebugSphere(
				DebugWorld,
				SpawnLocation,
				10.f,
				12,
				FColor::White,
				/*bPersistentLines=*/false,
				DebugDuration,
				/*DepthPriority=*/0,
				/*Thickness=*/1.5f);

			DrawDebugSphere(
				DebugWorld,
				DebugTargetPoint,
				18.f,
				16,
				FColor::Yellow,
				/*bPersistentLines=*/false,
				DebugDuration,
				/*DepthPriority=*/0,
				/*Thickness=*/2.f);

			DrawDebugLine(
				DebugWorld,
				SpawnLocation,
				DebugTargetPoint,
				FColor::Cyan,
				/*bPersistentLines=*/false,
				DebugDuration,
				/*DepthPriority=*/0,
				/*Thickness=*/2.f);

			UE_LOG(LogPanthelia, Display,
				TEXT("[AREA IMPACT DEBUG] Ability='%s' Mode=%s Spawn=%s TargetPoint=%s Direction=%s"),
				*GetName(),
				bHasExplicitTargetPoint ? TEXT("LockOn") : TEXT("Forward"),
				*SpawnLocation.ToCompactString(),
				*DebugTargetPoint.ToCompactString(),
				*LaunchDirection.ToCompactString());
		}
	}
#endif

	FRotator LaunchRotation = LaunchDirection.Rotation();
	LaunchRotation.Roll = 0.f;

	FPantheliaProjectileHomingSettings HomingSettings;
	HomingSettings.bEnabled = bEnableSoftHoming && IsValid(TargetActor);
	HomingSettings.StartDelay = HomingStartDelay.GetValueAtLevel(AbilityLevel);
	HomingSettings.Duration = HomingDuration.GetValueAtLevel(AbilityLevel);
	HomingSettings.AccelerationMagnitude =
		HomingAccelerationMagnitude.GetValueAtLevel(AbilityLevel);
	HomingSettings.MaxCorrectionAngleDegrees =
		MaxHomingCorrectionAngleDegrees.GetValueAtLevel(AbilityLevel);
	HomingSettings.TargetPointMode = AimPointMode;
	HomingSettings.GroundTraceUpDistance =
		GroundTraceUpDistance.GetValueAtLevel(AbilityLevel);
	HomingSettings.GroundTraceDownDistance =
		GroundTraceDownDistance.GetValueAtLevel(AbilityLevel);
	HomingSettings.GroundSurfaceOffset =
		GroundSurfaceOffset.GetValueAtLevel(AbilityLevel);
	HomingSettings.GroundTraceChannel = GroundTraceChannel;

	FTransform SpawnTransform(LaunchRotation, SpawnLocation);
	SpawnProjectileAtTransform(
		SpawnTransform,
		TargetActor,
		HomingSettings.bEnabled ? &HomingSettings : nullptr,
		ProjectileSpeedOverride.GetValueAtLevel(AbilityLevel),
		/*bPrepareForDelayedLaunch=*/false);
}

bool UPantheliaAreaImpactProjectileSpell::ConfigureProjectileBeforeFinishSpawning(
	APantheliaProjectile* Projectile)
{
	APantheliaAreaImpactProjectile* AreaProjectile =
		Cast<APantheliaAreaImpactProjectile>(Projectile);
	if (!AreaProjectile)
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("PantheliaAreaImpactProjectileSpell — ProjectileClass de %s no hereda de APantheliaAreaImpactProjectile."),
			*GetName());
		return false;
	}

	const float AbilityLevel = GetAbilityLevel();

	FPantheliaAreaImpactRuntimeConfig RuntimeConfig;
	RuntimeConfig.ExplosionRadius = ExplosionRadius.GetValueAtLevel(AbilityLevel);
	RuntimeConfig.MaxAffectedTargets = MaxAffectedTargets;
	RuntimeConfig.DamagePolicy = DamagePolicy;
	RuntimeConfig.bDirectTargetReceivesExplosion = bDirectTargetReceivesExplosion;
	RuntimeConfig.bExplosionRequiresLineOfSight = bExplosionRequiresLineOfSight;
	RuntimeConfig.ExplosionLineOfSightTraceChannel =
		ExplosionLineOfSightTraceChannel;
	RuntimeConfig.Element = AreaElement;
	RuntimeConfig.SourceAbilityTag = SourceAbilityTag;

	FGameplayEffectSpecHandle ExplosionSpec;
	if (DamagePolicy != EPantheliaAreaImpactDamagePolicy::DirectOnly)
	{
		ExplosionSpec = MakeDamageSpecWithMultipliers(
			ExplosionDamageMultiplier.GetValueAtLevel(AbilityLevel),
			ExplosionPoiseMultiplier.GetValueAtLevel(AbilityLevel),
			ExplosionBuildupMultiplier.GetValueAtLevel(AbilityLevel));
	}

	FGameplayEffectSpecHandle DirectSpec;
	if (DamagePolicy != EPantheliaAreaImpactDamagePolicy::ExplosionOnly)
	{
		DirectSpec = MakeDamageSpecWithMultipliers(
			DirectImpactDamageMultiplier.GetValueAtLevel(AbilityLevel),
			DirectImpactPoiseMultiplier.GetValueAtLevel(AbilityLevel),
			DirectImpactBuildupMultiplier.GetValueAtLevel(AbilityLevel));
	}

	return AreaProjectile->ConfigureAreaImpact(
		RuntimeConfig,
		ExplosionSpec,
		DirectSpec);
}
