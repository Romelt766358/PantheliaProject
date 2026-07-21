#include "AbilitySystem/Abilities/PantheliaSpellValidation.h"

#if WITH_EDITOR

#include "AbilitySystem/Abilities/PantheliaMultiProjectileSpell.h"
#include "AbilitySystem/Abilities/PantheliaProjectileSpell.h"
#include "GameplayEffect.h"
#include "Misc/DataValidation.h"

namespace
{
    void AddError(FDataValidationContext& Context, const FString& Message)
    {
        Context.AddError(FText::FromString(Message));
    }

    void AddWarning(FDataValidationContext& Context, const FString& Message)
    {
        Context.AddWarning(FText::FromString(Message));
    }

    bool ValidateScalableFloat(
        FDataValidationContext& Context,
        const FString& FieldName,
        const FScalableFloat& Value,
        const float Minimum,
        const TOptional<float> Maximum = TOptional<float>())
    {
        bool bValid = true;

        if (!Value.IsValid())
        {
            AddError(
                Context,
                FString::Printf(
                    TEXT("%s contiene una referencia de curva inválida."),
                    *FieldName));
            bValid = false;
        }

        // Contrato v1: se muestrea nivel 1. Esto detecta referencias rotas y
        // configuraciones base inválidas, pero no garantiza todos los niveles de la curva.
        const float LevelOneValue = Value.GetValueAtLevel(1.f);
        if (!FMath::IsFinite(LevelOneValue))
        {
            AddError(
                Context,
                FString::Printf(
                    TEXT("%s produce un valor no finito en nivel 1."),
                    *FieldName));
            return false;
        }

        if (LevelOneValue < Minimum)
        {
            AddError(
                Context,
                FString::Printf(
                    TEXT("%s debe ser >= %.3f; valor nivel 1: %.3f."),
                    *FieldName,
                    Minimum,
                    LevelOneValue));
            bValid = false;
        }

        if (Maximum.IsSet() && LevelOneValue > Maximum.GetValue())
        {
            AddError(
                Context,
                FString::Printf(
                    TEXT("%s debe ser <= %.3f; valor nivel 1: %.3f."),
                    *FieldName,
                    Maximum.GetValue(),
                    LevelOneValue));
            bValid = false;
        }

        return bValid;
    }

    EDataValidationResult ResultFromErrorDelta(
        const FDataValidationContext& Context,
        const uint32 ErrorsBefore)
    {
        return Context.GetNumErrors() > ErrorsBefore
            ? EDataValidationResult::Invalid
            : EDataValidationResult::Valid;
    }
}

EDataValidationResult PantheliaSpellValidation::ValidateProjectileSpell(
    const UPantheliaProjectileSpell& Spell,
    FDataValidationContext& Context)
{
    const uint32 ErrorsBefore = Context.GetNumErrors();

    if (Spell.IsPantheliaResourceCostEnabledForEditor())
    {
        if (Spell.GetResourceCostTypeForEditor()
            == EPantheliaResourceCostType::None)
        {
            AddError(
                Context,
                TEXT("Cost|Panthelia está activo, pero ResourceCostType es None."));
        }

        if (!IsValid(Spell.GetCostGameplayEffect()))
        {
            AddError(
                Context,
                TEXT("Cost|Panthelia está activo, pero falta Cost Gameplay Effect."));
        }

        ValidateScalableFloat(
            Context,
            TEXT("BaseResourceCost"),
            Spell.GetBaseResourceCostForEditor(),
            0.f);

        TSet<EPantheliaResourceCostType> SeenResources;
        SeenResources.Add(Spell.GetResourceCostTypeForEditor());

        for (int32 Index = 0;
             Index < Spell.GetAdditionalResourceCostsForEditor().Num();
             ++Index)
        {
            const FPantheliaAdditionalResourceCost& Cost =
                Spell.GetAdditionalResourceCostsForEditor()[Index];

            if (Cost.ResourceType == EPantheliaResourceCostType::None)
            {
                AddError(
                    Context,
                    FString::Printf(
                        TEXT("AdditionalResourceCosts[%d] usa None."),
                        Index));
            }

            if (SeenResources.Contains(Cost.ResourceType))
            {
                AddError(
                    Context,
                    FString::Printf(
                        TEXT("AdditionalResourceCosts[%d] duplica un recurso ya configurado."),
                        Index));
            }
            SeenResources.Add(Cost.ResourceType);

            if (!Cost.CostGameplayEffectClass)
            {
                AddError(
                    Context,
                    FString::Printf(
                        TEXT("AdditionalResourceCosts[%d] no tiene GameplayEffect."),
                        Index));
            }

            ValidateScalableFloat(
                Context,
                FString::Printf(
                    TEXT("AdditionalResourceCosts[%d].BaseCost"),
                    Index),
                Cost.BaseCost,
                0.f);
        }
    }

    if (!Spell.GetDamageEffectClassForEditor())
    {
        AddError(Context, TEXT("DamageEffectClass no está asignado."));
    }

    if (Spell.DamageTypes.IsEmpty())
    {
        AddError(Context, TEXT("DamageTypes no contiene entradas."));
    }

    for (const TPair<FGameplayTag, FScalableFloat>& Pair : Spell.DamageTypes)
    {
        if (!Pair.Key.IsValid())
        {
            AddError(Context, TEXT("DamageTypes contiene un tag inválido."));
            continue;
        }

        ValidateScalableFloat(
            Context,
            TEXT("DamageTypes.") + Pair.Key.ToString(),
            Pair.Value,
            0.f);
    }

    if (Spell.AttributeScalings.Num() > 2)
    {
        AddError(
            Context,
            FString::Printf(
                TEXT("AttributeScalings tiene %d entradas; el máximo de diseño es 2."),
                Spell.AttributeScalings.Num()));
    }

    TSet<FGameplayTag> ScalingTags;
    for (int32 Index = 0; Index < Spell.AttributeScalings.Num(); ++Index)
    {
        const FAbilityAttributeScaling& Scaling =
            Spell.AttributeScalings[Index];

        if (!Scaling.AttributeTag.IsValid())
        {
            AddError(
                Context,
                FString::Printf(
                    TEXT("AttributeScalings[%d].AttributeTag es inválido."),
                    Index));
        }
        else if (ScalingTags.Contains(Scaling.AttributeTag))
        {
            AddError(
                Context,
                FString::Printf(
                    TEXT("AttributeScalings repite el tag %s."),
                    *Scaling.AttributeTag.ToString()));
        }
        ScalingTags.Add(Scaling.AttributeTag);

        if (!FMath::IsFinite(Scaling.Ratio) || Scaling.Ratio < 0.f)
        {
            AddError(
                Context,
                FString::Printf(
                    TEXT("AttributeScalings[%d].Ratio debe ser finito y >= 0."),
                    Index));
        }
    }

    for (const TPair<FGameplayTag, FScalableFloat>& Pair : Spell.BuildupAmounts)
    {
        if (!Pair.Key.IsValid())
        {
            AddError(Context, TEXT("BuildupAmounts contiene un tag inválido."));
            continue;
        }

        if (!Spell.DamageTypes.Contains(Pair.Key))
        {
            AddError(
                Context,
                FString::Printf(
                    TEXT("BuildupAmounts.%s no tiene DamageTypes correspondiente."),
                    *Pair.Key.ToString()));
        }

        ValidateScalableFloat(
            Context,
            TEXT("BuildupAmounts.") + Pair.Key.ToString(),
            Pair.Value,
            0.f);

        const float LevelOneValue = Pair.Value.GetValueAtLevel(1.f);
        if (FMath::IsFinite(LevelOneValue) && LevelOneValue > 100.f)
        {
            AddWarning(
                Context,
                FString::Printf(
                    TEXT("BuildupAmounts.%s supera 100 en nivel 1 (%.3f). Es válido, pero llena más de una barra base por impacto."),
                    *Pair.Key.ToString(),
                    LevelOneValue));
        }
    }

    ValidateScalableFloat(
        Context,
        TEXT("PoiseDamage"),
        Spell.PoiseDamage,
        0.f);
    ValidateScalableFloat(
        Context,
        TEXT("GrievousWoundsPercent"),
        Spell.GrievousWoundsPercent,
        0.f,
        100.f);
    ValidateScalableFloat(
        Context,
        TEXT("GrievousWoundsDuration"),
        Spell.GrievousWoundsDuration,
        0.f);
    ValidateScalableFloat(
        Context,
        TEXT("KnockbackChance"),
        Spell.KnockbackChance,
        0.f,
        100.f);
    ValidateScalableFloat(
        Context,
        TEXT("KnockbackForceMagnitude"),
        Spell.KnockbackForceMagnitude,
        0.f);
    ValidateScalableFloat(
        Context,
        TEXT("LaunchChance"),
        Spell.LaunchChance,
        0.f,
        100.f);
    ValidateScalableFloat(
        Context,
        TEXT("LaunchForceMagnitude"),
        Spell.LaunchForceMagnitude,
        0.f);
    ValidateScalableFloat(
        Context,
        TEXT("LaunchPitchOverride"),
        Spell.LaunchPitchOverride,
        -89.f,
        89.f);

    if (!Spell.GetProjectileClassForEditor())
    {
        AddError(Context, TEXT("ProjectileClass no está asignado."));
    }
    if (!IsValid(Spell.GetCastMontageForEditor()))
    {
        AddError(Context, TEXT("CastMontage no está asignado."));
    }
    if (!Spell.GetSocketTagForEditor().IsValid())
    {
        AddError(Context, TEXT("SocketTag no es válido."));
    }
    if (!Spell.GetProjectileSpawnEventTagForEditor().IsValid())
    {
        AddError(Context, TEXT("ProjectileSpawnEventTag no es válido."));
    }

    // El contrato cross-asset montage/tag lo verifica el validator del módulo adaptador,
    // porque el runtime module no debe depender del tipo Blueprint de Anim Notify.
    return ResultFromErrorDelta(Context, ErrorsBefore);
}

EDataValidationResult PantheliaSpellValidation::ValidateMultiProjectileSpell(
    const UPantheliaMultiProjectileSpell& Spell,
    FDataValidationContext& Context)
{
    const uint32 ErrorsBefore = Context.GetNumErrors();

    if (Spell.GetMaxProjectileCountForEditor() < 1
        || Spell.GetMaxProjectileCountForEditor() > 64)
    {
        AddError(Context, TEXT("MaxProjectileCount debe estar entre 1 y 64."));
    }

    ValidateScalableFloat(
        Context,
        TEXT("ProjectileCountByAbilityLevel"),
        Spell.GetProjectileCountForEditor(),
        1.f,
        static_cast<float>(
            FMath::Max(1, Spell.GetMaxProjectileCountForEditor())));

    ValidateScalableFloat(
        Context,
        TEXT("ProjectileSpreadDegrees"),
        Spell.GetProjectileSpreadForEditor(),
        0.f,
        360.f);
    ValidateScalableFloat(
        Context,
        TEXT("ProjectileSpawnInterval"),
        Spell.GetProjectileSpawnIntervalForEditor(),
        0.f);
    ValidateScalableFloat(
        Context,
        TEXT("LaunchPitchDegrees"),
        Spell.GetLaunchPitchForEditor(),
        -89.f,
        89.f);
    ValidateScalableFloat(
        Context,
        TEXT("ProjectileSpeedOverride"),
        Spell.GetProjectileSpeedOverrideForEditor(),
        0.f);
    ValidateScalableFloat(
        Context,
        TEXT("HomingStartDelay"),
        Spell.GetHomingStartDelayForEditor(),
        0.f);
    ValidateScalableFloat(
        Context,
        TEXT("HomingDuration"),
        Spell.GetHomingDurationForEditor(),
        0.f);
    ValidateScalableFloat(
        Context,
        TEXT("HomingAccelerationMagnitude"),
        Spell.GetHomingAccelerationForEditor(),
        0.f);
    ValidateScalableFloat(
        Context,
        TEXT("MaxHomingCorrectionAngleDegrees"),
        Spell.GetMaxHomingCorrectionAngleForEditor(),
        0.f,
        180.f);

    if (Spell.IsSoftHomingEnabledForEditor())
    {
        if (Spell.GetHomingDurationForEditor().GetValueAtLevel(1.f)
            <= 0.f)
        {
            AddError(
                Context,
                TEXT("Soft homing está activo, pero HomingDuration es 0."));
        }
        if (Spell.GetHomingAccelerationForEditor().GetValueAtLevel(1.f)
            <= 0.f)
        {
            AddError(
                Context,
                TEXT("Soft homing está activo, pero HomingAccelerationMagnitude es 0."));
        }
    }

    return ResultFromErrorDelta(Context, ErrorsBefore);
}

#endif
