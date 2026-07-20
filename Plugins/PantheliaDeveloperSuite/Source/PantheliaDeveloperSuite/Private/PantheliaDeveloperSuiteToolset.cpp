#include "PantheliaDeveloperSuiteToolset.h"

#include "PDSAutomationService.h"

FString UPantheliaDeveloperSuiteToolset::GetToolsetVersion() const
{
    return FPDSAutomationService::GetAutomationApiVersion();
}

FPDSAutomationStatusResult
UPantheliaDeveloperSuiteToolset::GetStatus()
{
    const FPDSAutomationService Service;
    return Service.GetStatus();
}

FPDSAutomationSnapshotHistoryResult
UPantheliaDeveloperSuiteToolset::ListSnapshots(const int32 Limit)
{
    const FPDSAutomationService Service;
    return Service.ListSnapshots(Limit);
}

FPDSAutomationSnapshotExportResult
UPantheliaDeveloperSuiteToolset::ExportProjectSnapshot()
{
    const FPDSAutomationService Service;
    return Service.ExportProjectSnapshot();
}

FPDSAutomationValidationResult
UPantheliaDeveloperSuiteToolset::ValidateProfile(
    const EPDSAutomationValidationProfile Profile,
    const int32 MaxIssues)
{
    const FPDSAutomationService Service;
    return Service.ValidateProfile(Profile, MaxIssues);
}

FPDSAutomationBaselineUpdateResult
UPantheliaDeveloperSuiteToolset::SetLatestSnapshotAsBaseline()
{
    const FPDSAutomationService Service;
    return Service.SetLatestSnapshotAsBaseline();
}

FPDSAutomationDiffResult
UPantheliaDeveloperSuiteToolset::CompareLatestSnapshotWithBaseline(
    const int32 MaxEntries)
{
    const FPDSAutomationService Service;
    return Service.CompareLatestSnapshotWithBaseline(MaxEntries);
}

FPDSAutomationDiffResult
UPantheliaDeveloperSuiteToolset::CompareLatestTwoSnapshots(
    const int32 MaxEntries)
{
    const FPDSAutomationService Service;
    return Service.CompareLatestTwoSnapshots(MaxEntries);
}
