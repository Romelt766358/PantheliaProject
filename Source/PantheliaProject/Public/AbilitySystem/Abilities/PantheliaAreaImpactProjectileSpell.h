// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PantheliaProjectileSpell.h"
#include "AbilitySystem/PantheliaAreaImpactTypes.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "PantheliaAreaImpactProjectileSpell.generated.h"

class APantheliaAreaImpactProjectile;

/**
 * UPantheliaAreaImpactProjectileSpell
 *
 * Familia genérica para proyectiles que detonan. Resuelve dirección, punto de pies,
 * homing limitado y snapshots separados de impacto directo/explosión antes del spawn.
 * No contiene reglas de Fire ni del corazón activo.
 */
UCLASS(Abstract)
class PANTHELIAPROJECT_API UPantheliaAreaImpactProjectileSpell : public UPantheliaProjectileSpell
{
	GENERATED_BODY()

public:
	UPantheliaAreaImpactProjectileSpell();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;

	EPantheliaAreaImpactDamagePolicy GetAreaDamagePolicyForEditor() const { return DamagePolicy; }
	const FScalableFloat& GetExplosionRadiusForEditor() const { return ExplosionRadius; }
	int32 GetMaxAffectedTargetsForEditor() const { return MaxAffectedTargets; }
	EPantheliaElement GetAreaElementForEditor() const { return AreaElement; }
	const FGameplayTag& GetSourceAbilityTagForEditor() const { return SourceAbilityTag; }
	const FScalableFloat& GetDirectDamageMultiplierForEditor() const { return DirectImpactDamageMultiplier; }
	const FScalableFloat& GetDirectPoiseMultiplierForEditor() const { return DirectImpactPoiseMultiplier; }
	const FScalableFloat& GetDirectBuildupMultiplierForEditor() const { return DirectImpactBuildupMultiplier; }
	const FScalableFloat& GetExplosionDamageMultiplierForEditor() const { return ExplosionDamageMultiplier; }
	const FScalableFloat& GetExplosionPoiseMultiplierForEditor() const { return ExplosionPoiseMultiplier; }
	const FScalableFloat& GetExplosionBuildupMultiplierForEditor() const { return ExplosionBuildupMultiplier; }
	const FScalableFloat& GetProjectileSpeedOverrideForEditor() const { return ProjectileSpeedOverride; }
	bool IsSoftHomingEnabledForEditor() const { return bEnableSoftHoming; }
	const FScalableFloat& GetHomingStartDelayForEditor() const { return HomingStartDelay; }
	const FScalableFloat& GetHomingDurationForEditor() const { return HomingDuration; }
	const FScalableFloat& GetHomingAccelerationForEditor() const { return HomingAccelerationMagnitude; }
	const FScalableFloat& GetMaxHomingCorrectionAngleForEditor() const { return MaxHomingCorrectionAngleDegrees; }
	const FScalableFloat& GetGroundTraceUpDistanceForEditor() const { return GroundTraceUpDistance; }
	const FScalableFloat& GetGroundTraceDownDistanceForEditor() const { return GroundTraceDownDistance; }
	const FScalableFloat& GetGroundSurfaceOffsetForEditor() const { return GroundSurfaceOffset; }
	bool DoesExplosionRequireLineOfSightForEditor() const { return bExplosionRequiresLineOfSight; }
	ECollisionChannel GetExplosionLineOfSightTraceChannelForEditor() const
	{
		return ExplosionLineOfSightTraceChannel.GetValue();
	}
#endif

protected:
	// Llamar desde el Gameplay Event del montage en el Blueprint concreto.
	UFUNCTION(BlueprintCallable, Category = "Projectile|Area Impact")
	void SpawnAreaImpactProjectile();

	virtual bool ConfigureProjectileBeforeFinishSpawning(
		APantheliaProjectile* Projectile) override;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact")
	EPantheliaProjectileAimPointMode AimPointMode =
		EPantheliaProjectileAimPointMode::GroundUnderTarget;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact|Target", meta = (ClampMin = "0.0"))
	FScalableFloat GroundTraceUpDistance = FScalableFloat(150.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact|Target", meta = (ClampMin = "0.0"))
	FScalableFloat GroundTraceDownDistance = FScalableFloat(500.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact|Target", meta = (ClampMin = "0.0"))
	FScalableFloat GroundSurfaceOffset = FScalableFloat(5.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact|Target")
	TEnumAsByte<ECollisionChannel> GroundTraceChannel = ECC_Visibility;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact")
	EPantheliaAreaImpactDamagePolicy DamagePolicy =
		EPantheliaAreaImpactDamagePolicy::ExplosionOnly;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact")
	EPantheliaElement AreaElement = EPantheliaElement::None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact")
	FGameplayTag SourceAbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact", meta = (ClampMin = "0.0"))
	FScalableFloat ExplosionRadius = FScalableFloat(250.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact", meta = (ClampMin = "1", ClampMax = "128"))
	int32 MaxAffectedTargets = 32;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact")
	bool bDirectTargetReceivesExplosion = true;

	// La familia genérica conserva false para permitir ondas que atraviesen obstáculos.
	// El Blueprint concreto de Fireburst debe activarlo.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact|Line of Sight")
	bool bExplosionRequiresLineOfSight = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact|Line of Sight")
	TEnumAsByte<ECollisionChannel> ExplosionLineOfSightTraceChannel = ECC_Visibility;

	// Los tres multiplicadores son independientes para que un perk de impacto directo
	// no duplique automáticamente postura o buildup.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact|Direct", meta = (ClampMin = "0.0"))
	FScalableFloat DirectImpactDamageMultiplier = FScalableFloat(0.5f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact|Direct", meta = (ClampMin = "0.0"))
	FScalableFloat DirectImpactPoiseMultiplier = FScalableFloat(0.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact|Direct", meta = (ClampMin = "0.0"))
	FScalableFloat DirectImpactBuildupMultiplier = FScalableFloat(0.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact|Explosion", meta = (ClampMin = "0.0"))
	FScalableFloat ExplosionDamageMultiplier = FScalableFloat(1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact|Explosion", meta = (ClampMin = "0.0"))
	FScalableFloat ExplosionPoiseMultiplier = FScalableFloat(1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact|Explosion", meta = (ClampMin = "0.0"))
	FScalableFloat ExplosionBuildupMultiplier = FScalableFloat(1.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact|Movement", meta = (ClampMin = "0.0"))
	FScalableFloat ProjectileSpeedOverride = FScalableFloat(0.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact|Homing")
	bool bEnableSoftHoming = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact|Homing", meta = (ClampMin = "0.0"))
	FScalableFloat HomingStartDelay = FScalableFloat(0.05f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact|Homing", meta = (ClampMin = "0.0"))
	FScalableFloat HomingDuration = FScalableFloat(0.35f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact|Homing", meta = (ClampMin = "0.0"))
	FScalableFloat HomingAccelerationMagnitude = FScalableFloat(900.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Area Impact|Homing", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	FScalableFloat MaxHomingCorrectionAngleDegrees = FScalableFloat(35.f);
};
