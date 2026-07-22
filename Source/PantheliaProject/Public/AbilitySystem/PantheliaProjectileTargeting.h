// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/PantheliaAreaImpactTypes.h"

class AActor;

namespace PantheliaProjectileTargeting
{
	/**
	 * Resuelve un punto lógico de target compartido por la dirección inicial y el
	 * soft homing. GroundUnderTarget ignora al source y al target en el trace para
	 * encontrar la superficie real bajo sus pies/cápsula.
	 */
	PANTHELIAPROJECT_API bool TryResolveTargetPoint(
		const UObject* WorldContextObject,
		AActor* SourceActor,
		AActor* TargetActor,
		EPantheliaProjectileAimPointMode AimPointMode,
		float GroundTraceUpDistance,
		float GroundTraceDownDistance,
		ECollisionChannel GroundTraceChannel,
		float GroundSurfaceOffset,
		FVector& OutTargetPoint);
}
