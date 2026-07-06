#include "AbilitySystem/MMC_MaxStamina.h"
#include "AbilitySystem/PantheliaAttributeSet.h"
#include "Interfaces/CombatInterface.h"

UMMC_MaxStamina::UMMC_MaxStamina()
{
	EnduranceDef.AttributeToCapture = UPantheliaAttributeSet::GetEnduranceAttribute();
	EnduranceDef.AttributeSource = EGameplayEffectAttributeCaptureSource::Target;
	EnduranceDef.bSnapshot = false;

	RelevantAttributesToCapture.Add(EnduranceDef);
}

float UMMC_MaxStamina::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	FAggregatorEvaluateParameters EvaluationParams;
	EvaluationParams.SourceTags = SourceTags;
	EvaluationParams.TargetTags = TargetTags;

	float Endurance = 0.f;
	GetCapturedAttributeMagnitude(EnduranceDef, Spec, EvaluationParams, Endurance);
	Endurance = FMath::Max<float>(Endurance, 0.f);

	int32 PlayerLevel = 1;
	if (AActor* SourceActor = Cast<AActor>(Spec.GetContext().GetSourceObject()))
	{
		if (SourceActor->Implements<UCombatInterface>())
		{
			PlayerLevel = ICombatInterface::Execute_GetPlayerLevel(SourceActor);
		}
	}

	// Fórmula: MaxStamina = 80 + (Endurance * 2.5) + (Level * 8)
	return 80.f + (2.5f * Endurance) + (8.f * PlayerLevel);
}