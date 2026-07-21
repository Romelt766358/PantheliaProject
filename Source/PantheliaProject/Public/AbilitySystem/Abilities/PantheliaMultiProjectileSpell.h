// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PantheliaProjectileSpell.h"
#include "Actor/PantheliaProjectile.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "PantheliaMultiProjectileSpell.generated.h"

/**
 * UPantheliaMultiProjectileSpell
 *
 * Base genérica para la familia de hechizos que lanza varios proyectiles.
 * No conoce Fuego, Agua, Tormenta ni Naturaleza. Los Blueprints elementales cambian
 * exclusivamente datos: ProjectileClass, DamageTypes, BuildupAmounts, VFX/SFX y los
 * parámetros configurables de patrón/homing.
 *
 * SOPORTA:
 *   - Cantidad por FScalableFloat (el AbilityLevel técnico puede representar perks).
 *   - Abanico uniforme reutilizable.
 *   - Spawn simultáneo (intervalo 0) o secuencial (intervalo > 0).
 *   - Varios proyectiles impactando al mismo objetivo.
 *   - Soft homing con retraso, duración y cono máximo configurables.
 *   - Cancelación segura: EndAbility limpia proyectiles pendientes, no los ya lanzados.
 *
 * FLUJO BLUEPRINT:
 *   WaitGameplayEvent.EventReceived → SpawnProjectiles()
 *   PlayMontageAndWait.OnCompleted  → NotifyCastMontageFinished()
 *   PlayMontageAndWait.OnInterrupted/OnCancelled → EndAbility
 *
 * No conectar OnCompleted directamente a EndAbility cuando SpawnInterval > 0:
 * NotifyCastMontageFinished espera a que termine también la secuencia de spawn.
 */
UCLASS()
class PANTHELIAPROJECT_API UPantheliaMultiProjectileSpell : public UPantheliaProjectileSpell
{
	GENERATED_BODY()

public:
	UPantheliaMultiProjectileSpell();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(
		FDataValidationContext& Context) const override;

	const FScalableFloat& GetProjectileCountForEditor() const
	{
		return ProjectileCountByAbilityLevel;
	}

	int32 GetMaxProjectileCountForEditor() const
	{
		return MaxProjectileCount;
	}

	const FScalableFloat& GetProjectileSpreadForEditor() const
	{
		return ProjectileSpreadDegrees;
	}

	const FScalableFloat& GetProjectileSpawnIntervalForEditor() const
	{
		return ProjectileSpawnInterval;
	}

	const FScalableFloat& GetLaunchPitchForEditor() const
	{
		return LaunchPitchDegrees;
	}

	const FScalableFloat& GetProjectileSpeedOverrideForEditor() const
	{
		return ProjectileSpeedOverride;
	}

	bool IsSoftHomingEnabledForEditor() const
	{
		return bEnableSoftHoming;
	}

	const FScalableFloat& GetHomingStartDelayForEditor() const
	{
		return HomingStartDelay;
	}

	const FScalableFloat& GetHomingDurationForEditor() const
	{
		return HomingDuration;
	}

	const FScalableFloat& GetHomingAccelerationForEditor() const
	{
		return HomingAccelerationMagnitude;
	}

	const FScalableFloat& GetMaxHomingCorrectionAngleForEditor() const
	{
		return MaxHomingCorrectionAngleDegrees;
	}
#endif

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	// Inicia el patrón en el momento exacto del Anim Notify.
	UFUNCTION(BlueprintCallable, Category = "Projectile|Multi")
	void SpawnProjectiles();

	// Llamar desde OnCompleted de PlayMontageAndWait. La ability solo termina cuando
	// el montage Y la secuencia de spawn han finalizado.
	UFUNCTION(BlueprintCallable, Category = "Projectile|Multi")
	void NotifyCastMontageFinished();

	UFUNCTION(BlueprintPure, Category = "Projectile|Multi")
	int32 GetResolvedProjectileCount() const;

	UFUNCTION(BlueprintPure, Category = "Projectile|Multi")
	float GetResolvedProjectileSpreadDegrees() const;

	UFUNCTION(BlueprintPure, Category = "Projectile|Multi")
	float GetResolvedProjectileSpawnInterval() const;

	// Herramienta de desarrollo. No spawnea proyectiles ni modifica gameplay.
	UFUNCTION(BlueprintCallable, Category = "Projectile|Multi|Debug")
	void DrawProjectileSpreadDebug(int32 ProjectileCountOverride = 0) const;

protected:
	// Cantidad final evaluada al AbilityLevel técnico. No implica una regla fija de
	// “nivel = proyectil”: el diseñador define la curva. Ejemplo 3, 5, 7.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Pattern")
	FScalableFloat ProjectileCountByAbilityLevel = FScalableFloat(3.f);

	// Límite defensivo, no valor de balance.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Pattern",
		meta = (ClampMin = "1", ClampMax = "64"))
	int32 MaxProjectileCount = 32;

	// Arco total centrado alrededor de la dirección al target.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Pattern")
	FScalableFloat ProjectileSpreadDegrees = FScalableFloat(45.f);

	// 0 = todos simultáneos. >0 = un proyectil inmediatamente y los demás con este
	// intervalo. Esto permite compartir la clase entre variantes elementales.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Pattern")
	FScalableFloat ProjectileSpawnInterval = FScalableFloat(0.1f);

	// Pitch inicial del centro del abanico. Cero produce vuelo horizontal.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Pattern")
	FScalableFloat LaunchPitchDegrees = FScalableFloat(0.f);

	// Cero conserva InitialSpeed/MaxSpeed del Blueprint del proyectil. Un valor positivo
	// permite que la variante o sus futuros perks controlen la velocidad desde la ability.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Movement")
	FScalableFloat ProjectileSpeedOverride = FScalableFloat(0.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Homing")
	bool bEnableSoftHoming = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Homing")
	FScalableFloat HomingStartDelay = FScalableFloat(0.05f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Homing")
	FScalableFloat HomingDuration = FScalableFloat(0.5f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Homing")
	FScalableFloat HomingAccelerationMagnitude = FScalableFloat(1200.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Homing")
	FScalableFloat MaxHomingCorrectionAngleDegrees = FScalableFloat(55.f);

private:
	void SpawnNextProjectile();
	void MarkProjectileSequenceFinished();
	void TryCompleteAbility();
	void ClearProjectileSequenceTimer();
	void ResetRuntimeSequenceState();
	FPantheliaProjectileHomingSettings BuildResolvedHomingSettings() const;
	float GetResolvedProjectileSpeedOverride() const;
	float GetResolvedLaunchPitchDegrees() const;

	TArray<FRotator> PendingProjectileRotations;
	TWeakObjectPtr<AActor> PendingHomingTargetActor;
	FPantheliaProjectileHomingSettings ActiveHomingSettings;
	FTimerHandle ProjectileSpawnTimerHandle;
	int32 NextProjectileIndex = 0;
	float ActiveProjectileSpeedOverride = 0.f;
	float ActiveProjectileSpawnInterval = 0.f;
	bool bProjectileSequenceStarted = false;
	bool bProjectileSequenceFinished = false;
	bool bCastMontageFinished = false;
};
