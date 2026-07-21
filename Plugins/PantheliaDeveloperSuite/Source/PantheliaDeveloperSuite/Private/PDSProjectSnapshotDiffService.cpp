#include "PDSProjectSnapshotDiffService.h"

#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PDSAssetUtilities.h"
#include "PDSDeveloperSettings.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
    FString EscapeMarkdownCell(FString Value)
    {
        Value.ReplaceInline(TEXT("|"), TEXT("\\|"));
        Value.ReplaceInline(TEXT("\r"), TEXT(" "));
        Value.ReplaceInline(TEXT("\n"), TEXT(" "));
        return Value;
    }

    FString OriginToJson(const EPDSAssetOrigin Origin)
    {
        return PDSDeveloperTypes::AssetOriginToString(Origin);
    }

    TSharedRef<FJsonObject> AssetRecordToJson(
        const FPDSSnapshotAssetRecord& Asset)
    {
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("objectPath"), Asset.ObjectPath);
        Json->SetStringField(TEXT("packageName"), Asset.PackageName);
        Json->SetStringField(TEXT("packagePath"), Asset.PackagePath);
        Json->SetStringField(TEXT("classPath"), Asset.ClassPath);
        Json->SetStringField(TEXT("origin"), OriginToJson(Asset.Origin));
        return Json;
    }

    TArray<TSharedPtr<FJsonValue>> StringsToJson(const TArray<FString>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> JsonValues;
        JsonValues.Reserve(Values.Num());
        for (const FString& Value : Values)
        {
            JsonValues.Add(MakeShared<FJsonValueString>(Value));
        }
        return JsonValues;
    }

    int32 CountModifiedSemanticRecords(const FPDSSemanticDiff& SemanticDiff)
    {
        TSet<FString> ModifiedRecordKeys;
        for (const FPDSSemanticFieldChange& Change : SemanticDiff.ChangedFields)
        {
            ModifiedRecordKeys.Add(
                Change.DomainId + TEXT("\n") + Change.RecordId);
        }
        return ModifiedRecordKeys.Num();
    }

    void AppendSemanticMarkdown(
        FString& Markdown,
        const FPDSSemanticDiff& SemanticDiff)
    {
        const int32 ModifiedRecordCount =
            CountModifiedSemanticRecords(SemanticDiff);

        Markdown += TEXT("\n## Semantic summary\n\n");
        Markdown += TEXT("| Metric | Previous | Current | Delta |\n");
        Markdown += TEXT("|---|---:|---:|---:|\n");
        Markdown += FString::Printf(
            TEXT("| Semantic records | %d | %d | %+d |\n"),
            SemanticDiff.PreviousRecordCount,
            SemanticDiff.CurrentRecordCount,
            SemanticDiff.CurrentRecordCount - SemanticDiff.PreviousRecordCount);
        Markdown += FString::Printf(
            TEXT("| Added records | 0 | %d | %+d |\n"),
            SemanticDiff.AddedRecords.Num(),
            SemanticDiff.AddedRecords.Num());
        Markdown += FString::Printf(
            TEXT("| Removed records | %d | 0 | %d |\n"),
            SemanticDiff.RemovedRecords.Num(),
            -SemanticDiff.RemovedRecords.Num());
        Markdown += FString::Printf(
            TEXT("| Modified records | 0 | %d | %+d |\n"),
            ModifiedRecordCount,
            ModifiedRecordCount);
        Markdown += FString::Printf(
            TEXT("| Changed fields | 0 | %d | %+d |\n"),
            SemanticDiff.ChangedFields.Num(),
            SemanticDiff.ChangedFields.Num());
        Markdown += FString::Printf(
            TEXT("| Non-comparable domains | 0 | %d | %+d |\n\n"),
            SemanticDiff.NonComparableDomains.Num(),
            SemanticDiff.NonComparableDomains.Num());

        Markdown += FString::Printf(
            TEXT("## Semantic records added (%d)\n\n"),
            SemanticDiff.AddedRecords.Num());
        Markdown += TEXT("| Domain | Record | Display name | Kind | Source asset |\n");
        Markdown += TEXT("|---|---|---|---|---|\n");
        for (const FPDSSemanticRecordChange& Change : SemanticDiff.AddedRecords)
        {
            Markdown += FString::Printf(
                TEXT("| `%s` | `%s` | %s | %s | `%s` |\n"),
                *EscapeMarkdownCell(Change.DomainId),
                *EscapeMarkdownCell(Change.RecordId),
                *EscapeMarkdownCell(Change.DisplayName),
                *EscapeMarkdownCell(Change.Kind),
                *EscapeMarkdownCell(Change.SourceAssetPath));
        }

        Markdown += FString::Printf(
            TEXT("\n## Semantic records removed (%d)\n\n"),
            SemanticDiff.RemovedRecords.Num());
        Markdown += TEXT("| Domain | Record | Display name | Kind | Source asset |\n");
        Markdown += TEXT("|---|---|---|---|---|\n");
        for (const FPDSSemanticRecordChange& Change : SemanticDiff.RemovedRecords)
        {
            Markdown += FString::Printf(
                TEXT("| `%s` | `%s` | %s | %s | `%s` |\n"),
                *EscapeMarkdownCell(Change.DomainId),
                *EscapeMarkdownCell(Change.RecordId),
                *EscapeMarkdownCell(Change.DisplayName),
                *EscapeMarkdownCell(Change.Kind),
                *EscapeMarkdownCell(Change.SourceAssetPath));
        }

        Markdown += FString::Printf(
            TEXT("\n## Semantic fields changed (%d)\n\n"),
            SemanticDiff.ChangedFields.Num());
        Markdown += TEXT("| Domain | Record | Field | Previous | Current | Source asset |\n");
        Markdown += TEXT("|---|---|---|---|---|---|\n");
        for (const FPDSSemanticFieldChange& Change : SemanticDiff.ChangedFields)
        {
            Markdown += FString::Printf(
                TEXT("| `%s` | `%s` | `%s` | `%s` | `%s` | `%s` |\n"),
                *EscapeMarkdownCell(Change.DomainId),
                *EscapeMarkdownCell(Change.RecordId),
                *EscapeMarkdownCell(Change.FieldName),
                *EscapeMarkdownCell(Change.PreviousValue),
                *EscapeMarkdownCell(Change.CurrentValue),
                *EscapeMarkdownCell(Change.SourceAssetPath));
        }

        Markdown += FString::Printf(
            TEXT("\n## Semantic domains not comparable (%d)\n\n"),
            SemanticDiff.NonComparableDomains.Num());
        for (const FString& DomainId : SemanticDiff.NonComparableDomains)
        {
            Markdown += TEXT("- `")
                + EscapeMarkdownCell(DomainId)
                + TEXT("`\n");
        }
    }

    TSharedRef<FJsonObject> SemanticRecordChangeToJson(
        const FPDSSemanticRecordChange& Change)
    {
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("domainId"), Change.DomainId);
        Json->SetStringField(TEXT("recordId"), Change.RecordId);
        Json->SetStringField(TEXT("displayName"), Change.DisplayName);
        Json->SetStringField(TEXT("kind"), Change.Kind);
        Json->SetStringField(TEXT("sourceAssetPath"), Change.SourceAssetPath);
        return Json;
    }

    TSharedRef<FJsonObject> SemanticFieldChangeToJson(
        const FPDSSemanticFieldChange& Change)
    {
        TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
        Json->SetStringField(TEXT("domainId"), Change.DomainId);
        Json->SetStringField(TEXT("recordId"), Change.RecordId);
        Json->SetStringField(TEXT("displayName"), Change.DisplayName);
        Json->SetStringField(TEXT("sourceAssetPath"), Change.SourceAssetPath);
        Json->SetStringField(TEXT("fieldName"), Change.FieldName);
        Json->SetStringField(TEXT("previousValue"), Change.PreviousValue);
        Json->SetStringField(TEXT("currentValue"), Change.CurrentValue);
        return Json;
    }

    TSharedRef<FJsonObject> BuildSemanticDiffJson(
        const FPDSSemanticDiff& SemanticDiff)
    {
        TSharedRef<FJsonObject> Semantic = MakeShared<FJsonObject>();
        TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
        Summary->SetNumberField(
            TEXT("previousRecordCount"),
            SemanticDiff.PreviousRecordCount);
        Summary->SetNumberField(
            TEXT("currentRecordCount"),
            SemanticDiff.CurrentRecordCount);
        Summary->SetNumberField(
            TEXT("addedRecordCount"),
            SemanticDiff.AddedRecords.Num());
        Summary->SetNumberField(
            TEXT("removedRecordCount"),
            SemanticDiff.RemovedRecords.Num());
        Summary->SetNumberField(
            TEXT("modifiedRecordCount"),
            CountModifiedSemanticRecords(SemanticDiff));
        Summary->SetNumberField(
            TEXT("changedFieldCount"),
            SemanticDiff.ChangedFields.Num());
        Summary->SetNumberField(
            TEXT("nonComparableDomainCount"),
            SemanticDiff.NonComparableDomains.Num());
        Semantic->SetObjectField(TEXT("summary"), Summary);

        TArray<TSharedPtr<FJsonValue>> Added;
        for (const FPDSSemanticRecordChange& Change : SemanticDiff.AddedRecords)
        {
            Added.Add(MakeShared<FJsonValueObject>(
                SemanticRecordChangeToJson(Change)));
        }
        Semantic->SetArrayField(TEXT("addedRecords"), Added);

        TArray<TSharedPtr<FJsonValue>> Removed;
        for (const FPDSSemanticRecordChange& Change : SemanticDiff.RemovedRecords)
        {
            Removed.Add(MakeShared<FJsonValueObject>(
                SemanticRecordChangeToJson(Change)));
        }
        Semantic->SetArrayField(TEXT("removedRecords"), Removed);

        TArray<TSharedPtr<FJsonValue>> Fields;
        for (const FPDSSemanticFieldChange& Change : SemanticDiff.ChangedFields)
        {
            Fields.Add(MakeShared<FJsonValueObject>(
                SemanticFieldChangeToJson(Change)));
        }
        Semantic->SetArrayField(TEXT("changedFields"), Fields);

        TArray<TSharedPtr<FJsonValue>> NonComparable;
        for (const FString& DomainId : SemanticDiff.NonComparableDomains)
        {
            NonComparable.Add(MakeShared<FJsonValueString>(DomainId));
        }
        Semantic->SetArrayField(
            TEXT("nonComparableDomains"),
            NonComparable);
        return Semantic;
    }

    FString BuildMarkdown(const FPDSSnapshotDiff& Diff)
    {
        FString Markdown = TEXT("# Panthelia Developer Suite — Snapshot Diff\n\n");
        Markdown += FString::Printf(
            TEXT("Generated UTC: `%s`\n\n"),
            *FDateTime::UtcNow().ToIso8601());
        Markdown += FString::Printf(TEXT("- Project: `%s`\n"), *Diff.ProjectName);
        Markdown += FString::Printf(TEXT("- Previous: `%s`\n"), *Diff.PreviousSnapshotPath);
        Markdown += FString::Printf(TEXT("- Current: `%s`\n"), *Diff.CurrentSnapshotPath);
        Markdown += FString::Printf(
            TEXT("- Schemas: `%s` → `%s`\n\n"),
            *Diff.PreviousSchemaVersion,
            *Diff.CurrentSchemaVersion);

        Markdown += TEXT("## Summary\n\n");
        Markdown += TEXT("| Metric | Previous | Current | Delta |\n");
        Markdown += TEXT("|---|---:|---:|---:|\n");
        Markdown += FString::Printf(
            TEXT("| Assets | %d | %d | %+d |\n"),
            Diff.PreviousAssetCount,
            Diff.CurrentAssetCount,
            Diff.CurrentAssetCount - Diff.PreviousAssetCount);
        Markdown += FString::Printf(
            TEXT("| Gameplay Tags | %d | %d | %+d |\n"),
            Diff.PreviousGameplayTagCount,
            Diff.CurrentGameplayTagCount,
            Diff.CurrentGameplayTagCount - Diff.PreviousGameplayTagCount);
        Markdown += FString::Printf(
            TEXT("| Montages | %d | %d | %+d |\n"),
            Diff.PreviousMontageCount,
            Diff.CurrentMontageCount,
            Diff.CurrentMontageCount - Diff.PreviousMontageCount);
        Markdown += FString::Printf(
            TEXT("| Invalid assets | %d | %d | %+d |\n"),
            Diff.PreviousInvalidCount,
            Diff.CurrentInvalidCount,
            Diff.CurrentInvalidCount - Diff.PreviousInvalidCount);
        Markdown += FString::Printf(
            TEXT("| Validation warnings | %d | %d | %+d |\n\n"),
            Diff.PreviousWarningCount,
            Diff.CurrentWarningCount,
            Diff.CurrentWarningCount - Diff.PreviousWarningCount);
        if (Diff.NumAssetsWithOriginNotComparable > 0)
        {
            Markdown += FString::Printf(
                TEXT("Origin comparisons omitted for historical compatibility: `%d` assets.\n\n"),
                Diff.NumAssetsWithOriginNotComparable);
        }

        Markdown += FString::Printf(
            TEXT("## Assets added (%d)\n\n"),
            Diff.AddedAssets.Num());
        for (const FPDSSnapshotAssetRecord& Asset : Diff.AddedAssets)
        {
            Markdown += FString::Printf(
                TEXT("- `%s` — `%s` — %s\n"),
                *Asset.ObjectPath,
                *Asset.ClassPath,
                *OriginToJson(Asset.Origin));
        }

        Markdown += FString::Printf(
            TEXT("\n## Assets removed (%d)\n\n"),
            Diff.RemovedAssets.Num());
        for (const FPDSSnapshotAssetRecord& Asset : Diff.RemovedAssets)
        {
            Markdown += FString::Printf(
                TEXT("- `%s` — `%s` — %s\n"),
                *Asset.ObjectPath,
                *Asset.ClassPath,
                *OriginToJson(Asset.Origin));
        }

        Markdown += FString::Printf(
            TEXT("\n## Assets changed (%d)\n\n"),
            Diff.ChangedAssets.Num());
        Markdown += TEXT("| Asset | Previous class | Current class | Previous origin | Current origin |\n");
        Markdown += TEXT("|---|---|---|---|---|\n");
        for (const FPDSSnapshotAssetChange& Change : Diff.ChangedAssets)
        {
            Markdown += FString::Printf(
                TEXT("| `%s` | `%s` | `%s` | %s | %s |\n"),
                *EscapeMarkdownCell(Change.ObjectPath),
                *EscapeMarkdownCell(Change.PreviousClassPath),
                *EscapeMarkdownCell(Change.CurrentClassPath),
                *OriginToJson(Change.PreviousOrigin),
                *OriginToJson(Change.CurrentOrigin));
        }

        const auto AppendList = [&Markdown](
            const TCHAR* Heading,
            const TArray<FString>& Values)
        {
            Markdown += FString::Printf(TEXT("\n## %s (%d)\n\n"), Heading, Values.Num());
            for (const FString& Value : Values)
            {
                Markdown += TEXT("- `") + Value + TEXT("`\n");
            }
        };

        AppendList(TEXT("Gameplay Tags added"), Diff.AddedGameplayTags);
        AppendList(TEXT("Gameplay Tags removed"), Diff.RemovedGameplayTags);
        AppendList(TEXT("Montages added"), Diff.AddedMontages);
        AppendList(TEXT("Montages removed"), Diff.RemovedMontages);
        AppendList(TEXT("Montages changed"), Diff.ChangedMontages);

        AppendSemanticMarkdown(Markdown, Diff.SemanticDiff);

        Markdown += FString::Printf(
            TEXT("\n## Issues (%d)\n\n"),
            Diff.Issues.Num());
        for (const FPDSIssue& Issue : Diff.Issues)
        {
            Markdown += TEXT("- ") + Issue.ToDisplayString(false) + LINE_TERMINATOR;
        }

        return Markdown;
    }

    FString BuildJson(const FPDSSnapshotDiff& Diff)
    {
        TSharedRef<FJsonObject> Root = MakeShared<FJsonObject>();
        Root->SetStringField(TEXT("schemaVersion"), TEXT("0.4.0-alpha1-diff"));
        Root->SetStringField(TEXT("generatedAtUtc"), FDateTime::UtcNow().ToIso8601());
        Root->SetStringField(TEXT("projectName"), Diff.ProjectName);

        TSharedRef<FJsonObject> Comparison = MakeShared<FJsonObject>();
        Comparison->SetStringField(TEXT("previousSnapshotPath"), Diff.PreviousSnapshotPath);
        Comparison->SetStringField(TEXT("currentSnapshotPath"), Diff.CurrentSnapshotPath);
        Comparison->SetStringField(TEXT("previousGeneratedAtUtc"), Diff.PreviousGeneratedAtUtc);
        Comparison->SetStringField(TEXT("currentGeneratedAtUtc"), Diff.CurrentGeneratedAtUtc);
        Comparison->SetStringField(TEXT("previousSchemaVersion"), Diff.PreviousSchemaVersion);
        Comparison->SetStringField(TEXT("currentSchemaVersion"), Diff.CurrentSchemaVersion);
        Root->SetObjectField(TEXT("comparison"), Comparison);

        TSharedRef<FJsonObject> Summary = MakeShared<FJsonObject>();
        Summary->SetNumberField(TEXT("previousAssetCount"), Diff.PreviousAssetCount);
        Summary->SetNumberField(TEXT("currentAssetCount"), Diff.CurrentAssetCount);
        Summary->SetNumberField(TEXT("addedAssetCount"), Diff.AddedAssets.Num());
        Summary->SetNumberField(TEXT("removedAssetCount"), Diff.RemovedAssets.Num());
        Summary->SetNumberField(TEXT("changedAssetCount"), Diff.ChangedAssets.Num());
        Summary->SetNumberField(TEXT("originNotComparableAssetCount"), Diff.NumAssetsWithOriginNotComparable);
        Summary->SetNumberField(TEXT("addedGameplayTagCount"), Diff.AddedGameplayTags.Num());
        Summary->SetNumberField(TEXT("removedGameplayTagCount"), Diff.RemovedGameplayTags.Num());
        Summary->SetNumberField(TEXT("addedMontageCount"), Diff.AddedMontages.Num());
        Summary->SetNumberField(TEXT("removedMontageCount"), Diff.RemovedMontages.Num());
        Summary->SetNumberField(TEXT("changedMontageCount"), Diff.ChangedMontages.Num());
        Summary->SetBoolField(TEXT("hasChanges"), Diff.HasChanges());
        Root->SetObjectField(TEXT("summary"), Summary);

        TArray<TSharedPtr<FJsonValue>> AddedAssets;
        for (const FPDSSnapshotAssetRecord& Asset : Diff.AddedAssets)
        {
            AddedAssets.Add(MakeShared<FJsonValueObject>(AssetRecordToJson(Asset)));
        }
        TArray<TSharedPtr<FJsonValue>> RemovedAssets;
        for (const FPDSSnapshotAssetRecord& Asset : Diff.RemovedAssets)
        {
            RemovedAssets.Add(MakeShared<FJsonValueObject>(AssetRecordToJson(Asset)));
        }
        TArray<TSharedPtr<FJsonValue>> ChangedAssets;
        for (const FPDSSnapshotAssetChange& Change : Diff.ChangedAssets)
        {
            TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
            Json->SetStringField(TEXT("objectPath"), Change.ObjectPath);
            Json->SetStringField(TEXT("previousClassPath"), Change.PreviousClassPath);
            Json->SetStringField(TEXT("currentClassPath"), Change.CurrentClassPath);
            Json->SetStringField(TEXT("previousOrigin"), OriginToJson(Change.PreviousOrigin));
            Json->SetStringField(TEXT("currentOrigin"), OriginToJson(Change.CurrentOrigin));
            ChangedAssets.Add(MakeShared<FJsonValueObject>(Json));
        }

        TSharedRef<FJsonObject> Assets = MakeShared<FJsonObject>();
        Assets->SetArrayField(TEXT("added"), AddedAssets);
        Assets->SetArrayField(TEXT("removed"), RemovedAssets);
        Assets->SetArrayField(TEXT("changed"), ChangedAssets);
        Root->SetObjectField(TEXT("assets"), Assets);

        TSharedRef<FJsonObject> Tags = MakeShared<FJsonObject>();
        Tags->SetArrayField(TEXT("added"), StringsToJson(Diff.AddedGameplayTags));
        Tags->SetArrayField(TEXT("removed"), StringsToJson(Diff.RemovedGameplayTags));
        Root->SetObjectField(TEXT("gameplayTags"), Tags);

        TSharedRef<FJsonObject> Montages = MakeShared<FJsonObject>();
        Montages->SetArrayField(TEXT("added"), StringsToJson(Diff.AddedMontages));
        Montages->SetArrayField(TEXT("removed"), StringsToJson(Diff.RemovedMontages));
        Montages->SetArrayField(TEXT("changed"), StringsToJson(Diff.ChangedMontages));
        Root->SetObjectField(TEXT("montages"), Montages);
        Root->SetObjectField(
            TEXT("semantic"),
            BuildSemanticDiffJson(Diff.SemanticDiff));

        TSharedRef<FJsonObject> Validation = MakeShared<FJsonObject>();
        Validation->SetBoolField(TEXT("previousIncluded"), Diff.bPreviousValidationIncluded);
        Validation->SetBoolField(TEXT("currentIncluded"), Diff.bCurrentValidationIncluded);
        Validation->SetNumberField(TEXT("previousInvalid"), Diff.PreviousInvalidCount);
        Validation->SetNumberField(TEXT("currentInvalid"), Diff.CurrentInvalidCount);
        Validation->SetNumberField(TEXT("previousWarnings"), Diff.PreviousWarningCount);
        Validation->SetNumberField(TEXT("currentWarnings"), Diff.CurrentWarningCount);
        Root->SetObjectField(TEXT("validation"), Validation);

        TArray<TSharedPtr<FJsonValue>> Issues;
        for (const FPDSIssue& Issue : Diff.Issues)
        {
            TSharedRef<FJsonObject> Json = MakeShared<FJsonObject>();
            Json->SetStringField(TEXT("severity"), PDSDeveloperTypes::SeverityToString(Issue.Severity));
            Json->SetStringField(TEXT("ruleId"), Issue.RuleId);
            Json->SetStringField(TEXT("assetPath"), Issue.AssetPath);
            Json->SetStringField(TEXT("message"), Issue.Message);
            Issues.Add(MakeShared<FJsonValueObject>(Json));
        }
        Root->SetArrayField(TEXT("issues"), Issues);

        FString Serialized;
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Serialized);
        return FJsonSerializer::Serialize(Root, Writer) ? Serialized : FString();
    }

    bool LoadSnapshotDocument(
        const FString& Path,
        FPDSSnapshotDocument& OutDocument,
        TArray<FPDSIssue>& OutIssues)
    {
        FString JsonText;
        if (!FFileHelper::LoadFileToString(JsonText, *Path))
        {
            OutIssues.Add({
                EPDSIssueSeverity::Error,
                TEXT("PDS.SnapshotDiff.LoadFailed"),
                Path,
                TEXT("No fue posible leer el snapshot JSON.")
            });
            return false;
        }
        return PDSSnapshotDiff::ParseSnapshotJson(JsonText, Path, OutDocument, OutIssues);
    }
}

FString FPDSProjectSnapshotDiffService::GetSnapshotsDirectory()
{
    return FPaths::Combine(
        PDSAssetUtilities::GetSuiteSavedDirectory(),
        TEXT("Snapshots"));
}

FString FPDSProjectSnapshotDiffService::GetBaselineSnapshotPath()
{
    return FPaths::Combine(GetSnapshotsDirectory(), TEXT("baseline.json"));
}

TArray<FString> FPDSProjectSnapshotDiffService::FindTimestampedSnapshotPathsNewestFirst()
{
    TArray<FString> FileNames;
    const FString Directory = GetSnapshotsDirectory();
    IFileManager::Get().FindFiles(
        FileNames,
        *Directory,
        TEXT("json"));

    TArray<FString> Paths;
    for (const FString& FileName : FileNames)
    {
        // El prefijo excluye por contrato latest.json y baseline.json.
        if (FileName.StartsWith(TEXT("PantheliaSnapshot_"), ESearchCase::IgnoreCase))
        {
            Paths.Add(FPaths::Combine(Directory, FileName));
        }
    }

    Paths.Sort([](const FString& A, const FString& B)
    {
        return FPaths::GetCleanFilename(A) > FPaths::GetCleanFilename(B);
    });
    return Paths;
}

FPDSOperationResult FPDSProjectSnapshotDiffService::CompareLatestTwoSnapshots() const
{
    const TArray<FString> Paths = FindTimestampedSnapshotPathsNewestFirst();
    if (Paths.Num() < 2)
    {
        FPDSOperationResult Result;
        Result.Summary = TEXT("Se necesitan al menos dos snapshots timestamped para comparar.");
        Result.Issues.Add({
            EPDSIssueSeverity::Warning,
            TEXT("PDS.SnapshotDiff.NotEnoughSnapshots"),
            GetSnapshotsDirectory(),
            TEXT("Exporta otro Project Snapshot y vuelve a intentar.")
        });
        return Result;
    }

    return CompareSnapshotFiles(
        Paths[1],
        Paths[0],
        TEXT("Latest Two Snapshots"));
}

FPDSOperationResult FPDSProjectSnapshotDiffService::CompareLatestSnapshotWithBaseline() const
{
    const FString BaselinePath = GetBaselineSnapshotPath();
    const FString LatestPath = FPaths::Combine(GetSnapshotsDirectory(), TEXT("latest.json"));

    if (!IFileManager::Get().FileExists(*BaselinePath))
    {
        FPDSOperationResult Result;
        Result.Summary = TEXT("No existe baseline.json. Usa Set Latest Snapshot as Baseline primero.");
        Result.Issues.Add({
            EPDSIssueSeverity::Warning,
            TEXT("PDS.SnapshotDiff.BaselineMissing"),
            BaselinePath,
            TEXT("La comparación no se ejecutó.")
        });
        return Result;
    }

    if (!IFileManager::Get().FileExists(*LatestPath))
    {
        FPDSOperationResult Result;
        Result.Summary = TEXT("No existe latest.json. Exporta un Project Snapshot primero.");
        Result.Issues.Add({
            EPDSIssueSeverity::Warning,
            TEXT("PDS.SnapshotDiff.LatestMissing"),
            LatestPath,
            TEXT("La comparación no se ejecutó.")
        });
        return Result;
    }

    return CompareSnapshotFiles(
        BaselinePath,
        LatestPath,
        TEXT("Baseline vs Latest"));
}

FPDSOperationResult FPDSProjectSnapshotDiffService::SetLatestSnapshotAsBaseline() const
{
    FPDSOperationResult Result;
    const FString LatestPath = FPaths::Combine(GetSnapshotsDirectory(), TEXT("latest.json"));
    const FString BaselinePath = GetBaselineSnapshotPath();

    FString JsonText;
    if (!FFileHelper::LoadFileToString(JsonText, *LatestPath))
    {
        Result.Summary = TEXT("No se pudo leer latest.json. Exporta un Project Snapshot primero.");
        Result.Issues.Add({
            EPDSIssueSeverity::Error,
            TEXT("PDS.SnapshotDiff.LatestLoadFailed"),
            LatestPath,
            Result.Summary
        });
        return Result;
    }

    FPDSSnapshotDocument Parsed;
    if (!PDSSnapshotDiff::ParseSnapshotJson(JsonText, LatestPath, Parsed, Result.Issues))
    {
        Result.Summary = TEXT("latest.json no es un snapshot compatible; baseline no fue reemplazado.");
        return Result;
    }

    const bool bHadPreviousBaseline = IFileManager::Get().FileExists(*BaselinePath);
    FString PreviousGeneratedAtUtc = TEXT("<ninguno>");
    int32 PreviousAssetCount = 0;

    if (bHadPreviousBaseline)
    {
        FPDSSnapshotDocument PreviousBaseline;
        TArray<FPDSIssue> PreviousBaselineIssues;
        if (LoadSnapshotDocument(BaselinePath, PreviousBaseline, PreviousBaselineIssues))
        {
            PreviousGeneratedAtUtc = PreviousBaseline.GeneratedAtUtc.IsEmpty()
                ? TEXT("<sin fecha>")
                : PreviousBaseline.GeneratedAtUtc;
            PreviousAssetCount = PreviousBaseline.AssetsByObjectPath.Num();
        }
        else
        {
            PreviousGeneratedAtUtc = TEXT("<baseline anterior no legible>");
            Result.Issues.Add({
                EPDSIssueSeverity::Warning,
                TEXT("PDS.SnapshotDiff.PreviousBaselineMetadataUnavailable"),
                BaselinePath,
                TEXT("El baseline anterior no pudo parsearse para mostrar su contexto, pero latest.json es válido y puede reemplazarlo de forma segura.")
            });
        }
    }

    Result.bSuccess = PDSAssetUtilities::SaveStringAtomically(JsonText, BaselinePath);
    if (Result.bSuccess)
    {
        const FString NewGeneratedAtUtc = Parsed.GeneratedAtUtc.IsEmpty()
            ? TEXT("<sin fecha>")
            : Parsed.GeneratedAtUtc;

        if (bHadPreviousBaseline)
        {
            Result.Summary = FString::Printf(
                TEXT("Baseline reemplazado.\nAnterior: %s (%d assets)\nNuevo: %s (%d assets)"),
                *PreviousGeneratedAtUtc,
                PreviousAssetCount,
                *NewGeneratedAtUtc,
                Parsed.AssetsByObjectPath.Num());
        }
        else
        {
            Result.Summary = FString::Printf(
                TEXT("Baseline creado.\nAnterior: <ninguno>\nNuevo: %s (%d assets)"),
                *NewGeneratedAtUtc,
                Parsed.AssetsByObjectPath.Num());
        }
    }
    else
    {
        Result.Summary = TEXT("No fue posible reemplazar baseline.json de forma atómica.");
    }

    if (Result.bSuccess)
    {
        Result.OutputPath = BaselinePath;
    }
    else
    {
        Result.Issues.Add({
            EPDSIssueSeverity::Error,
            TEXT("PDS.SnapshotDiff.BaselineSaveFailed"),
            BaselinePath,
            Result.Summary
        });
    }
    return Result;
}

FPDSOperationResult FPDSProjectSnapshotDiffService::CompareSnapshotFiles(
    const FString& PreviousSnapshotPath,
    const FString& CurrentSnapshotPath,
    const FString& ComparisonLabel) const
{
    FPDSOperationResult LoadResult;
    FPDSSnapshotDocument Previous;
    FPDSSnapshotDocument Current;

    if (!LoadSnapshotDocument(PreviousSnapshotPath, Previous, LoadResult.Issues)
        || !LoadSnapshotDocument(CurrentSnapshotPath, Current, LoadResult.Issues))
    {
        LoadResult.Summary = TEXT("No fue posible cargar ambos snapshots para comparación.");
        return LoadResult;
    }

    const FPDSSnapshotDiff Diff = PDSSnapshotDiff::Compare(Previous, Current);
    FPDSOperationResult PersistResult = PersistDiffReports(Diff, ComparisonLabel);
    PersistResult.Issues.Append(LoadResult.Issues);
    return PersistResult;
}

FPDSOperationResult FPDSProjectSnapshotDiffService::PersistDiffReports(
    const FPDSSnapshotDiff& Diff,
    const FString& ComparisonLabel,
    const FString& ReportsDirectoryOverride,
    const bool bWriteLatestAliases) const
{
    FPDSOperationResult Result;
    Result.Issues.Append(Diff.Issues);

    const FString Markdown = BuildMarkdown(Diff);
    const FString Json = BuildJson(Diff);
    if (Json.IsEmpty())
    {
        Result.Summary = TEXT("El diff se calculó, pero no fue posible serializar el informe JSON.");
        Result.Issues.Add({
            EPDSIssueSeverity::Error,
            TEXT("PDS.SnapshotDiff.SerializationFailed"),
            Diff.CurrentSnapshotPath,
            Result.Summary
        });
        return Result;
    }

    FString ReportsDirectory = ReportsDirectoryOverride.IsEmpty()
        ? FPaths::Combine(
            PDSAssetUtilities::GetSuiteSavedDirectory(),
            TEXT("Reports"))
        : FPaths::ConvertRelativePathToFull(ReportsDirectoryOverride);
    FPaths::NormalizeDirectoryName(ReportsDirectory);

    if (!PDSAssetUtilities::EnsureDirectoryExists(ReportsDirectory))
    {
        Result.Summary = TEXT("El diff se calculó, pero no fue posible preparar la carpeta de informes.");
        Result.Issues.Add({
            EPDSIssueSeverity::Error,
            TEXT("PDS.SnapshotDiff.ReportDirectoryFailed"),
            ReportsDirectory,
            Result.Summary
        });
        return Result;
    }

    const FString Timestamp = PDSAssetUtilities::MakeTimestampForFileName();
    const FString MarkdownPath = FPaths::Combine(
        ReportsDirectory,
        FString::Printf(TEXT("SnapshotDiff_%s.md"), *Timestamp));
    const FString JsonPath = FPaths::Combine(
        ReportsDirectory,
        FString::Printf(TEXT("SnapshotDiff_%s.json"), *Timestamp));
    const FString LatestMarkdownPath = FPaths::Combine(
        ReportsDirectory,
        TEXT("latest_snapshot_diff.md"));
    const FString LatestJsonPath = FPaths::Combine(
        ReportsDirectory,
        TEXT("latest_snapshot_diff.json"));

    const bool bTimestampedMarkdownSaved =
        FFileHelper::SaveStringToFile(Markdown, *MarkdownPath);
    const bool bTimestampedJsonSaved =
        FFileHelper::SaveStringToFile(Json, *JsonPath);

    bool bLatestMarkdownSaved = true;
    bool bLatestJsonSaved = true;
    if (bWriteLatestAliases)
    {
        bLatestMarkdownSaved =
            PDSAssetUtilities::SaveStringAtomically(Markdown, LatestMarkdownPath);
        bLatestJsonSaved =
            PDSAssetUtilities::SaveStringAtomically(Json, LatestJsonPath);
    }

    Result.bSuccess = bTimestampedMarkdownSaved
        && bTimestampedJsonSaved
        && bLatestMarkdownSaved
        && bLatestJsonSaved;

    if (bTimestampedMarkdownSaved)
    {
        Result.OutputPath = MarkdownPath;
    }
    if (bTimestampedJsonSaved)
    {
        Result.OutputJsonPath = JsonPath;
    }

    const UPDSDeveloperSettings* Settings = GetDefault<UPDSDeveloperSettings>();
    const int32 PreviewLimit = Settings
        ? Settings->SnapshotDiffPreviewLimit
        : 50;
    Result.Summary = ComparisonLabel + TEXT("\n\n")
        + Diff.ToDashboardText(PreviewLimit);

    if (!Result.bSuccess)
    {
        Result.Issues.Add({
            EPDSIssueSeverity::Error,
            TEXT("PDS.SnapshotDiff.SaveFailed"),
            ReportsDirectory,
            bWriteLatestAliases
                ? TEXT("El diff se calculó, pero uno o más informes timestamped/latest no pudieron guardarse.")
                : TEXT("El diff se calculó, pero uno o más informes timestamped no pudieron guardarse.")
        });
    }

    return Result;
}
