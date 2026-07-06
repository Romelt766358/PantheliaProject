#include "AbilitySystem/Data/PantheliaAttributeInfo.h"

FPantheliaAttributeInfo UPantheliaAttributeInfoAsset::FindAttributeInfoForTag(
	const FGameplayTag& AttributeTag,
	bool bLogNotFound) const
{
	// Recorremos el array buscando el struct cuyo tag coincida exactamente.
	for (const FPantheliaAttributeInfo& Info : AttributeInformation)
	{
		if (Info.AttributeTag.MatchesTagExact(AttributeTag))
		{
			return Info;
		}
	}

	// Si llegamos aquí, no encontramos el tag en el array.
	if (bLogNotFound)
	{
		UE_LOG(LogTemp, Error,
			TEXT("Can't find Info for AttributeTag [%s] on AttributeInfo [%s]"),
			*AttributeTag.ToString(),
			*GetNameSafe(this));
	}

	// Devolvemos un struct vacío como fallback.
	return FPantheliaAttributeInfo();
}