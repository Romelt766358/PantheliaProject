// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "PantheliaProjectile.generated.h"

class USphereComponent;
class USceneComponent;
class UProjectileMovementComponent;
class UNiagaraSystem;
class UAudioComponent;

/**
 * FPantheliaProjectileHomingSettings
 *
 * Configuración runtime para una asistencia de seguimiento limitada.
 * No representa un misil perfecto: el proyectil solo corrige durante una ventana
 * concreta y abandona el seguimiento si el objetivo sale del cono permitido respecto
 * a la dirección inicial del disparo.
 */
USTRUCT(BlueprintType)
struct FPantheliaProjectileHomingSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Homing")
	bool bEnabled = false;

	// Tiempo que el proyectil viaja libremente antes de comenzar a corregir.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Homing", meta = (ClampMin = "0.0"))
	float StartDelay = 0.05f;

	// Duración máxima de la corrección. Cero desactiva el seguimiento.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Homing", meta = (ClampMin = "0.0"))
	float Duration = 0.5f;

	// Aceleración aplicada por UProjectileMovementComponent hacia el target.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Homing", meta = (ClampMin = "0.0"))
	float AccelerationMagnitude = 1200.f;

	// Cono total permitido respecto a la dirección inicial. Evita giros de 180°.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Homing", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxCorrectionAngleDegrees = 55.f;
};

/**
 * APantheliaProjectile
 *
 * Clase base para todos los proyectiles del juego (hechizos elementales, etc.).
 *
 * El proyectil lleva consigo un FGameplayEffectSpecHandle (DamageEffectSpecHandle)
 * que se crea en UPantheliaProjectileSpell antes de FinishSpawning. Al impactar,
 * aplica ese GE al ASC del actor golpeado. Los aliados del lanzador se ignoran por
 * completo: no reciben efectos, no generan feedback y no consumen el proyectil.
 * La geometría del mundo sí puede consumirlo normalmente.
 *
 * Puede recibir una configuración opcional de soft homing. El target se sigue mediante
 * un componente proxy propio cuya posición se actualiza al punto lógico de lock-on del
 * enemigo. Así no se obliga a que HomingTargetComponent apunte al origen/pies del actor.
 *
 * Efectos visuales/sonoros configurables desde el Blueprint de cada proyectil:
 * - ImpactEffect: Niagara al impactar
 * - ImpactSound: sonido de impacto
 * - LoopingSound: sonido en loop durante el vuelo
 */
UCLASS()
class PANTHELIAPROJECT_API APantheliaProjectile : public AActor
{
	GENERATED_BODY()

public:
	APantheliaProjectile();

	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	// El spec handle del GE de daño. Se setea desde UPantheliaProjectileSpell
	// entre SpawnActorDeferred y FinishSpawning para garantizar que está listo
	// antes de que el proyectil pueda detectar overlaps.
	// ExposeOnSpawn permite setearlo también desde nodos Blueprint de Spawn Actor.
	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = true))
	FGameplayEffectSpecHandle DamageEffectSpecHandle;

	// Sobrescribe velocidad inicial y máxima después del spawn. Un valor <= 0 no hace nada.
	void SetProjectileSpeed(float InSpeed);

	// Se llama durante el spawn diferido. BeginPlay programa el inicio real del homing,
	// porque UProjectileMovementComponent necesita un target runtime ya spawneado.
	void ConfigureSoftHoming(
		AActor* InTargetActor,
		const FPantheliaProjectileHomingSettings& InSettings);

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	UFUNCTION()
	void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

	// UProjectileMovementComponent la emite al detenerse por una colisión bloqueante.
	// Se usa para geometría porque una pared no necesita GenerateOverlapEvents.
	UFUNCTION()
	void OnProjectileStopped(const FHitResult& ImpactResult);

private:
	void ConsumeProjectile(bool bPlayImpactFeedback, const FVector& ImpactLocation);

	void StartSoftHoming();
	void StopSoftHoming();
	void UpdateSoftHomingTarget();
	FVector ResolveSoftHomingTargetLocation() const;
	bool IsSoftHomingTargetInsideCorrectionCone(const FVector& TargetLocation) const;

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Sphere;

	// Proxy móvil e invisible usado como HomingTargetComponent. Está unido al actor
	// únicamente para pertenecer a su ciclo de vida; usa transformación absoluta para
	// que el movimiento del proyectil no arrastre el punto objetivo.
	UPROPERTY(VisibleAnywhere, Category = "Projectile|Homing")
	TObjectPtr<USceneComponent> HomingTargetSceneComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TObjectPtr<USoundBase> ImpactSound;

	// Protección contra doble disparo de efectos. OnSphereOverlap puede llamarse más de una
	// vez en el mismo frame si el proyectil toca varios componentes del mismo actor o del
	// suelo simultáneamente. Solo se activa tras un overlap que realmente consume el
	// proyectil; atravesar un aliado o un objetivo en i-frames mantiene este flag en false.
	bool bHit = false;

	// Actores que este proyectil ya atravesó sin consumirse. Evita que varios componentes
	// del mismo personaje disparen repetidamente el ExecCalc o el evento de perfect dodge.
	TSet<TWeakObjectPtr<AActor>> IgnoredActors;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TObjectPtr<USoundBase> LoopingSound;

	UPROPERTY()
	TObjectPtr<UAudioComponent> LoopingSoundComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float Lifespan = 15.f;

	UPROPERTY()
	TWeakObjectPtr<AActor> SoftHomingTargetActor;

	FPantheliaProjectileHomingSettings SoftHomingSettings;
	FVector SoftHomingInitialDirection = FVector::ForwardVector;
	FTimerHandle SoftHomingStartTimerHandle;
	FTimerHandle SoftHomingStopTimerHandle;
	bool bSoftHomingConfigured = false;
	bool bSoftHomingActive = false;
};
