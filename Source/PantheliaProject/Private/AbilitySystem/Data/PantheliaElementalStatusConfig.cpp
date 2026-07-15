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

#if WITH_EDITOR

#include "Validation/PantheliaDataValidationUtils.h"

EDataValidationResult UPantheliaElementalStatusConfig::IsDataValid(FDataValidationContext& Context) const
{
	using namespace PantheliaDataValidation;

	EDataValidationResult Result = MakeInitialResult(Super::IsDataValid(Context));
	TSet<EPantheliaElement> SeenElements;

	for (int32 DefinitionIndex = 0; DefinitionIndex < StatusDefinitions.Num(); ++DefinitionIndex)
	{
		const FPantheliaElementalStatusDefinition& Definition = StatusDefinitions[DefinitionIndex];
		const FString Label = FString::Printf(TEXT("StatusDefinitions[%d]"), DefinitionIndex);

		if (Definition.Element == EPantheliaElement::None)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s: Element no puede ser None."), *Label));
		}
		else if (SeenElements.Contains(Definition.Element))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s: el elemento %d está duplicado."),
				*Label, static_cast<uint8>(Definition.Element)));
		}
		else
		{
			SeenElements.Add(Definition.Element);
		}

		auto ValidateNonNegative = [&](const float Value, const TCHAR* FieldName)
		{
			if (!IsFiniteNonNegative(Value))
			{
				AddError(Context, Result, FString::Printf(
					TEXT("%s (elemento %d): %s debe ser finito y no negativo."),
					*Label,
					static_cast<uint8>(Definition.Element),
					FieldName));
			}
		};

		ValidateNonNegative(Definition.BaseMagnitude, TEXT("BaseMagnitude"));
		ValidateNonNegative(Definition.BaseDuration, TEXT("BaseDuration"));
		ValidateNonNegative(Definition.MagnitudePercentPerStatusPower, TEXT("MagnitudePercentPerStatusPower"));
		ValidateNonNegative(Definition.DurationPercentPerStatusPower, TEXT("DurationPercentPerStatusPower"));
		ValidateNonNegative(Definition.FlatDamagePerMagicDamage, TEXT("FlatDamagePerMagicDamage"));
		ValidateNonNegative(Definition.MaxHealthPercentPerStatusPower, TEXT("MaxHealthPercentPerStatusPower"));
		ValidateNonNegative(Definition.CurrentHealthPercentPerStatusPower, TEXT("CurrentHealthPercentPerStatusPower"));
		ValidateNonNegative(Definition.MissingHealthPercentPerStatusPower, TEXT("MissingHealthPercentPerStatusPower"));
		ValidateNonNegative(Definition.MaxHealthPercentPerMagicDamage, TEXT("MaxHealthPercentPerMagicDamage"));
		ValidateNonNegative(Definition.CurrentHealthPercentPerMagicDamage, TEXT("CurrentHealthPercentPerMagicDamage"));
		ValidateNonNegative(Definition.MissingHealthPercentPerMagicDamage, TEXT("MissingHealthPercentPerMagicDamage"));
		ValidateNonNegative(Definition.BasePoiseDamage, TEXT("BasePoiseDamage"));
		ValidateNonNegative(Definition.PoiseDamagePerStatusPower, TEXT("PoiseDamagePerStatusPower"));
		ValidateNonNegative(Definition.PoiseDamagePerMagicDamage, TEXT("PoiseDamagePerMagicDamage"));
		ValidateNonNegative(Definition.GrievousWoundsPercentPerStatusPower, TEXT("GrievousWoundsPercentPerStatusPower"));

		if (!IsFinitePositive(Definition.TickFrequency))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (elemento %d): TickFrequency debe ser finita y mayor que 0."),
				*Label, static_cast<uint8>(Definition.Element)));
		}

		if (!IsFiniteInRange(Definition.GrievousWoundsPercent, 0.f, 100.f))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (elemento %d): GrievousWoundsPercent debe estar entre 0 y 100."),
				*Label, static_cast<uint8>(Definition.Element)));
		}
		else if (Definition.bAppliesGrievousWounds && Definition.GrievousWoundsPercent <= 0.f)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (elemento %d): bAppliesGrievousWounds está activo pero el porcentaje es 0."),
				*Label, static_cast<uint8>(Definition.Element)));
		}

		switch (Definition.PayloadType)
		{
		case EPantheliaElementalStatusPayload::DamageOverTime:
			if (!IsFinitePositive(Definition.BaseDuration))
			{
				AddError(Context, Result, FString::Printf(
					TEXT("%s (elemento %d): un DamageOverTime necesita BaseDuration > 0."),
					*Label, static_cast<uint8>(Definition.Element)));
			}
			break;

		case EPantheliaElementalStatusPayload::BurstDamage:
		case EPantheliaElementalStatusPayload::AttributeDebuff:
			break;

		default:
			AddError(Context, Result, FString::Printf(
				TEXT("%s (elemento %d): PayloadType contiene un valor desconocido."),
				*Label, static_cast<uint8>(Definition.Element)));
			break;
		}
	}

	const EPantheliaElement RequiredElements[] =
	{
		EPantheliaElement::Fire,
		EPantheliaElement::Water,
		EPantheliaElement::Storm,
		EPantheliaElement::Nature
	};

	for (const EPantheliaElement RequiredElement : RequiredElements)
	{
		if (!SeenElements.Contains(RequiredElement))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("Falta una definición para el elemento %d."),
				static_cast<uint8>(RequiredElement)));
		}
	}

	return Result;
}

#endif // WITH_EDITOR
