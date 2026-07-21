#include "PDSPantheliaSpellSnapshotContributor.h"

#include "AbilitySystem/Abilities/PantheliaDamageGameplayAbility.h"
#include "AbilitySystem/Abilities/PantheliaGameplayAbility.h"
#include "AbilitySystem/Abilities/PantheliaMultiProjectileSpell.h"
#include "AbilitySystem/Abilities/PantheliaProjectileSpell.h"
#include "Animation/AnimMontage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/BlueprintSupport.h"
#include "DataRegistryId.h"
#include "Engine/Blueprint.h"
#include "GameplayEffect.h"
#include "PDSPantheliaMontageEventInspector.h"
#include "UObject/Class.h"
#include "UObject/SoftObjectPath.h"

namespace
{
    FString BoolString(const bool bValue)
    {
        return bValue ? TEXT("true") : TEXT("false");
    }

    FString StableFloat(const double Value)
    {
        return FMath::IsFinite(Value)
            ? FString::Printf(TEXT("%.6f"), Value)
            : TEXT("non-finite");
    }

    FString ObjectPath(const UObject* Object)
    {
        return IsValid(Object) ? Object->GetPathName() : FString();
    }

    FString ObjectClassPath(const UObject* Object)
    {
        return IsValid(Object) && IsValid(Object->GetClass())
            ? Object->GetClass()->GetPathName()
            : FString();
    }

    FString ClassPath(const UClass* Class)
    {
        return IsValid(Class) ? Class->GetPathName() : FString();
    }

    FString ScalableFloatString(const FScalableFloat& Value)
    {
        // El catálogo v1 toma nivel 1 como muestra canónica. No afirma validar
        // todos los niveles potenciales de una curva.
        const FString CurveTablePath =
            IsValid(Value.Curve.CurveTable)
                ? Value.Curve.CurveTable->GetPathName()
                : FString();

        return FString::Printf(
            TEXT("raw=%s|curve=%s|row=%s|registry=%s|level1=%s"),
            *StableFloat(Value.Value),
            *CurveTablePath,
            *Value.Curve.RowName.ToString(),
            *Value.RegistryType.ToString(),
            *StableFloat(Value.GetValueAtLevel(1.f)));
    }

    FString GameplayTagContainerString(const FGameplayTagContainer& Container)
    {
        TArray<FGameplayTag> Tags;
        Container.GetGameplayTagArray(Tags);
        Tags.Sort([](const FGameplayTag& A, const FGameplayTag& B)
        {
            return A.ToString() < B.ToString();
        });

        TArray<FString> Values;
        Values.Reserve(Tags.Num());
        for (const FGameplayTag& Tag : Tags)
        {
            Values.Add(Tag.ToString());
        }
        return FString::Join(Values, TEXT(";"));
    }

    FString ScalableMapString(
        const TMap<FGameplayTag, FScalableFloat>& Values)
    {
        TArray<FGameplayTag> Keys;
        Values.GetKeys(Keys);
        Keys.Sort([](const FGameplayTag& A, const FGameplayTag& B)
        {
            return A.ToString() < B.ToString();
        });

        TArray<FString> Entries;
        for (const FGameplayTag& Key : Keys)
        {
            Entries.Add(
                Key.ToString()
                + TEXT("=")
                + ScalableFloatString(Values.FindChecked(Key)));
        }
        return FString::Join(Entries, TEXT(";"));
    }

    FString AttributeScalingString(
        const TArray<FAbilityAttributeScaling>& Values)
    {
        TArray<FString> Entries;
        Entries.Reserve(Values.Num());
        for (const FAbilityAttributeScaling& Value : Values)
        {
            Entries.Add(
                Value.AttributeTag.ToString()
                + TEXT("=")
                + StableFloat(Value.Ratio));
        }
        Entries.Sort();
        return FString::Join(Entries, TEXT(";"));
    }

    FString AdditionalCostsString(
        const TArray<FPantheliaAdditionalResourceCost>& Values)
    {
        TArray<FString> Entries;
        Entries.Reserve(Values.Num());

        const UEnum* ResourceEnum =
            StaticEnum<EPantheliaResourceCostType>();

        for (const FPantheliaAdditionalResourceCost& Value : Values)
        {
            Entries.Add(FString::Printf(
                TEXT("%s|base=%s|effect=%s"),
                ResourceEnum
                    ? *ResourceEnum->GetNameStringByValue(
                        static_cast<int64>(Value.ResourceType))
                    : TEXT("Unknown"),
                *ScalableFloatString(Value.BaseCost),
                *ClassPath(Value.CostGameplayEffectClass.Get())));
        }

        Entries.Sort();
        return FString::Join(Entries, TEXT(";"));
    }

    UClass* ResolveGeneratedClass(
        const FAssetData& AssetData,
        bool& bOutWasAlreadyLoaded)
    {
        bOutWasAlreadyLoaded = false;

        FString ExportPath;
        if (!AssetData.GetTagValue(
                FBlueprintTags::GeneratedClassPath,
                ExportPath))
        {
            return nullptr;
        }

        const FString ObjectPathValue =
            FPackageName::ExportTextPathToObjectPath(ExportPath);
        UClass* GeneratedClass =
            FindObject<UClass>(nullptr, *ObjectPathValue);
        bOutWasAlreadyLoaded = IsValid(GeneratedClass);

        return bOutWasAlreadyLoaded
            ? GeneratedClass
            : LoadObject<UClass>(nullptr, *ObjectPathValue);
    }

    bool CouldBeProjectileSpellBlueprint(
        const FAssetData& AssetData,
        TMap<FString, TWeakObjectPtr<UClass>>& NativeParentClassCache,
        int32& OutNewlyLoadedAssetCount)
    {
        FString NativeParentExportPath;
        if (!AssetData.GetTagValue(
                FBlueprintTags::NativeParentClassPath,
                NativeParentExportPath))
        {
            return false;
        }

        if (const TWeakObjectPtr<UClass>* Cached =
                NativeParentClassCache.Find(NativeParentExportPath))
        {
            const UClass* CachedClass = Cached->Get();
            return IsValid(CachedClass)
                && CachedClass->IsChildOf(
                    UPantheliaProjectileSpell::StaticClass());
        }

        const FString NativeParentObjectPath =
            FPackageName::ExportTextPathToObjectPath(
                NativeParentExportPath);

        UClass* NativeParentClass =
            FindObject<UClass>(nullptr, *NativeParentObjectPath);
        const bool bWasAlreadyLoaded = IsValid(NativeParentClass);
        if (!bWasAlreadyLoaded)
        {
            NativeParentClass =
                LoadObject<UClass>(nullptr, *NativeParentObjectPath);
        }

        if (!bWasAlreadyLoaded && IsValid(NativeParentClass))
        {
            ++OutNewlyLoadedAssetCount;
        }

        NativeParentClassCache.Add(
            NativeParentExportPath,
            NativeParentClass);

        return IsValid(NativeParentClass)
            && NativeParentClass->IsChildOf(
                UPantheliaProjectileSpell::StaticClass());
    }

    FString EnumName(const UEnum* Enum, const int64 Value)
    {
        return Enum
            ? Enum->GetNameStringByValue(Value)
            : FString::Printf(TEXT("%lld"), Value);
    }
}

FString FPDSPantheliaSpellSnapshotContributor::GetDomainId() const
{
    return TEXT("spells");
}

FString FPDSPantheliaSpellSnapshotContributor::GetSchemaVersion() const
{
    return TEXT("1.0.0");
}

void FPDSPantheliaSpellSnapshotContributor::GatherDomainSnapshot(
    FPDSSemanticDomainSnapshot& OutSnapshot) const
{
    OutSnapshot.DomainId = GetDomainId();
    OutSnapshot.SchemaVersion = GetSchemaVersion();

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(
            TEXT("AssetRegistry"));

    FARFilter Filter;
    Filter.ClassPaths.Add(UBlueprint::StaticClass()->GetClassPathName());
    Filter.PackagePaths.Add(FName(TEXT("/Game")));
    Filter.bRecursiveClasses = true;
    Filter.bRecursivePaths = true;

    TArray<FAssetData> BlueprintAssets;
    AssetRegistryModule.Get().GetAssets(Filter, BlueprintAssets);
    BlueprintAssets.Sort([](const FAssetData& A, const FAssetData& B)
    {
        return A.GetSoftObjectPath().ToString()
            < B.GetSoftObjectPath().ToString();
    });

    // PDS-75: muchos Blueprints comparten el mismo NativeParentClassPath.
    // Resolver cada parent una sola vez evita parseos y búsquedas globales repetidos.
    TMap<FString, TWeakObjectPtr<UClass>> NativeParentClassCache;

    for (const FAssetData& AssetData : BlueprintAssets)
    {
        if (!CouldBeProjectileSpellBlueprint(
                AssetData,
                NativeParentClassCache,
                OutSnapshot.NewlyLoadedAssetCount))
        {
            continue;
        }

        bool bGeneratedClassWasAlreadyLoaded = false;
        UClass* GeneratedClass = ResolveGeneratedClass(
            AssetData,
            bGeneratedClassWasAlreadyLoaded);
        if (!bGeneratedClassWasAlreadyLoaded && IsValid(GeneratedClass))
        {
            ++OutSnapshot.NewlyLoadedAssetCount;
        }
        if (!IsValid(GeneratedClass)
            || !GeneratedClass->IsChildOf(
                UPantheliaProjectileSpell::StaticClass())
            || GeneratedClass->HasAnyClassFlags(CLASS_Abstract))
        {
            continue;
        }

        const UPantheliaProjectileSpell* Spell =
            Cast<UPantheliaProjectileSpell>(
                GeneratedClass->GetDefaultObject());
        if (!IsValid(Spell))
        {
            OutSnapshot.Issues.Add({
                EPDSIssueSeverity::Warning,
                TEXT("PDS.SpellCatalog.CDOMissing"),
                AssetData.GetSoftObjectPath().ToString(),
                TEXT("La GeneratedClass es un ProjectileSpell, pero no produjo un CDO válido."),
                EPDSAssetOrigin::PantheliaCore
            });
            continue;
        }

        FPDSSemanticRecord Record;
        if (BuildRecord(
                AssetData,
                *Spell,
                Record,
                OutSnapshot.Issues))
        {
            OutSnapshot.Records.Add(MoveTemp(Record));
        }
    }
}

bool FPDSPantheliaSpellSnapshotContributor::BuildRecord(
    const FAssetData& AssetData,
    const UPantheliaProjectileSpell& Spell,
    FPDSSemanticRecord& OutRecord,
    TArray<FPDSIssue>& OutIssues) const
{
    const UClass* SpellClass = Spell.GetClass();

    OutRecord.RecordId = ClassPath(SpellClass);
    OutRecord.DisplayName = AssetData.AssetName.ToString();
    OutRecord.SourceAssetPath =
        AssetData.GetSoftObjectPath().ToString();
    OutRecord.Kind =
        Spell.IsA<UPantheliaMultiProjectileSpell>()
            ? TEXT("MultiProjectile")
            : TEXT("Projectile");

    OutRecord.Fields.Add(
        TEXT("class.parentPath"),
        ClassPath(SpellClass ? SpellClass->GetSuperClass() : nullptr));
    OutRecord.Fields.Add(
        TEXT("ability.assetTags"),
        GameplayTagContainerString(Spell.GetAssetTags()));
    OutRecord.Fields.Add(
        TEXT("ability.startupInputTag"),
        Spell.StartupInputTag.ToString());
    OutRecord.Fields.Add(
        TEXT("ability.inputActivationPolicy"),
        EnumName(
            StaticEnum<EPantheliaAbilityInputActivationPolicy>(),
            static_cast<int64>(Spell.GetInputActivationPolicyForEditor())));

    OutRecord.Fields.Add(
        TEXT("cost.enabled"),
        BoolString(Spell.IsPantheliaResourceCostEnabledForEditor()));
    OutRecord.Fields.Add(
        TEXT("cost.mainResource"),
        EnumName(
            StaticEnum<EPantheliaResourceCostType>(),
            static_cast<int64>(
                Spell.GetResourceCostTypeForEditor())));
    OutRecord.Fields.Add(
        TEXT("cost.base"),
        ScalableFloatString(Spell.GetBaseResourceCostForEditor()));
    OutRecord.Fields.Add(
        TEXT("cost.effect"),
        ObjectClassPath(Spell.GetCostGameplayEffect()));
    OutRecord.Fields.Add(
        TEXT("cost.additional"),
        AdditionalCostsString(
            Spell.GetAdditionalResourceCostsForEditor()));
    OutRecord.Fields.Add(
        TEXT("cooldown.effect"),
        ObjectClassPath(Spell.GetCooldownGameplayEffect()));

    OutRecord.Fields.Add(
        TEXT("damage.effect"),
        ClassPath(Spell.GetDamageEffectClassForEditor().Get()));
    OutRecord.Fields.Add(
        TEXT("damage.types"),
        ScalableMapString(Spell.DamageTypes));
    OutRecord.Fields.Add(
        TEXT("damage.attributeScalings"),
        AttributeScalingString(Spell.AttributeScalings));
    OutRecord.Fields.Add(
        TEXT("damage.buildup"),
        ScalableMapString(Spell.BuildupAmounts));
    OutRecord.Fields.Add(
        TEXT("damage.poise"),
        ScalableFloatString(Spell.PoiseDamage));
    OutRecord.Fields.Add(
        TEXT("damage.dodgeResponse"),
        EnumName(
            StaticEnum<EPantheliaDodgeResponse>(),
            static_cast<int64>(Spell.DodgeResponse)));
    OutRecord.Fields.Add(
        TEXT("damage.defenseAttackType"),
        EnumName(
            StaticEnum<EPantheliaDefenseAttackType>(),
            static_cast<int64>(Spell.DefenseAttackType)));
    OutRecord.Fields.Add(
        TEXT("damage.grievousWoundsPercent"),
        ScalableFloatString(Spell.GrievousWoundsPercent));
    OutRecord.Fields.Add(
        TEXT("damage.grievousWoundsDuration"),
        ScalableFloatString(Spell.GrievousWoundsDuration));
    OutRecord.Fields.Add(
        TEXT("damage.knockbackChance"),
        ScalableFloatString(Spell.KnockbackChance));
    OutRecord.Fields.Add(
        TEXT("damage.knockbackForce"),
        ScalableFloatString(Spell.KnockbackForceMagnitude));
    OutRecord.Fields.Add(
        TEXT("damage.knockbackHeavy"),
        BoolString(Spell.bKnockbackIsHeavy));
    OutRecord.Fields.Add(
        TEXT("damage.launchChance"),
        ScalableFloatString(Spell.LaunchChance));
    OutRecord.Fields.Add(
        TEXT("damage.launchForce"),
        ScalableFloatString(Spell.LaunchForceMagnitude));
    OutRecord.Fields.Add(
        TEXT("damage.launchPitch"),
        ScalableFloatString(Spell.LaunchPitchOverride));

    const UAnimMontage* CastMontage =
        Spell.GetCastMontageForEditor();
    const FGameplayTag SocketTag =
        Spell.GetSocketTagForEditor();

    OutRecord.Fields.Add(
        TEXT("projectile.class"),
        ClassPath(Spell.GetProjectileClassForEditor().Get()));
    OutRecord.Fields.Add(
        TEXT("projectile.castMontage"),
        ObjectPath(CastMontage));
    OutRecord.Fields.Add(
        TEXT("projectile.socketTag"),
        SocketTag.ToString());
    OutRecord.Fields.Add(
        TEXT("projectile.montageEventPresent"),
        BoolString(
            PDSPantheliaMontageEventInspector::ContainsGameplayTagEvent(
                CastMontage,
                SocketTag)));

    if (const UPantheliaMultiProjectileSpell* Multi =
            Cast<UPantheliaMultiProjectileSpell>(&Spell))
    {
        OutRecord.Fields.Add(
            TEXT("projectile.count"),
            ScalableFloatString(
                Multi->GetProjectileCountForEditor()));
        OutRecord.Fields.Add(
            TEXT("projectile.maxCount"),
            LexToString(Multi->GetMaxProjectileCountForEditor()));
        OutRecord.Fields.Add(
            TEXT("projectile.spread"),
            ScalableFloatString(
                Multi->GetProjectileSpreadForEditor()));
        OutRecord.Fields.Add(
            TEXT("projectile.spawnInterval"),
            ScalableFloatString(
                Multi->GetProjectileSpawnIntervalForEditor()));
        OutRecord.Fields.Add(
            TEXT("projectile.launchPitch"),
            ScalableFloatString(
                Multi->GetLaunchPitchForEditor()));
        OutRecord.Fields.Add(
            TEXT("projectile.speedOverride"),
            ScalableFloatString(
                Multi->GetProjectileSpeedOverrideForEditor()));
        OutRecord.Fields.Add(
            TEXT("projectile.softHoming.enabled"),
            BoolString(Multi->IsSoftHomingEnabledForEditor()));
        OutRecord.Fields.Add(
            TEXT("projectile.softHoming.startDelay"),
            ScalableFloatString(
                Multi->GetHomingStartDelayForEditor()));
        OutRecord.Fields.Add(
            TEXT("projectile.softHoming.duration"),
            ScalableFloatString(
                Multi->GetHomingDurationForEditor()));
        OutRecord.Fields.Add(
            TEXT("projectile.softHoming.acceleration"),
            ScalableFloatString(
                Multi->GetHomingAccelerationForEditor()));
        OutRecord.Fields.Add(
            TEXT("projectile.softHoming.maxCorrectionAngle"),
            ScalableFloatString(
                Multi->GetMaxHomingCorrectionAngleForEditor()));
    }

    OutRecord.Normalize();
    return !OutRecord.RecordId.IsEmpty();
}
