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