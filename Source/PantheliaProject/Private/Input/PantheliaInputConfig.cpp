// Fill out your copyright notice in the Description page of Project Settings.

#include "Input/PantheliaInputConfig.h"

const UInputAction* UPantheliaInputConfig::FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound) const
{
	// Recorremos todos los pares registrados en el Data Asset.
	for (const FPantheliaInputAction& Action : AbilityInputActions)
	{
		// Verificamos que la InputAction no sea nula y que el tag coincida exactamente.
		if (Action.InputAction && Action.InputTag == InputTag)
		{
			return Action.InputAction;
		}
	}

	// Si llegamos aquí, no encontramos el tag. Logueamos si se pidió.
	if (bLogNotFound)
	{
		UE_LOG(LogTemp, Error,
			TEXT("No se encontró InputAction para el tag [%s] en el InputConfig [%s]"),
			*InputTag.ToString(),
			*GetNameSafe(this)
		);
	}

	return nullptr;
}

#if WITH_EDITOR

#include "InputAction.h"
#include "PantheliaGameplayTags.h"
#include "Validation/PantheliaDataValidationUtils.h"

EDataValidationResult UPantheliaInputConfig::IsDataValid(FDataValidationContext& Context) const
{
	using namespace PantheliaDataValidation;

	EDataValidationResult Result = MakeInitialResult(Super::IsDataValid(Context));
	const FPantheliaGameplayTags& Tags = FPantheliaGameplayTags::Get();

	if (AbilityInputActions.IsEmpty())
	{
		AddError(Context, Result, TEXT("AbilityInputActions necesita al menos una entrada."));
		return Result;
	}

	TSet<FGameplayTag> InputTags;
	TSet<const UInputAction*> InputActions;

	for (int32 ActionIndex = 0; ActionIndex < AbilityInputActions.Num(); ++ActionIndex)
	{
		const FPantheliaInputAction& Action = AbilityInputActions[ActionIndex];
		const FString Label = FString::Printf(TEXT("AbilityInputActions[%d]"), ActionIndex);

		if (!Action.InputAction)
		{
			AddError(Context, Result, FString::Printf(TEXT("%s: InputAction no está asignada."), *Label));
		}
		else if (InputActions.Contains(Action.InputAction.Get()))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s: InputAction '%s' está repetida."),
				*Label, *GetNameSafe(Action.InputAction.Get())));
		}
		else
		{
			InputActions.Add(Action.InputAction.Get());
		}

		if (!Action.InputTag.IsValid()
			|| !Action.InputTag.MatchesTag(Tags.InputTag)
			|| Action.InputTag == Tags.InputTag)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s: InputTag '%s' debe ser una hoja bajo 'InputTag'."),
				*Label, *Action.InputTag.ToString()));
		}
		else if (InputTags.Contains(Action.InputTag))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s: InputTag '%s' está duplicado."),
				*Label, *Action.InputTag.ToString()));
		}
		else
		{
			InputTags.Add(Action.InputTag);
		}
	}

	return Result;
}

#endif // WITH_EDITOR
