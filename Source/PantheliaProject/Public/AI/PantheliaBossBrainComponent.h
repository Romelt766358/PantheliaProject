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

	UPROPERTY(BlueprintReadOnly, Category = "Panthelia|Boss")
	FGameplayTag ActivePhaseTag;

	// Logs ligeros para validar selección ponderada desde Output Log.
	// Mantener desactivado fuera de pruebas para no ensuciar el log.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panthelia|Boss|Debug")
	bool bLogActionSelection = false;

	// Muestra en pantalla qué acción se seleccionó/ejecutó. Útil cuando las animaciones
	// se parecen o se interrumpen demasiado rápido para identificarlas visualmente.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panthelia|Boss|Debug")
	bool bShowActionDebugOnScreen = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panthelia|Boss|Debug", meta = (ClampMin = "0.1", UIMin = "0.1", UIMax = "5.0"))
	float ActionDebugScreenDuration = 1.25f;

	// Memoria ligera anti-repetición. No decide acciones por sí sola: solo ajusta pesos
	// o bloquea repeticiones excesivas después de que una acción haya sido ejecutada.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panthelia|Boss|Memory")
	bool bUseActionMemory = true;

	// Cantidad de grupos de acción recientes recordados para aplicar RecentRepeatWeightMultiplier.
	// 0 desactiva la penalización reciente, pero mantiene la penalización inmediata.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panthelia|Boss|Memory", meta = (ClampMin = "0", UIMin = "0", UIMax = "5"))
	int32 RecentActionMemorySize = 3;

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

	UFUNCTION(BlueprintCallable, Category = "Panthelia|Boss")
	void ResetActionMemory();

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

	UPROPERTY(Transient)
	TArray<FGameplayTag> RecentActionMemoryGroups;

	UPROPERTY(Transient)
	FGameplayTag LastActionMemoryGroup;

	UPROPERTY(Transient)
	int32 ConsecutiveMemoryGroupUses = 0;

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
	bool IsActionAvailable(const FPantheliaBossActionDefinition& Action, AActor* TargetActor, float& OutWeight, bool bIgnoreMemory = false) const;
	bool PassesActionRangeChecks(const FPantheliaBossActionDefinition& Action, AActor* TargetActor) const;
	bool PassesActionTagChecks(const FPantheliaBossActionDefinition& Action) const;
	float GetActionPhaseWeightMultiplier() const;
	FGameplayTag ResolvePhaseTag(const FPantheliaBossPhaseDefinition& Phase) const;
	FGameplayTag GetActionMemoryGroup(const FPantheliaBossActionDefinition& Action) const;
	bool IsActionBlockedByMemory(const FPantheliaBossActionDefinition& Action) const;
	float GetActionMemoryWeightMultiplier(const FPantheliaBossActionDefinition& Action) const;
	bool IsMemoryGroupInRecentActions(const FGameplayTag& MemoryGroup) const;
	void RecordActionMemory(const FPantheliaBossActionDefinition& Action);
	void LogActionRejected(const FPantheliaBossActionDefinition& Action, AActor* TargetActor, const TCHAR* Reason) const;
	void ShowActionDebugMessage(const TCHAR* Prefix, const FPantheliaBossActionDefinition& Action, AActor* TargetActor) const;

	// Action Execution
	bool ActivateActionAbility(const FPantheliaBossActionDefinition& Action) const;
	void SetSelectedAction(AActor* TargetActor, const FPantheliaBossActionDefinition& Action);
	void SetActionFailure(EPantheliaBossActionFailureReason FailureReason, bool bClearAction);

	// Utility / Queries
	float GetCurrentTimeSeconds() const;
};
