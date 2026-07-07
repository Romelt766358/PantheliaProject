// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/PantheliaBossBrainComponent.h"

#include "AbilitySystemComponent.h"
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

bool UPantheliaBossBrainComponent::SelectAction(AActor* TargetActor, FPantheliaBossActionDefinition& OutAction) const
{
	if (!BossProfile || !TargetActor)
	{
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

	if (Candidates.Num() <= 0 || TotalWeight <= 0.f)
	{
		return false;
	}

	float Roll = FMath::FRandRange(0.f, TotalWeight);
	for (int32 Index = 0; Index < Candidates.Num(); ++Index)
	{
		Roll -= CandidateWeights[Index];
		if (Roll <= 0.f)
		{
			OutAction = *Candidates[Index];
			return true;
		}
	}

	OutAction = *Candidates.Last();
	return true;
}

bool UPantheliaBossBrainComponent::IsActionAvailable(const FPantheliaBossActionDefinition& Action, AActor* TargetActor, float& OutWeight) const
{
	OutWeight = 0.f;

	if (!OwnerEnemy || !OwnerASC || !TargetActor || !Action.ActionTag.IsValid() || !Action.AbilityTag.IsValid())
	{
		return false;
	}

	if (!IsActionInActivePhase(Action))
	{
		return false;
	}

	const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
	if (const float* CooldownEndTime = ActionCooldownEndTimes.Find(Action.ActionTag))
	{
		if (Now < *CooldownEndTime)
		{
			return false;
		}
	}

	const float Distance = GetDistanceToTarget(TargetActor);
	if (Distance < Action.MinDistance || Distance > Action.MaxDistance)
	{
		return false;
	}

	const float Angle = GetAngleToTarget(TargetActor);
	if (Angle > Action.MaxAngle)
	{
		return false;
	}

	if (Action.bRequiresLineOfSight && !HasLineOfSightToTarget(TargetActor))
	{
		return false;
	}

	FGameplayTagContainer OwnedTags;
	OwnerASC->GetOwnedGameplayTags(OwnedTags);

	if (!OwnedTags.HasAll(Action.RequiredOwnerTags))
	{
		return false;
	}

	if (OwnedTags.HasAny(Action.BlockedOwnerTags))
	{
		return false;
	}

	OutWeight = FMath::Max(0.f, Action.Weight);

	if (const FPantheliaBossPhaseDefinition* Phase = BossProfile->FindPhase(ActivePhaseID))
	{
		OutWeight *= FMath::Max(0.f, Phase->WeightMultiplier);
	}

	return OutWeight > 0.f;
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

bool UPantheliaBossBrainComponent::TryExecuteAction(AActor* TargetActor, const FGameplayTag& ActionTag)
{
	if (!OwnerEnemy || !OwnerASC || !BossProfile || !ActionTag.IsValid())
	{
		return false;
	}

	const FPantheliaBossActionDefinition* Action = BossProfile->FindAction(ActionTag);
	if (!Action)
	{
		return false;
	}

	float ActionWeight = 0.f;
	if (!IsActionAvailable(*Action, TargetActor, ActionWeight))
	{
		return false;
	}

	OwnerEnemy->SetCombatTarget_Implementation(TargetActor);

	FGameplayTagContainer AbilityTags;
	AbilityTags.AddTag(Action->AbilityTag);
	const bool bActivated = OwnerASC->TryActivateAbilitiesByTag(AbilityTags);

	if (bActivated && Action->Cooldown > 0.f)
	{
		const float Now = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
		ActionCooldownEndTimes.Add(Action->ActionTag, Now + Action->Cooldown);
	}

	return bActivated;
}

void UPantheliaBossBrainComponent::ResetActionCooldown(const FGameplayTag& ActionTag)
{
	ActionCooldownEndTimes.Remove(ActionTag);
}

void UPantheliaBossBrainComponent::ResetAllActionCooldowns()
{
	ActionCooldownEndTimes.Empty();
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
