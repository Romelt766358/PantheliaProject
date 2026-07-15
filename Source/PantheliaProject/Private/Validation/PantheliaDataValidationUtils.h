#pragma once

#if WITH_EDITOR

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "Misc/DataValidation.h"
#include "ScalableFloat.h"

namespace PantheliaDataValidation
{
	inline EDataValidationResult MakeInitialResult(const EDataValidationResult SuperResult)
	{
		return SuperResult == EDataValidationResult::Invalid
			? EDataValidationResult::Invalid
			: EDataValidationResult::Valid;
	}

	inline void AddError(
		FDataValidationContext& Context,
		EDataValidationResult& Result,
		const FString& Message)
	{
		Context.AddError(FText::FromString(Message));
		Result = EDataValidationResult::Invalid;
	}

	inline void AddWarning(
		FDataValidationContext& Context,
		const FString& Message)
	{
		Context.AddWarning(FText::FromString(Message));
	}

	inline bool IsFiniteNonNegative(const float Value)
	{
		return FMath::IsFinite(Value) && Value >= 0.f;
	}

	inline bool IsFinitePositive(const float Value)
	{
		return FMath::IsFinite(Value) && Value > 0.f;
	}

	inline bool IsFiniteInRange(const float Value, const float MinValue, const float MaxValue)
	{
		return FMath::IsFinite(Value) && Value >= MinValue && Value <= MaxValue;
	}

	inline bool ValidateScalableFloatFinite(
		const FScalableFloat& ScalableFloat,
		const int32 FirstLevel,
		const int32 LastLevel,
		const FString& FieldLabel,
		FDataValidationContext& Context,
		EDataValidationResult& Result)
	{
		const int32 SafeFirstLevel = FMath::Max(0, FirstLevel);
		const int32 SafeLastLevel = FMath::Max(SafeFirstLevel, LastLevel);

		// Los rangos actuales del proyecto son pequeños. Este límite evita que un dato
		// corrupto convierta la validación del editor en un bucle de millones de pasos.
		constexpr int32 MaxSamples = 256;
		if (SafeLastLevel - SafeFirstLevel > MaxSamples)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s requiere validar %d niveles. El máximo de seguridad es %d."),
				*FieldLabel,
				SafeLastLevel - SafeFirstLevel + 1,
				MaxSamples + 1));
			return false;
		}

		bool bValid = true;
		for (int32 Level = SafeFirstLevel; Level <= SafeLastLevel; ++Level)
		{
			const float Value = ScalableFloat.GetValueAtLevel(static_cast<float>(Level));
			if (!FMath::IsFinite(Value))
			{
				AddError(Context, Result, FString::Printf(
					TEXT("%s produce un valor no finito en el nivel %d: %f."),
					*FieldLabel,
					Level,
					Value));
				bValid = false;
			}
		}

		return bValid;
	}

	inline bool ValidateScalableFloatNonNegative(
		const FScalableFloat& ScalableFloat,
		const int32 FirstLevel,
		const int32 LastLevel,
		const FString& FieldLabel,
		FDataValidationContext& Context,
		EDataValidationResult& Result)
	{
		const int32 SafeFirstLevel = FMath::Max(0, FirstLevel);
		const int32 SafeLastLevel = FMath::Max(SafeFirstLevel, LastLevel);

		// Los rangos actuales del proyecto son pequeños. Este límite evita que un dato
		// corrupto convierta la validación del editor en un bucle de millones de pasos.
		constexpr int32 MaxSamples = 256;
		if (SafeLastLevel - SafeFirstLevel > MaxSamples)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s requiere validar %d niveles. El máximo de seguridad es %d."),
				*FieldLabel,
				SafeLastLevel - SafeFirstLevel + 1,
				MaxSamples + 1));
			return false;
		}

		bool bValid = true;
		for (int32 Level = SafeFirstLevel; Level <= SafeLastLevel; ++Level)
		{
			const float Value = ScalableFloat.GetValueAtLevel(static_cast<float>(Level));
			if (!IsFiniteNonNegative(Value))
			{
				AddError(Context, Result, FString::Printf(
					TEXT("%s produce un valor inválido en el nivel %d: %f."),
					*FieldLabel,
					Level,
					Value));
				bValid = false;
			}
		}

		return bValid;
	}
}

#endif // WITH_EDITOR
