#include "PDSSemanticDiffTypes.h"

namespace
{
    TMap<FString, const FPDSSemanticRecord*> IndexRecords(
        const FPDSSemanticDomainSnapshot& Domain)
    {
        TMap<FString, const FPDSSemanticRecord*> Result;
        for (const FPDSSemanticRecord& Record : Domain.Records)
        {
            if (!Record.RecordId.IsEmpty())
            {
                Result.Add(Record.RecordId, &Record);
            }
        }
        return Result;
    }

    FPDSSemanticRecordChange MakeRecordChange(
        const FString& DomainId,
        const FPDSSemanticRecord& Record)
    {
        return {
            DomainId,
            Record.RecordId,
            Record.DisplayName,
            Record.Kind,
            Record.SourceAssetPath
        };
    }

    void AddFieldChange(
        FPDSSemanticDiff& Diff,
        const FString& DomainId,
        const FPDSSemanticRecord& Previous,
        const FPDSSemanticRecord& Current,
        const FString& FieldName,
        const FString& PreviousValue,
        const FString& CurrentValue)
    {
        Diff.ChangedFields.Add({
            DomainId,
            Current.RecordId,
            Current.DisplayName.IsEmpty()
                ? Previous.DisplayName
                : Current.DisplayName,
            Current.SourceAssetPath.IsEmpty()
                ? Previous.SourceAssetPath
                : Current.SourceAssetPath,
            FieldName,
            PreviousValue,
            CurrentValue
        });
    }
}

bool FPDSSemanticDiff::HasChanges() const
{
    return !AddedRecords.IsEmpty()
        || !RemovedRecords.IsEmpty()
        || !ChangedFields.IsEmpty();
}

FPDSSemanticDiff PDSSemanticDiff::Compare(
    const TMap<FString, FPDSSemanticDomainSnapshot>& PreviousDomains,
    const TMap<FString, FPDSSemanticDomainSnapshot>& CurrentDomains)
{
    FPDSSemanticDiff Diff;

    for (const TPair<FString, FPDSSemanticDomainSnapshot>& Pair : PreviousDomains)
    {
        Diff.PreviousRecordCount += Pair.Value.Records.Num();
    }
    for (const TPair<FString, FPDSSemanticDomainSnapshot>& Pair : CurrentDomains)
    {
        Diff.CurrentRecordCount += Pair.Value.Records.Num();
    }

    TSet<FString> DomainIdSet;
    for (const TPair<FString, FPDSSemanticDomainSnapshot>& Pair : PreviousDomains)
    {
        DomainIdSet.Add(Pair.Key);
    }
    for (const TPair<FString, FPDSSemanticDomainSnapshot>& Pair : CurrentDomains)
    {
        DomainIdSet.Add(Pair.Key);
    }

    TArray<FString> DomainIds = DomainIdSet.Array();
    DomainIds.Sort();

    for (const FString& DomainId : DomainIds)
    {
        const FPDSSemanticDomainSnapshot* Previous =
            PreviousDomains.Find(DomainId);
        const FPDSSemanticDomainSnapshot* Current =
            CurrentDomains.Find(DomainId);

        if (!Previous || !Current)
        {
            Diff.NonComparableDomains.Add(DomainId);
            Diff.Issues.Add({
                EPDSIssueSeverity::Info,
                TEXT("PDS.SnapshotDiff.SemanticDomainNotComparable"),
                FString(),
                FString::Printf(
                    TEXT("El dominio '%s' existe en un solo snapshot. No se generaron altas/bajas masivas porque el dato histórico no es comparable."),
                    *DomainId),
                EPDSAssetOrigin::Unknown
            });
            continue;
        }

        if (Previous->SchemaVersion != Current->SchemaVersion)
        {
            Diff.Issues.Add({
                EPDSIssueSeverity::Info,
                TEXT("PDS.SnapshotDiff.SemanticDomainSchemaChanged"),
                FString(),
                FString::Printf(
                    TEXT("El dominio '%s' cambió de schema %s a %s; se comparó el subconjunto de fields por nombre."),
                    *DomainId,
                    *Previous->SchemaVersion,
                    *Current->SchemaVersion),
                EPDSAssetOrigin::Unknown
            });
        }

        const TMap<FString, const FPDSSemanticRecord*> PreviousById =
            IndexRecords(*Previous);
        const TMap<FString, const FPDSSemanticRecord*> CurrentById =
            IndexRecords(*Current);

        TSet<FString> RecordIdSet;
        for (const TPair<FString, const FPDSSemanticRecord*>& Pair : PreviousById)
        {
            RecordIdSet.Add(Pair.Key);
        }
        for (const TPair<FString, const FPDSSemanticRecord*>& Pair : CurrentById)
        {
            RecordIdSet.Add(Pair.Key);
        }

        TArray<FString> RecordIds = RecordIdSet.Array();
        RecordIds.Sort();

        for (const FString& RecordId : RecordIds)
        {
            const FPDSSemanticRecord* PreviousRecord =
                PreviousById.FindRef(RecordId);
            const FPDSSemanticRecord* CurrentRecord =
                CurrentById.FindRef(RecordId);

            if (!PreviousRecord && CurrentRecord)
            {
                Diff.AddedRecords.Add(
                    MakeRecordChange(DomainId, *CurrentRecord));
                continue;
            }

            if (PreviousRecord && !CurrentRecord)
            {
                Diff.RemovedRecords.Add(
                    MakeRecordChange(DomainId, *PreviousRecord));
                continue;
            }

            if (!PreviousRecord || !CurrentRecord)
            {
                continue;
            }

            if (PreviousRecord->DisplayName != CurrentRecord->DisplayName)
            {
                AddFieldChange(
                    Diff,
                    DomainId,
                    *PreviousRecord,
                    *CurrentRecord,
                    TEXT("$displayName"),
                    PreviousRecord->DisplayName,
                    CurrentRecord->DisplayName);
            }
            if (PreviousRecord->Kind != CurrentRecord->Kind)
            {
                AddFieldChange(
                    Diff,
                    DomainId,
                    *PreviousRecord,
                    *CurrentRecord,
                    TEXT("$kind"),
                    PreviousRecord->Kind,
                    CurrentRecord->Kind);
            }
            if (PreviousRecord->SourceAssetPath != CurrentRecord->SourceAssetPath)
            {
                AddFieldChange(
                    Diff,
                    DomainId,
                    *PreviousRecord,
                    *CurrentRecord,
                    TEXT("$sourceAssetPath"),
                    PreviousRecord->SourceAssetPath,
                    CurrentRecord->SourceAssetPath);
            }

            TSet<FString> FieldNameSet;
            for (const TPair<FString, FString>& Pair : PreviousRecord->Fields)
            {
                FieldNameSet.Add(Pair.Key);
            }
            for (const TPair<FString, FString>& Pair : CurrentRecord->Fields)
            {
                FieldNameSet.Add(Pair.Key);
            }

            TArray<FString> FieldNames = FieldNameSet.Array();
            FieldNames.Sort();

            for (const FString& FieldName : FieldNames)
            {
                const FString PreviousValue =
                    PreviousRecord->Fields.FindRef(FieldName);
                const FString CurrentValue =
                    CurrentRecord->Fields.FindRef(FieldName);

                if (PreviousValue != CurrentValue)
                {
                    AddFieldChange(
                        Diff,
                        DomainId,
                        *PreviousRecord,
                        *CurrentRecord,
                        FieldName,
                        PreviousValue,
                        CurrentValue);
                }
            }
        }
    }

    Diff.AddedRecords.Sort([](
        const FPDSSemanticRecordChange& A,
        const FPDSSemanticRecordChange& B)
    {
        return A.DomainId == B.DomainId
            ? A.RecordId < B.RecordId
            : A.DomainId < B.DomainId;
    });
    Diff.RemovedRecords.Sort([](
        const FPDSSemanticRecordChange& A,
        const FPDSSemanticRecordChange& B)
    {
        return A.DomainId == B.DomainId
            ? A.RecordId < B.RecordId
            : A.DomainId < B.DomainId;
    });
    Diff.ChangedFields.Sort([](
        const FPDSSemanticFieldChange& A,
        const FPDSSemanticFieldChange& B)
    {
        if (A.DomainId != B.DomainId)
        {
            return A.DomainId < B.DomainId;
        }
        if (A.RecordId != B.RecordId)
        {
            return A.RecordId < B.RecordId;
        }
        return A.FieldName < B.FieldName;
    });
    Diff.NonComparableDomains.Sort();

    return Diff;
}
