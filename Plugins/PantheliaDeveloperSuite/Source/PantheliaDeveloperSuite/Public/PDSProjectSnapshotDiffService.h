#pragma once

#include "CoreMinimal.h"
#include "PDSDeveloperTypes.h"
#include "PDSSnapshotDiffTypes.h"

/**
 * Historial y comparación read-only de snapshots JSON. Solo escribe informes y
 * baseline.json bajo Saved/PantheliaDeveloperSuite; nunca guarda assets.
 */
class PANTHELIADEVELOPERSUITE_API FPDSProjectSnapshotDiffService
{
public:
    FPDSOperationResult CompareLatestTwoSnapshots() const;
    FPDSOperationResult CompareLatestSnapshotWithBaseline() const;
    FPDSOperationResult SetLatestSnapshotAsBaseline() const;

    /** API reutilizable por tests y futuras integraciones IA. */
    FPDSOperationResult CompareSnapshotFiles(
        const FString& PreviousSnapshotPath,
        const FString& CurrentSnapshotPath,
        const FString& ComparisonLabel) const;

    /**
     * Persiste informes a partir de un diff ya calculado, sin volver a leer snapshots.
     * ReportsDirectoryOverride y bWriteLatestAliases permiten pruebas aisladas sin
     * sobrescribir los aliases de producción bajo Saved/PantheliaDeveloperSuite.
     */
    FPDSOperationResult PersistDiffReports(
        const FPDSSnapshotDiff& Diff,
        const FString& ComparisonLabel,
        const FString& ReportsDirectoryOverride = FString(),
        bool bWriteLatestAliases = true) const;

    static FString GetSnapshotsDirectory();
    static FString GetBaselineSnapshotPath();
    static TArray<FString> FindTimestampedSnapshotPathsNewestFirst();
};
