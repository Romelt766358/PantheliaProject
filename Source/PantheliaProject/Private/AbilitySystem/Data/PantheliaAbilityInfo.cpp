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

#if WITH_EDITOR

#include "PantheliaGameplayTags.h"
#include "Validation/PantheliaDataValidationUtils.h"

EDataValidationResult UPantheliaAbilityInfoAsset::IsDataValid(FDataValidationContext& Context) const
{
	using namespace PantheliaDataValidation;

	EDataValidationResult Result = MakeInitialResult(Super::IsDataValid(Context));
	const FPantheliaGameplayTags& Tags = FPantheliaGameplayTags::Get();

	if (AbilityInformation.IsEmpty())
	{
		AddError(Context, Result, TEXT("AbilityInformation necesita al menos una entrada."));
		return Result;
	}

	TSet<FGameplayTag> AbilityTags;
	TSet<FGameplayTag> CooldownTags;

	for (int32 InfoIndex = 0; InfoIndex < AbilityInformation.Num(); ++InfoIndex)
	{
		const FPantheliaAbilityInfo& Info = AbilityInformation[InfoIndex];
		const FString Label = FString::Printf(TEXT("AbilityInformation[%d]"), InfoIndex);

		if (!Info.AbilityTag.IsValid()
			|| !Info.AbilityTag.MatchesTag(Tags.Abilities)
			|| Info.AbilityTag == Tags.Abilities)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s: AbilityTag '%s' debe ser una hoja bajo 'Abilities'."),
				*Label, *Info.AbilityTag.ToString()));
		}
		else if (AbilityTags.Contains(Info.AbilityTag))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s: AbilityTag '%s' está duplicado."),
				*Label, *Info.AbilityTag.ToString()));
		}
		else
		{
			AbilityTags.Add(Info.AbilityTag);
		}

		if (!Info.CooldownTag.IsValid()
			|| !Info.CooldownTag.MatchesTag(Tags.Cooldown)
			|| Info.CooldownTag == Tags.Cooldown)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (%s): CooldownTag '%s' debe ser una hoja bajo 'Cooldown'."),
				*Label, *Info.AbilityTag.ToString(), *Info.CooldownTag.ToString()));
		}
		else if (CooldownTags.Contains(Info.CooldownTag))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (%s): CooldownTag '%s' está duplicado."),
				*Label, *Info.AbilityTag.ToString(), *Info.CooldownTag.ToString()));
		}
		else
		{
			CooldownTags.Add(Info.CooldownTag);
		}

		if (!Info.Icon)
		{
			AddWarning(Context, FString::Printf(
				TEXT("%s (%s): Icon no está asignado."),
				*Label, *Info.AbilityTag.ToString()));
		}

		if (!Info.BackgroundMaterial)
		{
			AddWarning(Context, FString::Printf(
				TEXT("%s (%s): BackgroundMaterial no está asignado."),
				*Label, *Info.AbilityTag.ToString()));
		}
	}

	return Result;
}

#endif // WITH_EDITOR
