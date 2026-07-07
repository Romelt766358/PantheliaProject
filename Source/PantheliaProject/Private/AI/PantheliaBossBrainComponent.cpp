// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/PantheliaBossBrainComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/PantheliaAbilitySystemComponent.h"
#include "AbilitySystem/PantheliaAttributeSet.h"
#include "Characters/PantheliaEnemy.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interfaces/CombatInterface.h"
#include "Engine/World.h"
#include "TimerManager.h"

UPantheliaBossBrainComponent::UPantheliaBossBrainComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

// Initialization

void UPantheliaBossBrainComponent::BeginPlay()
{
	Super::BeginPlay();

	CacheOwnerData();
	TryInitializeNextTick();
}

void UPantheliaBossBrainComponent::CacheOwnerData()
{
	OwnerEnemy = Cast<APantheliaEnemy>(GetOwner());
	if (!OwnerEnemy)
	{
		return;
	}

	OwnerASC = OwnerEnemy->GetAbilitySystemComponent();
	OwnerAttributeSet = Cast<UPantheliaAttributeSet>(OwnerEnemy->GetAttributeSet());
}

void UPantheliaBossBrainComponent::TryInitializeNextTick()
{
	// Los componentes pueden hacer BeginPlay antes de que APantheliaEnemy haya terminado
	// InitAbilityActorInfo() + InitializeDefaultAttributes(). Diferimos un tick para que
	// el perfil manual del boss pise los defaults del arquetipo, no al revés.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimerForNextTick(this, &UPantheliaBossBrainComponent::DeferredInitializeBossFromProfile);
	}
}

void UPantheliaBossBrainComponent::DeferredInitializeBossFromProfile()
{
	InitializeBossFromProfile();
}

bool UPantheliaBossBrainComponent::InitializeBossFromProfile()
{
	CacheOwnerData();

	if (!OwnerEnemy || !OwnerASC || !OwnerAttributeSet || !BossProfile)
	{
		return false;
	}

	const FPantheliaBossStatsPreset* StatsPreset = BossProfile->FindStatsPreset(ActiveStatsPresetID);
	if (!StatsPreset)
	{
		return false;
	}

	ApplyStatsPreset(*StatsPreset);
	UpdatePhaseFromHealth();
	ResetAllActionCooldowns();
	ClearCurrentAction();
	return true;
}

bool UPantheliaBossBrainComponent::ApplyStatsPreset(const FPantheliaBossStatsPreset& StatsPreset) const
{
	if (!OwnerEnemy || !OwnerASC)
	{
		return false;
	}

	OwnerASC->SetNumericAttributeBase(UPantheliaAttributeSet::GetMaxHealthAttribute(), StatsPreset.MaxHealth);
	OwnerASC->SetNumericAttributeBase(UPantheliaAttributeSet::GetHealthAttribute(), StatsPreset.MaxHealth);

	OwnerASC->SetNumericAttributeBase(UPantheliaAttributeSet::GetArmorAttribute(), StatsPreset.Armor);
	OwnerASC->SetNumericAttributeBase(UPantheliaAttributeSet::GetMagicResistanceAttribute(), StatsPreset.MagicResistance);

	OwnerASC->SetNumericAttributeBase(UPantheliaAttributeSet::GetMaxPoiseAttribute(), StatsPreset.MaxPoise);
	OwnerASC->SetNumericAttributeBase(UPantheliaAttributeSet::GetPoiseAttribute(), StatsPreset.MaxPoise);

	OwnerASC->SetNumericAttributeBase(UPantheliaAttributeSet::GetFireResistanceAttribute(), StatsPreset.FireResistance);
	OwnerASC->SetNumericAttributeBase(UPantheliaAttributeSet::GetWaterResistanceAttribute(), StatsPreset.WaterResistance);
	OwnerASC->SetNumericAttributeBase(UPantheliaAttributeSet::GetStormResistanceAttribute(), StatsPreset.StormResistance);
	OwnerASC->SetNumericAttributeBase(UPantheliaAttributeSet::GetNatureResistanceAttribute(), StatsPreset.NatureResistance);

	OwnerEnemy->BaseWalkSpeed = StatsPreset.BaseWalkSpeed;
	OwnerEnemy->SetDefensiveElement(StatsPreset.DefensiveElement);

	if (UCharacterMovementComponent* MovementComp = OwnerEnemy->GetCharacterMovement())
	{
		MovementComp->MaxWalkSpeed = StatsPreset.BaseWalkSpeed;
	}

	return true;
}

// Target Context

bool UPantheliaBossBrainComponent::HasValidTargetContext(AActor* TargetActor) const
{
	return OwnerEnemy && OwnerASC && TargetActor;
}

void UPantheliaBossBrainComponent::SetCombatTarget(AActor* TargetActor) const
{
	if (OwnerEnemy)
	{
		OwnerEnemy->SetCombatTarget_Implementation(TargetActor);
	}
}

bool UPantheliaBossBrainComponent::HasLineOfSightToTarget(AActor* TargetActor) const
{
	if (!OwnerEnemy || !TargetActor || !GetWorld())
	{
		return false;
	}

	FHitResult HitResult;
	const FVector Start = OwnerEnemy->GetActorLocation();
	const FVector End = TargetActor->GetActorLocation();

	FCollisionQueryParams Params(SCENE_QUERY_STAT(BossActionLineOfSight), false, OwnerEnemy);
	Params.AddIgnoredActor(TargetActor);

	const bool bHit = GetWorld()->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, Params);
	return !bHit;
}

float UPantheliaBossBrainComponent::GetDistanceToTarget(AActor* TargetActor) const
{
	if (!OwnerEnemy || !TargetActor)
	{
		return TNumericLimits<float>::Max();
	}

	return FVector::Dist2D(OwnerEnemy->GetActorLocation(), TargetActor->GetActorLocation());
}

float UPantheliaBossBrainComponent::GetAngleToTarget(AActor* TargetActor) const
{
	if (!OwnerEnemy || !TargetActor)
	{
		return 180.f;
	}

	const FVector ToTarget = (TargetActor->GetActorLocation() - OwnerEnemy->GetActorLocation()).GetSafeNormal2D();
	const FVector Forward = OwnerEnemy->GetActorForwardVector().GetSafeNormal2D();
	const float Dot = FMath::Clamp(FVector::DotProduct(Forward, ToTarget), -1.f, 1.f);
	return FMath::RadiansToDegrees(FMath::Acos(Dot));
}

// Phase Runtime

bool UPantheliaBossBrainComponent::UpdatePhaseFromHealth()
{
	if (!BossProfile)
	{
		return false;
	}

	const float HealthPercent = GetHealthPercent();
	const FPantheliaBossPhaseDefinition* BestPhase = nullptr;
	float BestThreshold = TNumericLimits<float>::Lowest();

	for (const FPantheliaBossPhaseDefinition& Phase : BossProfile->Phases)
	{
		if (HealthPercent <= Phase.EnterHealthPercent && Phase.EnterHealthPercent >= BestThreshold)
		{
			BestPhase = &Phase;
			BestThreshold = Phase.EnterHealthPercent;
		}
	}

	if (!BestPhase && BossProfile->Phases.Num() > 0)
	{
		BestPhase = &BossProfile->Phases[0];
	}

	if (!BestPhase)
	{
		return false;
	}

	const bool bChangedPhase = ActivePhaseID != BestPhase->PhaseID;
	ActivePhaseID = BestPhase->PhaseID;
	return bChangedPhase;
}

float UPantheliaBossBrainComponent::GetHealthPercent() const
{
	if (!OwnerAttributeSet)
	{
		return 1.f;
	}

	const float MaxHealth = OwnerAttributeSet->GetMaxHealth();
	if (MaxHealth <= 0.f)
	{
		return 1.f;
	}

	return FMath::Clamp(OwnerAttributeSet->GetHealth() / MaxHealth, 0.f, 1.f);
}

bool UPantheliaBossBrainComponent::IsActionInActivePhase(const FPantheliaBossActionDefinition& Action) const
{
	if (ActivePhaseID.IsNone())
	{
		return true;
	}

	if (const FPantheliaBossPhaseDefinition* Phase = BossProfile ? BossProfile->FindPhase(ActivePhaseID) : nullptr)
	{
		if (Phase->ExplicitActionPool.Num() > 0 && !Phase->ExplicitActionPool.Contains(Action.ActionTag))
		{
			return false;
		}
	}

	return Action.ValidPhases.Num() == 0 || Action.ValidPhases.Contains(ActivePhaseID);
}

// Decision Cooldowns

void UPantheliaBossBrainComponent::ResetActionCooldown(const FGameplayTag& ActionTag)
{
	ActionCooldownEndTimes.Remove(ActionTag);
}

void UPantheliaBossBrainComponent::ResetAllActionCooldowns()
{
	ActionCooldownEndTimes.Empty();
}

bool UPantheliaBossBrainComponent::IsActionOnCooldown(const FGameplayTag& ActionTag) const
{
	if (const float* CooldownEndTime = ActionCooldownEndTimes.Find(ActionTag))
	{
		return GetCurrentTimeSeconds() < *CooldownEndTime;
	}

	return false;
}

void UPantheliaBossBrainComponent::StartActionCooldown(const FPantheliaBossActionDefinition& Action)
{
	if (Action.ActionTag.IsValid() && Action.Cooldown > 0.f)
	{
		ActionCooldownEndTimes.Add(Action.ActionTag, GetCurrentTimeSeconds() + Action.Cooldown);
	}
}

// Action Selection

bool UPantheliaBossBrainComponent::SelectAction(AActor* TargetActor, FPantheliaBossActionDefinition& OutAction)
{
	if (!BossProfile)
	{
		SetActionFailure(EPantheliaBossActionFailureReason::NoProfile, true);
		return false;
	}

	if (!TargetActor)
	{
		SetActionFailure(EPantheliaBossActionFailureReason::NoTarget, true);
		return false;
	}

	TArray<const FPantheliaBossActionDefinition*> Candidates;
	TArray<float> CandidateWeights;
	float TotalWeight = 0.f;

	for (const FPantheliaBossActionDefinition& Action : BossProfile->Actions)
	{
		float ActionWeight = 0.f;
		if (!IsActionAvailable(Action, TargetActor, ActionWeight))
		{
			continue;
		}

		Candidates.Add(&Action);
		CandidateWeights.Add(ActionWeight);
		TotalWeight += ActionWeight;
	}

	if (Candidates.Num() <= 0)
	{
		SetActionFailure(BossProfile->Actions.Num() <= 0
			? EPantheliaBossActionFailureReason::NoAction
			: EPantheliaBossActionFailureReason::ActionUnavailable,
			true);
		return false;
	}

	if (TotalWeight <= 0.f)
	{
		SetActionFailure(EPantheliaBossActionFailureReason::ActionUnavailable, true);
		return false;
	}

	float Roll = FMath::FRandRange(0.f, TotalWeight);
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		Roll -= CandidateWeights[Index];
		if (Roll <= 0.f)
		{
			OutAction = *Candidates[Index];
			SetSelectedAction(TargetActor, OutAction);
			return true;
		}
	}

	OutAction = *Candidates.Last();
	SetSelectedAction(TargetActor, OutAction);
	return true;
}

bool UPantheliaBossBrainComponent::IsActionAvailable(const FPantheliaBossActionDefinition& Action, AActor* TargetActor, float& OutWeight) const
{
	OutWeight = 0.f;

	if (!HasValidTargetContext(TargetActor) || !Action.ActionTag.IsValid() || !Action.AbilityTag.IsValid())
	{
		return false;
	}

	if (!IsActionInActivePhase(Action))
	{
		return false;
	}

	if (IsActionOnCooldown(Action.ActionTag))
	{
		return false;
	}

	if (!PassesActionRangeChecks(Action, TargetActor))
	{
		return false;
	}

	if (Action.bRequiresLineOfSight && !HasLineOfSightToTarget(TargetActor))
	{
		return false;
	}

	if (!PassesActionTagChecks(Action))
	{
		return false;
	}

	OutWeight = FMath::Max(0.f, Action.Weight);
	OutWeight *= GetActionPhaseWeightMultiplier();

	return OutWeight > 0.f;
}

bool UPantheliaBossBrainComponent::PassesActionRangeChecks(const FPantheliaBossActionDefinition& Action, AActor* TargetActor) const
{
	const float Distance = GetDistanceToTarget(TargetActor);
	if (Distance < Action.MinDistance || Distance > Action.MaxDistance)
	{
		return false;
	}

	const float Angle = GetAngleToTarget(TargetActor);
	return Angle <= Action.MaxAngle;
}

bool UPantheliaBossBrainComponent::PassesActionTagChecks(const FPantheliaBossActionDefinition& Action) const
{
	if (!OwnerASC)
	{
		return false;
	}

	FGameplayTagContainer OwnedTags;
	OwnerASC->GetOwnedGameplayTags(OwnedTags);

	return OwnedTags.HasAll(Action.RequiredOwnerTags) && !OwnedTags.HasAny(Action.BlockedOwnerTags);
}

float UPantheliaBossBrainComponent::GetActionPhaseWeightMultiplier() const
{
	if (const FPantheliaBossPhaseDefinition* Phase = BossProfile ? BossProfile->FindPhase(ActivePhaseID) : nullptr)
	{
		return FMath::Max(0.f, Phase->WeightMultiplier);
	}

	return 1.f;
}

// Action Execution

bool UPantheliaBossBrainComponent::TryExecuteAction(AActor* TargetActor, const FGameplayTag& ActionTag)
{
	if (!OwnerEnemy)
	{
		SetActionFailure(EPantheliaBossActionFailureReason::InvalidAction, true);
		return false;
	}

	if (!OwnerASC)
	{
		SetActionFailure(EPantheliaBossActionFailureReason::MissingASC, true);
		return false;
	}

	if (!BossProfile)
	{
		SetActionFailure(EPantheliaBossActionFailureReason::NoProfile, true);
		return false;
	}

	if (!ActionTag.IsValid())
	{
		SetActionFailure(EPantheliaBossActionFailureReason::InvalidAction, true);
		return false;
	}

	const FPantheliaBossActionDefinition* Action = BossProfile->FindAction(ActionTag);
	if (!Action)
	{
		SetActionFailure(EPantheliaBossActionFailureReason::NoAction, true);
		return false;
	}

	SetSelectedAction(TargetActor, *Action);

	float ActionWeight = 0.f;
	if (!IsActionAvailable(*Action, TargetActor, ActionWeight))
	{
		SetActionFailure(!TargetActor
			? EPantheliaBossActionFailureReason::NoTarget
			: EPantheliaBossActionFailureReason::ActionUnavailable,
			false);
		return false;
	}

	CurrentActionState = EPantheliaBossActionRuntimeState::Starting;
	bActionActivationRequested = false;

	SetCombatTarget(TargetActor);
	const bool bActivated = ActivateActionAbility(*Action);

	if (!bActivated)
	{
		SetActionFailure(EPantheliaBossActionFailureReason::AbilityActivationFailed, false);
		return false;
	}

	CurrentActionState = EPantheliaBossActionRuntimeState::Running;
	LastFailureReason = EPantheliaBossActionFailureReason::None;
	bActionActivationRequested = true;

	if (Action->Cooldown > 0.f)
	{
		StartActionCooldown(*Action);
	}

	return true;
}

void UPantheliaBossBrainComponent::RefreshActionRuntimeState()
{
	if (CurrentActionState != EPantheliaBossActionRuntimeState::Running)
	{
		return;
	}

	if (!CurrentAbilityTag.IsValid())
	{
		SetActionFailure(EPantheliaBossActionFailureReason::InvalidAction, false);
		return;
	}

	if (!OwnerEnemy)
	{
		SetActionFailure(EPantheliaBossActionFailureReason::InvalidAction, false);
		return;
	}

	UPantheliaAbilitySystemComponent* PantheliaASC =
		Cast<UPantheliaAbilitySystemComponent>(OwnerEnemy->GetAbilitySystemComponent());
	if (!PantheliaASC)
	{
		SetActionFailure(EPantheliaBossActionFailureReason::MissingASC, false);
		return;
	}

	const FGameplayAbilitySpec* AbilitySpec = PantheliaASC->GetSpecFromAbilityTag(CurrentAbilityTag);
	if (!AbilitySpec)
	{
		SetActionFailure(EPantheliaBossActionFailureReason::InvalidAction, false);
		return;
	}

	if (AbilitySpec->IsActive())
	{
		return;
	}

	CurrentActionState = EPantheliaBossActionRuntimeState::Finished;
	LastFailureReason = EPantheliaBossActionFailureReason::AbilityEnded;
	bActionActivationRequested = false;
}

void UPantheliaBossBrainComponent::MarkActionInterrupted()
{
	if (CurrentActionState != EPantheliaBossActionRuntimeState::Selected
		&& CurrentActionState != EPantheliaBossActionRuntimeState::Starting
		&& CurrentActionState != EPantheliaBossActionRuntimeState::Running)
	{
		return;
	}

	CurrentActionState = EPantheliaBossActionRuntimeState::Interrupted;
	LastFailureReason = EPantheliaBossActionFailureReason::Interrupted;
	bActionActivationRequested = false;
}

bool UPantheliaBossBrainComponent::HasSelectedAction() const
{
	return CurrentActionTag.IsValid();
}

bool UPantheliaBossBrainComponent::IsActionRunning() const
{
	return CurrentActionState == EPantheliaBossActionRuntimeState::Running;
}

bool UPantheliaBossBrainComponent::HasActionFinished() const
{
	return CurrentActionState == EPantheliaBossActionRuntimeState::Finished;
}

bool UPantheliaBossBrainComponent::HasActionFailed() const
{
	return CurrentActionState == EPantheliaBossActionRuntimeState::Failed;
}

void UPantheliaBossBrainComponent::ClearCurrentAction()
{
	CurrentActionTag = FGameplayTag();
	CurrentAbilityTag = FGameplayTag();
	CurrentActionState = EPantheliaBossActionRuntimeState::None;
	LastFailureReason = EPantheliaBossActionFailureReason::None;
	CurrentTargetActor.Reset();
	bActionActivationRequested = false;
}

bool UPantheliaBossBrainComponent::ActivateActionAbility(const FPantheliaBossActionDefinition& Action) const
{
	if (!OwnerASC || !Action.AbilityTag.IsValid())
	{
		return false;
	}

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(Action.AbilityTag);
	return OwnerASC->TryActivateAbilitiesByTag(AbilityTags);
}

void UPantheliaBossBrainComponent::SetSelectedAction(AActor* TargetActor, const FPantheliaBossActionDefinition& Action)
{
	CurrentActionTag = Action.ActionTag;
	CurrentAbilityTag = Action.AbilityTag;
	CurrentTargetActor = TargetActor;
	CurrentActionState = EPantheliaBossActionRuntimeState::Selected;
	LastFailureReason = EPantheliaBossActionFailureReason::None;
	bActionActivationRequested = false;
}

void UPantheliaBossBrainComponent::SetActionFailure(EPantheliaBossActionFailureReason FailureReason, bool bClearAction)
{
	if (bClearAction)
	{
		CurrentActionTag = FGameplayTag();
		CurrentAbilityTag = FGameplayTag();
		CurrentTargetActor.Reset();
	}

	CurrentActionState = EPantheliaBossActionRuntimeState::Failed;
	LastFailureReason = FailureReason;
	bActionActivationRequested = false;
}

// Utility / Queries

float UPantheliaBossBrainComponent::GetCurrentTimeSeconds() const
{
	return GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
}
