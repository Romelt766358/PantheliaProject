// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/PantheliaWeaponDefinition.h"

#if WITH_EDITOR

#include "Animation/AnimMontage.h"
#include "Animations/WeaponTraceNotifyState.h"
#include "Combat/ComboWindowNotifyState.h"
#include "PantheliaGameplayTags.h"
#include "Validation/PantheliaDataValidationUtils.h"

namespace
{
	bool HasWeaponTraceNotify(const UAnimMontage* Montage)
	{
		if (!Montage)
		{
			return false;
		}

		for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
		{
			if (NotifyEvent.NotifyStateClass
				&& NotifyEvent.NotifyStateClass->IsA<UWeaponTraceNotifyState>())
			{
				return true;
			}
		}

		return false;
	}

	bool HasComboWindowNotify(const UAnimMontage* Montage)
	{
		if (!Montage)
		{
			return false;
		}

		for (const FAnimNotifyEvent& NotifyEvent : Montage->Notifies)
		{
			if (NotifyEvent.NotifyStateClass
				&& NotifyEvent.NotifyStateClass->IsA<UComboWindowNotifyState>())
			{
				return true;
			}
		}

		return false;
	}
}

EDataValidationResult UPantheliaWeaponDefinition::IsDataValid(FDataValidationContext& Context) const
{
	using namespace PantheliaDataValidation;

	EDataValidationResult Result = MakeInitialResult(Super::IsDataValid(Context));
	const FPantheliaGameplayTags& Tags = FPantheliaGameplayTags::Get();

	if (WeaponName.IsEmpty())
	{
		AddWarning(Context, TEXT("WeaponName está vacío; el arma puede funcionar, pero la UI no tendrá nombre legible."));
	}

	auto ValidateMontage = [&](
		const UAnimMontage* Montage,
		const FString& Label,
		const bool bRequireComboWindow)
	{
		if (!Montage)
		{
			AddError(Context, Result, FString::Printf(TEXT("%s no está asignado."), *Label));
			return;
		}

		if (!HasWeaponTraceNotify(Montage))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s ('%s') no contiene UWeaponTraceNotifyState."),
				*Label,
				*GetNameSafe(Montage)));
		}

		if (bRequireComboWindow && !HasComboWindowNotify(Montage))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s ('%s') no contiene UComboWindowNotifyState."),
				*Label,
				*GetNameSafe(Montage)));
		}
	};

	auto ValidateMontageArray = [&](
		const TArray<TObjectPtr<UAnimMontage>>& Montages,
		const TCHAR* ArrayName)
	{
		if (Montages.IsEmpty())
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s necesita al menos un montage."), ArrayName));
			return;
		}

		TSet<const UAnimMontage*> SeenMontages;
		for (int32 MontageIndex = 0; MontageIndex < Montages.Num(); ++MontageIndex)
		{
			const UAnimMontage* Montage = Montages[MontageIndex];
			const FString Label = FString::Printf(TEXT("%s[%d]"), ArrayName, MontageIndex);
			ValidateMontage(Montage, Label, true);

			if (Montage && SeenMontages.Contains(Montage))
			{
				AddError(Context, Result, FString::Printf(
					TEXT("%s repite el montage '%s' dentro de la misma cadena."),
					*Label,
					*GetNameSafe(Montage)));
			}
			SeenMontages.Add(Montage);
		}
	};

	ValidateMontageArray(LightAttackMontages, TEXT("LightAttackMontages"));
	ValidateMontageArray(HeavyAttackMontages, TEXT("HeavyAttackMontages"));

	if (DodgeLightAttackMontage)
	{
		ValidateMontage(DodgeLightAttackMontage, TEXT("DodgeLightAttackMontage"), true);
	}
	if (DodgeHeavyAttackMontage)
	{
		ValidateMontage(DodgeHeavyAttackMontage, TEXT("DodgeHeavyAttackMontage"), true);
	}
	ValidateMontage(ChargedHeavyMontage, TEXT("ChargedHeavyMontage"), false);

	auto ValidateAttackModifiers = [&](
		const FPantheliaWeaponAttackModifiers& Modifiers,
		const TCHAR* Label)
	{
		if (!IsFiniteNonNegative(Modifiers.DamageMultiplier)
			|| !IsFiniteNonNegative(Modifiers.PoiseDamageMultiplier)
			|| !IsFiniteNonNegative(Modifiers.BuildupMultiplier))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s: todos los multiplicadores deben ser finitos y no negativos."),
				Label));
		}
	};

	ValidateAttackModifiers(LightAttackModifiers, TEXT("LightAttackModifiers"));
	ValidateAttackModifiers(HeavyAttackModifiers, TEXT("HeavyAttackModifiers"));
	ValidateAttackModifiers(ChargedHeavyAttackModifiers, TEXT("ChargedHeavyAttackModifiers"));

	if (MaxUpgradeLevel < 0)
	{
		AddError(Context, Result, TEXT("MaxUpgradeLevel no puede ser negativo."));
	}
	const int32 SafeMaxUpgradeLevel = FMath::Max(0, MaxUpgradeLevel);

	if (DamageTypes.IsEmpty())
	{
		AddError(Context, Result, TEXT("DamageTypes necesita al menos una entrada."));
	}

	for (const TPair<FGameplayTag, FScalableFloat>& DamagePair : DamageTypes)
	{
		if (!DamagePair.Key.IsValid() || !Tags.DamageTypesToResistances.Contains(DamagePair.Key))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("DamageTypes contiene el tag no soportado '%s'."),
				*DamagePair.Key.ToString()));
			continue;
		}

		ValidateScalableFloatNonNegative(
			DamagePair.Value,
			0,
			SafeMaxUpgradeLevel,
			FString::Printf(TEXT("DamageTypes['%s']"), *DamagePair.Key.ToString()),
			Context,
			Result);
	}

	if (AttributeScalings.Num() > 2)
	{
		AddError(Context, Result, FString::Printf(
			TEXT("AttributeScalings tiene %d entradas; el contrato permite un máximo de 2."),
			AttributeScalings.Num()));
	}

	TSet<FGameplayTag> ScalingTags;
	for (int32 ScalingIndex = 0; ScalingIndex < AttributeScalings.Num(); ++ScalingIndex)
	{
		const FAbilityAttributeScaling& Scaling = AttributeScalings[ScalingIndex];
		const FString TagString = Scaling.AttributeTag.ToString();
		const bool bAllowedAttribute = TagString.StartsWith(TEXT("Attributes.Secondary."))
			|| TagString.StartsWith(TEXT("Attributes.Vital."));

		if (!Scaling.AttributeTag.IsValid() || !bAllowedAttribute)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("AttributeScalings[%d]: AttributeTag '%s' debe pertenecer a Attributes.Secondary o Attributes.Vital."),
				ScalingIndex,
				*TagString));
		}
		else if (ScalingTags.Contains(Scaling.AttributeTag))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("AttributeScalings[%d]: AttributeTag '%s' está duplicado."),
				ScalingIndex,
				*TagString));
		}
		else
		{
			ScalingTags.Add(Scaling.AttributeTag);
		}

		if (!IsFiniteNonNegative(Scaling.Ratio))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("AttributeScalings[%d] (%s): Ratio debe ser finito y no negativo."),
				ScalingIndex,
				*TagString));
		}
	}

	ValidateScalableFloatNonNegative(
		PoiseDamage,
		0,
		SafeMaxUpgradeLevel,
		TEXT("PoiseDamage"),
		Context,
		Result);

	for (const TPair<FGameplayTag, FScalableFloat>& BuildupPair : BuildupAmounts)
	{
		if (!BuildupPair.Key.IsValid() || !Tags.DamageTypesToResistances.Contains(BuildupPair.Key))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("BuildupAmounts contiene el tag no soportado '%s'."),
				*BuildupPair.Key.ToString()));
			continue;
		}

		if (!DamageTypes.Contains(BuildupPair.Key))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("BuildupAmounts['%s'] no tiene una entrada correspondiente en DamageTypes."),
				*BuildupPair.Key.ToString()));
		}

		ValidateScalableFloatNonNegative(
			BuildupPair.Value,
			0,
			SafeMaxUpgradeLevel,
			FString::Printf(TEXT("BuildupAmounts['%s']"), *BuildupPair.Key.ToString()),
			Context,
			Result);
	}

	if (UpgradeMaterialCostPerLevel.Num() > SafeMaxUpgradeLevel)
	{
		AddError(Context, Result, FString::Printf(
			TEXT("UpgradeMaterialCostPerLevel tiene %d entradas pero MaxUpgradeLevel es %d."),
			UpgradeMaterialCostPerLevel.Num(),
			SafeMaxUpgradeLevel));
	}
	for (int32 CostIndex = 0; CostIndex < UpgradeMaterialCostPerLevel.Num(); ++CostIndex)
	{
		if (UpgradeMaterialCostPerLevel[CostIndex] < 0)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("UpgradeMaterialCostPerLevel[%d] no puede ser negativo."),
				CostIndex));
		}
	}

	if (!IsFiniteNonNegative(LightAttackStaminaCost)
		|| !IsFiniteNonNegative(HeavyAttackStaminaCost))
	{
		AddError(Context, Result, TEXT("Los costes de stamina de ataque deben ser finitos y no negativos."));
	}

	if (!IsFiniteInRange(PhysicalImperfectBlockMultiplier, 0.f, 1.f)
		|| !IsFiniteInRange(MagicOnPhysicalParryMultiplier, 0.f, 1.f)
		|| !IsFiniteInRange(MagicImperfectBlockMultiplier, 0.f, 1.f)
		|| !IsFiniteInRange(PhysicalOnMagicParryMultiplier, 0.f, 1.f))
	{
		AddError(Context, Result, TEXT("Los multiplicadores de mitigación defensiva deben estar entre 0 y 1."));
	}

	if (!IsFiniteNonNegative(PhysicalParryPoiseDamage)
		|| !IsFiniteNonNegative(MagicParryPoiseDamage)
		|| !IsFiniteNonNegative(ParryStaminaCost)
		|| !IsFiniteNonNegative(BlockStaminaCost)
		|| !IsFiniteNonNegative(PerfectParryStaminaCostMultiplier))
	{
		AddError(Context, Result, TEXT("Los valores defensivos de poise/coste deben ser finitos y no negativos."));
	}

	if (!FMath::IsFinite(HeavyBlockStaminaCostMultiplier)
		|| HeavyBlockStaminaCostMultiplier < 1.f)
	{
		AddError(Context, Result, TEXT("HeavyBlockStaminaCostMultiplier debe ser finito y al menos 1."));
	}

	if (!FMath::IsFinite(NoStaminaBlockPoiseMultiplier)
		|| NoStaminaBlockPoiseMultiplier < 1.f)
	{
		AddError(Context, Result, TEXT("NoStaminaBlockPoiseMultiplier legacy debe ser finito y al menos 1."));
	}

	if (WeaponBaseSocketName.IsNone() || WeaponTipSocketName.IsNone())
	{
		AddError(Context, Result, TEXT("WeaponBaseSocketName y WeaponTipSocketName no pueden ser None."));
	}
	else if (WeaponBaseSocketName == WeaponTipSocketName)
	{
		AddError(Context, Result, TEXT("WeaponBaseSocketName y WeaponTipSocketName deben ser distintos."));
	}

	return Result;
}

#endif // WITH_EDITOR
