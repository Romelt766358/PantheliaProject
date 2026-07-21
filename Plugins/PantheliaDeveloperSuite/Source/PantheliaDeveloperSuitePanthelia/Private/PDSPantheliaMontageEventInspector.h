#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

class UAnimMontage;

namespace PDSPantheliaMontageEventInspector
{
    bool ContainsGameplayTagEvent(
        const UAnimMontage* Montage,
        const FGameplayTag& RequiredTag);
}
