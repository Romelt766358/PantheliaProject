#pragma once

#include "CoreMinimal.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"

class UPantheliaMultiProjectileSpell;
class UPantheliaProjectileSpell;

namespace PantheliaSpellValidation
{
    PANTHELIAPROJECT_API EDataValidationResult ValidateProjectileSpell(
        const UPantheliaProjectileSpell& Spell,
        FDataValidationContext& Context);

    PANTHELIAPROJECT_API EDataValidationResult ValidateMultiProjectileSpell(
        const UPantheliaMultiProjectileSpell& Spell,
        FDataValidationContext& Context);
}

#endif
