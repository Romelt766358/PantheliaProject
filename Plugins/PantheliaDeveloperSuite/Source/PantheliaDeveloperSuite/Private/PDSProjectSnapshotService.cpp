#include "PDSProjectSnapshotService.h"

#include "Animation/AnimMontage.h"
#include "Dom/JsonObject.h"
#include "GameplayTagsManager.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "PDSAssetUtilities.h"
#include "PDSMontageInspectorService.h"
#include "PDSDeveloperSettings.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"

namespace
{
    TSharedRef<FJsonObject> BuildSnapshotIssueJson(const FPDSIssue& Issue)
    {
        TSharedRef<FJsonObject> IssueJson = MakeShared<FJsonObject>();
        IssueJson->SetStringField(
            TEXT("severity"),
            PDSDeveloperTypes::SeverityToString(Issue.Severity));
        IssueJson->SetStringField(
            TEXT("origin"),
            PDSDeveloperTypes::AssetOriginToString(Issue.Origin));
        IssueJson->SetStringField(TEXT("ruleId"), Issue.RuleId);
        IssueJson->SetStringField(TEXT("assetPath"), Issue.AssetPath);
        IssueJson->SetStringField(TEXT("message"), Issue.Message);
        return IssueJson;
    }

    TSharedRef<FJsonObject> BuildValidationJson(
        const FPDSValidationSummary* Summary)
    {
        TSharedRef<FJsonObject> ValidationJson = MakeShared<FJsonObject>();

        if (!Summary)
        {
            ValidationJson->SetBoolField(TEXT("included"), false);
            return ValidationJson;
        }

        ValidationJson->SetBoolField(TEXT("included"), true);
        ValidationJson->SetStringField(TEXT("scopeId"), Summary->ScopeId);
        ValidationJson->SetStringField(TEXT("scopeLabel"), Summary->ScopeLabel);
        ValidationJson->SetStringField(
            TEXT("generatedAtUtc"),
            Summary->GeneratedAtUtc);
        ValidationJson->SetStringField(
            TEXT("markdownReportPath"),
            Summary->OutputPath);
        ValidationJson->SetStringField(
            TEXT("jsonReportPath"),
            Summary->OutputJsonPath);
        ValidationJson->SetNumberField(TEXT("numRequested"), Summary->NumRequested);
        ValidationJson->SetNumberField(TEXT("numChecked"), Summary->NumChecked);
        ValidationJson->SetNumberField(TEXT("numValid"), Summary->NumValid);
        ValidationJson->SetNumberField(TEXT("numInvalid"), Summary->NumInvalid);
        ValidationJson->SetNumberField(TEXT("numWithWarnings"), Summary->NumWithWarnings);
        ValidationJson->SetNumberField(TEXT("numNotValidated"), Summary->NumNotValidated);
        ValidationJson->SetNumberField(TEXT("numSkipped"), Summary->NumSkipped);
        ValidationJson->SetNumberField(
            TEXT("numUnableToValidate"),
            Summary->NumUnableToValidate);
        ValidationJson->SetBoolField(
            TEXT("assetLimitReached"),
            Summary->bAssetLimitReached);
        ValidationJson->SetBoolField(TEXT("cancelled"), Summary->bCancelled);

        TSharedRef<FJsonObject> OriginsJson = MakeShared<FJsonObject>();
        OriginsJson->SetNumberField(
            TEXT("pantheliaCore"),
            Summary->NumPantheliaCoreAssetsRequested);
        OriginsJson->SetNumberField(
            TEXT("gameContent"),
            Summary->NumGameContentAssetsRequested);
        OriginsJson->SetNumberField(
            TEXT("externalContent"),
            Summary->NumExternalContentAssetsRequested);
        OriginsJson->SetNumberField(
            TEXT("unknown"),
            Summary->NumUnknownOriginAssetsRequested);
        ValidationJson->SetObjectField(TEXT("assetOriginsRequested"), OriginsJson);

        TSharedRef<FJsonObject> ReadOnlyJson = MakeShared<FJsonObject>();
        // "passed" se conserva por compatibilidad: significa que no se
        // observaron paquetes nuevos dirty, no causalidad probada.
        ReadOnlyJson->SetBoolField(
            TEXT("passed"),
            Summary->bReadOnlyVerificationPassed);
        ReadOnlyJson->SetBoolField(
            TEXT("observableSideEffectsDetected"),
            !Summary->bReadOnlyVerificationPassed);
        ReadOnlyJson->SetBoolField(
            TEXT("contractViolationConfirmed"),
            false);
        ReadOnlyJson->SetStringField(
            TEXT("interpretation"),
            TEXT("Newly dirty packages are observable side effects correlated with the operation; they do not prove exclusive causation by the plugin."));
        ReadOnlyJson->SetNumberField(
            TEXT("dirtyPackagesBefore"),
            Summary->NumDirtyPackagesBefore);
        ReadOnlyJson->SetNumberField(
            TEXT("dirtyPackagesAfter"),
            Summary->NumDirtyPackagesAfter);

        TArray<TSharedPtr<FJsonValue>> NewlyDirtiedJson;
        for (const FString& PackageName : Summary->NewlyDirtiedPackages)
        {
            NewlyDirtiedJson.Add(MakeShared<FJsonValueString>(PackageName));
        }
        ReadOnlyJson->SetArrayField(
            TEXT("newlyDirtiedPackages"),
            NewlyDirtiedJson);
        ValidationJson->SetObjectField(TEXT("readOnlyVerification"), ReadOnlyJson);

        TArray<TSharedPtr<FJsonValue>> IssuesJson;
        for (const FPDSIssue& Issue : Summary->Issues)
        {
            IssuesJson.Add(MakeShared<FJsonValueObject>(BuildSnapshotIssueJson(Issue)));
        }
        ValidationJson->SetArrayField(TEXT("issues"), IssuesJson);

        return ValidationJson;
    }

    void CollectMontagesLoadedBySnapshot(const int32 NewlyLoadedMontageCount)
    {
        if (NewlyLoadedMontageCount > 0)
        {
            CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
        }
    }
}

FPDSOperationResult FPDSProjectSnapshotService::ExportProjectSnapshot(
    const FPDSValidationSummary* OptionalValidationSummary) const
{
    FPDSOperationResult Result;
    int32 ExcludedExternalObjectCount = 0;
    const TArray<FAssetData> Assets = PDSAssetUtilities::GetAllGameAssetsForSnapshot(
        &ExcludedExternalObjectCount);
    const FString ProjectName = FApp::GetProjectName();
    const bool bIsPantheliaProjectContext =
        ProjectName.Equals(TEXT("PantheliaProject"), ESearchCase::IgnoreCase);
    const FPDSOriginResolver OriginResolver = FPDSOriginResolver::FromSettings();
    const UPDSDeveloperSettings* DeveloperSettings = GetDefault<UPDSDeveloperSettings>();

    TSharedRef<FJsonObject> RootJson = MakeShared<FJsonObject>();
    RootJson->SetStringField(TEXT("schemaVersion"), TEXT("0.3.0-alpha2"));
    RootJson->SetStringField(
        TEXT("generatedAtUtc"),
        FDateTime::UtcNow().ToIso8601());

    TSharedRef<FJsonObject> ProjectJson = MakeShared<FJsonObject>();
    ProjectJson->SetStringField(TEXT("name"), ProjectName);
    ProjectJson->SetStringField(
        TEXT("engineVersion"),
        FEngineVersion::Current().ToString());
    ProjectJson->SetStringField(
        TEXT("projectDirectory"),
        FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()));
    ProjectJson->SetStringField(TEXT("assetRoot"), TEXT("/Game"));
    ProjectJson->SetBoolField(
        TEXT("isPantheliaProjectContext"),
        bIsPantheliaProjectContext);
    RootJson->SetObjectField(TEXT("project"), ProjectJson);

    TArray<FGameplayTag> SortedTags;
    FGameplayTagContainer AllTags;
    UGameplayTagsManager::Get().RequestAllGameplayTags(AllTags, true);
    AllTags.GetGameplayTagArray(SortedTags);
    SortedTags.Sort([](const FGameplayTag& A, const FGameplayTag& B)
    {
        return A.ToString() < B.ToString();
    });

    TArray<TSharedPtr<FJsonValue>> GameplayTagsJson;
    for (const FGameplayTag& Tag : SortedTags)
    {
        GameplayTagsJson.Add(MakeShared<FJsonValueString>(Tag.ToString()));
    }
    RootJson->SetArrayField(TEXT("gameplayTags"), GameplayTagsJson);

    TSharedRef<FJsonObject> GameplayTagContextJson = MakeShared<FJsonObject>();
    GameplayTagContextJson->SetStringField(TEXT("projectName"), ProjectName);
    GameplayTagContextJson->SetBoolField(
        TEXT("pantheliaNativeTagsExpected"),
        bIsPantheliaProjectContext);
    GameplayTagContextJson->SetStringField(
        TEXT("note"),
        bIsPantheliaProjectContext
            ? TEXT("El módulo del proyecto Panthelia está cargado; se esperan sus tags nativos registrados durante startup.")
            : TEXT("Este snapshot se generó fuera de Panthelia. En el HostProject solo se esperan tags del engine, plugins y configuración del host; la ausencia de tags nativos Panthelia.* no es un error."));
    RootJson->SetObjectField(
        TEXT("gameplayTagExportContext"),
        GameplayTagContextJson);

    TSharedRef<FJsonObject> InventoryContextJson = MakeShared<FJsonObject>();
    InventoryContextJson->SetNumberField(
        TEXT("excludedExternalObjectPackages"),
        ExcludedExternalObjectCount);
    InventoryContextJson->SetStringField(
        TEXT("note"),
        TEXT("Los paquetes /Game/__ExternalActors__ y /Game/__ExternalObjects__ se excluyen para evitar ruido masivo en snapshots destinados a IA. Los mapas UWorld permanecen en el inventario."));
    RootJson->SetObjectField(TEXT("assetInventoryContext"), InventoryContextJson);


    TSharedRef<FJsonObject> ClassificationJson = MakeShared<FJsonObject>();
    const auto AddStringArray = [&ClassificationJson](
        const TCHAR* FieldName,
        const TArray<FString>& Values)
    {
        TArray<TSharedPtr<FJsonValue>> JsonValues;
        for (const FString& Value : Values)
        {
            JsonValues.Add(MakeShared<FJsonValueString>(Value));
        }
        ClassificationJson->SetArrayField(FieldName, JsonValues);
    };
    if (DeveloperSettings)
    {
        AddStringArray(TEXT("pantheliaCoreRoots"), DeveloperSettings->PantheliaCorePackageRoots);
        AddStringArray(TEXT("externalContentRoots"), DeveloperSettings->ExternalContentPackageRoots);
        AddStringArray(TEXT("alwaysExcludedRoots"), DeveloperSettings->AlwaysExcludedPackageRoots);
    }
    ClassificationJson->SetStringField(
        TEXT("precedence"),
        TEXT("Always Excluded -> External Content -> Panthelia Core -> Game Content"));
    RootJson->SetObjectField(TEXT("assetClassificationContext"), ClassificationJson);

    TArray<TSharedPtr<FJsonValue>> InventoryJson;
    TArray<TSharedPtr<FJsonValue>> MontagesJson;
    int32 MontageCount = 0;
    int32 NewlyLoadedMontageCount = 0;

    FScopedSlowTask SlowTask(
        static_cast<float>(FMath::Max(Assets.Num(), 1)),
        NSLOCTEXT(
            "PantheliaDeveloperSuite",
            "ExportSnapshotProgress",
            "Exporting Panthelia project snapshot..."));
    SlowTask.MakeDialog(true);

    for (const FAssetData& AssetData : Assets)
    {
        SlowTask.EnterProgressFrame(
            1.0f,
            FText::FromString(AssetData.AssetName.ToString()));

        if (SlowTask.ShouldCancel())
        {
            Result.bCancelled = true;
            Result.Summary = FString::Printf(
                TEXT("Exportación cancelada después de procesar %d de %d assets. No se escribió latest.json ni un snapshot parcial."),
                InventoryJson.Num(),
                Assets.Num());
            Result.Issues.Add({
                EPDSIssueSeverity::Warning,
                TEXT("PDS.Snapshot.Cancelled"),
                FString(),
                TEXT("La operación fue cancelada por el usuario antes de completar el inventario.")
            });
            CollectMontagesLoadedBySnapshot(NewlyLoadedMontageCount);
            return Result;
        }

        TSharedRef<FJsonObject> AssetJson = MakeShared<FJsonObject>();
        AssetJson->SetStringField(TEXT("assetName"), AssetData.AssetName.ToString());
        AssetJson->SetStringField(TEXT("packageName"), AssetData.PackageName.ToString());
        AssetJson->SetStringField(TEXT("packagePath"), AssetData.PackagePath.ToString());
        AssetJson->SetStringField(
            TEXT("objectPath"),
            AssetData.GetSoftObjectPath().ToString());
        AssetJson->SetStringField(
            TEXT("classPath"),
            AssetData.AssetClassPath.ToString());
        AssetJson->SetStringField(
            TEXT("origin"),
            PDSDeveloperTypes::AssetOriginToString(
                OriginResolver.Classify(AssetData.PackagePath.ToString())));
        InventoryJson.Add(MakeShared<FJsonValueObject>(AssetJson));

        if (AssetData.IsInstanceOf(UAnimMontage::StaticClass(), EResolveClass::No))
        {
            const bool bWasLoadedBeforeSnapshot = AssetData.IsAssetLoaded();
            if (UAnimMontage* Montage = Cast<UAnimMontage>(AssetData.GetAsset()))
            {
                ++MontageCount;
                if (!bWasLoadedBeforeSnapshot)
                {
                    ++NewlyLoadedMontageCount;
                }
                MontagesJson.Add(MakeShared<FJsonValueObject>(
                    FPDSMontageInspectorService::BuildMontageJson(*Montage)));
            }
            else
            {
                Result.Issues.Add({
                    EPDSIssueSeverity::Warning,
                    TEXT("PDS.Snapshot.MontageLoadFailed"),
                    AssetData.GetSoftObjectPath().ToString(),
                    TEXT("Asset Registry lo identificó como UAnimMontage o subclase, pero no pudo cargarse como tal.")
                });
            }
        }
    }

    RootJson->SetArrayField(TEXT("assetInventory"), InventoryJson);
    RootJson->SetArrayField(TEXT("montages"), MontagesJson);
    RootJson->SetObjectField(
        TEXT("validation"),
        BuildValidationJson(OptionalValidationSummary));

    TSharedRef<FJsonObject> SummaryJson = MakeShared<FJsonObject>();
    SummaryJson->SetNumberField(TEXT("assetCount"), Assets.Num());
    SummaryJson->SetNumberField(TEXT("gameplayTagCount"), SortedTags.Num());
    SummaryJson->SetNumberField(TEXT("montageCount"), MontageCount);
    SummaryJson->SetNumberField(
        TEXT("excludedExternalObjectPackages"),
        ExcludedExternalObjectCount);
    SummaryJson->SetNumberField(TEXT("snapshotWarnings"), Result.Issues.Num());
    RootJson->SetObjectField(TEXT("summary"), SummaryJson);

    FString SerializedJson;
    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&SerializedJson);
    const bool bSerialized = FJsonSerializer::Serialize(RootJson, Writer);

    if (!bSerialized)
    {
        Result.Summary = TEXT("No fue posible serializar el snapshot JSON.");
        Result.Issues.Add({
            EPDSIssueSeverity::Error,
            TEXT("PDS.Snapshot.SerializationFailed"),
            FString(),
            Result.Summary
        });
        CollectMontagesLoadedBySnapshot(NewlyLoadedMontageCount);
        return Result;
    }

    const FString SnapshotsDirectory = FPaths::Combine(
        PDSAssetUtilities::GetSuiteSavedDirectory(),
        TEXT("Snapshots"));
    PDSAssetUtilities::EnsureDirectoryExists(SnapshotsDirectory);

    const FString TimestampedPath = FPaths::Combine(
        SnapshotsDirectory,
        FString::Printf(
            TEXT("PantheliaSnapshot_%s.json"),
            *PDSAssetUtilities::MakeTimestampForFileName()));
    const FString LatestPath = FPaths::Combine(
        SnapshotsDirectory,
        TEXT("latest.json"));

    const bool bSavedTimestamped =
        FFileHelper::SaveStringToFile(SerializedJson, *TimestampedPath);
    const bool bSavedLatest =
        PDSAssetUtilities::SaveStringAtomically(SerializedJson, LatestPath);

    Result.bSuccess = bSavedTimestamped && bSavedLatest;
    Result.OutputPath = TimestampedPath;
    Result.Summary = FString::Printf(
        TEXT("Snapshot: %d assets, %d tags, %d montages; %d paquetes externos excluidos. Guardado: %s"),
        Assets.Num(),
        SortedTags.Num(),
        MontageCount,
        ExcludedExternalObjectCount,
        Result.bSuccess ? TEXT("Sí") : TEXT("No"));

    if (!Result.bSuccess)
    {
        Result.Issues.Add({
            EPDSIssueSeverity::Error,
            TEXT("PDS.IO.SaveFailed"),
            TimestampedPath,
            TEXT("No fue posible guardar el snapshot timestamped o reemplazar latest.json de forma atómica.")
        });
    }

    CollectMontagesLoadedBySnapshot(NewlyLoadedMontageCount);
    return Result;
}
