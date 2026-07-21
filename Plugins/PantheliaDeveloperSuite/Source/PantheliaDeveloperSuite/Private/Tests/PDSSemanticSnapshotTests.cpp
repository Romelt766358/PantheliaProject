#if WITH_DEV_AUTOMATION_TESTS

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "PDSSemanticDiffTypes.h"
#include "PDSSemanticSnapshotJson.h"
#include "PDSSemanticSnapshotTypes.h"

namespace
{
    class FTestSemanticContributor final : public IPDSSnapshotDomainContributor
    {
    public:
        FTestSemanticContributor(FString InDomainId, FString InRecordId)
            : DomainId(MoveTemp(InDomainId))
            , RecordId(MoveTemp(InRecordId))
        {
        }

        virtual FString GetDomainId() const override
        {
            return DomainId;
        }

        virtual FString GetSchemaVersion() const override
        {
            return TEXT("1.0.0");
        }

        virtual void GatherDomainSnapshot(
            FPDSSemanticDomainSnapshot& OutSnapshot) const override
        {
            FPDSSemanticRecord Record;
            Record.RecordId = RecordId;
            Record.DisplayName = RecordId;
            Record.Fields.Add(TEXT("value"), RecordId);
            OutSnapshot.Records.Add(MoveTemp(Record));
        }

    private:
        FString DomainId;
        FString RecordId;
    };

    FPDSSemanticDomainSnapshot MakeDomain(
        const FString& DomainId,
        const FString& RecordId,
        const FString& FieldValue)
    {
        FPDSSemanticDomainSnapshot Domain;
        Domain.DomainId = DomainId;
        Domain.SchemaVersion = TEXT("1.0.0");

        FPDSSemanticRecord Record;
        Record.RecordId = RecordId;
        Record.DisplayName = FPaths::GetBaseFilename(RecordId);
        Record.Kind = TEXT("Test");
        Record.SourceAssetPath = RecordId;
        Record.Fields.Add(TEXT("value"), FieldValue);
        Domain.Records.Add(MoveTemp(Record));
        Domain.Normalize();
        return Domain;
    }

    FString DiffSignature(const FPDSSemanticDiff& Diff)
    {
        FString Result;
        for (const FPDSSemanticRecordChange& Change : Diff.AddedRecords)
        {
            Result += TEXT("A|") + Change.DomainId + TEXT("|") + Change.RecordId + TEXT("\n");
        }
        for (const FPDSSemanticRecordChange& Change : Diff.RemovedRecords)
        {
            Result += TEXT("R|") + Change.DomainId + TEXT("|") + Change.RecordId + TEXT("\n");
        }
        for (const FPDSSemanticFieldChange& Change : Diff.ChangedFields)
        {
            Result += TEXT("F|") + Change.DomainId + TEXT("|") + Change.RecordId
                + TEXT("|") + Change.FieldName + TEXT("|") + Change.PreviousValue
                + TEXT("|") + Change.CurrentValue + TEXT("\n");
        }
        return Result;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSSemanticNormalizeTrimTest,
    "Panthelia.DeveloperSuite.Semantic.Normalize.TrimmedFieldPreservesValue",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSSemanticNormalizeTrimTest::RunTest(const FString& Parameters)
{
    FPDSSemanticRecord Record;
    Record.RecordId = TEXT("  /Game/Test/GA_Test.GA_Test_C  ");
    Record.Fields.Add(TEXT(" projectile.count "), TEXT(" 3 "));
    Record.Normalize();

    TestEqual(TEXT("RecordId recortado."), Record.RecordId, FString(TEXT("/Game/Test/GA_Test.GA_Test_C")));
    TestTrue(TEXT("Clave recortada presente."), Record.Fields.Contains(TEXT("projectile.count")));
    TestEqual(TEXT("Valor preservado."), Record.Fields.FindRef(TEXT("projectile.count")), FString(TEXT("3")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSSemanticRegistryOrderingTest,
    "Panthelia.DeveloperSuite.Semantic.Registry.Ordering",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSSemanticRegistryOrderingTest::RunTest(const FString& Parameters)
{
    FPDSSnapshotDomainRegistry& Registry = FPDSSnapshotDomainRegistry::Get();
    const FString DomainA = TEXT("zz_test_semantic_a");
    const FString DomainB = TEXT("zz_test_semantic_b");
    Registry.UnregisterContributor(DomainA);
    Registry.UnregisterContributor(DomainB);

    const TSharedRef<FTestSemanticContributor> ContributorB =
        MakeShared<FTestSemanticContributor>(DomainB, TEXT("B"));
    const TSharedRef<FTestSemanticContributor> ContributorA =
        MakeShared<FTestSemanticContributor>(DomainA, TEXT("A"));

    TestTrue(TEXT("Registro B."), Registry.RegisterContributor(ContributorB));
    TestTrue(TEXT("Registro A."), Registry.RegisterContributor(ContributorA));

    TArray<FPDSIssue> Issues;
    const TArray<FPDSSemanticDomainSnapshot> Domains = Registry.GatherAllDomains(Issues);
    int32 IndexA = INDEX_NONE;
    int32 IndexB = INDEX_NONE;
    for (int32 Index = 0; Index < Domains.Num(); ++Index)
    {
        if (Domains[Index].DomainId == DomainA)
        {
            IndexA = Index;
        }
        if (Domains[Index].DomainId == DomainB)
        {
            IndexB = Index;
        }
    }

    TestTrue(TEXT("Ambos dominios encontrados."), IndexA != INDEX_NONE && IndexB != INDEX_NONE);
    TestTrue(TEXT("Orden determinista por DomainId."), IndexA < IndexB);

    Registry.UnregisterContributor(DomainA);
    Registry.UnregisterContributor(DomainB);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSSemanticRegistryDuplicateTest,
    "Panthelia.DeveloperSuite.Semantic.Registry.Duplicate",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSSemanticRegistryDuplicateTest::RunTest(const FString& Parameters)
{
    FPDSSnapshotDomainRegistry& Registry = FPDSSnapshotDomainRegistry::Get();
    const FString DomainId = TEXT("zz_test_semantic_duplicate");
    Registry.UnregisterContributor(DomainId);

    const TSharedRef<FTestSemanticContributor> First =
        MakeShared<FTestSemanticContributor>(DomainId, TEXT("First"));
    const TSharedRef<FTestSemanticContributor> Second =
        MakeShared<FTestSemanticContributor>(DomainId, TEXT("Second"));

    TestTrue(TEXT("Primer registro aceptado."), Registry.RegisterContributor(First));
    FString Error;
    TestFalse(TEXT("Duplicado rechazado."), Registry.RegisterContributor(Second, &Error));
    TestFalse(TEXT("Mensaje de error no vacío."), Error.IsEmpty());

    Registry.UnregisterContributor(DomainId);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSSemanticRegistryUnregisterTest,
    "Panthelia.DeveloperSuite.Semantic.Registry.Unregister",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSSemanticRegistryUnregisterTest::RunTest(const FString& Parameters)
{
    FPDSSnapshotDomainRegistry& Registry = FPDSSnapshotDomainRegistry::Get();
    const FString DomainId = TEXT("zz_test_semantic_unregister");
    Registry.UnregisterContributor(DomainId);

    const TSharedRef<FTestSemanticContributor> Contributor =
        MakeShared<FTestSemanticContributor>(DomainId, TEXT("Record"));
    TestTrue(TEXT("Registro aceptado."), Registry.RegisterContributor(Contributor));
    TestTrue(TEXT("Unregister devuelve true."), Registry.UnregisterContributor(DomainId));
    TestFalse(TEXT("Segundo unregister devuelve false."), Registry.UnregisterContributor(DomainId));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSSemanticJsonRoundTripTest,
    "Panthelia.DeveloperSuite.Semantic.Json.RoundTrip",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSSemanticJsonRoundTripTest::RunTest(const FString& Parameters)
{
    TArray<FPDSSemanticDomainSnapshot> Domains;
    Domains.Add(MakeDomain(TEXT("spells"), TEXT("/Game/Test/GA_Test.GA_Test_C"), TEXT("3")));

    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    Root->SetObjectField(TEXT("semanticDomains"), PDSSemanticSnapshotJson::BuildDomainsJson(Domains));

    TMap<FString, FPDSSemanticDomainSnapshot> Parsed;
    TArray<FPDSIssue> Issues;
    TestTrue(TEXT("Parse correcto."), PDSSemanticSnapshotJson::ParseDomainsJson(Root, Parsed, Issues));
    TestEqual(TEXT("Un dominio."), Parsed.Num(), 1);
    TestEqual(TEXT("Un record."), Parsed.FindChecked(TEXT("spells")).Records.Num(), 1);
    TestEqual(TEXT("Valor round-trip."), Parsed.FindChecked(TEXT("spells")).Records[0].Fields.FindRef(TEXT("value")), FString(TEXT("3")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSSemanticHistoricalMissingDomainTest,
    "Panthelia.DeveloperSuite.Semantic.Diff.HistoricalMissingDomain",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSSemanticHistoricalMissingDomainTest::RunTest(const FString& Parameters)
{
    TMap<FString, FPDSSemanticDomainSnapshot> Previous;
    TMap<FString, FPDSSemanticDomainSnapshot> Current;
    Current.Add(TEXT("spells"), MakeDomain(TEXT("spells"), TEXT("/Game/Test/GA_Test.GA_Test_C"), TEXT("3")));

    const FPDSSemanticDiff Diff = PDSSemanticDiff::Compare(Previous, Current);
    TestFalse(TEXT("Ausencia histórica no es cambio."), Diff.HasChanges());
    TestEqual(TEXT("Sin altas masivas."), Diff.AddedRecords.Num(), 0);
    TestEqual(TEXT("Un dominio no comparable."), Diff.NonComparableDomains.Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSSemanticAddRemoveTest,
    "Panthelia.DeveloperSuite.Semantic.Diff.AddRemove",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSSemanticAddRemoveTest::RunTest(const FString& Parameters)
{
    FPDSSemanticDomainSnapshot Previous = MakeDomain(TEXT("spells"), TEXT("/Game/Old.GA_Old_C"), TEXT("3"));
    FPDSSemanticDomainSnapshot Current = MakeDomain(TEXT("spells"), TEXT("/Game/New.GA_New_C"), TEXT("3"));
    TMap<FString, FPDSSemanticDomainSnapshot> PreviousMap;
    TMap<FString, FPDSSemanticDomainSnapshot> CurrentMap;
    PreviousMap.Add(TEXT("spells"), Previous);
    CurrentMap.Add(TEXT("spells"), Current);

    const FPDSSemanticDiff Diff = PDSSemanticDiff::Compare(PreviousMap, CurrentMap);
    TestTrue(TEXT("Rename/move se expresa como cambios."), Diff.HasChanges());
    TestEqual(TEXT("Una alta."), Diff.AddedRecords.Num(), 1);
    TestEqual(TEXT("Una baja."), Diff.RemovedRecords.Num(), 1);
    TestEqual(TEXT("Sin field changes."), Diff.ChangedFields.Num(), 0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSSemanticFieldChangeTest,
    "Panthelia.DeveloperSuite.Semantic.Diff.FieldChange",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSSemanticFieldChangeTest::RunTest(const FString& Parameters)
{
    FPDSSemanticDomainSnapshot Previous = MakeDomain(TEXT("spells"), TEXT("/Game/Test/GA_Test.GA_Test_C"), TEXT("3"));
    FPDSSemanticDomainSnapshot Current = MakeDomain(TEXT("spells"), TEXT("/Game/Test/GA_Test.GA_Test_C"), TEXT("5"));
    TMap<FString, FPDSSemanticDomainSnapshot> PreviousMap;
    TMap<FString, FPDSSemanticDomainSnapshot> CurrentMap;
    PreviousMap.Add(TEXT("spells"), Previous);
    CurrentMap.Add(TEXT("spells"), Current);

    const FPDSSemanticDiff Diff = PDSSemanticDiff::Compare(PreviousMap, CurrentMap);
    TestTrue(TEXT("El field activa cambios."), Diff.HasChanges());
    TestEqual(TEXT("Un field modificado."), Diff.ChangedFields.Num(), 1);
    TestEqual(TEXT("Valor anterior."), Diff.ChangedFields[0].PreviousValue, FString(TEXT("3")));
    TestEqual(TEXT("Valor actual."), Diff.ChangedFields[0].CurrentValue, FString(TEXT("5")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSSemanticDeterministicTest,
    "Panthelia.DeveloperSuite.Semantic.Diff.Deterministic",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSSemanticDeterministicTest::RunTest(const FString& Parameters)
{
    FPDSSemanticDomainSnapshot Previous;
    Previous.DomainId = TEXT("spells");
    Previous.SchemaVersion = TEXT("1.0.0");
    Previous.Records.Add(MakeDomain(TEXT("x"), TEXT("B"), TEXT("1")).Records[0]);
    Previous.Records.Add(MakeDomain(TEXT("x"), TEXT("A"), TEXT("1")).Records[0]);
    Previous.Normalize();

    FPDSSemanticDomainSnapshot Current = Previous;
    Current.Records[0].Fields[TEXT("value")] = TEXT("2");
    Current.Records[1].Fields[TEXT("value")] = TEXT("3");
    Current.Normalize();

    TMap<FString, FPDSSemanticDomainSnapshot> PreviousMap;
    TMap<FString, FPDSSemanticDomainSnapshot> CurrentMap;
    PreviousMap.Add(TEXT("spells"), Previous);
    CurrentMap.Add(TEXT("spells"), Current);

    const FString First = DiffSignature(PDSSemanticDiff::Compare(PreviousMap, CurrentMap));
    const FString Second = DiffSignature(PDSSemanticDiff::Compare(PreviousMap, CurrentMap));
    TestEqual(TEXT("Firma estable."), First, Second);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSSemanticNoChangesTest,
    "Panthelia.DeveloperSuite.Semantic.Diff.NoChanges",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSSemanticNoChangesTest::RunTest(const FString& Parameters)
{
    const FPDSSemanticDomainSnapshot Domain = MakeDomain(TEXT("spells"), TEXT("/Game/Test/GA_Test.GA_Test_C"), TEXT("3"));
    TMap<FString, FPDSSemanticDomainSnapshot> Previous;
    TMap<FString, FPDSSemanticDomainSnapshot> Current;
    Previous.Add(TEXT("spells"), Domain);
    Current.Add(TEXT("spells"), Domain);
    TestFalse(TEXT("Snapshots iguales no cambian."), PDSSemanticDiff::Compare(Previous, Current).HasChanges());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSSemanticDuplicateRecordIdTest,
    "Panthelia.DeveloperSuite.Semantic.Json.DuplicateRecordId",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSSemanticDuplicateRecordIdTest::RunTest(const FString& Parameters)
{
    FPDSSemanticDomainSnapshot Domain = MakeDomain(TEXT("spells"), TEXT("Duplicate"), TEXT("1"));
    const FPDSSemanticRecord DuplicateRecord = Domain.Records[0];
    Domain.Records.Add(DuplicateRecord);

    const TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
    TArray<FPDSSemanticDomainSnapshot> Domains;
    Domains.Add(Domain);
    Root->SetObjectField(TEXT("semanticDomains"), PDSSemanticSnapshotJson::BuildDomainsJson(Domains));

    TMap<FString, FPDSSemanticDomainSnapshot> Parsed;
    TArray<FPDSIssue> Issues;
    TestFalse(TEXT("Duplicado invalida parse semántico."), PDSSemanticSnapshotJson::ParseDomainsJson(Root, Parsed, Issues));
    TestEqual(TEXT("Solo se conserva un record."), Parsed.FindChecked(TEXT("spells")).Records.Num(), 1);
    TestTrue(TEXT("Issue de duplicado emitido."), Issues.ContainsByPredicate([](const FPDSIssue& Issue)
    {
        return Issue.RuleId == TEXT("PDS.Semantic.DuplicateRecordId");
    }));
    return true;
}

#endif
