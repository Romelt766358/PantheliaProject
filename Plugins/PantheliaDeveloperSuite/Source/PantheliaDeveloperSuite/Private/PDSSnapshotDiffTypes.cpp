#include "PDSSnapshotDiffTypes.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "PDSSemanticSnapshotJson.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
    FString GetSnapshotDiffOptionalString(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* FieldName)
    {
        if (!Object.IsValid() || !Object->HasTypedField<EJson::String>(FieldName))
        {
            return FString();
        }
        return Object->GetStringField(FieldName);
    }

    int32 GetOptionalInt(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* FieldName)
    {
        if (!Object.IsValid() || !Object->HasTypedField<EJson::Number>(FieldName))
        {
            return 0;
        }
        return static_cast<int32>(Object->GetNumberField(FieldName));
    }

    TSharedPtr<FJsonObject> GetOptionalObject(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* FieldName)
    {
        if (!Object.IsValid() || !Object->HasTypedField<EJson::Object>(FieldName))
        {
            return nullptr;
        }
        return Object->GetObjectField(FieldName);
    }

    const TArray<TSharedPtr<FJsonValue>>* GetOptionalArray(
        const TSharedPtr<FJsonObject>& Object,
        const TCHAR* FieldName)
    {
        if (!Object.IsValid() || !Object->HasTypedField<EJson::Array>(FieldName))
        {
            return nullptr;
        }
        return &Object->GetArrayField(FieldName);
    }

    EPDSAssetOrigin ParseOrigin(const FString& Value)
    {
        if (Value.Equals(TEXT("Panthelia Core"), ESearchCase::IgnoreCase))
        {
            return EPDSAssetOrigin::PantheliaCore;
        }
        if (Value.Equals(TEXT("Game Content"), ESearchCase::IgnoreCase))
        {
            return EPDSAssetOrigin::GameContent;
        }
        if (Value.Equals(TEXT("External Content"), ESearchCase::IgnoreCase))
        {
            return EPDSAssetOrigin::ExternalContent;
        }
        return EPDSAssetOrigin::Unknown;
    }

    FString StableFloat(const double Value)
    {
        return FString::Printf(TEXT("%.6f"), Value);
    }

    FString BuildMontageFingerprint(const TSharedPtr<FJsonObject>& MontageObject)
    {
        if (!MontageObject.IsValid())
        {
            return FString();
        }

        FString Fingerprint;
        Fingerprint += TEXT("length=") + StableFloat(
            MontageObject->HasTypedField<EJson::Number>(TEXT("playLengthSeconds"))
                ? MontageObject->GetNumberField(TEXT("playLengthSeconds"))
                : 0.0);
        Fingerprint += FString::Printf(
            TEXT("|slots=%d|sections=%d|notifyTracks=%d"),
            GetOptionalInt(MontageObject, TEXT("slotTrackCount")),
            GetOptionalInt(MontageObject, TEXT("sectionCount")),
            GetOptionalInt(MontageObject, TEXT("notifyTrackCount")));

        if (const TArray<TSharedPtr<FJsonValue>>* Sections =
            GetOptionalArray(MontageObject, TEXT("sections")))
        {
            for (const TSharedPtr<FJsonValue>& Value : *Sections)
            {
                const TSharedPtr<FJsonObject> Section = Value.IsValid()
                    ? Value->AsObject()
                    : nullptr;
                Fingerprint += TEXT("|section:")
                    + GetSnapshotDiffOptionalString(Section, TEXT("name"))
                    + TEXT("@")
                    + StableFloat(
                        Section.IsValid()
                        && Section->HasTypedField<EJson::Number>(TEXT("startTimeSeconds"))
                            ? Section->GetNumberField(TEXT("startTimeSeconds"))
                            : 0.0);
            }
        }

        if (const TArray<TSharedPtr<FJsonValue>>* Notifies =
            GetOptionalArray(MontageObject, TEXT("notifies")))
        {
            for (const TSharedPtr<FJsonValue>& Value : *Notifies)
            {
                const TSharedPtr<FJsonObject> Notify = Value.IsValid()
                    ? Value->AsObject()
                    : nullptr;
                Fingerprint += FString::Printf(
                    TEXT("|notify:%d:%s@%s+%s"),
                    GetOptionalInt(Notify, TEXT("trackIndex")),
                    *GetSnapshotDiffOptionalString(Notify, TEXT("classOrName")),
                    *StableFloat(
                        Notify.IsValid()
                        && Notify->HasTypedField<EJson::Number>(TEXT("triggerTimeSeconds"))
                            ? Notify->GetNumberField(TEXT("triggerTimeSeconds"))
                            : 0.0),
                    *StableFloat(
                        Notify.IsValid()
                        && Notify->HasTypedField<EJson::Number>(TEXT("durationSeconds"))
                            ? Notify->GetNumberField(TEXT("durationSeconds"))
                            : 0.0));
            }
        }

        return Fingerprint;
    }

    template <typename ValueType>
    void SortRecordsByObjectPath(TArray<ValueType>& Records)
    {
        Records.Sort([](const ValueType& A, const ValueType& B)
        {
            return A.ObjectPath < B.ObjectPath;
        });
    }

    void SortStrings(TArray<FString>& Values)
    {
        Values.Sort([](const FString& A, const FString& B)
        {
            return A < B;
        });
    }

    void AppendStringPreview(
        FString& Output,
        const TCHAR* Heading,
        const TArray<FString>& Values,
        const int32 Limit)
    {
        Output += FString::Printf(TEXT("\n%s: %d\n"), Heading, Values.Num());
        const int32 SafeLimit = FMath::Max(Limit, 0);
        for (int32 Index = 0; Index < Values.Num() && Index < SafeLimit; ++Index)
        {
            Output += FString::Printf(TEXT("- %s%s"), *Values[Index], LINE_TERMINATOR);
        }
        if (Values.Num() > SafeLimit)
        {
            Output += FString::Printf(
                TEXT("- ... %d entradas adicionales en el informe completo.\n"),
                Values.Num() - SafeLimit);
        }
    }

    int32 CountSnapshotDiffModifiedSemanticRecords(const FPDSSemanticDiff& SemanticDiff)
    {
        TSet<FString> ModifiedRecordKeys;
        for (const FPDSSemanticFieldChange& Change : SemanticDiff.ChangedFields)
        {
            ModifiedRecordKeys.Add(
                Change.DomainId + TEXT("\n") + Change.RecordId);
        }
        return ModifiedRecordKeys.Num();
    }
}

bool FPDSSnapshotDiff::HasChanges() const
{
    return !AddedAssets.IsEmpty()
        || !RemovedAssets.IsEmpty()
        || !ChangedAssets.IsEmpty()
        || !AddedGameplayTags.IsEmpty()
        || !RemovedGameplayTags.IsEmpty()
        || !AddedMontages.IsEmpty()
        || !RemovedMontages.IsEmpty()
        || !ChangedMontages.IsEmpty()
        || SemanticDiff.HasChanges()
        || PreviousInvalidCount != CurrentInvalidCount
        || PreviousWarningCount != CurrentWarningCount;
}

FString FPDSSnapshotDiff::ToDashboardText(const int32 MaxEntriesPerCategory) const
{
    FString Output = TEXT("Comparación de snapshots completada");
    Output += FString::Printf(TEXT("\nProyecto: %s"), *ProjectName);
    Output += FString::Printf(TEXT("\nAnterior: %s"), *PreviousSnapshotPath);
    Output += FString::Printf(TEXT("\nActual: %s"), *CurrentSnapshotPath);
    Output += FString::Printf(
        TEXT("\n\nAssets: %d → %d (añadidos %d, eliminados %d, modificados %d)"),
        PreviousAssetCount,
        CurrentAssetCount,
        AddedAssets.Num(),
        RemovedAssets.Num(),
        ChangedAssets.Num());
    Output += FString::Printf(
        TEXT("\nGameplay Tags: %d → %d (añadidos %d, eliminados %d)"),
        PreviousGameplayTagCount,
        CurrentGameplayTagCount,
        AddedGameplayTags.Num(),
        RemovedGameplayTags.Num());
    Output += FString::Printf(
        TEXT("\nMontages: %d → %d (añadidos %d, eliminados %d, modificados %d)"),
        PreviousMontageCount,
        CurrentMontageCount,
        AddedMontages.Num(),
        RemovedMontages.Num(),
        ChangedMontages.Num());
    Output += FString::Printf(
        TEXT("\nSemantic records: %d → %d (añadidos %d, eliminados %d, modificados %d; fields %d; dominios no comparables %d)"),
        SemanticDiff.PreviousRecordCount,
        SemanticDiff.CurrentRecordCount,
        SemanticDiff.AddedRecords.Num(),
        SemanticDiff.RemovedRecords.Num(),
        CountSnapshotDiffModifiedSemanticRecords(SemanticDiff),
        SemanticDiff.ChangedFields.Num(),
        SemanticDiff.NonComparableDomains.Num());

    if (bPreviousValidationIncluded || bCurrentValidationIncluded)
    {
        Output += FString::Printf(
            TEXT("\nValidación: inválidos %d → %d; warnings %d → %d"),
            PreviousInvalidCount,
            CurrentInvalidCount,
            PreviousWarningCount,
            CurrentWarningCount);
    }

    if (NumAssetsWithOriginNotComparable > 0)
    {
        Output += FString::Printf(
            TEXT("\nOrigen histórico no comparable: %d assets"),
            NumAssetsWithOriginNotComparable);
    }

    TArray<FString> AddedAssetPaths;
    for (const FPDSSnapshotAssetRecord& Asset : AddedAssets)
    {
        AddedAssetPaths.Add(Asset.ObjectPath);
    }
    TArray<FString> RemovedAssetPaths;
    for (const FPDSSnapshotAssetRecord& Asset : RemovedAssets)
    {
        RemovedAssetPaths.Add(Asset.ObjectPath);
    }
    TArray<FString> ChangedAssetPaths;
    for (const FPDSSnapshotAssetChange& Asset : ChangedAssets)
    {
        ChangedAssetPaths.Add(Asset.ObjectPath);
    }

    AppendStringPreview(Output, TEXT("Assets añadidos"), AddedAssetPaths, MaxEntriesPerCategory);
    AppendStringPreview(Output, TEXT("Assets eliminados"), RemovedAssetPaths, MaxEntriesPerCategory);
    AppendStringPreview(Output, TEXT("Assets modificados"), ChangedAssetPaths, MaxEntriesPerCategory);
    AppendStringPreview(Output, TEXT("Tags añadidos"), AddedGameplayTags, MaxEntriesPerCategory);
    AppendStringPreview(Output, TEXT("Tags eliminados"), RemovedGameplayTags, MaxEntriesPerCategory);
    AppendStringPreview(Output, TEXT("Montages añadidos"), AddedMontages, MaxEntriesPerCategory);
    AppendStringPreview(Output, TEXT("Montages eliminados"), RemovedMontages, MaxEntriesPerCategory);
    AppendStringPreview(Output, TEXT("Montages modificados"), ChangedMontages, MaxEntriesPerCategory);

    TArray<FString> AddedSemanticRecords;
    for (const FPDSSemanticRecordChange& Change : SemanticDiff.AddedRecords)
    {
        AddedSemanticRecords.Add(Change.DomainId + TEXT("/") + Change.RecordId);
    }
    TArray<FString> RemovedSemanticRecords;
    for (const FPDSSemanticRecordChange& Change : SemanticDiff.RemovedRecords)
    {
        RemovedSemanticRecords.Add(Change.DomainId + TEXT("/") + Change.RecordId);
    }
    TArray<FString> ChangedSemanticFields;
    for (const FPDSSemanticFieldChange& Change : SemanticDiff.ChangedFields)
    {
        ChangedSemanticFields.Add(
            Change.DomainId + TEXT("/") + Change.RecordId
            + TEXT(" :: ") + Change.FieldName);
    }

    AppendStringPreview(
        Output,
        TEXT("Semantic records añadidos"),
        AddedSemanticRecords,
        MaxEntriesPerCategory);
    AppendStringPreview(
        Output,
        TEXT("Semantic records eliminados"),
        RemovedSemanticRecords,
        MaxEntriesPerCategory);
    AppendStringPreview(
        Output,
        TEXT("Semantic fields modificados"),
        ChangedSemanticFields,
        MaxEntriesPerCategory);
    AppendStringPreview(
        Output,
        TEXT("Semantic domains no comparables"),
        SemanticDiff.NonComparableDomains,
        MaxEntriesPerCategory);

    // Los issues se muestran una sola vez mediante FPDSOperationResult::ToMultilineText().
    return Output;
}

bool PDSSnapshotDiff::ParseSnapshotJson(
    const FString& JsonText,
    const FString& SourcePath,
    FPDSSnapshotDocument& OutDocument,
    TArray<FPDSIssue>& OutIssues)
{
    OutDocument = FPDSSnapshotDocument();
    OutDocument.SourcePath = SourcePath;

    TSharedPtr<FJsonObject> RootObject;
    const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(JsonText);
    if (!FJsonSerializer::Deserialize(Reader, RootObject) || !RootObject.IsValid())
    {
        OutIssues.Add({
            EPDSIssueSeverity::Error,
            TEXT("PDS.SnapshotDiff.InvalidJson"),
            SourcePath,
            TEXT("El archivo no contiene un objeto JSON válido.")
        });
        return false;
    }

    OutDocument.SchemaVersion = GetSnapshotDiffOptionalString(RootObject, TEXT("schemaVersion"));
    OutDocument.GeneratedAtUtc = GetSnapshotDiffOptionalString(RootObject, TEXT("generatedAtUtc"));

    const TSharedPtr<FJsonObject> ProjectObject =
        GetOptionalObject(RootObject, TEXT("project"));
    OutDocument.ProjectName = GetSnapshotDiffOptionalString(ProjectObject, TEXT("name"));
    OutDocument.EngineVersion = GetSnapshotDiffOptionalString(ProjectObject, TEXT("engineVersion"));

    const TArray<TSharedPtr<FJsonValue>>* Inventory =
        GetOptionalArray(RootObject, TEXT("assetInventory"));
    if (!Inventory)
    {
        OutIssues.Add({
            EPDSIssueSeverity::Error,
            TEXT("PDS.SnapshotDiff.MissingInventory"),
            SourcePath,
            TEXT("El snapshot no contiene assetInventory y no puede compararse.")
        });
        return false;
    }

    for (const TSharedPtr<FJsonValue>& Value : *Inventory)
    {
        const TSharedPtr<FJsonObject> AssetObject = Value.IsValid()
            ? Value->AsObject()
            : nullptr;
        if (!AssetObject.IsValid())
        {
            continue;
        }

        FPDSSnapshotAssetRecord Record;
        Record.ObjectPath = GetSnapshotDiffOptionalString(AssetObject, TEXT("objectPath"));
        Record.PackageName = GetSnapshotDiffOptionalString(AssetObject, TEXT("packageName"));
        Record.PackagePath = GetSnapshotDiffOptionalString(AssetObject, TEXT("packagePath"));
        Record.ClassPath = GetSnapshotDiffOptionalString(AssetObject, TEXT("classPath"));
        Record.Origin = ParseOrigin(GetSnapshotDiffOptionalString(AssetObject, TEXT("origin")));

        if (Record.ObjectPath.IsEmpty())
        {
            Record.ObjectPath = Record.PackageName;
        }
        if (!Record.ObjectPath.IsEmpty())
        {
            const FString RecordKey = Record.ObjectPath;
            OutDocument.AssetsByObjectPath.Add(RecordKey, MoveTemp(Record));
        }
    }

    if (const TArray<TSharedPtr<FJsonValue>>* Tags =
        GetOptionalArray(RootObject, TEXT("gameplayTags")))
    {
        for (const TSharedPtr<FJsonValue>& Value : *Tags)
        {
            if (Value.IsValid() && Value->Type == EJson::String)
            {
                OutDocument.GameplayTags.Add(Value->AsString());
            }
        }
    }

    if (const TArray<TSharedPtr<FJsonValue>>* Montages =
        GetOptionalArray(RootObject, TEXT("montages")))
    {
        for (const TSharedPtr<FJsonValue>& Value : *Montages)
        {
            const TSharedPtr<FJsonObject> MontageObject = Value.IsValid()
                ? Value->AsObject()
                : nullptr;
            const FString Path = GetSnapshotDiffOptionalString(MontageObject, TEXT("path"));
            if (!Path.IsEmpty())
            {
                OutDocument.MontagesByPath.Add(
                    Path,
                    FPDSSnapshotMontageRecord{ Path, BuildMontageFingerprint(MontageObject) });
            }
        }
    }

    if (!PDSSemanticSnapshotJson::ParseDomainsJson(
            RootObject,
            OutDocument.SemanticDomainsById,
            OutIssues))
    {
        return false;
    }

    const TSharedPtr<FJsonObject> ValidationObject =
        GetOptionalObject(RootObject, TEXT("validation"));
    if (ValidationObject.IsValid())
    {
        OutDocument.Validation.bIncluded =
            ValidationObject->HasTypedField<EJson::Boolean>(TEXT("included"))
            && ValidationObject->GetBoolField(TEXT("included"));
        OutDocument.Validation.ScopeId =
            GetSnapshotDiffOptionalString(ValidationObject, TEXT("scopeId"));
        OutDocument.Validation.NumValid =
            GetOptionalInt(ValidationObject, TEXT("numValid"));
        OutDocument.Validation.NumInvalid =
            GetOptionalInt(ValidationObject, TEXT("numInvalid"));
        OutDocument.Validation.NumWithWarnings =
            GetOptionalInt(ValidationObject, TEXT("numWithWarnings"));
    }

    return true;
}

FPDSSnapshotDiff PDSSnapshotDiff::Compare(
    const FPDSSnapshotDocument& Previous,
    const FPDSSnapshotDocument& Current)
{
    FPDSSnapshotDiff Diff;
    Diff.PreviousSnapshotPath = Previous.SourcePath;
    Diff.CurrentSnapshotPath = Current.SourcePath;
    Diff.PreviousGeneratedAtUtc = Previous.GeneratedAtUtc;
    Diff.CurrentGeneratedAtUtc = Current.GeneratedAtUtc;
    Diff.PreviousSchemaVersion = Previous.SchemaVersion;
    Diff.CurrentSchemaVersion = Current.SchemaVersion;
    Diff.ProjectName = Current.ProjectName.IsEmpty()
        ? Previous.ProjectName
        : Current.ProjectName;

    Diff.PreviousAssetCount = Previous.AssetsByObjectPath.Num();
    Diff.CurrentAssetCount = Current.AssetsByObjectPath.Num();
    Diff.PreviousGameplayTagCount = Previous.GameplayTags.Num();
    Diff.CurrentGameplayTagCount = Current.GameplayTags.Num();
    Diff.PreviousMontageCount = Previous.MontagesByPath.Num();
    Diff.CurrentMontageCount = Current.MontagesByPath.Num();
    Diff.SemanticDiff = PDSSemanticDiff::Compare(
        Previous.SemanticDomainsById,
        Current.SemanticDomainsById);
    Diff.Issues.Append(Diff.SemanticDiff.Issues);

    for (const TPair<FString, FPDSSnapshotAssetRecord>& Pair : Current.AssetsByObjectPath)
    {
        const FPDSSnapshotAssetRecord* PreviousAsset =
            Previous.AssetsByObjectPath.Find(Pair.Key);
        if (!PreviousAsset)
        {
            Diff.AddedAssets.Add(Pair.Value);
            continue;
        }

        const bool bPreviousOriginMissing =
            PreviousAsset->Origin == EPDSAssetOrigin::Unknown
            && Pair.Value.Origin != EPDSAssetOrigin::Unknown;
        if (bPreviousOriginMissing)
        {
            // En snapshots históricos, Unknown representa dato ausente, no un origen real.
            ++Diff.NumAssetsWithOriginNotComparable;
        }

        const bool bOriginChanged =
            PreviousAsset->Origin != EPDSAssetOrigin::Unknown
            && PreviousAsset->Origin != Pair.Value.Origin;

        if (PreviousAsset->ClassPath != Pair.Value.ClassPath
            || PreviousAsset->PackageName != Pair.Value.PackageName
            || bOriginChanged)
        {
            Diff.ChangedAssets.Add({
                Pair.Key,
                PreviousAsset->ClassPath,
                Pair.Value.ClassPath,
                PreviousAsset->Origin,
                Pair.Value.Origin
            });
        }
    }

    for (const TPair<FString, FPDSSnapshotAssetRecord>& Pair : Previous.AssetsByObjectPath)
    {
        if (!Current.AssetsByObjectPath.Contains(Pair.Key))
        {
            Diff.RemovedAssets.Add(Pair.Value);
        }
    }

    for (const FString& Tag : Current.GameplayTags)
    {
        if (!Previous.GameplayTags.Contains(Tag))
        {
            Diff.AddedGameplayTags.Add(Tag);
        }
    }
    for (const FString& Tag : Previous.GameplayTags)
    {
        if (!Current.GameplayTags.Contains(Tag))
        {
            Diff.RemovedGameplayTags.Add(Tag);
        }
    }

    for (const TPair<FString, FPDSSnapshotMontageRecord>& Pair : Current.MontagesByPath)
    {
        const FPDSSnapshotMontageRecord* PreviousMontage =
            Previous.MontagesByPath.Find(Pair.Key);
        if (!PreviousMontage)
        {
            Diff.AddedMontages.Add(Pair.Key);
        }
        else if (PreviousMontage->Fingerprint != Pair.Value.Fingerprint)
        {
            Diff.ChangedMontages.Add(Pair.Key);
        }
    }
    for (const TPair<FString, FPDSSnapshotMontageRecord>& Pair : Previous.MontagesByPath)
    {
        if (!Current.MontagesByPath.Contains(Pair.Key))
        {
            Diff.RemovedMontages.Add(Pair.Key);
        }
    }

    Diff.bPreviousValidationIncluded = Previous.Validation.bIncluded;
    Diff.bCurrentValidationIncluded = Current.Validation.bIncluded;
    Diff.PreviousInvalidCount = Previous.Validation.NumInvalid;
    Diff.CurrentInvalidCount = Current.Validation.NumInvalid;
    Diff.PreviousWarningCount = Previous.Validation.NumWithWarnings;
    Diff.CurrentWarningCount = Current.Validation.NumWithWarnings;

    if (Diff.NumAssetsWithOriginNotComparable > 0)
    {
        Diff.Issues.Add({
            EPDSIssueSeverity::Info,
            TEXT("PDS.SnapshotDiff.OriginNotComparable"),
            Previous.SourcePath,
            FString::Printf(
                TEXT("El snapshot anterior carece de origin comparable para %d assets emparejados. Esas transiciones Unknown → clasificado se omitieron para evitar falsos cambios masivos."),
                Diff.NumAssetsWithOriginNotComparable)
        });
    }

    if (!Previous.ProjectName.IsEmpty()
        && !Current.ProjectName.IsEmpty()
        && !Previous.ProjectName.Equals(Current.ProjectName, ESearchCase::IgnoreCase))
    {
        Diff.Issues.Add({
            EPDSIssueSeverity::Warning,
            TEXT("PDS.SnapshotDiff.ProjectMismatch"),
            Current.SourcePath,
            FString::Printf(
                TEXT("Los snapshots pertenecen a proyectos distintos: %s y %s."),
                *Previous.ProjectName,
                *Current.ProjectName)
        });
    }

    if (Previous.SchemaVersion != Current.SchemaVersion)
    {
        Diff.Issues.Add({
            EPDSIssueSeverity::Info,
            TEXT("PDS.SnapshotDiff.SchemaChanged"),
            Current.SourcePath,
            FString::Printf(
                TEXT("Se compararon esquemas distintos (%s → %s) usando el subconjunto compatible."),
                *Previous.SchemaVersion,
                *Current.SchemaVersion)
        });
    }

    SortRecordsByObjectPath(Diff.AddedAssets);
    SortRecordsByObjectPath(Diff.RemovedAssets);
    Diff.ChangedAssets.Sort([](
        const FPDSSnapshotAssetChange& A,
        const FPDSSnapshotAssetChange& B)
    {
        return A.ObjectPath < B.ObjectPath;
    });
    SortStrings(Diff.AddedGameplayTags);
    SortStrings(Diff.RemovedGameplayTags);
    SortStrings(Diff.AddedMontages);
    SortStrings(Diff.RemovedMontages);
    SortStrings(Diff.ChangedMontages);

    return Diff;
}
