#pragma once

#include "CoreMinimal.h"
#include "PDSDeveloperTypes.h"
#include "Widgets/SCompoundWidget.h"

class SMultiLineEditableTextBox;

class SPDSDashboard final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPDSDashboard) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    FReply OnValidateSelectedClicked();
    FReply OnValidatePantheliaCoreClicked();
    FReply OnValidateGameContentClicked();
    FReply OnValidateExternalContentClicked();
    FReply OnValidateProjectClicked();
    FReply OnExportSnapshotClicked();
    FReply OnCompareLatestSnapshotsClicked();
    FReply OnSetSnapshotBaselineClicked();
    FReply OnCompareBaselineClicked();
    FReply OnOpenSnapshotDiffBrowserClicked();
    FReply OnInspectMontagesClicked();
    FReply OnOpenOutputFolderClicked();

    void ShowValidationSummary(FPDSValidationSummary&& Summary);
    void SetOutput(const FString& NewOutput);

    FPDSValidationSummary LastValidationSummary;
    bool bHasValidationSummary = false;
    TSharedPtr<SMultiLineEditableTextBox> OutputTextBox;
};
