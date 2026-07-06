#pragma once

#include "CoreMinimal.h"
#include "GameplayModMagnitudeCalculation.h"
#include "MMC_MaxStamina.generated.h"

UCLASS()
class PANTHELIAPROJECT_API UMMC_MaxStamina : public UGameplayModMagnitudeCalculation
{
	GENERATED_BODY()

public:
	UMMC_MaxStamina();

	virtual float CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const override;

private:
	// Captura del atributo Endurance del objetivo.
	FGameplayEffectAttributeCaptureDefinition EnduranceDef;
};