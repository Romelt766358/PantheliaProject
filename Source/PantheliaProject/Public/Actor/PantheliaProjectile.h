// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "PantheliaProjectile.generated.h"

class USphereComponent;
class UProjectileMovementComponent;
class UNiagaraSystem;
class UAudioComponent;

/**
 * APantheliaProjectile
 *
 * Clase base para todos los proyectiles del juego (hechizos elementales, etc.).
 *
 * El proyectil lleva consigo un FGameplayEffectSpecHandle (DamageEffectSpecHandle)
 * que se crea en UPantheliaProjectileSpell::SpawnProjectile() antes de FinishSpawning.
 * Al impactar, aplica ese GE al ASC del actor golpeado.
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

	UPROPERTY(VisibleAnywhere)
	TObjectPtr<UProjectileMovementComponent> ProjectileMovement;

	// El spec handle del GE de daño. Se setea desde UPantheliaProjectileSpell::SpawnProjectile()
	// entre SpawnActorDeferred y FinishSpawning para garantizar que está listo
	// antes de que el proyectil pueda detectar overlaps.
	// ExposeOnSpawn permite setearlo también desde nodos Blueprint de Spawn Actor.
	UPROPERTY(BlueprintReadWrite, meta = (ExposeOnSpawn = true))
	FGameplayEffectSpecHandle DamageEffectSpecHandle;

protected:
	virtual void BeginPlay() override;

	UFUNCTION()
	void OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
		UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
		bool bFromSweep, const FHitResult& SweepResult);

private:
	UPROPERTY(VisibleAnywhere)
	TObjectPtr<USphereComponent> Sphere;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TObjectPtr<UNiagaraSystem> ImpactEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TObjectPtr<USoundBase> ImpactSound;

	// Protección contra doble disparo de efectos. OnSphereOverlap puede llamarse más de una
	// vez en el mismo frame si el proyectil toca varios componentes del mismo actor o del
	// suelo simultáneamente. Una vez que los efectos se reproducen y el proyectil se destruye,
	// este flag evita que se reproduzcan de nuevo antes de que Unreal procese la destrucción.
	bool bHit = false;

	UPROPERTY(EditDefaultsOnly, Category = "Effects")
	TObjectPtr<USoundBase> LoopingSound;

	UPROPERTY()
	TObjectPtr<UAudioComponent> LoopingSoundComponent;

	UPROPERTY(EditDefaultsOnly, Category = "Projectile")
	float Lifespan = 15.f;
};