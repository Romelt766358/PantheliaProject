#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystem/Abilities/PantheliaMultiProjectileSpell.h"
#include "AbilitySystem/Abilities/PantheliaProjectileSpell.h"
#include "AbilitySystem/Abilities/PantheliaSpellValidation.h"
#include "Animation/AnimMontage.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Misc/AutomationTest.h"
#include "Misc/DataValidation.h"
#include "PDSPantheliaMontageEventInspector.h"
#include "PDSPantheliaSpellSnapshotContributor.h"
#include "PDSSemanticSnapshotJson.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Class.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectGlobals.h"

namespace
{
    bool SetGameplayTagProperty(
        UObject* Object,
        const FName PropertyName,
        const FGameplayTag& Value)
    {
        if (!IsValid(Object))
        {
            return false;
        }

        FStructProperty* Property =
            FindFProperty<FStructProperty>(Object->GetClass(), PropertyName);
        if (!Property || Property->Struct != FGameplayTag::StaticStruct())
        {
            return false;
        }

        FGameplayTag* TagValue =
            Property->ContainerPtrToValuePtr<FGameplayTag>(Object);
        if (!TagValue)
        {
            return false;
        }

        *TagValue = Value;
        return true;
    }

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

    int32 CountValidationErrors(
        const FDataValidationContext& Context,
        const FString& ExpectedMessage)
    {
        int32 Count = 0;
        for (const FDataValidationContext::FIssue& Issue : Context.GetIssues())
        {
            if (Issue.Severity == EMessageSeverity::Error
                && Issue.Message.ToString() == ExpectedMessage)
            {
                ++Count;
            }
        }
        return Count;
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
        TEXT("class.parentPath"),
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
        TestFalse(
            *FString::Printf(TEXT("%s no debe usar un RecordId de CDO."), *Record.RecordId),
            Record.RecordId.Contains(TEXT("Default__")));
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

    const FString Serialized = SerializeDomain(Domain);
    TSharedPtr<FJsonObject> RootObject;
    const TSharedRef<TJsonReader<>> Reader =
        TJsonReaderFactory<>::Create(Serialized);
    const bool bJsonValid =
        !Serialized.IsEmpty()
        && FJsonSerializer::Deserialize(Reader, RootObject)
        && RootObject.IsValid();
    TestTrue(TEXT("La serializacion produce JSON valido."), bJsonValid);
    if (!bJsonValid)
    {
        return false;
    }

    TMap<FString, FPDSSemanticDomainSnapshot> ParsedDomains;
    TArray<FPDSIssue> ParseIssues;
    const bool bSemanticDomainValid =
        PDSSemanticSnapshotJson::ParseDomainsJson(
            RootObject,
            ParsedDomains,
            ParseIssues);
    TestTrue(
        TEXT("El JSON contiene un dominio semantico valido."),
        bSemanticDomainValid);
    if (!bSemanticDomainValid)
    {
        return false;
    }

    const FPDSSemanticDomainSnapshot* ParsedDomain =
        ParsedDomains.Find(Domain.DomainId);
    TestTrue(
        TEXT("El JSON contiene el dominio de spells."),
        ParsedDomain != nullptr);
    if (!ParsedDomain)
    {
        return false;
    }

    const bool bHasDomainsObject =
        RootObject->HasTypedField<EJson::Object>(TEXT("semanticDomains"));
    TestTrue(
        TEXT("El JSON contiene semanticDomains como objeto."),
        bHasDomainsObject);
    if (!bHasDomainsObject)
    {
        return false;
    }

    const TSharedPtr<FJsonObject> DomainsObject =
        RootObject->GetObjectField(TEXT("semanticDomains"));
    const TSharedPtr<FJsonObject>* DomainObjectPtr = nullptr;
    const bool bHasDomainObject =
        DomainsObject.IsValid()
        && DomainsObject->TryGetObjectField(
            Domain.DomainId,
            DomainObjectPtr)
        && DomainObjectPtr
        && DomainObjectPtr->IsValid();
    TestTrue(
        TEXT("El JSON contiene el objeto del dominio spells."),
        bHasDomainObject);
    if (!bHasDomainObject)
    {
        return false;
    }

    const TSharedPtr<FJsonObject> DomainObject = *DomainObjectPtr;
    const TArray<TSharedPtr<FJsonValue>>* JsonRecords = nullptr;
    const bool bHasRecordsArray =
        DomainObject->HasTypedField<EJson::Array>(TEXT("records"))
        && DomainObject->TryGetArrayField(TEXT("records"), JsonRecords)
        && JsonRecords;
    TestTrue(
        TEXT("El dominio contiene records como array."),
        bHasRecordsArray);
    if (!bHasRecordsArray)
    {
        return false;
    }

    TestEqual(
        TEXT("El JSON conserva todos los records del dominio."),
        JsonRecords->Num(),
        Domain.Records.Num());

    for (const FPDSSemanticRecord& Record : Domain.Records)
    {
        TestTrue(
            *FString::Printf(TEXT("%s expone spawnEventTag canónico."), *Record.RecordId),
            Record.Fields.Contains(TEXT("projectile.spawnEventTag")));

        TSharedPtr<FJsonObject> JsonRecord;
        for (const TSharedPtr<FJsonValue>& JsonValue : *JsonRecords)
        {
            const TSharedPtr<FJsonObject> Candidate = JsonValue.IsValid()
                ? JsonValue->AsObject()
                : nullptr;
            if (Candidate.IsValid()
                && Candidate->HasTypedField<EJson::String>(TEXT("recordId"))
                && Candidate->GetStringField(TEXT("recordId"))
                    == Record.RecordId)
            {
                JsonRecord = Candidate;
                break;
            }
        }

        TestTrue(
            *FString::Printf(
                TEXT("%s existe como objeto JSON."),
                *Record.RecordId),
            JsonRecord.IsValid());
        if (!JsonRecord.IsValid())
        {
            continue;
        }

        const bool bHasFieldsObject =
            JsonRecord->HasTypedField<EJson::Object>(TEXT("fields"));
        TestTrue(
            *FString::Printf(
                TEXT("%s contiene fields como objeto JSON."),
                *Record.RecordId),
            bHasFieldsObject);
        if (!bHasFieldsObject)
        {
            continue;
        }

        const TSharedPtr<FJsonObject> FieldsObject =
            JsonRecord->GetObjectField(TEXT("fields"));
        const FString SpawnEventField =
            TEXT("projectile.spawnEventTag");
        const bool bSpawnEventIsString =
            FieldsObject.IsValid()
            && FieldsObject->HasTypedField<EJson::String>(SpawnEventField);
        TestTrue(
            *FString::Printf(
                TEXT("%s.spawnEventTag se serializa como string JSON."),
                *Record.RecordId),
            bSpawnEventIsString);
        if (bSpawnEventIsString)
        {
            TestEqual(
                *FString::Printf(
                    TEXT("%s.spawnEventTag conserva su valor exacto."),
                    *Record.RecordId),
                FieldsObject->GetStringField(SpawnEventField),
                Record.Fields.FindRef(SpawnEventField));
        }

        const FString Value = Record.Fields.FindRef(TEXT("projectile.montageEventPresent"));
        TestTrue(
            *FString::Printf(TEXT("%s expone montageEventPresent canónico."), *Record.RecordId),
            Value == TEXT("true") || Value == TEXT("false"));

        const FString SpawnEventTag =
            Record.Fields.FindRef(TEXT("projectile.spawnEventTag"));
        const FString CastMontagePath =
            Record.Fields.FindRef(TEXT("projectile.castMontage"));
        const UAnimMontage* CastMontage = CastMontagePath.IsEmpty()
            ? nullptr
            : LoadObject<UAnimMontage>(nullptr, *CastMontagePath);
        const FGameplayTag RequiredTag = SpawnEventTag.IsEmpty()
            ? FGameplayTag()
            : FGameplayTag::RequestGameplayTag(FName(*SpawnEventTag), false);
        const bool bExpected =
            PDSPantheliaMontageEventInspector::ContainsGameplayTagEvent(
                CastMontage,
                RequiredTag);
        const bool bActual = Value == TEXT("true");
        TestEqual(
            *FString::Printf(
                TEXT("%s deriva montageEventPresent con ProjectileSpawnEventTag."),
                *Record.RecordId),
            bActual,
            bExpected);

        if (SpawnEventTag.IsEmpty())
        {
            TestEqual(
                *FString::Printf(
                    TEXT("%s sin ProjectileSpawnEventTag no reporta un evento de montage."),
                    *Record.RecordId),
                Value,
                FString(TEXT("false")));
        }
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSPantheliaSpellMissingProjectileSpawnEventTagTest,
    "Panthelia.DeveloperSuite.Panthelia.Spell.Validator.ProjectileSpawnEventTag",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSPantheliaSpellMissingProjectileSpawnEventTagTest::RunTest(const FString& Parameters)
{
    const TSet<FString> DirtyBefore = GetDirtyPackageNames();
    const FString FireboltClassPath =
        TEXT("/Game/Blueprints/AbilitySystem/GameplayAbilities/Projectiles/Fire/Firebolt/GA_Firebolt.GA_Firebolt_C");
    UClass* FireboltClass = LoadObject<UClass>(nullptr, *FireboltClassPath);
    TestTrue(TEXT("GA_Firebolt resolves to a valid class."), IsValid(FireboltClass));
    if (!IsValid(FireboltClass))
    {
        return false;
    }
    TestFalse(
        TEXT("GA_Firebolt must remain concrete."),
        FireboltClass->HasAnyClassFlags(CLASS_Abstract));

    const UPantheliaProjectileSpell* SourceCDO =
        Cast<UPantheliaProjectileSpell>(FireboltClass->GetDefaultObject());
    TestTrue(TEXT("GA_Firebolt produce un CDO de ProjectileSpell."), IsValid(SourceCDO));
    if (!IsValid(SourceCDO))
    {
        return false;
    }

    bool bDamageTypesValid = !SourceCDO->DamageTypes.IsEmpty();
    for (const TPair<FGameplayTag, FScalableFloat>& Pair : SourceCDO->DamageTypes)
    {
        bDamageTypesValid = bDamageTypesValid
            && Pair.Key.IsValid()
            && Pair.Value.IsValid();
    }

    const FGameplayTag OriginalSocketTag =
        SourceCDO->GetSocketTagForEditor();
    const FGameplayTag OriginalSpawnEventTag =
        SourceCDO->GetProjectileSpawnEventTagForEditor();
    const UClass* OriginalDamageEffectClass =
        SourceCDO->GetDamageEffectClassForEditor().Get();
    const UClass* OriginalProjectileClass =
        SourceCDO->GetProjectileClassForEditor().Get();
    const UAnimMontage* OriginalCastMontage =
        SourceCDO->GetCastMontageForEditor();
    const int32 OriginalDamageTypesCount = SourceCDO->DamageTypes.Num();
    const bool bSourcePackageWasDirty = SourceCDO->GetOutermost()->IsDirty();

    TestTrue(
        TEXT("GA_Firebolt DamageEffectClass is valid."),
        IsValid(SourceCDO->GetDamageEffectClassForEditor().Get()));
    TestTrue(
        TEXT("GA_Firebolt DamageTypes are valid."),
        bDamageTypesValid);
    TestTrue(
        TEXT("GA_Firebolt ProjectileClass is valid."),
        IsValid(SourceCDO->GetProjectileClassForEditor().Get()));
    TestTrue(
        TEXT("GA_Firebolt CastMontage is valid."),
        IsValid(SourceCDO->GetCastMontageForEditor()));
    TestTrue(
        TEXT("GA_Firebolt SocketTag is valid."),
        OriginalSocketTag.IsValid());
    TestTrue(
        TEXT("GA_Firebolt ProjectileSpawnEventTag is valid."),
        OriginalSpawnEventTag.IsValid());

    UPantheliaProjectileSpell* Spell =
        DuplicateObject<UPantheliaProjectileSpell>(
            SourceCDO,
            GetTransientPackage());
    TestTrue(TEXT("El CDO se duplica en el paquete transient."), IsValid(Spell));
    if (!IsValid(Spell))
    {
        return false;
    }

    FDataValidationContext ValidContext;
    const EDataValidationResult ValidResult =
        PantheliaSpellValidation::ValidateProjectileSpell(*Spell, ValidContext);
    TestEqual(
        TEXT("The copy with a valid ProjectileSpawnEventTag has no errors."),
        ValidResult,
        EDataValidationResult::Valid);
    const uint32 ValidErrorCount = ValidContext.GetNumErrors();

    TestTrue(
        TEXT("La copia transient permite limpiar ProjectileSpawnEventTag."),
        SetGameplayTagProperty(Spell, TEXT("ProjectileSpawnEventTag"), FGameplayTag()));
    TestFalse(
        TEXT("La copia transient queda sin ProjectileSpawnEventTag."),
        Spell->GetProjectileSpawnEventTagForEditor().IsValid());

    FDataValidationContext MissingTagContext;
    const EDataValidationResult MissingTagResult =
        PantheliaSpellValidation::ValidateProjectileSpell(
            *Spell,
            MissingTagContext);
    const FString MissingTagError =
        TEXT("ProjectileSpawnEventTag no es v\u00E1lido.");

    TestEqual(
        TEXT("Un hechizo concreto sin ProjectileSpawnEventTag es inválido."),
        MissingTagResult,
        EDataValidationResult::Invalid);

    TestEqual(
        TEXT("Clearing ProjectileSpawnEventTag adds exactly one error."),
        MissingTagContext.GetNumErrors(),
        ValidErrorCount + 1);
    TestEqual(
        TEXT("El error nuevo identifica ProjectileSpawnEventTag."),
        CountValidationErrors(MissingTagContext, MissingTagError),
        1);

    TestEqual(
        TEXT("El CDO original conserva SocketTag."),
        SourceCDO->GetSocketTagForEditor(),
        OriginalSocketTag);
    TestEqual(
        TEXT("El CDO original conserva ProjectileSpawnEventTag."),
        SourceCDO->GetProjectileSpawnEventTagForEditor(),
        OriginalSpawnEventTag);
    TestTrue(
        TEXT("El CDO original conserva DamageEffectClass."),
        SourceCDO->GetDamageEffectClassForEditor().Get()
            == OriginalDamageEffectClass);
    TestTrue(
        TEXT("El CDO original conserva ProjectileClass."),
        SourceCDO->GetProjectileClassForEditor().Get()
            == OriginalProjectileClass);
    TestTrue(
        TEXT("El CDO original conserva CastMontage."),
        SourceCDO->GetCastMontageForEditor() == OriginalCastMontage);
    TestEqual(
        TEXT("El CDO original conserva DamageTypes."),
        SourceCDO->DamageTypes.Num(),
        OriginalDamageTypesCount);
    TestEqual(
        TEXT("El paquete original conserva su estado dirty."),
        SourceCDO->GetOutermost()->IsDirty(),
        bSourcePackageWasDirty);

    const TSet<FString> DirtyAfter = GetDirtyPackageNames();
    int32 NewlyDirtyCount = 0;
    for (const FString& PackageName : DirtyAfter)
    {
        if (!DirtyBefore.Contains(PackageName))
        {
            ++NewlyDirtyCount;
            TestFalse(
                *FString::Printf(
                    TEXT("The test dirtied package %s."),
                    *PackageName),
                true);
        }
    }
    TestEqual(
        TEXT("La prueba aislada no ensucia paquetes."),
        NewlyDirtyCount,
        0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSPantheliaProjectileSpellTagIndependenceTest,
    "Panthelia.DeveloperSuite.Panthelia.Spell.ProjectileTagIndependence",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSPantheliaProjectileSpellTagIndependenceTest::RunTest(const FString& Parameters)
{
    UPantheliaProjectileSpell* Spell =
        NewObject<UPantheliaProjectileSpell>(GetTransientPackage());
    const FGameplayTag SocketTag = FGameplayTag::RequestGameplayTag(
        FName(TEXT("Montage.Attack.LeftHand")),
        false);
    const FGameplayTag SpawnEventTag = FGameplayTag::RequestGameplayTag(
        FName(TEXT("Event.Montage.SpawnProjectile")),
        false);

    TestTrue(TEXT("El tag de socket de prueba existe."), SocketTag.IsValid());
    TestTrue(TEXT("El tag de evento de prueba existe."), SpawnEventTag.IsValid());
    TestTrue(
        TEXT("SocketTag se puede configurar sin asignar el evento."),
        SetGameplayTagProperty(Spell, TEXT("SocketTag"), SocketTag));
    TestEqual(
        TEXT("Configurar SocketTag no asigna ProjectileSpawnEventTag."),
        Spell->GetProjectileSpawnEventTagForEditor().IsValid(),
        false);

    TestTrue(
        TEXT("ProjectileSpawnEventTag se puede configurar de forma independiente."),
        SetGameplayTagProperty(Spell, TEXT("ProjectileSpawnEventTag"), SpawnEventTag));
    TestEqual(
        TEXT("SocketTag conserva exclusivamente el tag físico."),
        Spell->GetSocketTagForEditor(),
        SocketTag);
    TestEqual(
        TEXT("ProjectileSpawnEventTag conserva exclusivamente el tag del evento."),
        Spell->GetProjectileSpawnEventTagForEditor(),
        SpawnEventTag);
    TestFalse(
        TEXT("Los dos campos permanecen independientes."),
        Spell->GetSocketTagForEditor() ==
            Spell->GetProjectileSpawnEventTagForEditor());
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
