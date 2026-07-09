// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/PantheliaBossBrainComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystem/PantheliaAbilitySystemComponent.h"
#include "AbilitySystem/PantheliaAttributeSet.h"
#include "Characters/PantheliaEnemy.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interfaces/CombatInterface.h"
#include "PantheliaLogChannels.h"
#include "PantheliaGameplayTags.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
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
	ResetActionMemory();
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
	float BestThreshold = TNumericLimits<float>::Max();

	// Elegimos la fase válida con el umbral más bajo que todavía contiene la vida actual.
	// Ejemplo: Phase1=1.0, Phase2=0.5. A 40% ambas condiciones no deben dejar Phase1;
	// debe ganar Phase2 porque es la fase más específica/profunda.
	for (const FPantheliaBossPhaseDefinition& Phase : BossProfile->Phases)
	{
		if (HealthPercent <= Phase.EnterHealthPercent && Phase.EnterHealthPercent <= BestThreshold)
		{
			BestPhase = &Phase;
			BestThreshold = Phase.EnterHealthPercent;
		}
	}

	// Fallback defensivo para perfiles mal configurados donde ninguna fase cubre el
	// porcentaje actual. Se usa el umbral más alto, normalmente Phase1.
	if (!BestPhase)
	{
		float HighestThreshold = TNumericLimits<float>::Lowest();
		for (const FPantheliaBossPhaseDefinition& Phase : BossProfile->Phases)
		{
			if (Phase.EnterHealthPercent >= HighestThreshold)
			{
				BestPhase = &Phase;
				HighestThreshold = Phase.EnterHealthPercent;
			}
		}
	}

	if (!BestPhase)
	{
		return false;
	}

	const FGameplayTag NewPhaseTag = ResolvePhaseTag(*BestPhase);
	const bool bChangedPhase = ActivePhaseID != BestPhase->PhaseID || ActivePhaseTag != NewPhaseTag;
	ActivePhaseID = BestPhase->PhaseID;
	ActivePhaseTag = NewPhaseTag;

	if (bChangedPhase && bLogActionSelection)
	{
		UE_LOG(LogPanthelia, Log, TEXT("BossBrain [%s] changed phase to %s (%s) at HealthPercent %.2f"),
			*GetNameSafe(GetOwner()),
			*ActivePhaseID.ToString(),
			*ActivePhaseTag.ToString(),
			HealthPercent);
	}

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
	if (ActivePhaseID.IsNone() && !ActivePhaseTag.IsValid())
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

	// Configuración nueva: fases por GameplayTag (ej. Boss.Phase.1).
	if (!Action.ValidPhaseTags.IsEmpty())
	{
		return ActivePhaseTag.IsValid() && Action.ValidPhaseTags.HasTagExact(ActivePhaseTag);
	}

	// Compatibilidad legacy: perfiles antiguos pueden usar PhaseID como FName (ej. Phase1).
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

void UPantheliaBossBrainComponent::ResetActionMemory()
{
	RecentActionMemoryGroups.Empty();
	LastActionMemoryGroup = FGameplayTag();
	ConsecutiveMemoryGroupUses = 0;
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
		if (!IsActionAvailable(Action, TargetActor, ActionWeight, false))
		{
			continue;
		}

		Candidates.Add(&Action);
		CandidateWeights.Add(ActionWeight);
		TotalWeight += ActionWeight;
	}

	if (Candidates.Num() <= 0 && bUseActionMemory)
	{
		// Si la memoria bloquea la única acción contextual disponible (caso típico:
		// el jugador sigue lejos y solo el gap closer está en rango), hacemos una
		// segunda pasada ignorando memoria. La memoria debe mejorar variedad, no dejar
		// al boss parado sin opciones.
		for (const FPantheliaBossActionDefinition& Action : BossProfile->Actions)
		{
			float ActionWeight = 0.f;
			if (!IsActionAvailable(Action, TargetActor, ActionWeight, true))
			{
				continue;
			}

			Candidates.Add(&Action);
			CandidateWeights.Add(ActionWeight);
			TotalWeight += ActionWeight;
		}

		if (Candidates.Num() > 0 && bLogActionSelection)
		{
			UE_LOG(LogPanthelia, Log, TEXT("BossBrain [%s] allowed memory fallback because no normal candidates were available. Candidates=%d"),
				*GetNameSafe(GetOwner()),
				Candidates.Num());
		}
	}

	if (Candidates.Num() <= 0)
	{
		SetActionFailure(BossProfile->Actions.Num() <= 0
			? EPantheliaBossActionFailureReason::NoAction
			: EPantheliaBossActionFailureReason::ActionUnavailable,
			true);

		if (bLogActionSelection)
		{
			UE_LOG(LogPanthelia, Warning, TEXT("BossBrain [%s] found no valid actions. Phase=%s (%s) Distance=%.1f Angle=%.1f Failure=%d"),
				*GetNameSafe(GetOwner()),
				*ActivePhaseID.ToString(),
				*ActivePhaseTag.ToString(),
				GetDistanceToTarget(TargetActor),
				GetAngleToTarget(TargetActor),
				static_cast<int32>(LastFailureReason));
		}

		return false;
	}

	if (TotalWeight <= 0.f)
	{
		SetActionFailure(EPantheliaBossActionFailureReason::ActionUnavailable, true);
		return false;
	}

	const float InitialRoll = FMath::FRandRange(0.f, TotalWeight);
	float Roll = InitialRoll;
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		Roll -= CandidateWeights[Index];
		if (Roll <= 0.f)
		{
			OutAction = *Candidates[Index];
			SetSelectedAction(TargetActor, OutAction);
			ShowActionDebugMessage(TEXT("SELECT"), OutAction, TargetActor);

			if (bLogActionSelection)
			{
				UE_LOG(LogPanthelia, Log, TEXT("BossBrain [%s] selected Action=%s Ability=%s Weight=%.2f TotalWeight=%.2f Roll=%.2f Candidates=%d Phase=%s (%s) Distance=%.1f Angle=%.1f"),
					*GetNameSafe(GetOwner()),
					*OutAction.ActionTag.ToString(),
					*OutAction.AbilityTag.ToString(),
					CandidateWeights[Index],
					TotalWeight,
					InitialRoll,
					Candidates.Num(),
					*ActivePhaseID.ToString(),
					*ActivePhaseTag.ToString(),
					GetDistanceToTarget(TargetActor),
					GetAngleToTarget(TargetActor));
			}

			return true;
		}
	}

	OutAction = *Candidates.Last();
	SetSelectedAction(TargetActor, OutAction);
	ShowActionDebugMessage(TEXT("SELECT"), OutAction, TargetActor);

	if (bLogActionSelection)
	{
		UE_LOG(LogPanthelia, Log, TEXT("BossBrain [%s] selected fallback Action=%s Ability=%s TotalWeight=%.2f Roll=%.2f Candidates=%d Phase=%s (%s) Distance=%.1f Angle=%.1f"),
			*GetNameSafe(GetOwner()),
			*OutAction.ActionTag.ToString(),
			*OutAction.AbilityTag.ToString(),
			TotalWeight,
			InitialRoll,
			Candidates.Num(),
			*ActivePhaseID.ToString(),
			*ActivePhaseTag.ToString(),
			GetDistanceToTarget(TargetActor),
			GetAngleToTarget(TargetActor));
	}

	return true;
}

bool UPantheliaBossBrainComponent::IsActionAvailable(const FPantheliaBossActionDefinition& Action, AActor* TargetActor, float& OutWeight, const bool bIgnoreMemory) const
{
	OutWeight = 0.f;

	if (!HasValidTargetContext(TargetActor))
	{
		LogActionRejected(Action, TargetActor, TEXT("invalid target context"));
		return false;
	}

	if (!Action.ActionTag.IsValid())
	{
		LogActionRejected(Action, TargetActor, TEXT("invalid ActionTag"));
		return false;
	}

	if (!Action.AbilityTag.IsValid())
	{
		LogActionRejected(Action, TargetActor, TEXT("invalid AbilityTag"));
		return false;
	}

	if (!IsActionInActivePhase(Action))
	{
		LogActionRejected(Action, TargetActor, TEXT("inactive phase"));
		return false;
	}

	if (IsActionOnCooldown(Action.ActionTag))
	{
		LogActionRejected(Action, TargetActor, TEXT("cooldown"));
		return false;
	}

	if (!PassesActionRangeChecks(Action, TargetActor))
	{
		LogActionRejected(Action, TargetActor, TEXT("distance or angle"));
		return false;
	}

	if (Action.bRequiresLineOfSight && !HasLineOfSightToTarget(TargetActor))
	{
		LogActionRejected(Action, TargetActor, TEXT("line of sight"));
		return false;
	}

	if (!PassesActionTagChecks(Action))
	{
		LogActionRejected(Action, TargetActor, TEXT("owner tags"));
		return false;
	}

	if (!bIgnoreMemory && IsActionBlockedByMemory(Action))
	{
		LogActionRejected(Action, TargetActor, TEXT("memory limit"));
		return false;
	}

	OutWeight = FMath::Max(0.f, Action.Weight);
	OutWeight *= GetActionPhaseWeightMultiplier();
	if (!bIgnoreMemory)
	{
		OutWeight *= GetActionMemoryWeightMultiplier(Action);
	}

	if (OutWeight <= 0.f)
	{
		LogActionRejected(Action, TargetActor, TEXT("zero weight"));
		return false;
	}

	return true;
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

FGameplayTag UPantheliaBossBrainComponent::ResolvePhaseTag(const FPantheliaBossPhaseDefinition& Phase) const
{
	if (Phase.PhaseTag.IsValid())
	{
		return Phase.PhaseTag;
	}

	// Compatibilidad para perfiles existentes: PhaseID sigue siendo FName, pero el
	// sistema nuevo de acciones puede validar contra Gameplay Tags reales.
	const FPantheliaGameplayTags& Tags = FPantheliaGameplayTags::Get();
	if (Phase.PhaseID == FName("Phase1"))
	{
		return Tags.Boss_Phase_1;
	}
	if (Phase.PhaseID == FName("Phase2"))
	{
		return Tags.Boss_Phase_2;
	}

	return FGameplayTag();
}

FGameplayTag UPantheliaBossBrainComponent::GetActionMemoryGroup(const FPantheliaBossActionDefinition& Action) const
{
	return Action.MemoryGroupTag.IsValid() ? Action.MemoryGroupTag : Action.ActionTag;
}

bool UPantheliaBossBrainComponent::IsActionBlockedByMemory(const FPantheliaBossActionDefinition& Action) const
{
	if (!bUseActionMemory || Action.MaxConsecutiveUses <= 0)
	{
		return false;
	}

	const FGameplayTag MemoryGroup = GetActionMemoryGroup(Action);
	if (!MemoryGroup.IsValid() || !LastActionMemoryGroup.IsValid())
	{
		return false;
	}

	return MemoryGroup.MatchesTagExact(LastActionMemoryGroup)
		&& ConsecutiveMemoryGroupUses >= Action.MaxConsecutiveUses;
}

float UPantheliaBossBrainComponent::GetActionMemoryWeightMultiplier(const FPantheliaBossActionDefinition& Action) const
{
	if (!bUseActionMemory)
	{
		return 1.f;
	}

	const FGameplayTag MemoryGroup = GetActionMemoryGroup(Action);
	if (!MemoryGroup.IsValid())
	{
		return 1.f;
	}

	if (LastActionMemoryGroup.IsValid() && MemoryGroup.MatchesTagExact(LastActionMemoryGroup))
	{
		return FMath::Clamp(Action.ImmediateRepeatWeightMultiplier, 0.f, 1.f);
	}

	if (IsMemoryGroupInRecentActions(MemoryGroup))
	{
		return FMath::Clamp(Action.RecentRepeatWeightMultiplier, 0.f, 1.f);
	}

	return 1.f;
}

bool UPantheliaBossBrainComponent::IsMemoryGroupInRecentActions(const FGameplayTag& MemoryGroup) const
{
	if (!MemoryGroup.IsValid() || RecentActionMemorySize <= 0)
	{
		return false;
	}

	for (const FGameplayTag& RecentGroup : RecentActionMemoryGroups)
	{
		if (RecentGroup.MatchesTagExact(MemoryGroup))
		{
			return true;
		}
	}

	return false;
}

void UPantheliaBossBrainComponent::RecordActionMemory(const FPantheliaBossActionDefinition& Action)
{
	if (!bUseActionMemory)
	{
		return;
	}

	const FGameplayTag MemoryGroup = GetActionMemoryGroup(Action);
	if (!MemoryGroup.IsValid())
	{
		return;
	}

	if (LastActionMemoryGroup.IsValid() && MemoryGroup.MatchesTagExact(LastActionMemoryGroup))
	{
		++ConsecutiveMemoryGroupUses;
	}
	else
	{
		LastActionMemoryGroup = MemoryGroup;
		ConsecutiveMemoryGroupUses = 1;
	}

	if (RecentActionMemorySize <= 0)
	{
		RecentActionMemoryGroups.Empty();
		return;
	}

	RecentActionMemoryGroups.Remove(MemoryGroup);
	RecentActionMemoryGroups.Insert(MemoryGroup, 0);

	while (RecentActionMemoryGroups.Num() > RecentActionMemorySize)
	{
		RecentActionMemoryGroups.RemoveAt(RecentActionMemoryGroups.Num() - 1);
	}
}

void UPantheliaBossBrainComponent::LogActionRejected(const FPantheliaBossActionDefinition& Action, AActor* TargetActor, const TCHAR* Reason) const
{
	if (!bLogActionSelection)
	{
		return;
	}

	const float CooldownRemaining = Action.ActionTag.IsValid() && IsActionOnCooldown(Action.ActionTag)
		? FMath::Max(0.f, ActionCooldownEndTimes.FindRef(Action.ActionTag) - GetCurrentTimeSeconds())
		: 0.f;

	UE_LOG(LogPanthelia, Log, TEXT("BossBrain [%s] rejected Action=%s Ability=%s Reason=%s Phase=%s (%s) Distance=%.1f Range=[%.1f, %.1f] Angle=%.1f MaxAngle=%.1f CooldownRemaining=%.2f"),
		*GetNameSafe(GetOwner()),
		*Action.ActionTag.ToString(),
		*Action.AbilityTag.ToString(),
		Reason ? Reason : TEXT("unknown"),
		*ActivePhaseID.ToString(),
		*ActivePhaseTag.ToString(),
		GetDistanceToTarget(TargetActor),
		Action.MinDistance,
		Action.MaxDistance,
		GetAngleToTarget(TargetActor),
		Action.MaxAngle,
		CooldownRemaining);
}

void UPantheliaBossBrainComponent::ShowActionDebugMessage(const TCHAR* Prefix, const FPantheliaBossActionDefinition& Action, AActor* TargetActor) const
{
	if (!bShowActionDebugOnScreen || !GEngine)
	{
		return;
	}

	const FString Message = FString::Printf(TEXT("%s %s | Dist %.0f | Angle %.0f"),
		Prefix ? Prefix : TEXT("ACTION"),
		*Action.ActionTag.ToString(),
		GetDistanceToTarget(TargetActor),
		GetAngleToTarget(TargetActor));

	GEngine->AddOnScreenDebugMessage(-1, ActionDebugScreenDuration, FColor::Orange, Message);
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
	if (!IsActionAvailable(*Action, TargetActor, ActionWeight, true))
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

	if (bActivated)
	{
		ShowActionDebugMessage(TEXT("EXEC"), *Action, TargetActor);
	}

	if (!bActivated)
	{
		SetActionFailure(EPantheliaBossActionFailureReason::AbilityActivationFailed, false);
		return false;
	}

	CurrentActionState = EPantheliaBossActionRuntimeState::Running;
	LastFailureReason = EPantheliaBossActionFailureReason::None;
	bActionActivationRequested = true;

	RecordActionMemory(*Action);

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

	UPantheliaAbilitySystemComponent* PantheliaASC = Cast<UPantheliaAbilitySystemComponent>(OwnerASC);
	if (!PantheliaASC)
	{
		return false;
	}

	FGameplayAbilitySpec* AbilitySpec = PantheliaASC->GetSpecFromAbilityTag(Action.AbilityTag);
	if (!AbilitySpec)
	{
		if (bLogActionSelection)
		{
			UE_LOG(LogPanthelia, Warning, TEXT("BossBrain [%s] cannot activate Action=%s because Ability=%s was not granted to the ASC"),
				*GetNameSafe(GetOwner()),
				*Action.ActionTag.ToString(),
				*Action.AbilityTag.ToString());
		}

		return false;
	}

	const bool bActivated = PantheliaASC->TryActivateAbility(AbilitySpec->Handle);
	if (!bActivated && bLogActionSelection)
	{
		UE_LOG(LogPanthelia, Warning, TEXT("BossBrain [%s] failed to activate Action=%s Ability=%s"),
			*GetNameSafe(GetOwner()),
			*Action.ActionTag.ToString(),
			*Action.AbilityTag.ToString());
	}

	return bActivated;
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
