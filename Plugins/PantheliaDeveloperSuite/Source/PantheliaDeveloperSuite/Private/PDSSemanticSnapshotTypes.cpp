#include "PDSSemanticSnapshotTypes.h"

void FPDSSemanticRecord::Normalize()
{
    RecordId.TrimStartAndEndInline();
    DisplayName.TrimStartAndEndInline();
    Kind.TrimStartAndEndInline();
    SourceAssetPath.TrimStartAndEndInline();

    TMap<FString, FString> NormalizedFields;
    for (const TPair<FString, FString>& Pair : Fields)
    {
        FString Key = Pair.Key;
        FString Value = Pair.Value;
        Key.TrimStartAndEndInline();
        Value.TrimStartAndEndInline();

        if (!Key.IsEmpty())
        {
            NormalizedFields.Add(MoveTemp(Key), MoveTemp(Value));
        }
    }

    Fields = MoveTemp(NormalizedFields);
}

void FPDSSemanticDomainSnapshot::Normalize()
{
    DomainId.TrimStartAndEndInline();
    SchemaVersion.TrimStartAndEndInline();

    for (FPDSSemanticRecord& Record : Records)
    {
        Record.Normalize();
    }

    Records.RemoveAll([](const FPDSSemanticRecord& Record)
    {
        return Record.RecordId.IsEmpty();
    });

    Records.Sort([](const FPDSSemanticRecord& A, const FPDSSemanticRecord& B)
    {
        return A.RecordId < B.RecordId;
    });

    TArray<FPDSSemanticRecord> UniqueRecords;
    UniqueRecords.Reserve(Records.Num());

    FString PreviousRecordId;
    for (FPDSSemanticRecord& Record : Records)
    {
        if (!PreviousRecordId.IsEmpty() && Record.RecordId == PreviousRecordId)
        {
            Issues.Add({
                EPDSIssueSeverity::Error,
                TEXT("PDS.Semantic.DuplicateRecordId"),
                Record.SourceAssetPath,
                FString::Printf(
                    TEXT("El dominio '%s' contiene más de un record con RecordId '%s'. Se conservó la primera entrada para evitar sobrescritura silenciosa."),
                    *DomainId,
                    *Record.RecordId),
                EPDSAssetOrigin::Unknown
            });
            continue;
        }

        PreviousRecordId = Record.RecordId;
        UniqueRecords.Add(MoveTemp(Record));
    }

    Records = MoveTemp(UniqueRecords);
}

FPDSSnapshotDomainRegistry& FPDSSnapshotDomainRegistry::Get()
{
    static FPDSSnapshotDomainRegistry Registry;
    return Registry;
}

bool FPDSSnapshotDomainRegistry::RegisterContributor(
    const TSharedRef<IPDSSnapshotDomainContributor>& Contributor,
    FString* OutError)
{
    check(IsInGameThread());

    FString DomainId = Contributor->GetDomainId();
    DomainId.TrimStartAndEndInline();

    if (DomainId.IsEmpty())
    {
        if (OutError)
        {
            *OutError = TEXT("El contributor no declaró DomainId.");
        }
        return false;
    }

    if (ContributorsByDomainId.Contains(DomainId))
    {
        if (OutError)
        {
            *OutError = FString::Printf(
                TEXT("Ya existe un contributor para el dominio '%s'."),
                *DomainId);
        }
        return false;
    }

    ContributorsByDomainId.Add(DomainId, Contributor);
    return true;
}

bool FPDSSnapshotDomainRegistry::UnregisterContributor(const FString& DomainId)
{
    check(IsInGameThread());
    return ContributorsByDomainId.Remove(DomainId) > 0;
}

TArray<FPDSSemanticDomainSnapshot>
FPDSSnapshotDomainRegistry::GatherAllDomains(TArray<FPDSIssue>& OutIssues) const
{
    check(IsInGameThread());

    TArray<FString> DomainIds;
    ContributorsByDomainId.GetKeys(DomainIds);
    DomainIds.Sort();

    TArray<FPDSSemanticDomainSnapshot> Result;
    Result.Reserve(DomainIds.Num());

    for (const FString& DomainId : DomainIds)
    {
        const TSharedRef<IPDSSnapshotDomainContributor>* Contributor =
            ContributorsByDomainId.Find(DomainId);
        if (!Contributor)
        {
            continue;
        }

        FPDSSemanticDomainSnapshot Domain;
        Domain.DomainId = DomainId;
        Domain.SchemaVersion = (*Contributor)->GetSchemaVersion();
        (*Contributor)->GatherDomainSnapshot(Domain);
        Domain.Normalize();

        OutIssues.Append(Domain.Issues);
        Result.Add(MoveTemp(Domain));
    }

    return Result;
}
