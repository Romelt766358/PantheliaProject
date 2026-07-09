// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/PantheliaMeleeAbility.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interfaces/Enemy.h"
#include "PantheliaLogChannels.h"

AActor* UPantheliaMeleeAbility::GetCombatTargetFromAvatar() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor || !AvatarActor->GetClass()->ImplementsInterface(UEnemy::StaticClass()))
	{
		return nullptr;
	}

	return IEnemy::Execute_GetCombatTarget(AvatarActor);
}

float UPantheliaMeleeAbility::GetDistanceToCombatTarget2D() const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	const AActor* TargetActor = GetCombatTargetFromAvatar();
	if (!AvatarActor || !TargetActor)
	{
		return -1.f;
	}

	return FVector::Dist2D(AvatarActor->GetActorLocation(), TargetActor->GetActorLocation());
}

bool UPantheliaMeleeAbility::LaunchAvatarTowardCombatTarget(
	float StopDistance,
	float TravelTime,
	float MaxHorizontalSpeed,
	float VerticalSpeed,
	bool bOverrideXY,
	bool bOverrideZ)
{
	ACharacter* AvatarCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	AActor* TargetActor = GetCombatTargetFromAvatar();
	if (!AvatarCharacter || !TargetActor)
	{
		UE_LOG(LogPanthelia, Warning, TEXT("GapCloser launch failed. Avatar=%s Target=%s"),
			*GetNameSafe(AvatarCharacter),
			*GetNameSafe(TargetActor));
		return false;
	}

	const FVector AvatarLocation = AvatarCharacter->GetActorLocation();
	const FVector TargetLocation = TargetActor->GetActorLocation();
	const FVector ToTarget2D = FVector(TargetLocation.X - AvatarLocation.X, TargetLocation.Y - AvatarLocation.Y, 0.f);
	const float DistanceToTarget = ToTarget2D.Size();

	StopDistance = FMath::Max(0.f, StopDistance);
	const float DistanceToTravel = FMath::Max(0.f, DistanceToTarget - StopDistance);
	if (DistanceToTravel <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogPanthelia, Log, TEXT("GapCloser launch skipped. Avatar=%s Target=%s Distance=%.1f StopDistance=%.1f"),
			*GetNameSafe(AvatarCharacter),
			*GetNameSafe(TargetActor),
			DistanceToTarget,
			StopDistance);
		return false;
	}

	const FVector DirectionToTarget = ToTarget2D.GetSafeNormal();
	if (DirectionToTarget.IsNearlyZero())
	{
		UE_LOG(LogPanthelia, Warning, TEXT("GapCloser launch failed because direction is zero. Avatar=%s Target=%s"),
			*GetNameSafe(AvatarCharacter),
			*GetNameSafe(TargetActor));
		return false;
	}

	TravelTime = FMath::Max(0.05f, TravelTime);
	float HorizontalSpeed = DistanceToTravel / TravelTime;
	if (MaxHorizontalSpeed > 0.f)
	{
		HorizontalSpeed = FMath::Min(HorizontalSpeed, MaxHorizontalSpeed);
	}

	FVector LaunchVelocity = DirectionToTarget * HorizontalSpeed;
	LaunchVelocity.Z = VerticalSpeed;

	if (UCharacterMovementComponent* MovementComp = AvatarCharacter->GetCharacterMovement())
	{
		MovementComp->StopMovementImmediately();
	}

	AvatarCharacter->LaunchCharacter(LaunchVelocity, bOverrideXY, bOverrideZ);

	UE_LOG(LogPanthelia, Log, TEXT("GapCloser launch applied. Avatar=%s Target=%s Distance=%.1f Travel=%.1f Stop=%.1f TravelTime=%.2f Speed=%.1f Velocity=%s"),
		*GetNameSafe(AvatarCharacter),
		*GetNameSafe(TargetActor),
		DistanceToTarget,
		DistanceToTravel,
		StopDistance,
		TravelTime,
		HorizontalSpeed,
		*LaunchVelocity.ToString());

	return true;
}
