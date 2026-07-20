#pragma once

#include "CoreMinimal.h"
#include "PDSAutomationTypes.h"
#include "PDSDeveloperTypes.h"
#include "PDSSnapshotDiffTypes.h"

/**
 * Fachada independiente de MCP/Slate para exponer diagnósticos PDS a cualquier adaptador.
 * Solo escribe informes, snapshots y baseline bajo Saved/PantheliaDeveloperSuite.
 */
class PANTHELIADEVELOPERSUITE_API FPDSAutomationService
{
public:
    FPDSAutomationStatusResult GetStatus() const;
    FPDSAutomationSnapshotHistoryResult ListSnapshots(int32 Limit) const;
    FPDSAutomationSnapshotExportResult ExportProjectSnapshot() const;
    FPDSAutomationValidationResult ValidateProfile(
        EPDSAutomationValidationProfile Profile,
        int32 MaxIssues) const;
    FPDSAutomationBaselineUpdateResult SetLatestSnapshotAsBaseline() const;
    FPDSAutomationDiffResult CompareLatestSnapshotWithBaseline(int32 MaxEntries) const;
    FPDSAutomationDiffResult CompareLatestTwoSnapshots(int32 MaxEntries) const;

    static FString GetAutomationApiVersion();
};

namespace PDSAutomation
{
    /** Proyección pura y testeable de un snapshot ya parseado. */
    PANTHELIADEVELOPERSUITE_API FPDSAutomationSnapshotMetadata
    BuildSnapshotMetadata(
        const FPDSSnapshotDocument& Document,
        const FString& FilePath);

    /** Conversión común para futuros adaptadores, tests y commandlets. */
    PANTHELIADEVELOPERSUITE_API FPDSAutomationOperationResult ConvertOperationResult(
        const FPDSOperationResult& Source,
        int32 IssueLimit);

    /** Conversión acotada del resultado Project Doctor. */
    PANTHELIADEVELOPERSUITE_API FPDSAutomationValidationResult ConvertValidationSummary(
        const FPDSValidationSummary& Source,
        int32 IssueLimit);

    /** Conversión acotada de un diff ya calculado y su persistencia asociada. */
    PANTHELIADEVELOPERSUITE_API FPDSAutomationDiffResult BuildDiffResult(
        const FPDSSnapshotDiff& Diff,
        const FPDSOperationResult& PersistResult,
        const FString& ComparisonLabel,
        int32 MaxEntries,
        int32 IssueLimit);

    /** Mapea el enum reflejado de Automation API al perfil interno existente. */
    PANTHELIADEVELOPERSUITE_API EPDSValidationProfile ToNativeValidationProfile(
        EPDSAutomationValidationProfile Profile);
}
