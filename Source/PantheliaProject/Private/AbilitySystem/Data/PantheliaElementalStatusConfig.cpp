// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Data/PantheliaElementalStatusConfig.h"
#include "PantheliaLogChannels.h"

const FPantheliaElementalStatusDefinition* UPantheliaElementalStatusConfig::FindStatusDefinition(
	EPantheliaElement Element,
	bool bLogNotFound) const
{
	const FPantheliaElementalStatusDefinition* FoundDefinition = nullptr;

	for (const FPantheliaElementalStatusDefinition& Definition : StatusDefinitions)
	{
		if (Definition.Element != Element) continue;

		// Dos entradas para el mismo elemento hacen ambiguo qué estado aplicar.
		// Fallamos de forma visible en vez de elegir una silenciosamente por orden.
		if (FoundDefinition)
		{
			UE_LOG(LogPanthelia, Error,
				TEXT("[STATUS CONFIG] El elemento %d esta duplicado en '%s'."),
				static_cast<uint8>(Element), *GetName());
			return nullptr;
		}

		FoundDefinition = &Definition;
	}

	if (!FoundDefinition && bLogNotFound)
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[STATUS CONFIG] No existe una definicion para el elemento %d en '%s'."),
			static_cast<uint8>(Element), *GetName());
	}

	return FoundDefinition;
}
