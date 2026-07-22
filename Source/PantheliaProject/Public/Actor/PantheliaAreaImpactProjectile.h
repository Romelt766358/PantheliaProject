// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Actor/PantheliaProjectile.h"
#include "AbilitySystem/PantheliaAreaImpactTypes.h"
#include "PantheliaAreaImpactProjectile.generated.h"

class UNiagaraSystem;
class USoundBase;

/**
 * APantheliaAreaImpactProjectile
 *
 * Vehículo genérico de detonación radial. No conoce Fireburst, corazones ni perks:
 * recibe una configuración runtime ya resuelta y dos templates de spec independientes
 * (impacto directo opcional + explosión). Cada target radial recibe un clone con
 * EffectContext propio antes de aplicar el GE.
 */
UCLASS()
class PANTHELIAPROJECT_API APantheliaAreaImpactProjectile : public APantheliaProjectile
{
	GENERATED_BODY()

public:
	// Debe llamarse durante SpawnActorDeferred, antes de FinishSpawning.
	bool ConfigureAreaImpact(
		const FPantheliaAreaImpactRuntimeConfig& InRuntimeConfig,
		const FGameplayEffectSpecHandle& InExplosionDamageSpecTemplate,
		const FGameplayEffectSpecHandle& InDirectImpactDamageSpecTemplate);

	UFUNCTION(BlueprintPure, Category = "Projectile|Area Impact")
	FPantheliaAreaImpactRuntimeConfig GetAreaImpactRuntimeConfig() const
	{
		return RuntimeConfig;
	}

protected:
	virtual void HandleActorImpact(
		AActor* OtherActor,
		const FHitResult& ImpactResult) override;
	virtual void HandleWorldImpact(const FHitResult& ImpactResult) override;

	// Hook de presentación/telemetría. No debe aplicar curación ni interpretar el
	// corazón; solo informa el resultado de una detonación ya resuelta.
	UFUNCTION(BlueprintImplementableEvent, Category = "Projectile|Area Impact", meta = (DisplayName = "On Area Impact Resolved"))
	void K2_OnAreaImpactResolved(const FPantheliaAreaImpactResult& Result);

private:
	void TryDetonate(
		AActor* DirectHitActor,
		const FVector& ImpactLocation,
		bool bWasWorldImpact);

	void CollectExplosionTargets(
		const FVector& ImpactLocation,
		AActor* DirectHitActor,
		TArray<AActor*>& OutTargets);

	bool IsValidAreaTarget(AActor* Candidate);
	bool HasExplosionLineOfSight(
		const FVector& ImpactLocation,
		AActor* Candidate,
		AActor* DirectHitActor) const;

	FPantheliaProjectileDamageApplicationResult ApplyTemplateSpecToTarget(
		const FGameplayEffectSpecHandle& TemplateSpec,
		AActor* TargetActor,
		const FVector& ImpactDirection);

	UPROPERTY(EditDefaultsOnly, Category = "Effects|Area Impact")
	TObjectPtr<UNiagaraSystem> ExplosionEffect;

	UPROPERTY(EditDefaultsOnly, Category = "Effects|Area Impact")
	TObjectPtr<USoundBase> ExplosionSound;

	UPROPERTY(VisibleInstanceOnly, Category = "Projectile|Area Impact")
	FPantheliaAreaImpactRuntimeConfig RuntimeConfig;

	FGameplayEffectSpecHandle ExplosionDamageSpecTemplate;
	FGameplayEffectSpecHandle DirectImpactDamageSpecTemplate;
	bool bAreaImpactConfigured = false;
};
