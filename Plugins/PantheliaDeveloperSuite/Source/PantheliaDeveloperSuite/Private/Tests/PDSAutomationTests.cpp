#include "Misc/AutomationTest.h"

#include "EditorValidatorSubsystem.h"
#include "PantheliaDeveloperSuiteToolset.h"
#include "PDSAutomationService.h"
#include "PDSProjectDoctorService.h"
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
    TestTrue(TEXT("Schema exposes duration"), Schema.Contains(TEXT("durationMilliseconds")));
    TestTrue(TEXT("Schema exposes validation state"), Schema.Contains(TEXT("executionState")));
    TestTrue(TEXT("Schema exposes infrastructure failures"), Schema.Contains(TEXT("bInfrastructureFailure")));
    TestTrue(TEXT("Schema exposes structured export"), Schema.Contains(TEXT("timestampedSnapshot")));
    TestTrue(TEXT("Schema exposes previous baseline metadata"), Schema.Contains(TEXT("previousBaseline")));
    TestTrue(TEXT("Schema exposes new baseline metadata"), Schema.Contains(TEXT("newBaseline")));
    TestTrue(
        TEXT("Schema exposes the explicit timestamped snapshot path"),
        Schema.Contains(TEXT("timestampedSnapshotPath")));
    TestTrue(
        TEXT("Schema exposes the canonical baseline path"),
        Schema.Contains(TEXT("baselinePath")));
    TestTrue(
        TEXT("Schema exposes explicit Markdown report paths"),
        Schema.Contains(TEXT("markdownReportPath")));
    TestTrue(
        TEXT("Schema exposes explicit JSON report paths"),
        Schema.Contains(TEXT("jsonReportPath")));
    TestFalse(
        TEXT("v0.5 schema removes the ambiguous outputPath alias"),
        Schema.Contains(TEXT("\"outputPath\"")));
    TestFalse(
        TEXT("v0.5 schema removes the ambiguous outputJsonPath alias"),
        Schema.Contains(TEXT("\"outputJsonPath\"")));

    const UPantheliaDeveloperSuiteToolset* ToolsetCDO =
        GetDefault<UPantheliaDeveloperSuiteToolset>();
    TestNotNull(TEXT("Toolset CDO exists"), ToolsetCDO);
    if (ToolsetCDO)
    {
        TestEqual(
            TEXT("Toolset version matches Automation API"),
            ToolsetCDO->GetToolsetVersion(),
            FString(TEXT("0.5.0-alpha1")));
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
    FPDSAutomationValidationSettingsContractTest,
    "Panthelia.DeveloperSuite.Automation.ValidationSettingsContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSAutomationValidationSettingsContractTest::RunTest(
    const FString& Parameters)
{
    FValidateAssetsSettings NonInteractiveSettings;
    FPDSProjectDoctorService::ConfigureValidationSettings(
        NonInteractiveSettings,
        true);

    TestEqual(
        TEXT("Non-interactive Project Doctor uses the manual validation usecase"),
        static_cast<uint8>(NonInteractiveSettings.ValidationUsecase),
        static_cast<uint8>(EDataValidationUsecase::Manual));
    TestTrue(
        TEXT("Non-interactive Project Doctor loads assets for validation"),
        NonInteractiveSettings.bLoadAssetsForValidation);
    TestFalse(
        TEXT("Non-interactive Project Doctor does not load external objects separately"),
        NonInteractiveSettings.bLoadExternalObjectsForValidation);
    TestTrue(
        TEXT("Non-interactive Project Doctor collects per-asset details"),
        NonInteractiveSettings.bCollectPerAssetDetails);
    TestTrue(
        TEXT("Non-interactive Project Doctor skips excluded directories"),
        NonInteractiveSettings.bSkipExcludedDirectories);
    TestTrue(
        TEXT("Non-interactive Project Doctor unloads assets loaded for validation"),
        NonInteractiveSettings.bUnloadAssetsLoadedForValidation);
    TestFalse(
        TEXT("Non-interactive Project Doctor does not show results when there are no failures"),
        NonInteractiveSettings.bShowIfNoFailures);
    TestTrue(
        TEXT("Non-interactive Project Doctor disables validation progress dialogs"),
        NonInteractiveSettings.bSilent);
    TestFalse(
        TEXT("Non-interactive Project Doctor prevents automatic Message Log opening"),
        NonInteractiveSettings.ShowMessageLogSeverity.IsSet());

    FValidateAssetsSettings InteractiveSettings;
    const auto DefaultShowMessageLogSeverity =
        InteractiveSettings.ShowMessageLogSeverity;
    FPDSProjectDoctorService::ConfigureValidationSettings(
        InteractiveSettings,
        false);

    TestEqual(
        TEXT("Interactive Project Doctor uses the manual validation usecase"),
        static_cast<uint8>(InteractiveSettings.ValidationUsecase),
        static_cast<uint8>(EDataValidationUsecase::Manual));
    TestTrue(
        TEXT("Interactive Project Doctor loads assets for validation"),
        InteractiveSettings.bLoadAssetsForValidation);
    TestFalse(
        TEXT("Interactive Project Doctor does not load external objects separately"),
        InteractiveSettings.bLoadExternalObjectsForValidation);
    TestTrue(
        TEXT("Interactive Project Doctor collects per-asset details"),
        InteractiveSettings.bCollectPerAssetDetails);
    TestTrue(
        TEXT("Interactive Project Doctor skips excluded directories"),
        InteractiveSettings.bSkipExcludedDirectories);
    TestTrue(
        TEXT("Interactive Project Doctor unloads assets loaded for validation"),
        InteractiveSettings.bUnloadAssetsLoadedForValidation);
    TestFalse(
        TEXT("Interactive Project Doctor does not show results when there are no failures"),
        InteractiveSettings.bShowIfNoFailures);
    TestFalse(
        TEXT("Interactive Project Doctor allows validation progress dialogs"),
        InteractiveSettings.bSilent);
    TestTrue(
        TEXT("Interactive Project Doctor preserves automatic Message Log behavior"),
        InteractiveSettings.ShowMessageLogSeverity.IsSet());
    TestTrue(
        TEXT("Interactive Project Doctor preserves the default Message Log severity"),
        InteractiveSettings.ShowMessageLogSeverity
            == DefaultShowMessageLogSeverity);
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
    CompletedWithErrors.SetExecutionState(
        EPDSValidationExecutionState::Completed);
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
    TestFalse(
        TEXT("Content findings are not infrastructure failures"),
        CompletedResult.bInfrastructureFailure);
    TestEqual(
        TEXT("Completed state is exposed"),
        CompletedResult.ExecutionState,
        FString(TEXT("Completed")));

    FPDSValidationSummary CancelledSummary;
    CancelledSummary.SetExecutionState(
        EPDSValidationExecutionState::Cancelled);
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
        FString(TEXT("0.5.0-alpha1")));
    TestFalse(
        TEXT("GetStatus does not claim snapshot validity was inspected"),
        Result.bSnapshotValidityKnown);
    TestEqual(TEXT("Valid count is unknown"), Result.ValidSnapshotCount, -1);
    TestEqual(TEXT("Invalid count is unknown"), Result.InvalidSnapshotCount, -1);
    TestTrue(
        TEXT("Duration is non-negative"),
        Result.DurationMilliseconds >= 0.0);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSAutomationInfrastructureFailureSemanticsTest,
    "Panthelia.DeveloperSuite.Automation.InfrastructureFailureSemantics",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSAutomationInfrastructureFailureSemanticsTest::RunTest(
    const FString& Parameters)
{
    FPDSValidationSummary Summary;
    Summary.SetExecutionState(
        EPDSValidationExecutionState::InfrastructureFailure);

    FPDSIssue FailureIssue;
    FailureIssue.Severity = EPDSIssueSeverity::Error;
    FailureIssue.RuleId = TEXT("PDS.Validation.EditorUnavailable");
    Summary.Issues.Add(MoveTemp(FailureIssue));

    const FPDSAutomationValidationResult Result =
        PDSAutomation::ConvertValidationSummary(Summary, 10);

    TestFalse(TEXT("Infrastructure failure is not completed"), Result.bValidationCompleted);
    TestFalse(TEXT("Infrastructure failure is not successful"), Result.bSuccess);
    TestFalse(TEXT("Infrastructure failure is not cancellation"), Result.bCancelled);
    TestTrue(TEXT("Infrastructure failure flag is explicit"), Result.bInfrastructureFailure);
    TestEqual(
        TEXT("Infrastructure state is exposed"),
        Result.ExecutionState,
        FString(TEXT("InfrastructureFailure")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSAutomationSnapshotMetadataProjectionTest,
    "Panthelia.DeveloperSuite.Automation.SnapshotMetadataProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSAutomationSnapshotMetadataProjectionTest::RunTest(
    const FString& Parameters)
{
    FPDSSnapshotDocument Document;
    Document.GeneratedAtUtc = TEXT("2026-07-19T18:25:22.487Z");
    Document.SchemaVersion = TEXT("0.3.0-alpha3");
    Document.ProjectName = TEXT("PantheliaProject");
    Document.EngineVersion = TEXT("5.8.0-test");

    FPDSSnapshotAssetRecord Asset;
    Asset.ObjectPath = TEXT("/Game/Test.Test");
    Document.AssetsByObjectPath.Add(Asset.ObjectPath, Asset);
    Document.GameplayTags.Add(TEXT("Test.Tag"));

    FPDSSnapshotMontageRecord Montage;
    Montage.Path = TEXT("/Game/Montage.Montage");
    Document.MontagesByPath.Add(Montage.Path, Montage);

    const FPDSAutomationSnapshotMetadata Result =
        PDSAutomation::BuildSnapshotMetadata(
            Document,
            TEXT("C:/Saved/test.json"));

    TestTrue(TEXT("Metadata is valid"), Result.bValid);
    TestEqual(TEXT("Asset count"), Result.AssetCount, 1);
    TestEqual(TEXT("Gameplay Tag count"), Result.GameplayTagCount, 1);
    TestEqual(TEXT("Montage count"), Result.MontageCount, 1);
    TestEqual(
        TEXT("Path is preserved"),
        Result.FilePath,
        FString(TEXT("C:/Saved/test.json")));
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSAutomationPathContractTest,
    "Panthelia.DeveloperSuite.Automation.PathContractV05",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSAutomationPathContractTest::RunTest(
    const FString& Parameters)
{
    FPDSOperationResult NativeResult;
    NativeResult.bSuccess = true;
    NativeResult.OutputPath = TEXT("C:/Saved/PDS/report.md");
    NativeResult.OutputJsonPath = TEXT("C:/Saved/PDS/report.json");

    const FPDSAutomationOperationResult Result =
        PDSAutomation::ConvertOperationResult(NativeResult, 10);

    TestEqual(
        TEXT("Markdown report path is projected explicitly"),
        Result.MarkdownReportPath,
        NativeResult.OutputPath);
    TestEqual(
        TEXT("JSON report path is projected explicitly"),
        Result.JsonReportPath,
        NativeResult.OutputJsonPath);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSAutomationResponseLimitContractTest,
    "Panthelia.DeveloperSuite.Automation.ResponseLimitContract",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSAutomationResponseLimitContractTest::RunTest(
    const FString& Parameters)
{
    FPDSSnapshotDiff Diff;
    FPDSOperationResult PersistResult;
    PersistResult.bSuccess = true;

    const FPDSAutomationDiffResult ZeroResult =
        PDSAutomation::BuildDiffResult(
            Diff,
            PersistResult,
            TEXT("Zero"),
            0,
            100);
    TestEqual(TEXT("Zero keeps raw request"), ZeroResult.RequestedEntryLimit, 0);
    TestEqual(TEXT("Zero uses default"), ZeroResult.AppliedEntryLimit, 100);

    const FPDSAutomationDiffResult NegativeResult =
        PDSAutomation::BuildDiffResult(
            Diff,
            PersistResult,
            TEXT("Negative"),
            -7,
            100);
    TestEqual(TEXT("Negative keeps raw request"), NegativeResult.RequestedEntryLimit, -7);
    TestEqual(TEXT("Negative uses default"), NegativeResult.AppliedEntryLimit, 100);

    const FPDSAutomationDiffResult OneResult =
        PDSAutomation::BuildDiffResult(
            Diff,
            PersistResult,
            TEXT("One"),
            1,
            100);
    TestEqual(TEXT("One is applied exactly"), OneResult.AppliedEntryLimit, 1);

    const FPDSAutomationDiffResult CappedResult =
        PDSAutomation::BuildDiffResult(
            Diff,
            PersistResult,
            TEXT("Capped"),
            9999,
            100);
    TestEqual(TEXT("Large request keeps raw value"), CappedResult.RequestedEntryLimit, 9999);
    TestEqual(TEXT("Large request is capped"), CappedResult.AppliedEntryLimit, 500);
    TestEqual(TEXT("Maximum is explicit"), CappedResult.MaximumAllowedEntryLimit, 500);
    return true;
}

#endif
