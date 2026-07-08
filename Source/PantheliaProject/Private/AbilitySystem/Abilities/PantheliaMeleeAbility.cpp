// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/PantheliaMeleeAbility.h"

#include "GameFramework/Character.h"
#include "Interfaces/Enemy.h"

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
	bool bOverrideZ) const
{
	ACharacter* AvatarCharacter = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	AActor* TargetActor = GetCombatTargetFromAvatar();
	if (!AvatarCharacter || !TargetActor)
	{
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
		return false;
	}

	const FVector DirectionToTarget = ToTarget2D.GetSafeNormal();
	if (DirectionToTarget.IsNearlyZero())
	{
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

	AvatarCharacter->LaunchCharacter(LaunchVelocity, bOverrideXY, bOverrideZ);
	return true;
}
