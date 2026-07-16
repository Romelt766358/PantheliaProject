// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Utilities/PantheliaProjectilePatternLibrary.h"

TArray<FVector> UPantheliaProjectilePatternLibrary::MakeEvenlySpacedDirections(
	const FVector& ForwardDirection,
	const FVector& RotationAxis,
	const float SpreadAngleDegrees,
	const int32 NumDirections)
{
	TArray<FVector> Directions;

	if (NumDirections <= 0)
	{
		return Directions;
	}

	const FVector SafeForward = ForwardDirection.GetSafeNormal();
	const FVector SafeAxis = RotationAxis.GetSafeNormal();
	if (SafeForward.IsNearlyZero() || SafeAxis.IsNearlyZero())
	{
		return Directions;
	}

	Directions.Reserve(NumDirections);

	if (NumDirections == 1)
	{
		Directions.Add(SafeForward);
		return Directions;
	}

	const float SafeSpread = FMath::Clamp(FMath::Abs(SpreadAngleDegrees), 0.f, 360.f);
	const bool bFullCircle = FMath::IsNearlyEqual(SafeSpread, 360.f);

	// En un arco normal centramos las direcciones alrededor de Forward. En un círculo
	// completo empezamos en Forward y damos una vuelta sin repetir el primer vector.
	const float StartAngle = bFullCircle ? 0.f : -SafeSpread * 0.5f;
	const float DeltaAngle = bFullCircle
		? SafeSpread / static_cast<float>(NumDirections)
		: SafeSpread / static_cast<float>(NumDirections - 1);

	for (int32 Index = 0; Index < NumDirections; ++Index)
	{
		const float CurrentAngle = StartAngle + DeltaAngle * static_cast<float>(Index);
		Directions.Add(SafeForward.RotateAngleAxis(CurrentAngle, SafeAxis).GetSafeNormal());
	}

	return Directions;
}
