// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "AI/Data/PantheliaBossProfile.h"
#include "PantheliaBossBrainComponent.generated.h"

class APantheliaEnemy;
class UAbilitySystemComponent;
class UPantheliaAttributeSet;

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PANTHELIAPROJECT_API UPantheliaBossBrainComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPantheliaBossBrainComponent();

	// Perfil central del boss. Se asigna en el Blueprint hijo de BP_PantheliaEnemy.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panthelia|Boss")
	TObjectPtr<UPantheliaBossProfile> BossProfile;

	// Para la demo normalmente queda en None y usa BossProfile.DefaultStatsPresetID.
	// En el futuro, el sistema de historia podrá setearlo antes de inicializar.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panthelia|Boss")
	FName ActiveStatsPresetID = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Panthelia|Boss")
	FName ActivePhaseID = NAME_None;

	UFUNCTION(BlueprintCallable, Category = "Panthelia|Boss")
	bool InitializeBossFromProfile();

	UFUNCTION(BlueprintCallable, Category = "Panthelia|Boss")
	bool UpdatePhaseFromHealth();

	UFUNCTION(BlueprintCallable, Category = "Panthelia|Boss")
	bool SelectAction(AActor* TargetActor, FPantheliaBossActionDefinition& OutAction) const;

	UFUNCTION(BlueprintCallable, Category = "Panthelia|Boss")
	bool TryExecuteAction(AActor* TargetActor, const FGameplayTag& ActionTag);

	UFUNCTION(BlueprintPure, Category = "Panthelia|Boss")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintCallable, Category = "Panthelia|Boss")
	void ResetActionCooldown(const FGameplayTag& ActionTag);

	UFUNCTION(BlueprintCallable, Category = "Panthelia|Boss")
	void ResetAllActionCooldowns();

protected:
	virtual void BeginPlay() override;

private:
	UPROPERTY()
	TObjectPtr<APantheliaEnemy> OwnerEnemy;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> OwnerASC;

	UPROPERTY()
	TObjectPtr<UPantheliaAttributeSet> OwnerAttributeSet;

	UPROPERTY()
	TMap<FGameplayTag, float> ActionCooldownEndTimes;

	void CacheOwnerData();
	void TryInitializeNextTick();
	void DeferredInitializeBossFromProfile();
	bool ApplyStatsPreset(const FPantheliaBossStatsPreset& StatsPreset) const;
	bool IsActionAvailable(const FPantheliaBossActionDefinition& Action, AActor* TargetActor, float& OutWeight) const;
	bool IsActionInActivePhase(const FPantheliaBossActionDefinition& Action) const;
	bool HasLineOfSightToTarget(AActor* TargetActor) const;
	float GetDistanceToTarget(AActor* TargetActor) const;
	float GetAngleToTarget(AActor* TargetActor) const;
};
