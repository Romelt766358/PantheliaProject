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
 * EPantheliaMultiProjectileSpawnPattern
 *
 * SocketFan conserva el comportamiento existente: cada proyectil nace desde el socket.
 * FixedWorldLineWaves materializa grupos completos en una línea fija del mundo y los
 * lanza después uno por uno. El movimiento posterior del caster no mueve la formación.
 */
UENUM(BlueprintType)
enum class EPantheliaMultiProjectileSpawnPattern : uint8
{
	SocketFan UMETA(DisplayName = "Socket Fan"),
	FixedWorldLineWaves UMETA(DisplayName = "Fixed World Line Waves")
};

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
 *   - Formación lineal fija en mundo, materializada por oleadas.
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

	EPantheliaMultiProjectileSpawnPattern GetSpawnPatternForEditor() const
	{
		return SpawnPattern;
	}

	const FScalableFloat& GetFormationForwardOffsetForEditor() const
	{
		return FormationForwardOffset;
	}

	const FScalableFloat& GetFormationHeightOffsetForEditor() const
	{
		return FormationHeightOffset;
	}

	const FScalableFloat& GetFormationProjectileSpacingForEditor() const
	{
		return FormationProjectileSpacing;
	}

	int32 GetInitialWaveProjectileCountForEditor() const
	{
		return InitialWaveProjectileCount;
	}

	int32 GetAdditionalWaveProjectileCountForEditor() const
	{
		return AdditionalWaveProjectileCount;
	}

	const FScalableFloat& GetFormationHoldDurationForEditor() const
	{
		return FormationHoldDuration;
	}

	const FScalableFloat& GetProjectileWaveDelayForEditor() const
	{
		return ProjectileWaveDelay;
	}

	const FScalableFloat& GetFallbackTargetSearchRadiusForEditor() const
	{
		return FallbackTargetSearchRadius;
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

	// Hook visual opcional. El gameplay ya ha materializado todos los proyectiles de
	// la oleada cuando se emite. Fire Volley puede usarlo para dibujar la estela de fuego
	// desde la mano hasta la línea sin acoplar Niagara al runtime genérico.
	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile|Multi|Formation",
		meta = (DisplayName = "On Projectile Wave Materialized"))
	void K2_OnProjectileWaveMaterialized(
		int32 WaveIndex,
		const TArray<FVector>& ProjectileLocations,
		FVector SourceLocation,
		FVector LineStart,
		FVector LineEnd);

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

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Pattern")
	EPantheliaMultiProjectileSpawnPattern SpawnPattern =
		EPantheliaMultiProjectileSpawnPattern::SocketFan;

	// Arco total centrado alrededor de la dirección al target.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Pattern",
		meta = (EditCondition = "SpawnPattern == EPantheliaMultiProjectileSpawnPattern::SocketFan", EditConditionHides))
	FScalableFloat ProjectileSpreadDegrees = FScalableFloat(45.f);

	// 0 = todos simultáneos. >0 = un proyectil inmediatamente y los demás con este
	// intervalo. Esto permite compartir la clase entre variantes elementales.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Pattern")
	FScalableFloat ProjectileSpawnInterval = FScalableFloat(0.1f);

	// Pitch inicial del centro del abanico. Cero produce vuelo horizontal.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Pattern",
		meta = (EditCondition = "SpawnPattern == EPantheliaMultiProjectileSpawnPattern::SocketFan", EditConditionHides))
	FScalableFloat LaunchPitchDegrees = FScalableFloat(0.f);

	// Offset frontal desde el socket capturado al materializar la primera oleada.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Formation",
		meta = (EditCondition = "SpawnPattern == EPantheliaMultiProjectileSpawnPattern::FixedWorldLineWaves", EditConditionHides))
	FScalableFloat FormationForwardOffset = FScalableFloat(100.f);

	// Offset vertical adicional. La base ya parte del socket de la mano.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Formation",
		meta = (EditCondition = "SpawnPattern == EPantheliaMultiProjectileSpawnPattern::FixedWorldLineWaves", EditConditionHides))
	FScalableFloat FormationHeightOffset = FScalableFloat(40.f);

	// Separación espacial entre proyectiles contiguos de la línea.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Formation",
		meta = (EditCondition = "SpawnPattern == EPantheliaMultiProjectileSpawnPattern::FixedWorldLineWaves", EditConditionHides, ClampMin = "0.0"))
	FScalableFloat FormationProjectileSpacing = FScalableFloat(65.f);

	// Primera oleada: 3 para Fire Volley. Si el total es menor, se usa el total.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Formation",
		meta = (EditCondition = "SpawnPattern == EPantheliaMultiProjectileSpawnPattern::FixedWorldLineWaves", EditConditionHides, ClampMin = "1", ClampMax = "64"))
	int32 InitialWaveProjectileCount = 3;

	// Oleadas posteriores: 2 para la progresión 3/5/7/9. El sistema admite un remanente
	// menor para no acoplarse a una curva concreta.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Formation",
		meta = (EditCondition = "SpawnPattern == EPantheliaMultiProjectileSpawnPattern::FixedWorldLineWaves", EditConditionHides, ClampMin = "1", ClampMax = "64"))
	int32 AdditionalWaveProjectileCount = 2;

	// Tiempo visible entre materializar una oleada completa y lanzar el primer proyectil.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Formation",
		meta = (EditCondition = "SpawnPattern == EPantheliaMultiProjectileSpawnPattern::FixedWorldLineWaves", EditConditionHides, ClampMin = "0.0"))
	FScalableFloat FormationHoldDuration = FScalableFloat(0.15f);

	// Pausa entre el último disparo de una oleada y la materialización de la siguiente.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Formation",
		meta = (EditCondition = "SpawnPattern == EPantheliaMultiProjectileSpawnPattern::FixedWorldLineWaves", EditConditionHides, ClampMin = "0.0"))
	FScalableFloat ProjectileWaveDelay = FScalableFloat(0.2f);

	// Sin lock-on, se captura el enemigo válido más cercano dentro de este radio.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile|Multi|Formation",
		meta = (EditCondition = "SpawnPattern == EPantheliaMultiProjectileSpawnPattern::FixedWorldLineWaves", EditConditionHides, ClampMin = "1.0"))
	FScalableFloat FallbackTargetSearchRadius = FScalableFloat(1200.f);

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
	void StartSocketFanSequence();
	void SpawnNextSocketFanProjectile();

	void StartFixedWorldLineSequence();
	void MaterializeNextFixedWorldLineWave();
	void LaunchNextFixedWorldLineProjectile();
	void HandleFixedWorldLineWaveFinished();
	void DestroyUnlaunchedPreparedProjectiles();

	AActor* ResolveFormationTargetActor() const;
	bool RefreshCapturedTargetLocation();
	bool BuildFrozenFormationBasis();
	TArray<FVector> BuildCenteredLineLocations(int32 ProjectileCount) const;

	void MarkProjectileSequenceFinished();
	void TryCompleteAbility();
	void ClearProjectileSequenceTimers();
	void ResetRuntimeSequenceState();
	FPantheliaProjectileHomingSettings BuildResolvedHomingSettings() const;
	float GetResolvedProjectileSpeedOverride() const;
	float GetResolvedLaunchPitchDegrees() const;
	float GetResolvedFormationForwardOffset() const;
	float GetResolvedFormationHeightOffset() const;
	float GetResolvedFormationProjectileSpacing() const;
	float GetResolvedFormationHoldDuration() const;
	float GetResolvedProjectileWaveDelay() const;
	float GetResolvedFallbackTargetSearchRadius() const;

	TArray<FRotator> PendingProjectileRotations;
	TArray<TWeakObjectPtr<APantheliaProjectile>> PreparedWaveProjectiles;
	TWeakObjectPtr<AActor> PendingHomingTargetActor;
	FPantheliaProjectileHomingSettings ActiveHomingSettings;
	FTimerHandle ProjectileSpawnTimerHandle;
	FTimerHandle ProjectileWaveTimerHandle;
	FVector FrozenFormationSourceLocation = FVector::ZeroVector;
	FVector FrozenFormationCenter = FVector::ZeroVector;
	FVector FrozenFormationForward = FVector::ForwardVector;
	FVector FrozenFormationRight = FVector::RightVector;
	FVector LastKnownTargetLocation = FVector::ZeroVector;
	int32 NextProjectileIndex = 0;
	int32 RemainingProjectileCount = 0;
	int32 CurrentWaveIndex = 0;
	float ActiveProjectileSpeedOverride = 0.f;
	float ActiveProjectileSpawnInterval = 0.f;
	bool bHasLastKnownTargetLocation = false;
	bool bProjectileSequenceStarted = false;
	bool bProjectileSequenceFinished = false;
	bool bCastMontageFinished = false;
};
