#include "PDSPantheliaSpellBlueprintValidator.h"

#include "AbilitySystem/Abilities/PantheliaProjectileSpell.h"
#include "Engine/Blueprint.h"
#include "PDSPantheliaMontageEventInspector.h"
#include "UObject/Class.h"

bool UPDSPantheliaSpellBlueprintValidator::CanValidateAsset_Implementation(
    const FAssetData& InAssetData,
    UObject* InObject,
    FDataValidationContext& InContext) const
{
    const UBlueprint* Blueprint = Cast<UBlueprint>(InObject);
    return IsValid(Blueprint)
        && IsValid(Blueprint->GeneratedClass)
        && Blueprint->GeneratedClass->IsChildOf(
            UPantheliaProjectileSpell::StaticClass())
        && !Blueprint->GeneratedClass->HasAnyClassFlags(
            CLASS_Abstract);
}

EDataValidationResult
UPDSPantheliaSpellBlueprintValidator::ValidateLoadedAsset_Implementation(
    const FAssetData& InAssetData,
    UObject* InAsset,
    FDataValidationContext& Context)
{
    UBlueprint* Blueprint = Cast<UBlueprint>(InAsset);
    if (!IsValid(Blueprint)
        || !IsValid(Blueprint->GeneratedClass))
    {
        AssetFails(
            InAsset,
            NSLOCTEXT(
                "PantheliaDeveloperSuite",
                "SpellBlueprintGeneratedClassMissing",
                "El Blueprint de hechizo no tiene GeneratedClass válida."));
        return EDataValidationResult::Invalid;
    }

    if (Blueprint->GeneratedClass->HasAnyClassFlags(CLASS_Abstract))
    {
        return EDataValidationResult::NotValidated;
    }

    const UPantheliaProjectileSpell* SpellCDO =
        Cast<UPantheliaProjectileSpell>(
            Blueprint->GeneratedClass->GetDefaultObject());
    if (!IsValid(SpellCDO))
    {
        AssetFails(
            InAsset,
            NSLOCTEXT(
                "PantheliaDeveloperSuite",
                "SpellBlueprintCDOMissing",
                "La GeneratedClass no produjo un CDO de UPantheliaProjectileSpell."));
        return EDataValidationResult::Invalid;
    }

    const uint32 ErrorsBefore = Context.GetNumErrors();
    const EDataValidationResult Result =
        SpellCDO->IsDataValid(Context);

    const bool bMontageContractValid =
        PDSPantheliaMontageEventInspector::ContainsGameplayTagEvent(
            SpellCDO->GetCastMontageForEditor(),
            SpellCDO->GetProjectileSpawnEventTagForEditor());

    if (IsValid(SpellCDO->GetCastMontageForEditor())
        && SpellCDO->GetProjectileSpawnEventTagForEditor().IsValid()
        && !bMontageContractValid)
    {
        Context.AddError(FText::Format(
            NSLOCTEXT(
                "PantheliaDeveloperSuite",
                "SpellMontageEventMissing",
                "CastMontage no contiene un Notify o Notify State con el Gameplay Tag requerido: {0}."),
            FText::FromString(
                SpellCDO->GetProjectileSpawnEventTagForEditor().ToString())));
    }

    if (Result == EDataValidationResult::Invalid
        || Context.GetNumErrors() > ErrorsBefore)
    {
        AssetFails(
            InAsset,
            NSLOCTEXT(
                "PantheliaDeveloperSuite",
                "SpellBlueprintInvalid",
                "La configuración del CDO del hechizo es inválida."));
        return EDataValidationResult::Invalid;
    }

    AssetPasses(InAsset);
    return EDataValidationResult::Valid;
}
