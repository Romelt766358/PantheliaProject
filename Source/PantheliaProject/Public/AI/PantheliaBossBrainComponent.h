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

UENUM(BlueprintType)
enum class EPantheliaBossActionRuntimeState : uint8
{
	None UMETA(DisplayName = "None"),
	Selected UMETA(DisplayName = "Selected"),
	Starting UMETA(DisplayName = "Starting"),
	Running UMETA(DisplayName = "Running"),
	Finished UMETA(DisplayName = "Finished"),
	Failed UMETA(DisplayName = "Failed"),
	Interrupted UMETA(DisplayName = "Interrupted")
};

UENUM(BlueprintType)
enum class EPantheliaBossActionFailureReason : uint8
{
	None UMETA(DisplayName = "None"),
	NoProfile UMETA(DisplayName = "No Profile"),
	NoTarget UMETA(DisplayName = "No Target"),
	NoAction UMETA(DisplayName = "No Action"),
	InvalidAction UMETA(DisplayName = "Invalid Action"),
	ActionUnavailable UMETA(DisplayName = "Action Unavailable"),
	MissingASC UMETA(DisplayName = "Missing ASC"),
	AbilityActivationFailed UMETA(DisplayName = "Ability Activation Failed"),
	AbilityEnded UMETA(DisplayName = "Ability Ended"),
	Interrupted UMETA(DisplayName = "Interrupted")
};

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
	bool SelectAction(AActor* TargetActor, FPantheliaBossActionDefinition& OutAction);

	UFUNCTION(BlueprintCallable, Category = "Panthelia|Boss")
	bool TryExecuteAction(AActor* TargetActor, const FGameplayTag& ActionTag);

	UFUNCTION(BlueprintCallable, Category = "Panthelia|Boss")
	void RefreshActionRuntimeState();

	UFUNCTION(BlueprintCallable, Category = "Panthelia|Boss")
	void MarkActionInterrupted();

	UFUNCTION(BlueprintPure, Category = "Panthelia|Boss")
	bool HasSelectedAction() const;

	UFUNCTION(BlueprintPure, Category = "Panthelia|Boss")
	FGameplayTag GetCurrentActionTag() const { return CurrentActionTag; }

	UFUNCTION(BlueprintPure, Category = "Panthelia|Boss")
	FGameplayTag GetCurrentAbilityTag() const { return CurrentAbilityTag; }

	UFUNCTION(BlueprintPure, Category = "Panthelia|Boss")
	EPantheliaBossActionRuntimeState GetCurrentActionState() const { return CurrentActionState; }

	UFUNCTION(BlueprintPure, Category = "Panthelia|Boss")
	EPantheliaBossActionFailureReason GetLastFailureReason() const { return LastFailureReason; }

	UFUNCTION(BlueprintPure, Category = "Panthelia|Boss")
	bool IsActionRunning() const;

	UFUNCTION(BlueprintPure, Category = "Panthelia|Boss")
	bool HasActionFinished() const;

	UFUNCTION(BlueprintPure, Category = "Panthelia|Boss")
	bool HasActionFailed() const;

	UFUNCTION(BlueprintCallable, Category = "Panthelia|Boss")
	void ClearCurrentAction();

	UFUNCTION(BlueprintPure, Category = "Panthelia|Boss")
	float GetHealthPercent() const;

	UFUNCTION(BlueprintCallable, Category = "Panthelia|Boss")
	void ResetActionCooldown(const FGameplayTag& ActionTag);

	UFUNCTION(BlueprintCallable, Category = "Panthelia|Boss")
	void ResetAllActionCooldowns();

protected:
	virtual void BeginPlay() override;

private:
	// Initialization
	UPROPERTY()
	TObjectPtr<APantheliaEnemy> OwnerEnemy;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> OwnerASC;

	UPROPERTY()
	TObjectPtr<UPantheliaAttributeSet> OwnerAttributeSet;

	// Runtime decision cooldowns owned by the brain. Combat execution remains in GAS.
	UPROPERTY()
	TMap<FGameplayTag, float> ActionCooldownEndTimes;

	UPROPERTY(Transient)
	FGameplayTag CurrentActionTag;

	UPROPERTY(Transient)
	FGameplayTag CurrentAbilityTag;

	UPROPERTY(Transient)
	EPantheliaBossActionRuntimeState CurrentActionState = EPantheliaBossActionRuntimeState::None;

	UPROPERTY(Transient)
	EPantheliaBossActionFailureReason LastFailureReason = EPantheliaBossActionFailureReason::None;

	UPROPERTY(Transient)
	TWeakObjectPtr<AActor> CurrentTargetActor;

	UPROPERTY(Transient)
	bool bActionActivationRequested = false;

	void CacheOwnerData();
	void TryInitializeNextTick();
	void DeferredInitializeBossFromProfile();
	bool ApplyStatsPreset(const FPantheliaBossStatsPreset& StatsPreset) const;

	// Target Context
	bool HasValidTargetContext(AActor* TargetActor) const;
	void SetCombatTarget(AActor* TargetActor) const;
	bool HasLineOfSightToTarget(AActor* TargetActor) const;
	float GetDistanceToTarget(AActor* TargetActor) const;
	float GetAngleToTarget(AActor* TargetActor) const;

	// Phase Runtime
	bool IsActionInActivePhase(const FPantheliaBossActionDefinition& Action) const;

	// Decision Cooldowns
	bool IsActionOnCooldown(const FGameplayTag& ActionTag) const;
	void StartActionCooldown(const FPantheliaBossActionDefinition& Action);

	// Action Selection
	bool IsActionAvailable(const FPantheliaBossActionDefinition& Action, AActor* TargetActor, float& OutWeight) const;
	bool PassesActionRangeChecks(const FPantheliaBossActionDefinition& Action, AActor* TargetActor) const;
	bool PassesActionTagChecks(const FPantheliaBossActionDefinition& Action) const;
	float GetActionPhaseWeightMultiplier() const;

	// Action Execution
	bool ActivateActionAbility(const FPantheliaBossActionDefinition& Action) const;
	void SetSelectedAction(AActor* TargetActor, const FPantheliaBossActionDefinition& Action);
	void SetActionFailure(EPantheliaBossActionFailureReason FailureReason, bool bClearAction);

	// Utility / Queries
	float GetCurrentTimeSeconds() const;
};
