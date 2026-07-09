// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Tasks/AbilityTask_PantheliaDashToTarget.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "PantheliaLogChannels.h"

UAbilityTask_PantheliaDashToTarget::UAbilityTask_PantheliaDashToTarget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	bTickingTask = true;
}

UAbilityTask_PantheliaDashToTarget* UAbilityTask_PantheliaDashToTarget::DashAvatarTowardActor(
	UGameplayAbility* OwningAbility,
	AActor* TargetActor,
	float StopDistance,
	float Duration,
	float MaxTravelDistance,
	bool bSweep,
	bool bStopMovementBeforeDash)
{
	UAbilityTask_PantheliaDashToTarget* Task = NewAbilityTask<UAbilityTask_PantheliaDashToTarget>(OwningAbility);
	Task->TargetActor = TargetActor;
	Task->StopDistance = FMath::Max(0.f, StopDistance);
	Task->DashDuration = FMath::Max(0.02f, Duration);
	Task->MaxTravelDistance = MaxTravelDistance;
	Task->bSweep = bSweep;
	Task->bStopMovementBeforeDash = bStopMovementBeforeDash;
	return Task;
}

void UAbilityTask_PantheliaDashToTarget::Activate()
{
	Super::Activate();

	if (!InitializeDash())
	{
		UE_LOG(LogPanthelia, Warning, TEXT("GapCloser dash task failed to initialize. Avatar=%s Target=%s"),
			*GetNameSafe(AvatarCharacter),
			*GetNameSafe(TargetActor));
		BroadcastAndFinish(OnFailed);
		return;
	}

	if (bStopMovementBeforeDash)
	{
		StopAvatarMovement();
	}

	UE_LOG(LogPanthelia, Log, TEXT("GapCloser dash task started. Avatar=%s Target=%s Start=%s End=%s Travel=%.1f Duration=%.2f Stop=%.1f"),
		*GetNameSafe(AvatarCharacter),
		*GetNameSafe(TargetActor),
		*StartLocation.ToString(),
		*EndLocation.ToString(),
		FVector::Dist2D(StartLocation, EndLocation),
		DashDuration,
		StopDistance);
}

void UAbilityTask_PantheliaDashToTarget::TickTask(float DeltaTime)
{
	Super::TickTask(DeltaTime);

	if (bDidFinish || !AvatarCharacter)
	{
		return;
	}

	ElapsedTime += DeltaTime;
	const float RawAlpha = FMath::Clamp(ElapsedTime / DashDuration, 0.f, 1.f);
	const float SmoothedAlpha = FMath::InterpEaseOut(0.f, 1.f, RawAlpha, 2.f);
	const FVector NewLocation = FMath::Lerp(StartLocation, EndLocation, SmoothedAlpha);

	FHitResult Hit;
	AvatarCharacter->SetActorLocation(NewLocation, bSweep, &Hit, ETeleportType::None);

	if (Hit.bBlockingHit)
	{
		UE_LOG(LogPanthelia, Log, TEXT("GapCloser dash task blocked. Avatar=%s HitActor=%s Location=%s"),
			*GetNameSafe(AvatarCharacter),
			*GetNameSafe(Hit.GetActor()),
			*AvatarCharacter->GetActorLocation().ToString());
		BroadcastAndFinish(OnBlocked);
		return;
	}

	if (RawAlpha >= 1.f)
	{
		UE_LOG(LogPanthelia, Log, TEXT("GapCloser dash task finished. Avatar=%s FinalLocation=%s"),
			*GetNameSafe(AvatarCharacter),
			*AvatarCharacter->GetActorLocation().ToString());
		BroadcastAndFinish(OnFinished);
	}
}

void UAbilityTask_PantheliaDashToTarget::OnDestroy(bool bInOwnerFinished)
{
	if (AvatarCharacter)
	{
		StopAvatarMovement();
	}

	Super::OnDestroy(bInOwnerFinished);
}

bool UAbilityTask_PantheliaDashToTarget::InitializeDash()
{
	AvatarCharacter = Cast<ACharacter>(GetAvatarActor());
	if (!AvatarCharacter || !TargetActor)
	{
		return false;
	}

	StartLocation = AvatarCharacter->GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();
	const FVector ToTarget2D = FVector(TargetLocation.X - StartLocation.X, TargetLocation.Y - StartLocation.Y, 0.f);
	const float DistanceToTarget = ToTarget2D.Size();
	const float DesiredTravelDistance = FMath::Max(0.f, DistanceToTarget - StopDistance);

	if (DesiredTravelDistance <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogPanthelia, Log, TEXT("GapCloser dash task skipped because already inside stop distance. Avatar=%s Target=%s Distance=%.1f Stop=%.1f"),
			*GetNameSafe(AvatarCharacter),
			*GetNameSafe(TargetActor),
			DistanceToTarget,
			StopDistance);
		return false;
	}

	const FVector DirectionToTarget = ToTarget2D.GetSafeNormal();
	if (DirectionToTarget.IsNearlyZero())
	{
		return false;
	}

	float TravelDistance = DesiredTravelDistance;
	if (MaxTravelDistance > 0.f)
	{
		TravelDistance = FMath::Min(TravelDistance, MaxTravelDistance);
	}

	EndLocation = StartLocation + DirectionToTarget * TravelDistance;
	EndLocation.Z = StartLocation.Z;
	return true;
}

void UAbilityTask_PantheliaDashToTarget::StopAvatarMovement() const
{
	if (!AvatarCharacter)
	{
		return;
	}

	if (AController* Controller = AvatarCharacter->GetController())
	{
		Controller->StopMovement();
	}

	if (UCharacterMovementComponent* MovementComp = AvatarCharacter->GetCharacterMovement())
	{
		MovementComp->StopMovementImmediately();
	}
}

void UAbilityTask_PantheliaDashToTarget::BroadcastAndFinish(FPantheliaDashTaskDelegate& DelegateToBroadcast)
{
	if (bDidFinish)
	{
		return;
	}

	bDidFinish = true;

	if (ShouldBroadcastAbilityTaskDelegates())
	{
		DelegateToBroadcast.Broadcast();
	}

	EndTask();
}
