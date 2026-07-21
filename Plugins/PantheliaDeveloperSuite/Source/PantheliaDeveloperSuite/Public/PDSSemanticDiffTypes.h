#pragma once

#include "CoreMinimal.h"
#include "PDSSemanticSnapshotTypes.h"

struct PANTHELIADEVELOPERSUITE_API FPDSSemanticRecordChange
{
    FString DomainId;
    FString RecordId;
    FString DisplayName;
    FString Kind;
    FString SourceAssetPath;
};

struct PANTHELIADEVELOPERSUITE_API FPDSSemanticFieldChange
{
    FString DomainId;
    FString RecordId;
    FString DisplayName;
    FString SourceAssetPath;
    FString FieldName;
    FString PreviousValue;
    FString CurrentValue;
};

/**
 * Diff semántico determinista.
 *
 * Contratos v1:
 * - RecordId es la identidad dentro del dominio.
 * - Un rename/move que cambie RecordId se representa como Removed + Added.
 * - No existe seguimiento de identidad estable a través de renames en v1.
 * - Los fields que empiezan por '$' son pseudo-campos estructurales reservados.
 */
struct PANTHELIADEVELOPERSUITE_API FPDSSemanticDiff
{
    int32 PreviousRecordCount = 0;
    int32 CurrentRecordCount = 0;

    TArray<FPDSSemanticRecordChange> AddedRecords;
    TArray<FPDSSemanticRecordChange> RemovedRecords;
    TArray<FPDSSemanticFieldChange> ChangedFields;
    TArray<FString> NonComparableDomains;
    TArray<FPDSIssue> Issues;

    bool HasChanges() const;
};

namespace PDSSemanticDiff
{
    /**
     * Compara fields por nombre. En el contrato v1, un field ausente se proyecta como
     * string vacío; por tanto, no se distingue de un field presente con valor vacío.
     */
    PANTHELIADEVELOPERSUITE_API FPDSSemanticDiff Compare(
        const TMap<FString, FPDSSemanticDomainSnapshot>& PreviousDomains,
        const TMap<FString, FPDSSemanticDomainSnapshot>& CurrentDomains);
}
