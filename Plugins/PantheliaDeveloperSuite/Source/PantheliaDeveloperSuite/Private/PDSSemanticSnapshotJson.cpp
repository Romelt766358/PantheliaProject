#include "PDSSemanticSnapshotJson.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
    TSharedRef<FJsonObject> BuildRecordJson(const FPDSSemanticRecord& Record)
    {
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("recordId"), Record.RecordId);
        Json->SetStringField(TEXT("displayName"), Record.DisplayName);
        Json->SetStringField(TEXT("kind"), Record.Kind);
        Json->SetStringField(TEXT("sourceAssetPath"), Record.SourceAssetPath);

        TSharedRef<FJsonObject> FieldsJson = MakeShared<FJsonObject>();
        TArray<FString> Keys;
        Record.Fields.GetKeys(Keys);
        Keys.Sort();
        for (const FString& Key : Keys)
        {
            FieldsJson->SetStringField(Key, Record.Fields.FindRef(Key));
        }

        Json->SetObjectField(TEXT("fields"), FieldsJson);
        return Json;
    }

    FString GetOptionalString(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* FieldName)
    {
        return Object.IsValid() && Object->HasTypedField<EJson::String>(FieldName)
            ? Object->GetStringField(FieldName)
            : FString();
    }
}

TSharedRef<FJsonObject> PDSSemanticSnapshotJson::BuildDomainsJson(
    const TArray<FPDSSemanticDomainSnapshot>& Domains)
{
    TSharedRef<FJsonObject> DomainsJson = MakeShared<FJsonObject>();

    TArray<const FPDSSemanticDomainSnapshot*> SortedDomains;
    SortedDomains.Reserve(Domains.Num());
    for (const FPDSSemanticDomainSnapshot& Domain : Domains)
    {
        SortedDomains.Add(&Domain);
    }
    SortedDomains.Sort([](
        const FPDSSemanticDomainSnapshot& A,
        const FPDSSemanticDomainSnapshot& B)
    {
        return A.DomainId < B.DomainId;
    });

    for (const FPDSSemanticDomainSnapshot* Domain : SortedDomains)
    {
        if (Domain->DomainId.IsEmpty())
        {
            continue;
        }

        TSharedRef<FJsonObject> DomainJson = MakeShared<FJsonObject>();
        DomainJson->SetStringField(TEXT("schemaVersion"), Domain->SchemaVersion);

        TArray<TSharedPtr<FJsonValue>> RecordsJson;
        RecordsJson.Reserve(Domain->Records.Num());
        for (const FPDSSemanticRecord& Record : Domain->Records)
        {
            RecordsJson.Add(MakeShared<FJsonValueObject>(BuildRecordJson(Record)));
        }

        DomainJson->SetArrayField(TEXT("records"), RecordsJson);
        DomainsJson->SetObjectField(Domain->DomainId, DomainJson);
    }

    return DomainsJson;
}

bool PDSSemanticSnapshotJson::ParseDomainsJson(
    const TSharedPtr<FJsonObject>& RootObject,
    TMap<FString, FPDSSemanticDomainSnapshot>& OutDomainsById,
    TArray<FPDSIssue>& OutIssues)
{
    OutDomainsById.Reset();

    if (!RootObject.IsValid())
    {
        OutIssues.Add({
            EPDSIssueSeverity::Error,
            TEXT("PDS.Semantic.InvalidRoot"),
            FString(),
            TEXT("No existe un objeto JSON raíz válido para leer semanticDomains."),
            EPDSAssetOrigin::Unknown
        });
        return false;
    }

    if (!RootObject->HasField(TEXT("semanticDomains")))
    {
        // Snapshot histórico: ausencia válida y explícitamente no comparable.
        return true;
    }

    if (!RootObject->HasTypedField<EJson::Object>(TEXT("semanticDomains")))
    {
        OutIssues.Add({
            EPDSIssueSeverity::Error,
            TEXT("PDS.Semantic.InvalidDomainsRoot"),
            FString(),
            TEXT("semanticDomains existe, pero no es un objeto JSON."),
            EPDSAssetOrigin::Unknown
        });
        return false;
    }

    const TSharedPtr<FJsonObject> DomainsObject =
        RootObject->GetObjectField(TEXT("semanticDomains"));

    bool bAllDomainsValid = true;
    TArray<FString> DomainIds;
    DomainIds.Reserve(DomainsObject->Values.Num());
    for (const TPair<FJsonObject::FStringType, TSharedPtr<FJsonValue>>& Pair
        : DomainsObject->Values)
    {
        DomainIds.Emplace(Pair.Key.ToView());
    }
    DomainIds.Sort();

    for (const FString& DomainId : DomainIds)
    {
        const TSharedPtr<FJsonObject>* DomainObjectPtr = nullptr;
        if (!DomainsObject->TryGetObjectField(DomainId, DomainObjectPtr)
            || !DomainObjectPtr
            || !DomainObjectPtr->IsValid())
        {
            OutIssues.Add({
                EPDSIssueSeverity::Error,
                TEXT("PDS.Semantic.InvalidDomain"),
                FString(),
                FString::Printf(
                    TEXT("El dominio semántico '%s' no contiene un objeto JSON válido."),
                    *DomainId),
                EPDSAssetOrigin::Unknown
            });
            bAllDomainsValid = false;
            continue;
        }

        FPDSSemanticDomainSnapshot Domain;
        Domain.DomainId = DomainId;
        Domain.SchemaVersion =
            GetOptionalString(*DomainObjectPtr, TEXT("schemaVersion"));

        const TArray<TSharedPtr<FJsonValue>>* RecordsJson = nullptr;
        if ((*DomainObjectPtr)->HasField(TEXT("records"))
            && !(*DomainObjectPtr)->TryGetArrayField(TEXT("records"), RecordsJson))
        {
            OutIssues.Add({
                EPDSIssueSeverity::Error,
                TEXT("PDS.Semantic.InvalidRecordsArray"),
                FString(),
                FString::Printf(
                    TEXT("El dominio semántico '%s' contiene records, pero no es un array JSON."),
                    *DomainId),
                EPDSAssetOrigin::Unknown
            });
            bAllDomainsValid = false;
        }
        else if (RecordsJson)
        {
            for (const TSharedPtr<FJsonValue>& Value : *RecordsJson)
            {
                const TSharedPtr<FJsonObject> RecordObject =
                    Value.IsValid() ? Value->AsObject() : nullptr;
                if (!RecordObject.IsValid())
                {
                    OutIssues.Add({
                        EPDSIssueSeverity::Warning,
                        TEXT("PDS.Semantic.InvalidRecord"),
                        FString(),
                        FString::Printf(
                            TEXT("El dominio '%s' contiene un record que no es un objeto JSON y fue omitido."),
                            *DomainId),
                        EPDSAssetOrigin::Unknown
                    });
                    continue;
                }

                FPDSSemanticRecord Record;
                Record.RecordId = GetOptionalString(RecordObject, TEXT("recordId"));
                Record.DisplayName =
                    GetOptionalString(RecordObject, TEXT("displayName"));
                Record.Kind = GetOptionalString(RecordObject, TEXT("kind"));
                Record.SourceAssetPath =
                    GetOptionalString(RecordObject, TEXT("sourceAssetPath"));

                const TSharedPtr<FJsonObject>* FieldsObjectPtr = nullptr;
                if (RecordObject->TryGetObjectField(
                        TEXT("fields"),
                        FieldsObjectPtr)
                    && FieldsObjectPtr
                    && FieldsObjectPtr->IsValid())
                {
                    TArray<FString> FieldNames;
                    FieldNames.Reserve((*FieldsObjectPtr)->Values.Num());
                    for (const TPair<FJsonObject::FStringType, TSharedPtr<FJsonValue>>& Pair
                        : (*FieldsObjectPtr)->Values)
                    {
                        FieldNames.Emplace(Pair.Key.ToView());
                    }
                    FieldNames.Sort();

                    for (const FString& FieldName : FieldNames)
                    {
                        FString FieldValue;
                        if ((*FieldsObjectPtr)->TryGetStringField(
                                FieldName,
                                FieldValue))
                        {
                            Record.Fields.Add(FieldName, MoveTemp(FieldValue));
                        }
                    }
                }

                Record.Normalize();
                if (Record.RecordId.IsEmpty())
                {
                    OutIssues.Add({
                        EPDSIssueSeverity::Warning,
                        TEXT("PDS.Semantic.RecordIdMissing"),
                        Record.SourceAssetPath,
                        FString::Printf(
                            TEXT("Un record del dominio '%s' no tiene recordId y fue omitido."),
                            *DomainId),
                        EPDSAssetOrigin::Unknown
                    });
                    continue;
                }

                Domain.Records.Add(MoveTemp(Record));
            }
        }

        Domain.Normalize();
        if (!Domain.Issues.IsEmpty())
        {
            OutIssues.Append(Domain.Issues);
            bAllDomainsValid = false;
        }
        OutDomainsById.Add(DomainId, MoveTemp(Domain));
    }

    return bAllDomainsValid;
}
