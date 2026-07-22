// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/PantheliaProjectileTargeting.h"

#include "CollisionQueryParams.h"
#include "Combat/LockonComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Interfaces/Enemy.h"

namespace
{
	FVector ResolveBoundsBottom(AActor* TargetActor)
	{
		if (const ACharacter* TargetCharacter = Cast<ACharacter>(TargetActor))
		{
			if (const UCapsuleComponent* Capsule = TargetCharacter->GetCapsuleComponent())
			{
				return Capsule->GetComponentLocation()
					- Capsule->GetUpVector() * Capsule->GetScaledCapsuleHalfHeight();
			}
		}

		FVector BoundsOrigin = FVector::ZeroVector;
		FVector BoundsExtent = FVector::ZeroVector;
		TargetActor->GetActorBounds(
			/*bOnlyCollidingComponents=*/true,
			BoundsOrigin,
			BoundsExtent,
			/*bIncludeFromChildActors=*/true);

		if (!BoundsExtent.IsNearlyZero())
		{
			return FVector(
				BoundsOrigin.X,
				BoundsOrigin.Y,
				BoundsOrigin.Z - BoundsExtent.Z);
		}

		return TargetActor->GetActorLocation();
	}

	bool TryResolveLogicalLockonPoint(
		AActor* SourceActor,
		AActor* TargetActor,
		FVector& OutTargetPoint)
	{
		if (IsValid(SourceActor))
		{
			if (const ULockonComponent* LockonComp =
				SourceActor->FindComponentByClass<ULockonComponent>())
			{
				OutTargetPoint = LockonComp->GetLockonLocation(TargetActor);
				return true;
			}
		}

		if (TargetActor->Implements<UEnemy>())
		{
			OutTargetPoint = IEnemy::Execute_GetLockonLocation(TargetActor);
			return true;
		}

		if (const USceneComponent* TargetRoot = TargetActor->GetRootComponent())
		{
			OutTargetPoint = TargetRoot->GetComponentLocation();
			return true;
		}

		OutTargetPoint = TargetActor->GetActorLocation();
		return true;
	}
}

bool PantheliaProjectileTargeting::TryResolveTargetPoint(
	const UObject* WorldContextObject,
	AActor* SourceActor,
	AActor* TargetActor,
	const EPantheliaProjectileAimPointMode AimPointMode,
	const float GroundTraceUpDistance,
	const float GroundTraceDownDistance,
	const ECollisionChannel GroundTraceChannel,
	const float GroundSurfaceOffset,
	FVector& OutTargetPoint)
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	if (AimPointMode == EPantheliaProjectileAimPointMode::LockonLocation)
	{
		return TryResolveLogicalLockonPoint(SourceActor, TargetActor, OutTargetPoint);
	}

	const FVector BoundsBottom = ResolveBoundsBottom(TargetActor);
	const float SafeTraceUpDistance = FMath::Max(0.f, GroundTraceUpDistance);
	const float SafeTraceDownDistance = FMath::Max(0.f, GroundTraceDownDistance);

	UWorld* World = IsValid(WorldContextObject)
		? WorldContextObject->GetWorld()
		: nullptr;
	if (!IsValid(World))
	{
		OutTargetPoint = BoundsBottom;
		return true;
	}

	FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(PantheliaProjectileGroundTarget), false);
	QueryParams.AddIgnoredActor(TargetActor);
	if (IsValid(SourceActor))
	{
		QueryParams.AddIgnoredActor(SourceActor);
	}

	const FVector TraceStart = BoundsBottom + FVector::UpVector * SafeTraceUpDistance;
	const FVector TraceEnd = BoundsBottom - FVector::UpVector * SafeTraceDownDistance;

	FHitResult GroundHit;
	if (World->LineTraceSingleByChannel(
		GroundHit,
		TraceStart,
		TraceEnd,
		GroundTraceChannel,
		QueryParams))
	{
		const FVector SafeImpactNormal = GroundHit.ImpactNormal.IsNearlyZero()
			? FVector::UpVector
			: GroundHit.ImpactNormal.GetSafeNormal();
		OutTargetPoint = GroundHit.ImpactPoint
			+ SafeImpactNormal * FMath::Max(0.f, GroundSurfaceOffset);
		return true;
	}

	OutTargetPoint = BoundsBottom;
	return true;
}
