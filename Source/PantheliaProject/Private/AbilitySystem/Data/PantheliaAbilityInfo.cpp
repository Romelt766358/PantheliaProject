// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Data/PantheliaAbilityInfo.h"
#include "PantheliaLogChannels.h"

FPantheliaAbilityInfo UPantheliaAbilityInfoAsset::FindAbilityInfoForTag(
	const FGameplayTag& AbilityTag,
	bool bLogNotFound) const
{
	// Recorre el array buscando la entrada cuyo AbilityTag coincida.
	for (const FPantheliaAbilityInfo& Info : AbilityInformation)
	{
		if (Info.AbilityTag == AbilityTag)
		{
			return Info;
		}
	}

	// No encontrado: logueamos si se pidió y devolvemos un struct vacío.
	// Un struct vacío tiene AbilityTag inválido e Icon/BackgroundMaterial nulos,
	// así que el caller puede comprobar Info.AbilityTag.IsValid() para detectarlo.
	if (bLogNotFound)
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[AbilityInfo] No se encontró entrada para el tag '%s' en '%s'."),
			*AbilityTag.ToString(),
			*GetNameSafe(this));
	}

	return FPantheliaAbilityInfo();
}
