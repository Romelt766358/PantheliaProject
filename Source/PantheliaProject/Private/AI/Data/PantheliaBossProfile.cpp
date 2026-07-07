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
