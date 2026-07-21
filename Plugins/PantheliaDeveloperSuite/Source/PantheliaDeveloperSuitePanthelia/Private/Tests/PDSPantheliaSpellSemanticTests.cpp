#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/Abilities/PantheliaMultiProjectileSpell.h"
#include "AbilitySystem/Abilities/PantheliaProjectileSpell.h"
#include "AbilitySystem/Abilities/PantheliaSpellValidation.h"
#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "PDSPantheliaSpellSnapshotContributor.h"
#include "PDSSemanticSnapshotJson.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UObjectIterator.h"

namespace
{
    FString SerializeDomain(const FPDSSemanticDomainSnapshot& Domain)
    {
        TArray<FPDSSemanticDomainSnapshot> Domains;
        Domains.Add(Domain);

        const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetObjectField(
            TEXT("semanticDomains"),
            PDSSemanticSnapshotJson::BuildDomainsJson(Domains));

        FString Serialized;
        const TSharedRef<TJsonWriter<>> Writer =
            TJsonWriterFactory<>::Create(&Serialized);
        return FJsonSerializer::Serialize(Root, Writer)
            ? Serialized
            : FString();
    }

    TSet<FString> GetDirtyPackageNames()
    {
        TSet<FString> Result;
        for (TObjectIterator<UPackage> It; It; ++It)
        {
            if (It->IsDirty())
            {
                Result.Add(It->GetName());
            }
        }
        return Result;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSPantheliaSpellCandidateFilterTest,
    "Panthelia.DeveloperSuite.Panthelia.Spell.CandidateFilter",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSPantheliaSpellCandidateFilterTest::RunTest(const FString& Parameters)
{
    const FPDSPantheliaSpellSnapshotContributor Contributor;
    FPDSSemanticDomainSnapshot Domain;
    Contributor.GatherDomainSnapshot(Domain);
    Domain.Normalize();

    TestTrue(TEXT("El proyecto contiene al menos un ProjectileSpell Blueprint."), Domain.Records.Num() > 0);

    TSet<FString> RecordIds;
    for (const FPDSSemanticRecord& Record : Domain.Records)
    {
        RecordIds.Add(Record.RecordId);

        UClass* SpellClass = LoadObject<UClass>(nullptr, *Record.RecordId);
        TestTrue(
            *FString::Printf(TEXT("%s resuelve a clase válida."), *Record.RecordId),
            IsValid(SpellClass));
        if (IsValid(SpellClass))
        {
            TestTrue(
                *FString::Printf(TEXT("%s hereda de UPantheliaProjectileSpell."), *Record.RecordId),
                SpellClass->IsChildOf(UPantheliaProjectileSpell::StaticClass()));
            TestFalse(
                *FString::Printf(TEXT("%s no debe ser abstracta."), *Record.RecordId),
                SpellClass->HasAnyClassFlags(CLASS_Abstract));
        }
    }

    const TArray<FString> AbstractTemplatePaths = {
        TEXT("/Game/Blueprints/AbilitySystem/GameplayAbilities/Enemies/GA_RangedAttack.GA_RangedAttack_C"),
        TEXT("/Game/Blueprints/AbilitySystem/GameplayAbilities/Projectiles/MultiProjectiles/GA_MultiProjectile_Base.GA_MultiProjectile_Base_C")
    };

    for (const FString& TemplatePath : AbstractTemplatePaths)
    {
        UClass* TemplateClass = LoadObject<UClass>(nullptr, *TemplatePath);
        TestTrue(
            *FString::Printf(TEXT("%s resuelve a clase plantilla válida."), *TemplatePath),
            IsValid(TemplateClass));
        if (IsValid(TemplateClass))
        {
            TestTrue(
                *FString::Printf(TEXT("%s debe declarar CLASS_Abstract."), *TemplatePath),
                TemplateClass->HasAnyClassFlags(CLASS_Abstract));
        }
        TestFalse(
            *FString::Printf(TEXT("%s no debe aparecer en el catálogo semántico."), *TemplatePath),
            RecordIds.Contains(TemplatePath));
    }

    const TArray<FString> ConcreteSpellPaths = {
        TEXT("/Game/Blueprints/AbilitySystem/GameplayAbilities/Projectiles/Fire/Firebolt/GA_Firebolt.GA_Firebolt_C"),
        TEXT("/Game/Blueprints/AbilitySystem/GameplayAbilities/Enemies/GA_RangedAttack_Shaman.GA_RangedAttack_Shaman_C"),
        TEXT("/Game/Blueprints/AbilitySystem/GameplayAbilities/Enemies/GA_RangedAttack_TestBoss.GA_RangedAttack_TestBoss_C")
    };

    for (const FString& SpellPath : ConcreteSpellPaths)
    {
        UClass* SpellClass = LoadObject<UClass>(nullptr, *SpellPath);
        TestTrue(
            *FString::Printf(TEXT("%s resuelve a clase concreta válida."), *SpellPath),
            IsValid(SpellClass));
        if (IsValid(SpellClass))
        {
            TestFalse(
                *FString::Printf(TEXT("%s debe permanecer concreta."), *SpellPath),
                SpellClass->HasAnyClassFlags(CLASS_Abstract));
        }
        TestTrue(
            *FString::Printf(TEXT("%s debe permanecer en el catálogo semántico."), *SpellPath),
            RecordIds.Contains(SpellPath));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSPantheliaSpellCatalogStableTest,
    "Panthelia.DeveloperSuite.Panthelia.Spell.CatalogStable",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSPantheliaSpellCatalogStableTest::RunTest(const FString& Parameters)
{
    const FPDSPantheliaSpellSnapshotContributor Contributor;
    FPDSSemanticDomainSnapshot First;
    FPDSSemanticDomainSnapshot Second;
    Contributor.GatherDomainSnapshot(First);
    Contributor.GatherDomainSnapshot(Second);
    First.Normalize();
    Second.Normalize();

    const FString FirstJson = SerializeDomain(First);
    const FString SecondJson = SerializeDomain(Second);
    TestFalse(TEXT("La serialización no debe quedar vacía."), FirstJson.IsEmpty());
    TestEqual(TEXT("Dos gathers consecutivos deben ser idénticos."), FirstJson, SecondJson);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSPantheliaSpellClassPathContractTest,
    "Panthelia.DeveloperSuite.Panthelia.Spell.ClassPathContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSPantheliaSpellClassPathContractTest::RunTest(const FString& Parameters)
{
    const FPDSPantheliaSpellSnapshotContributor Contributor;
    FPDSSemanticDomainSnapshot Domain;
    Contributor.GatherDomainSnapshot(Domain);
    Domain.Normalize();

    const TArray<FString> ClassFields = {
        TEXT("cost.effect"),
        TEXT("cooldown.effect"),
        TEXT("damage.effect"),
        TEXT("projectile.class")
    };

    for (const FPDSSemanticRecord& Record : Domain.Records)
    {
        for (const FString& FieldName : ClassFields)
        {
            const FString Value = Record.Fields.FindRef(FieldName);
            TestFalse(
                *FString::Printf(TEXT("%s.%s no debe usar path de CDO."), *Record.RecordId, *FieldName),
                Value.Contains(TEXT("Default__")));
        }
        TestFalse(TEXT("class.path fue eliminado por redundancia."), Record.Fields.Contains(TEXT("class.path")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSPantheliaSpellDerivedFieldTest,
    "Panthelia.DeveloperSuite.Panthelia.Spell.DerivedMontageField",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSPantheliaSpellDerivedFieldTest::RunTest(const FString& Parameters)
{
    const FPDSPantheliaSpellSnapshotContributor Contributor;
    FPDSSemanticDomainSnapshot Domain;
    Contributor.GatherDomainSnapshot(Domain);
    Domain.Normalize();

    for (const FPDSSemanticRecord& Record : Domain.Records)
    {
        const FString Value = Record.Fields.FindRef(TEXT("projectile.montageEventPresent"));
        TestTrue(
            *FString::Printf(TEXT("%s expone montageEventPresent canónico."), *Record.RecordId),
            Value == TEXT("true") || Value == TEXT("false"));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSPantheliaSpellValidatorDamageTest,
    "Panthelia.DeveloperSuite.Panthelia.Spell.Validator.Damage",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSPantheliaSpellValidatorDamageTest::RunTest(const FString& Parameters)
{
    const UPantheliaProjectileSpell* Spell =
        NewObject<UPantheliaProjectileSpell>(GetTransientPackage());
    FDataValidationContext Context;
    const EDataValidationResult Result =
        PantheliaSpellValidation::ValidateProjectileSpell(*Spell, Context);

    TestEqual(TEXT("Un hechizo vacío es inválido."), Result, EDataValidationResult::Invalid);
    TestTrue(TEXT("Se emitieron errores concretos."), Context.GetNumErrors() > 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSPantheliaSpellValidatorMultiTest,
    "Panthelia.DeveloperSuite.Panthelia.Spell.Validator.MultiProjectileDefaults",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSPantheliaSpellValidatorMultiTest::RunTest(const FString& Parameters)
{
    const UPantheliaMultiProjectileSpell* Spell =
        NewObject<UPantheliaMultiProjectileSpell>(GetTransientPackage());
    FDataValidationContext Context;
    const EDataValidationResult Result =
        PantheliaSpellValidation::ValidateMultiProjectileSpell(*Spell, Context);

    TestEqual(TEXT("Los defaults numéricos de MultiProjectile son válidos."), Result, EDataValidationResult::Valid);
    TestEqual(TEXT("Sin errores numéricos."), Context.GetNumErrors(), static_cast<uint32>(0));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSPantheliaSpellReadOnlyTest,
    "Panthelia.DeveloperSuite.Panthelia.Spell.ReadOnly",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSPantheliaSpellReadOnlyTest::RunTest(const FString& Parameters)
{
    const TSet<FString> DirtyBefore = GetDirtyPackageNames();

    const FPDSPantheliaSpellSnapshotContributor Contributor;
    FPDSSemanticDomainSnapshot Domain;
    Contributor.GatherDomainSnapshot(Domain);
    Domain.Normalize();

    const TSet<FString> DirtyAfter = GetDirtyPackageNames();
    TArray<FString> NewlyDirty;
    for (const FString& PackageName : DirtyAfter)
    {
        if (!DirtyBefore.Contains(PackageName))
        {
            NewlyDirty.Add(PackageName);
        }
    }
    NewlyDirty.Sort();

    for (const FString& PackageName : NewlyDirty)
    {
        AddError(FString::Printf(TEXT("El catálogo ensució el paquete %s."), *PackageName));
    }
    TestEqual(TEXT("El gather no ensucia paquetes."), NewlyDirty.Num(), 0);
    return true;
}

#endif
