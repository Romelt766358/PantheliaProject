#include "SPDSDashboard.h"

#include "HAL/PlatformProcess.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PantheliaDeveloperSuiteModule.h"
#include "PDSAssetUtilities.h"
#include "PDSDeveloperSettings.h"
#include "PDSMontageInspectorService.h"
#include "PDSProjectDoctorService.h"
#include "PDSProjectSnapshotService.h"
#include "PDSProjectSnapshotDiffService.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"

#define LOCTEXT_NAMESPACE "SPDSDashboard"

void SPDSDashboard::Construct(const FArguments& InArgs)
{
    ChildSlot
    [
        SNew(SBorder)
        .Padding(12.0f)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT("Title", "Panthelia Developer Suite — v0.4 Alpha 2 MCP Automation Read-Only"))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 12.0f)
            [
                SNew(STextBlock)
                .AutoWrapText(true)
                .Text(LOCTEXT(
                    "Description",
                    "Project Doctor, Snapshot History y Snapshot Diff Browser continúan read-only. La Automation API expone respuestas estructuradas mediante Toolset Registry para Unreal MCP sin guardar assets."))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 12.0f)
            [
                SNew(SUniformGridPanel)
                .SlotPadding(FMargin(4.0f))

                + SUniformGridPanel::Slot(0, 0)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("ValidateSelected", "Validate Selected Assets"))
                    .OnClicked(this, &SPDSDashboard::OnValidateSelectedClicked)
                ]

                + SUniformGridPanel::Slot(1, 0)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("ValidateCore", "Validate Panthelia Core"))
                    .OnClicked(this, &SPDSDashboard::OnValidatePantheliaCoreClicked)
                ]

                + SUniformGridPanel::Slot(0, 1)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("ValidateGameContent", "Validate Game Content"))
                    .OnClicked(this, &SPDSDashboard::OnValidateGameContentClicked)
                ]

                + SUniformGridPanel::Slot(1, 1)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("ValidateExternal", "Validate External Packs"))
                    .OnClicked(this, &SPDSDashboard::OnValidateExternalContentClicked)
                ]

                + SUniformGridPanel::Slot(0, 2)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("ValidateProject", "Validate Entire Project"))
                    .OnClicked(this, &SPDSDashboard::OnValidateProjectClicked)
                ]

                + SUniformGridPanel::Slot(1, 2)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("ExportSnapshot", "Export Project Snapshot"))
                    .OnClicked(this, &SPDSDashboard::OnExportSnapshotClicked)
                ]

                + SUniformGridPanel::Slot(0, 3)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("CompareLatestSnapshots", "Compare Latest Two Snapshots"))
                    .OnClicked(this, &SPDSDashboard::OnCompareLatestSnapshotsClicked)
                ]

                + SUniformGridPanel::Slot(1, 3)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("SetSnapshotBaseline", "Set Latest Snapshot as Baseline"))
                    .OnClicked(this, &SPDSDashboard::OnSetSnapshotBaselineClicked)
                ]

                + SUniformGridPanel::Slot(0, 4)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("CompareBaseline", "Compare Latest with Baseline"))
                    .OnClicked(this, &SPDSDashboard::OnCompareBaselineClicked)
                ]

                + SUniformGridPanel::Slot(1, 4)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("InspectMontages", "Inspect Selected Montages"))
                    .OnClicked(this, &SPDSDashboard::OnInspectMontagesClicked)
                ]

                + SUniformGridPanel::Slot(0, 5)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("OpenSnapshotDiffBrowser", "Open Snapshot Diff Browser"))
                    .OnClicked(this, &SPDSDashboard::OnOpenSnapshotDiffBrowserClicked)
                ]

                + SUniformGridPanel::Slot(1, 5)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("OpenOutput", "Open Saved Output Folder"))
                    .OnClicked(this, &SPDSDashboard::OnOpenOutputFolderClicked)
                ]
            ]

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            [
                SAssignNew(OutputTextBox, SMultiLineEditableTextBox)
                .IsReadOnly(true)
                .AutoWrapText(false)
                .Text(LOCTEXT(
                    "InitialOutput",
                    "La suite está lista. Esta versión preliminar no guarda ni corrige assets."))
            ]
        ]
    ];
}

FReply SPDSDashboard::OnValidateSelectedClicked()
{
    const FPDSProjectDoctorService Service;
    ShowValidationSummary(Service.ValidateSelectedAssets());
    return FReply::Handled();
}

FReply SPDSDashboard::OnValidatePantheliaCoreClicked()
{
    const FPDSProjectDoctorService Service;
    ShowValidationSummary(Service.ValidatePantheliaCore());
    return FReply::Handled();
}

FReply SPDSDashboard::OnValidateGameContentClicked()
{
    const FPDSProjectDoctorService Service;
    ShowValidationSummary(Service.ValidateGameContent());
    return FReply::Handled();
}

FReply SPDSDashboard::OnValidateExternalContentClicked()
{
    const FPDSProjectDoctorService Service;
    ShowValidationSummary(Service.ValidateExternalContent());
    return FReply::Handled();
}

FReply SPDSDashboard::OnValidateProjectClicked()
{
    const FPDSProjectDoctorService Service;
    ShowValidationSummary(Service.ValidateEntireProject());
    return FReply::Handled();
}

FReply SPDSDashboard::OnExportSnapshotClicked()
{
    const FPDSProjectSnapshotService Service;
    const FPDSOperationResult Result = Service.ExportProjectSnapshot(
        bHasValidationSummary ? &LastValidationSummary : nullptr);
    SetOutput(Result.ToMultilineText());
    return FReply::Handled();
}

FReply SPDSDashboard::OnCompareLatestSnapshotsClicked()
{
    const FPDSProjectSnapshotDiffService Service;
    SetOutput(Service.CompareLatestTwoSnapshots().ToMultilineText());
    return FReply::Handled();
}

FReply SPDSDashboard::OnSetSnapshotBaselineClicked()
{
    const FPDSProjectSnapshotDiffService Service;
    SetOutput(Service.SetLatestSnapshotAsBaseline().ToMultilineText());
    return FReply::Handled();
}

FReply SPDSDashboard::OnCompareBaselineClicked()
{
    const FPDSProjectSnapshotDiffService Service;
    SetOutput(Service.CompareLatestSnapshotWithBaseline().ToMultilineText());
    return FReply::Handled();
}

FReply SPDSDashboard::OnOpenSnapshotDiffBrowserClicked()
{
    FPantheliaDeveloperSuiteModule& Module =
        FModuleManager::LoadModuleChecked<FPantheliaDeveloperSuiteModule>(
            TEXT("PantheliaDeveloperSuite"));
    Module.OpenSnapshotDiffBrowser();
    return FReply::Handled();
}

FReply SPDSDashboard::OnInspectMontagesClicked()
{
    const FPDSMontageInspectorService Service;
    const FPDSOperationResult Result = Service.InspectSelectedMontages();
    SetOutput(Result.ToMultilineText());
    return FReply::Handled();
}

FReply SPDSDashboard::OnOpenOutputFolderClicked()
{
    FString OutputDirectory = FPaths::ConvertRelativePathToFull(
        PDSAssetUtilities::GetSuiteSavedDirectory());
    FPaths::NormalizeDirectoryName(OutputDirectory);

    if (!PDSAssetUtilities::EnsureDirectoryExists(OutputDirectory))
    {
        SetOutput(FString::Printf(
            TEXT("No se pudo crear o verificar la carpeta de salida:\n%s"),
            *OutputDirectory));
        return FReply::Handled();
    }

    FPlatformProcess::ExploreFolder(*OutputDirectory);
    // En éxito no se reemplaza el resumen visible que el usuario está consultando.
    return FReply::Handled();
}

void SPDSDashboard::ShowValidationSummary(FPDSValidationSummary&& Summary)
{
    LastValidationSummary = MoveTemp(Summary);
    bHasValidationSummary = true;

    const UPDSDeveloperSettings* Settings = GetDefault<UPDSDeveloperSettings>();
    const int32 PreviewLimit = Settings
        ? Settings->DashboardIssuePreviewLimit
        : 100;
    SetOutput(LastValidationSummary.ToMultilineText(PreviewLimit));
}

void SPDSDashboard::SetOutput(const FString& NewOutput)
{
    if (OutputTextBox.IsValid())
    {
        OutputTextBox->SetText(FText::FromString(NewOutput));
    }
}

#undef LOCTEXT_NAMESPACE
