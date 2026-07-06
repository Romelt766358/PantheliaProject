#include "AbilitySystem/MMC_MaxMana.h"
#include "AbilitySystem/PantheliaAttributeSet.h"
#include "Interfaces/CombatInterface.h"

UMMC_MaxMana::UMMC_MaxMana()
{
	SpiritDef.AttributeToCapture = UPantheliaAttributeSet::GetSpiritAttribute();
	SpiritDef.AttributeSource = EGameplayEffectAttributeCaptureSource::Target;
	SpiritDef.bSnapshot = false;

	RelevantAttributesToCapture.Add(SpiritDef);
}

float UMMC_MaxMana::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	FAggregatorEvaluateParameters EvaluationParams;
	EvaluationParams.SourceTags = SourceTags;
	EvaluationParams.TargetTags = TargetTags;

	float Spirit = 0.f;
	GetCapturedAttributeMagnitude(SpiritDef, Spec, EvaluationParams, Spirit);
	Spirit = FMath::Max<float>(Spirit, 0.f);

	int32 PlayerLevel = 1;
	if (AActor* SourceActor = Cast<AActor>(Spec.GetContext().GetSourceObject()))
	{
		if (SourceActor->Implements<UCombatInterface>())
		{
			PlayerLevel = ICombatInterface::Execute_GetPlayerLevel(SourceActor);
		}
	}

	// Fórmula: MaxMana = 50 + (Spirit * 2.5) + (Level * 5)
	return 50.f + (2.5f * Spirit) + (5.f * PlayerLevel);
}