#include "Misc/AutomationTest.h"
#include "PDSSnapshotDiffTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    FString MakeSnapshotJson(
        const FString& GeneratedAt,
        const FString& AssetsJson,
        const FString& TagsJson,
        const FString& MontagesJson,
        const int32 InvalidCount,
        const int32 WarningCount)
    {
        return FString::Printf(
            TEXT("{\"schemaVersion\":\"0.3.0-alpha2\",\"generatedAtUtc\":\"%s\","
                 "\"project\":{\"name\":\"PantheliaProject\",\"engineVersion\":\"5.8\"},"
                 "\"assetInventory\":%s,\"gameplayTags\":%s,\"montages\":%s,"
                 "\"validation\":{\"included\":true,\"numValid\":1,\"numInvalid\":%d,\"numWithWarnings\":%d}}"),
            *GeneratedAt,
            *AssetsJson,
            *TagsJson,
            *MontagesJson,
            InvalidCount,
            WarningCount);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSSnapshotDiffAssetChangesTest,
    "Panthelia.DeveloperSuite.SnapshotDiff.AssetChanges",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSSnapshotDiffAssetChangesTest::RunTest(const FString& Parameters)
{
    const FString PreviousJson = MakeSnapshotJson(
        TEXT("2026-07-18T00:00:00Z"),
        TEXT("[{\"objectPath\":\"/Game/A.A\",\"packageName\":\"/Game/A\",\"packagePath\":\"/Game\",\"classPath\":\"/Script/Engine.DataAsset\",\"origin\":\"Panthelia Core\"}]"),
        TEXT("[\"Abilities.Old\"]"),
        TEXT("[]"),
        0,
        1);
    const FString CurrentJson = MakeSnapshotJson(
        TEXT("2026-07-19T00:00:00Z"),
        TEXT("[{\"objectPath\":\"/Game/A.A\",\"packageName\":\"/Game/A\",\"packagePath\":\"/Game\",\"classPath\":\"/Script/Engine.PrimaryDataAsset\",\"origin\":\"Panthelia Core\"},{\"objectPath\":\"/Game/B.B\",\"packageName\":\"/Game/B\",\"packagePath\":\"/Game\",\"classPath\":\"/Script/Engine.DataAsset\",\"origin\":\"Game Content\"}]"),
        TEXT("[\"Abilities.New\"]"),
        TEXT("[]"),
        1,
        0);

    FPDSSnapshotDocument Previous;
    FPDSSnapshotDocument Current;
    TArray<FPDSIssue> Issues;
    TestTrue(TEXT("Previous parses"), PDSSnapshotDiff::ParseSnapshotJson(PreviousJson, TEXT("previous.json"), Previous, Issues));
    TestTrue(TEXT("Current parses"), PDSSnapshotDiff::ParseSnapshotJson(CurrentJson, TEXT("current.json"), Current, Issues));

    const FPDSSnapshotDiff Diff = PDSSnapshotDiff::Compare(Previous, Current);
    TestEqual(TEXT("One asset added"), Diff.AddedAssets.Num(), 1);
    TestEqual(TEXT("One asset changed"), Diff.ChangedAssets.Num(), 1);
    TestEqual(TEXT("No assets removed"), Diff.RemovedAssets.Num(), 0);
    TestEqual(TEXT("One tag added"), Diff.AddedGameplayTags.Num(), 1);
    TestEqual(TEXT("One tag removed"), Diff.RemovedGameplayTags.Num(), 1);
    TestEqual(TEXT("Invalid count updated"), Diff.CurrentInvalidCount, 1);

    const FString DashboardText = Diff.ToDashboardText(10);
    TestTrue(
        TEXT("Dashboard text has real line breaks"),
        DashboardText.Contains(TEXT("\n")));
    TestFalse(
        TEXT("Dashboard text has no escaped newline literals"),
        DashboardText.Contains(TEXT("\\n")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSSnapshotDiffMontageFingerprintTest,
    "Panthelia.DeveloperSuite.SnapshotDiff.MontageFingerprint",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSSnapshotDiffMontageFingerprintTest::RunTest(const FString& Parameters)
{
    const FString PreviousJson = MakeSnapshotJson(
        TEXT("2026-07-18T00:00:00Z"),
        TEXT("[]"),
        TEXT("[]"),
        TEXT("[{\"path\":\"/Game/M.M\",\"playLengthSeconds\":0.9,\"slotTrackCount\":1,\"sectionCount\":1,\"notifyTrackCount\":1,\"sections\":[],\"notifies\":[{\"trackIndex\":0,\"classOrName\":\"Followup\",\"triggerTimeSeconds\":0.5,\"durationSeconds\":0.3}]}]"),
        0,
        0);
    const FString CurrentJson = MakeSnapshotJson(
        TEXT("2026-07-19T00:00:00Z"),
        TEXT("[]"),
        TEXT("[]"),
        TEXT("[{\"path\":\"/Game/M.M\",\"playLengthSeconds\":0.9,\"slotTrackCount\":1,\"sectionCount\":1,\"notifyTrackCount\":1,\"sections\":[],\"notifies\":[{\"trackIndex\":0,\"classOrName\":\"Followup\",\"triggerTimeSeconds\":0.55,\"durationSeconds\":0.3}]}]"),
        0,
        0);

    FPDSSnapshotDocument Previous;
    FPDSSnapshotDocument Current;
    TArray<FPDSIssue> Issues;
    TestTrue(TEXT("Previous parses"), PDSSnapshotDiff::ParseSnapshotJson(PreviousJson, TEXT("previous.json"), Previous, Issues));
    TestTrue(TEXT("Current parses"), PDSSnapshotDiff::ParseSnapshotJson(CurrentJson, TEXT("current.json"), Current, Issues));

    const FPDSSnapshotDiff Diff = PDSSnapshotDiff::Compare(Previous, Current);
    TestEqual(TEXT("Montage change detected"), Diff.ChangedMontages.Num(), 1);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSSnapshotDiffSchemaCompatibilityTest,
    "Panthelia.DeveloperSuite.SnapshotDiff.SchemaCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSSnapshotDiffSchemaCompatibilityTest::RunTest(const FString& Parameters)
{
    const FString PreviousJson =
        TEXT("{\"schemaVersion\":\"0.2.0-alpha3\",\"generatedAtUtc\":\"old\",\"project\":{\"name\":\"PantheliaProject\"},\"assetInventory\":[],\"gameplayTags\":[],\"montages\":[],\"validation\":{\"included\":false}}");
    const FString CurrentJson =
        TEXT("{\"schemaVersion\":\"0.3.0-alpha2\",\"generatedAtUtc\":\"new\",\"project\":{\"name\":\"PantheliaProject\"},\"assetInventory\":[],\"gameplayTags\":[],\"montages\":[],\"validation\":{\"included\":false}}");

    FPDSSnapshotDocument Previous;
    FPDSSnapshotDocument Current;
    TArray<FPDSIssue> Issues;
    TestTrue(TEXT("Old schema parses"), PDSSnapshotDiff::ParseSnapshotJson(PreviousJson, TEXT("old.json"), Previous, Issues));
    TestTrue(TEXT("New schema parses"), PDSSnapshotDiff::ParseSnapshotJson(CurrentJson, TEXT("new.json"), Current, Issues));

    const FPDSSnapshotDiff Diff = PDSSnapshotDiff::Compare(Previous, Current);
    TestEqual(TEXT("Schema change creates one info issue"), Diff.Issues.Num(), 1);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSSnapshotDiffHistoricalOriginCompatibilityTest,
    "Panthelia.DeveloperSuite.SnapshotDiff.HistoricalOriginCompatibility",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSSnapshotDiffHistoricalOriginCompatibilityTest::RunTest(const FString& Parameters)
{
    const FString PreviousJson = MakeSnapshotJson(
        TEXT("2026-07-18T00:00:00Z"),
        TEXT("[{\"objectPath\":\"/Game/A.A\",\"packageName\":\"/Game/A\",\"packagePath\":\"/Game\",\"classPath\":\"/Script/Engine.DataAsset\"}]"),
        TEXT("[]"),
        TEXT("[]"),
        0,
        0);
    const FString CurrentJson = MakeSnapshotJson(
        TEXT("2026-07-19T00:00:00Z"),
        TEXT("[{\"objectPath\":\"/Game/A.A\",\"packageName\":\"/Game/A\",\"packagePath\":\"/Game\",\"classPath\":\"/Script/Engine.DataAsset\",\"origin\":\"Panthelia Core\"}]"),
        TEXT("[]"),
        TEXT("[]"),
        0,
        0);

    FPDSSnapshotDocument Previous;
    FPDSSnapshotDocument Current;
    TArray<FPDSIssue> Issues;
    TestTrue(TEXT("Previous parses without origin"), PDSSnapshotDiff::ParseSnapshotJson(PreviousJson, TEXT("previous.json"), Previous, Issues));
    TestTrue(TEXT("Current parses with origin"), PDSSnapshotDiff::ParseSnapshotJson(CurrentJson, TEXT("current.json"), Current, Issues));

    const FPDSSnapshotDiff Diff = PDSSnapshotDiff::Compare(Previous, Current);
    TestEqual(TEXT("Missing historical origin does not mark asset changed"), Diff.ChangedAssets.Num(), 0);
    TestEqual(TEXT("One origin comparison was skipped"), Diff.NumAssetsWithOriginNotComparable, 1);
    TestEqual(TEXT("One informational issue explains the compatibility behavior"), Diff.Issues.Num(), 1);
    if (Diff.Issues.Num() == 1)
    {
        TestEqual(
            TEXT("Compatibility issue rule id"),
            Diff.Issues[0].RuleId,
            FString(TEXT("PDS.SnapshotDiff.OriginNotComparable")));
        TestEqual(
            TEXT("Compatibility issue severity"),
            static_cast<uint8>(Diff.Issues[0].Severity),
            static_cast<uint8>(EPDSIssueSeverity::Info));
    }
    return true;
}

#endif
