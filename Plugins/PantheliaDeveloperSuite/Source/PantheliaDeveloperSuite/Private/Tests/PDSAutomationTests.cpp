#include "Misc/AutomationTest.h"

#include "PantheliaDeveloperSuiteToolset.h"
#include "PDSAutomationService.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

#if WITH_DEV_AUTOMATION_TESTS

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSAutomationToolsetSchemaTest,
    "Panthelia.DeveloperSuite.Automation.ToolsetSchema",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSAutomationToolsetSchemaTest::RunTest(const FString& Parameters)
{
    const FString Schema = UToolsetRegistry::GetToolsetJsonSchema(
        UPantheliaDeveloperSuiteToolset::StaticClass());

    TestFalse(TEXT("Toolset schema is not empty"), Schema.IsEmpty());
    TestTrue(TEXT("Schema exposes GetStatus"), Schema.Contains(TEXT("GetStatus")));
    TestTrue(TEXT("Schema exposes ListSnapshots"), Schema.Contains(TEXT("ListSnapshots")));
    TestTrue(TEXT("Schema exposes ExportProjectSnapshot"), Schema.Contains(TEXT("ExportProjectSnapshot")));
    TestTrue(TEXT("Schema exposes ValidateProfile"), Schema.Contains(TEXT("ValidateProfile")));
    TestTrue(TEXT("Schema exposes SetLatestSnapshotAsBaseline"), Schema.Contains(TEXT("SetLatestSnapshotAsBaseline")));
    TestTrue(TEXT("Schema exposes baseline comparison"), Schema.Contains(TEXT("CompareLatestSnapshotWithBaseline")));
    TestTrue(TEXT("Schema exposes latest-two comparison"), Schema.Contains(TEXT("CompareLatestTwoSnapshots")));

    const UPantheliaDeveloperSuiteToolset* ToolsetCDO =
        GetDefault<UPantheliaDeveloperSuiteToolset>();
    TestNotNull(TEXT("Toolset CDO exists"), ToolsetCDO);
    if (ToolsetCDO)
    {
        TestEqual(
            TEXT("Toolset version matches Automation API"),
            ToolsetCDO->GetToolsetVersion(),
            FString(TEXT("0.4.0-alpha2")));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSAutomationValidationProfileMappingTest,
    "Panthelia.DeveloperSuite.Automation.ValidationProfileMapping",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSAutomationValidationProfileMappingTest::RunTest(
    const FString& Parameters)
{
    TestEqual(
        TEXT("Panthelia Core maps"),
        static_cast<uint8>(PDSAutomation::ToNativeValidationProfile(
            EPDSAutomationValidationProfile::PantheliaCore)),
        static_cast<uint8>(EPDSValidationProfile::PantheliaCore));
    TestEqual(
        TEXT("Game Content maps"),
        static_cast<uint8>(PDSAutomation::ToNativeValidationProfile(
            EPDSAutomationValidationProfile::GameContent)),
        static_cast<uint8>(EPDSValidationProfile::GameContent));
    TestEqual(
        TEXT("External Content maps"),
        static_cast<uint8>(PDSAutomation::ToNativeValidationProfile(
            EPDSAutomationValidationProfile::ExternalContent)),
        static_cast<uint8>(EPDSValidationProfile::ExternalContent));
    TestEqual(
        TEXT("Entire Project maps"),
        static_cast<uint8>(PDSAutomation::ToNativeValidationProfile(
            EPDSAutomationValidationProfile::EntireProject)),
        static_cast<uint8>(EPDSValidationProfile::EntireProject));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSAutomationDiffResponseBoundsTest,
    "Panthelia.DeveloperSuite.Automation.DiffResponseBounds",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSAutomationDiffResponseBoundsTest::RunTest(
    const FString& Parameters)
{
    FPDSSnapshotDiff Diff;
    Diff.PreviousSnapshotPath = TEXT("previous.json");
    Diff.CurrentSnapshotPath = TEXT("current.json");
    Diff.PreviousAssetCount = 3;
    Diff.CurrentAssetCount = 3;

    for (int32 Index = 0; Index < 3; ++Index)
    {
        FPDSSnapshotAssetRecord Asset;
        Asset.ObjectPath = FString::Printf(TEXT("/Game/A%d.A%d"), Index, Index);
        Asset.ClassPath = TEXT("/Script/Engine.DataAsset");
        Asset.Origin = EPDSAssetOrigin::PantheliaCore;
        Diff.AddedAssets.Add(MoveTemp(Asset));
    }

    Diff.AddedGameplayTags = {
        TEXT("Test.One"),
        TEXT("Test.Two"),
        TEXT("Test.Three")
    };
    Diff.AddedMontages = {
        TEXT("/Game/M1.M1"),
        TEXT("/Game/M2.M2"),
        TEXT("/Game/M3.M3")
    };

    FPDSOperationResult PersistResult;
    PersistResult.bSuccess = true;
    PersistResult.Summary = TEXT("Synthetic diff");

    const FPDSAutomationDiffResult Result =
        PDSAutomation::BuildDiffResult(
            Diff,
            PersistResult,
            TEXT("Bounds Test"),
            2,
            2);

    TestEqual(TEXT("Asset changes are bounded"), Result.AssetChanges.Num(), 2);
    TestTrue(TEXT("Asset truncation is reported"), Result.bAssetChangesTruncated);
    TestEqual(TEXT("Tags are bounded"), Result.AddedGameplayTags.Num(), 2);
    TestEqual(TEXT("Montages are bounded"), Result.AddedMontages.Num(), 2);
    TestTrue(TEXT("Any truncation is reported"), Result.bEntriesTruncated);
    return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSAutomationValidationCompletionSemanticsTest,
    "Panthelia.DeveloperSuite.Automation.ValidationCompletionSemantics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSAutomationValidationCompletionSemanticsTest::RunTest(
    const FString& Parameters)
{
    FPDSValidationSummary CompletedWithErrors;
    CompletedWithErrors.ScopeId = TEXT("external-content");
    CompletedWithErrors.ScopeLabel = TEXT("External Content");
    CompletedWithErrors.NumRequested = 1;
    CompletedWithErrors.NumChecked = 1;
    CompletedWithErrors.NumInvalid = 1;

    FPDSIssue ValidationError;
    ValidationError.Severity = EPDSIssueSeverity::Error;
    ValidationError.RuleId = TEXT("PDS.Test.Invalid");
    ValidationError.AssetPath = TEXT("/Game/Test.Test");
    ValidationError.Message = TEXT("Synthetic blocking validation error.");
    CompletedWithErrors.Issues.Add(MoveTemp(ValidationError));

    const FPDSAutomationValidationResult CompletedResult =
        PDSAutomation::ConvertValidationSummary(CompletedWithErrors, 10);

    TestTrue(
        TEXT("Validation execution completed despite findings"),
        CompletedResult.bValidationCompleted);
    TestFalse(
        TEXT("Blocking findings keep bSuccess false"),
        CompletedResult.bSuccess);
    TestFalse(TEXT("Completed validation was not cancelled"), CompletedResult.bCancelled);

    FPDSValidationSummary CancelledSummary;
    CancelledSummary.bCancelled = true;
    const FPDSAutomationValidationResult CancelledResult =
        PDSAutomation::ConvertValidationSummary(CancelledSummary, 10);

    TestFalse(
        TEXT("Cancelled validation is not completed"),
        CancelledResult.bValidationCompleted);
    TestFalse(
        TEXT("Cancelled validation is not successful"),
        CancelledResult.bSuccess);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSAutomationStatusContractTest,
    "Panthelia.DeveloperSuite.Automation.StatusContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSAutomationStatusContractTest::RunTest(const FString& Parameters)
{
    const FPDSAutomationService Service;
    const FPDSAutomationStatusResult Result = Service.GetStatus();

    TestTrue(TEXT("GetStatus succeeds as a lightweight availability probe"), Result.bSuccess);
    TestEqual(
        TEXT("Automation API version"),
        Result.AutomationApiVersion,
        FString(TEXT("0.4.0-alpha2")));
    TestFalse(
        TEXT("GetStatus does not claim snapshot validity was inspected"),
        Result.bSnapshotValidityKnown);
    TestEqual(TEXT("Valid count is unknown"), Result.ValidSnapshotCount, -1);
    TestEqual(TEXT("Invalid count is unknown"), Result.InvalidSnapshotCount, -1);
    return true;
}

#endif
