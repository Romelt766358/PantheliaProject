// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/Data/PantheliaBossProfile.h"

const FPantheliaBossStatsPreset* UPantheliaBossProfile::FindStatsPreset(const FName PresetID) const
{
	const FName DesiredPresetID = PresetID.IsNone() ? DefaultStatsPresetID : PresetID;

	for (const FPantheliaBossStatsPreset& Preset : StatsPresets)
	{
		if (Preset.PresetID == DesiredPresetID)
		{
			return &Preset;
		}
	}

	return StatsPresets.Num() > 0 ? &StatsPresets[0] : nullptr;
}

const FPantheliaBossPhaseDefinition* UPantheliaBossProfile::FindPhase(const FName PhaseID) const
{
	for (const FPantheliaBossPhaseDefinition& Phase : Phases)
	{
		if (Phase.PhaseID == PhaseID)
		{
			return &Phase;
		}
	}

	return Phases.Num() > 0 ? &Phases[0] : nullptr;
}

const FPantheliaBossActionDefinition* UPantheliaBossProfile::FindAction(const FGameplayTag& ActionTag) const
{
	if (!ActionTag.IsValid())
	{
		return nullptr;
	}

	for (const FPantheliaBossActionDefinition& Action : Actions)
	{
		if (Action.ActionTag.MatchesTagExact(ActionTag))
		{
			return &Action;
		}
	}

	return nullptr;
}

#if WITH_EDITOR

#include "PantheliaGameplayTags.h"
#include "Validation/PantheliaDataValidationUtils.h"

EDataValidationResult UPantheliaBossProfile::IsDataValid(FDataValidationContext& Context) const
{
	using namespace PantheliaDataValidation;

	EDataValidationResult Result = MakeInitialResult(Super::IsDataValid(Context));
	const FPantheliaGameplayTags& Tags = FPantheliaGameplayTags::Get();

	if (BossID.IsNone())
	{
		AddError(Context, Result, TEXT("BossID no puede ser None."));
	}

	if (BossDisplayName.IsEmpty())
	{
		AddWarning(Context, TEXT("BossDisplayName está vacío; el gameplay funciona, pero la UI no tendrá nombre legible."));
	}

	if (StatsPresets.IsEmpty())
	{
		AddError(Context, Result, TEXT("StatsPresets necesita al menos un preset."));
	}

	TSet<FName> PresetIDs;
	for (int32 PresetIndex = 0; PresetIndex < StatsPresets.Num(); ++PresetIndex)
	{
		const FPantheliaBossStatsPreset& Preset = StatsPresets[PresetIndex];
		const FString Label = FString::Printf(TEXT("StatsPresets[%d]"), PresetIndex);

		if (Preset.PresetID.IsNone())
		{
			AddError(Context, Result, FString::Printf(TEXT("%s: PresetID no puede ser None."), *Label));
		}
		else if (PresetIDs.Contains(Preset.PresetID))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s: PresetID '%s' está duplicado."), *Label, *Preset.PresetID.ToString()));
		}
		else
		{
			PresetIDs.Add(Preset.PresetID);
		}

		if (!IsFinitePositive(Preset.MaxHealth))
		{
			AddError(Context, Result, FString::Printf(TEXT("%s (%s): MaxHealth debe ser finita y mayor que 0."),
				*Label, *Preset.PresetID.ToString()));
		}
		if (!IsFiniteNonNegative(Preset.Armor))
		{
			AddError(Context, Result, FString::Printf(TEXT("%s (%s): Armor debe ser finita y no negativa."),
				*Label, *Preset.PresetID.ToString()));
		}
		if (!IsFiniteNonNegative(Preset.MagicResistance))
		{
			AddError(Context, Result, FString::Printf(TEXT("%s (%s): MagicResistance debe ser finita y no negativa."),
				*Label, *Preset.PresetID.ToString()));
		}
		if (!IsFinitePositive(Preset.MaxPoise))
		{
			AddError(Context, Result, FString::Printf(TEXT("%s (%s): MaxPoise debe ser finita y mayor que 0."),
				*Label, *Preset.PresetID.ToString()));
		}
		if (!IsFiniteNonNegative(Preset.FireResistance)
			|| !IsFiniteNonNegative(Preset.WaterResistance)
			|| !IsFiniteNonNegative(Preset.StormResistance)
			|| !IsFiniteNonNegative(Preset.NatureResistance))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (%s): todas las resistencias elementales deben ser finitas y no negativas."),
				*Label, *Preset.PresetID.ToString()));
		}
		if (!IsFiniteNonNegative(Preset.BaseWalkSpeed))
		{
			AddError(Context, Result, FString::Printf(TEXT("%s (%s): BaseWalkSpeed debe ser finita y no negativa."),
				*Label, *Preset.PresetID.ToString()));
		}
	}

	if (DefaultStatsPresetID.IsNone())
	{
		AddError(Context, Result, TEXT("DefaultStatsPresetID no puede ser None."));
	}
	else if (!PresetIDs.Contains(DefaultStatsPresetID))
	{
		AddError(Context, Result, FString::Printf(
			TEXT("DefaultStatsPresetID '%s' no existe en StatsPresets."),
			*DefaultStatsPresetID.ToString()));
	}

	if (Phases.IsEmpty())
	{
		AddError(Context, Result, TEXT("Phases necesita al menos una fase."));
	}

	TSet<FName> PhaseIDs;
	TSet<FGameplayTag> PhaseTags;
	int32 FullHealthPhaseCount = 0;
	for (int32 PhaseIndex = 0; PhaseIndex < Phases.Num(); ++PhaseIndex)
	{
		const FPantheliaBossPhaseDefinition& Phase = Phases[PhaseIndex];
		const FString Label = FString::Printf(TEXT("Phases[%d]"), PhaseIndex);

		if (Phase.PhaseID.IsNone())
		{
			AddError(Context, Result, FString::Printf(TEXT("%s: PhaseID no puede ser None."), *Label));
		}
		else if (PhaseIDs.Contains(Phase.PhaseID))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s: PhaseID '%s' está duplicado."), *Label, *Phase.PhaseID.ToString()));
		}
		else
		{
			PhaseIDs.Add(Phase.PhaseID);
		}

		if (!Phase.PhaseTag.IsValid())
		{
			AddError(Context, Result, FString::Printf(TEXT("%s (%s): PhaseTag es inválido."),
				*Label, *Phase.PhaseID.ToString()));
		}
		else if (!Phase.PhaseTag.MatchesTag(Tags.Boss_Phase) || Phase.PhaseTag == Tags.Boss_Phase)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (%s): PhaseTag '%s' debe ser una hoja bajo 'Boss.Phase'."),
				*Label, *Phase.PhaseID.ToString(), *Phase.PhaseTag.ToString()));
		}
		else if (PhaseTags.Contains(Phase.PhaseTag))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (%s): PhaseTag '%s' está duplicado."),
				*Label, *Phase.PhaseID.ToString(), *Phase.PhaseTag.ToString()));
		}
		else
		{
			PhaseTags.Add(Phase.PhaseTag);
		}

		if (!IsFiniteInRange(Phase.EnterHealthPercent, 0.f, 1.f))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (%s): EnterHealthPercent debe estar entre 0 y 1."),
				*Label, *Phase.PhaseID.ToString()));
		}
		else if (FMath::IsNearlyEqual(Phase.EnterHealthPercent, 1.f))
		{
			++FullHealthPhaseCount;
		}

		if (!IsFiniteNonNegative(Phase.WeightMultiplier))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (%s): WeightMultiplier debe ser finito y no negativo."),
				*Label, *Phase.PhaseID.ToString()));
		}

		for (int32 OtherIndex = 0; OtherIndex < PhaseIndex; ++OtherIndex)
		{
			if (FMath::IsNearlyEqual(Phase.EnterHealthPercent, Phases[OtherIndex].EnterHealthPercent))
			{
				AddError(Context, Result, FString::Printf(
					TEXT("%s (%s): comparte EnterHealthPercent=%f con Phases[%d] (%s). Umbrales iguales son ambiguos."),
					*Label,
					*Phase.PhaseID.ToString(),
					Phase.EnterHealthPercent,
					OtherIndex,
					*Phases[OtherIndex].PhaseID.ToString()));
			}
		}
	}

	if (!Phases.IsEmpty() && FullHealthPhaseCount != 1)
	{
		AddError(Context, Result, FString::Printf(
			TEXT("Debe existir exactamente una fase con EnterHealthPercent=1.0; se encontraron %d."),
			FullHealthPhaseCount));
	}

	if (Actions.IsEmpty())
	{
		AddError(Context, Result, TEXT("Actions necesita al menos una acción."));
	}

	TSet<FGameplayTag> ActionTags;
	for (int32 ActionIndex = 0; ActionIndex < Actions.Num(); ++ActionIndex)
	{
		const FPantheliaBossActionDefinition& Action = Actions[ActionIndex];
		const FString Label = FString::Printf(TEXT("Actions[%d]"), ActionIndex);

		if (!Action.ActionTag.IsValid())
		{
			AddError(Context, Result, FString::Printf(TEXT("%s: ActionTag es inválido."), *Label));
		}
		else if (!Action.ActionTag.MatchesTag(Tags.Boss_Action) || Action.ActionTag == Tags.Boss_Action)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s: ActionTag '%s' debe ser una hoja bajo 'Boss.Action'."),
				*Label, *Action.ActionTag.ToString()));
		}
		else if (ActionTags.Contains(Action.ActionTag))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s: ActionTag '%s' está duplicado."),
				*Label, *Action.ActionTag.ToString()));
		}
		else
		{
			ActionTags.Add(Action.ActionTag);
		}

		if (!Action.AbilityTag.IsValid()
			|| !Action.AbilityTag.MatchesTag(Tags.Abilities)
			|| Action.AbilityTag == Tags.Abilities)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (%s): AbilityTag '%s' debe ser una hoja bajo 'Abilities'."),
				*Label, *Action.ActionTag.ToString(), *Action.AbilityTag.ToString()));
		}

		if (!IsFiniteNonNegative(Action.MinDistance)
			|| !IsFiniteNonNegative(Action.MaxDistance)
			|| Action.MinDistance > Action.MaxDistance)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (%s): las distancias deben ser finitas, no negativas y MinDistance <= MaxDistance."),
				*Label, *Action.ActionTag.ToString()));
		}

		if (!IsFiniteInRange(Action.MaxAngle, 0.f, 180.f))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (%s): MaxAngle debe estar entre 0 y 180."),
				*Label, *Action.ActionTag.ToString()));
		}

		if (!IsFiniteNonNegative(Action.Weight) || !IsFiniteNonNegative(Action.Cooldown))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (%s): Weight y Cooldown deben ser finitos y no negativos."),
				*Label, *Action.ActionTag.ToString()));
		}

		if (!IsFiniteInRange(Action.ImmediateRepeatWeightMultiplier, 0.f, 1.f)
			|| !IsFiniteInRange(Action.RecentRepeatWeightMultiplier, 0.f, 1.f))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (%s): los multiplicadores de memoria deben estar entre 0 y 1."),
				*Label, *Action.ActionTag.ToString()));
		}

		if (Action.MaxConsecutiveUses < 0)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (%s): MaxConsecutiveUses no puede ser negativo."),
				*Label, *Action.ActionTag.ToString()));
		}

		if (Action.MemoryGroupTag.IsValid() && !Action.MemoryGroupTag.MatchesTag(Tags.Boss_Action))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (%s): MemoryGroupTag '%s' debe pertenecer a 'Boss.Action'."),
				*Label, *Action.ActionTag.ToString(), *Action.MemoryGroupTag.ToString()));
		}

		if (Action.RequiredOwnerTags.HasAny(Action.BlockedOwnerTags))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (%s): RequiredOwnerTags y BlockedOwnerTags se contradicen."),
				*Label, *Action.ActionTag.ToString()));
		}

		TArray<FGameplayTag> ValidPhaseTagArray;
		Action.ValidPhaseTags.GetGameplayTagArray(ValidPhaseTagArray);
		for (const FGameplayTag& ValidPhaseTag : ValidPhaseTagArray)
		{
			if (!PhaseTags.Contains(ValidPhaseTag))
			{
				AddError(Context, Result, FString::Printf(
					TEXT("%s (%s): ValidPhaseTag '%s' no existe en Phases."),
					*Label, *Action.ActionTag.ToString(), *ValidPhaseTag.ToString()));
			}
		}

		for (const FName LegacyPhaseID : Action.ValidPhases)
		{
			if (LegacyPhaseID.IsNone() || !PhaseIDs.Contains(LegacyPhaseID))
			{
				AddError(Context, Result, FString::Printf(
					TEXT("%s (%s): ValidPhases contiene el ID inexistente '%s'."),
					*Label, *Action.ActionTag.ToString(), *LegacyPhaseID.ToString()));
			}
		}
	}

	for (int32 PhaseIndex = 0; PhaseIndex < Phases.Num(); ++PhaseIndex)
	{
		const FPantheliaBossPhaseDefinition& Phase = Phases[PhaseIndex];
		TSet<FGameplayTag> PoolTags;
		for (const FGameplayTag& PoolActionTag : Phase.ExplicitActionPool)
		{
			if (!PoolActionTag.IsValid() || !ActionTags.Contains(PoolActionTag))
			{
				AddError(Context, Result, FString::Printf(
					TEXT("Phases[%d] (%s): ExplicitActionPool referencia la acción inexistente '%s'."),
					PhaseIndex, *Phase.PhaseID.ToString(), *PoolActionTag.ToString()));
			}
			else if (PoolTags.Contains(PoolActionTag))
			{
				AddError(Context, Result, FString::Printf(
					TEXT("Phases[%d] (%s): ExplicitActionPool repite '%s'."),
					PhaseIndex, *Phase.PhaseID.ToString(), *PoolActionTag.ToString()));
			}
			PoolTags.Add(PoolActionTag);
		}

		bool bHasSelectableAction = false;
		for (const FPantheliaBossActionDefinition& Action : Actions)
		{
			if (Phase.ExplicitActionPool.Num() > 0 && !Phase.ExplicitActionPool.Contains(Action.ActionTag))
			{
				continue;
			}

			bool bAllowedByPhase = true;
			if (!Action.ValidPhaseTags.IsEmpty())
			{
				bAllowedByPhase = Action.ValidPhaseTags.HasTagExact(Phase.PhaseTag);
			}
			else if (!Action.ValidPhases.IsEmpty())
			{
				bAllowedByPhase = Action.ValidPhases.Contains(Phase.PhaseID);
			}

			if (bAllowedByPhase && Action.Weight > 0.f && Phase.WeightMultiplier > 0.f)
			{
				bHasSelectableAction = true;
				break;
			}
		}

		if (!bHasSelectableAction)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("Phases[%d] (%s) no tiene ninguna acción seleccionable con peso positivo."),
				PhaseIndex, *Phase.PhaseID.ToString()));
		}
	}

	return Result;
}

#endif // WITH_EDITOR
