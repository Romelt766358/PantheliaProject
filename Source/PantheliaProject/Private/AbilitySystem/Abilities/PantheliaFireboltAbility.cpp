// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/PantheliaFireboltAbility.h"

#include "AbilitySystem/Utilities/PantheliaProjectilePatternLibrary.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "Interfaces/CombatInterface.h"

int32 UPantheliaFireboltAbility::GetCurrentProjectileCount() const
{
	const int32 SafeMaxProjectileCount = FMath::Max(1, MaxProjectileCount);
	return FMath::Clamp(GetAbilityLevel(), 1, SafeMaxProjectileCount);
}

float UPantheliaFireboltAbility::GetCurrentProjectileSpreadDegrees() const
{
	return FMath::Clamp(ProjectileSpreadDegrees, 0.f, 360.f);
}

void UPantheliaFireboltAbility::DrawProjectileSpreadDebug(
	const int32 ProjectileCountOverride) const
{
#if ENABLE_DRAW_DEBUG
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!IsValid(AvatarActor) || !IsValid(World))
	{
		return;
	}

	if (!AvatarActor->GetClass()->ImplementsInterface(UCombatInterface::StaticClass()))
	{
		return;
	}

	const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(
		AvatarActor, GetResolvedSocketTag());
	const FVector TargetLocation = GetFacingTargetLocation();

	const FVector ToTarget = TargetLocation - SocketLocation;
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	// SpawnProjectile() aplana actualmente el pitch a 0°. El debug debe representar
	// exactamente la misma orientación inicial para no enseñar un patrón distinto.
	FRotator InitialRotation = ToTarget.Rotation();
	InitialRotation.Pitch = 0.f;
	InitialRotation.Roll = 0.f;
	const FVector ForwardDirection = InitialRotation.Vector().GetSafeNormal();

	const int32 RequestedCount = ProjectileCountOverride > 0
		? ProjectileCountOverride
		: GetCurrentProjectileCount();
	const int32 ProjectileCount = FMath::Clamp(
		RequestedCount, 1, FMath::Max(1, MaxProjectileCount));
	const float SpreadDegrees = GetCurrentProjectileSpreadDegrees();

	const TArray<FVector> SpreadDirections =
		UPantheliaProjectilePatternLibrary::MakeEvenlySpacedDirections(
			ForwardDirection,
			FVector::UpVector,
			SpreadDegrees,
			ProjectileCount);

	constexpr float MainArrowLength = 180.f;
	constexpr float SpreadArrowLength = 145.f;
	constexpr float DebugDuration = 20.f;
	constexpr float ArrowSize = 10.f;
	constexpr float Thickness = 1.25f;

	// Blanco: dirección inicial real hacia el objetivo.
	DrawDebugDirectionalArrow(
		World,
		SocketLocation,
		SocketLocation + ForwardDirection * MainArrowLength,
		ArrowSize,
		FColor::White,
		false,
		DebugDuration,
		0,
		Thickness);

	// Gris: límites izquierdo y derecho del arco configurado.
	const float HalfSpread = SpreadDegrees * 0.5f;
	const FVector LeftBoundary = ForwardDirection.RotateAngleAxis(
		-HalfSpread, FVector::UpVector);
	const FVector RightBoundary = ForwardDirection.RotateAngleAxis(
		HalfSpread, FVector::UpVector);

	DrawDebugDirectionalArrow(
		World,
		SocketLocation,
		SocketLocation + LeftBoundary * MainArrowLength,
		ArrowSize,
		FColor(128, 128, 128),
		false,
		DebugDuration,
		0,
		Thickness);

	DrawDebugDirectionalArrow(
		World,
		SocketLocation,
		SocketLocation + RightBoundary * MainArrowLength,
		ArrowSize,
		FColor(128, 128, 128),
		false,
		DebugDuration,
		0,
		Thickness);

	// Rojo y ligeramente elevado: una flecha por cada futuro proyectil.
	const FVector SpreadStart = SocketLocation + FVector::UpVector * 8.f;
	for (const FVector& Direction : SpreadDirections)
	{
		DrawDebugDirectionalArrow(
			World,
			SpreadStart,
			SpreadStart + Direction * SpreadArrowLength,
			ArrowSize,
			FColor::Red,
			false,
			DebugDuration,
			0,
			Thickness);
	}
#else
	(void)ProjectileCountOverride;
#endif
}
