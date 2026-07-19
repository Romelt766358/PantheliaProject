#include "PDSProjectDoctorService.h"

#include "Dom/JsonObject.h"
#include "Editor.h"
#include "EditorValidatorSubsystem.h"
#include "FileHelpers.h"
#include "Misc/DataValidation.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "PDSAssetUtilities.h"
#include "PDSDeveloperSettings.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/Package.h"

namespace
{
    FString EscapeMarkdownInline(FString Value)
    {
        Value.ReplaceInline(TEXT("`"), TEXT("'"));
        Value.ReplaceInline(TEXT("\r"), TEXT(" "));
        Value.ReplaceInline(TEXT("\n"), TEXT(" "));
        return Value;
    }

    FString EscapeMarkdownCodeBlock(FString Value)
    {
        Value.ReplaceInline(TEXT("```"), TEXT("'''"));
        return Value;
    }

    struct FPDSDirtyPackageState
    {
        TSet<FString> PackageNames;
    };

    FPDSDirtyPackageState CaptureDirtyPackageState()
    {
        FPDSDirtyPackageState State;
        TArray<UPackage*> DirtyContentPackages;
        TArray<UPackage*> DirtyMapPackages;
        UEditorLoadingAndSavingUtils::GetDirtyContentPackages(DirtyContentPackages);
        UEditorLoadingAndSavingUtils::GetDirtyMapPackages(DirtyMapPackages);

        const auto AddPackagesToState = [&State](const TArray<UPackage*>& Packages)
        {
            for (const UPackage* Package : Packages)
            {
                if (Package)
                {
                    State.PackageNames.Add(Package->GetName());
                }
            }
        };

        AddPackagesToState(DirtyContentPackages);
        AddPackagesToState(DirtyMapPackages);

        return State;
    }

    void ApplyReadOnlyVerification(
        FPDSValidationSummary& InOutSummary,
        const FPDSDirtyPackageState& Before,
        const FPDSDirtyPackageState& After,
        const FPDSOriginResolver& OriginResolver)
    {
        InOutSummary.NumDirtyPackagesBefore = Before.PackageNames.Num();
        InOutSummary.NumDirtyPackagesAfter = After.PackageNames.Num();
        InOutSummary.NewlyDirtiedPackages.Reset();

        for (const FString& PackageName : After.PackageNames)
        {
            if (!Before.PackageNames.Contains(PackageName))
            {
                InOutSummary.NewlyDirtiedPackages.Add(PackageName);
            }
        }

        InOutSummary.NewlyDirtiedPackages.Sort();
        InOutSummary.bReadOnlyVerificationPassed =
            InOutSummary.NewlyDirtiedPackages.IsEmpty();

        if (InOutSummary.bReadOnlyVerificationPassed)
        {
            return;
        }

        const UPDSDeveloperSettings* Settings = GetDefault<UPDSDeveloperSettings>();
        const EPDSIssueSeverity Severity = Settings
            && Settings->bTreatNewlyDirtiedPackagesAsError
            ? EPDSIssueSeverity::Error
            : EPDSIssueSeverity::Warning;

        for (const FString& PackageName : InOutSummary.NewlyDirtiedPackages)
        {
            InOutSummary.Issues.Add({
                Severity,
                TEXT("PDS.ReadOnly.NewlyDirtiedPackage"),
                PackageName,
                TEXT("El paquete no figuraba dirty antes de la operación y aparece dirty después. Es un efecto secundario observable, pero no confirma por sí solo que el plugin haya modificado deliberadamente el asset: PostLoad, upgrades, recompilación o validadores externos también pueden producirlo. El plugin no guarda ni revierte automáticamente."),
                OriginResolver.Classify(PackageName)
            });
        }
    }


    EPDSAssetOrigin ResolveValidationResultOrigin(
        const FString& AssetPath,
        const TMap<FString, EPDSAssetOrigin>& OriginByAssetPath,
        const FPDSOriginResolver& OriginResolver)
    {
        if (const EPDSAssetOrigin* ExactOrigin = OriginByAssetPath.Find(AssetPath))
        {
            return *ExactOrigin;
        }

        const FString PackageName = FPackageName::ObjectPathToPackageName(AssetPath);
        if (const EPDSAssetOrigin* PackageOrigin = OriginByAssetPath.Find(PackageName))
        {
            return *PackageOrigin;
        }

        return OriginResolver.Classify(AssetPath);
    }

    TSharedRef<FJsonObject> BuildIssueJson(const FPDSIssue& Issue)
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

    TSharedRef<FJsonObject> BuildIssueGroupJson(const FPDSIssueGroup& Group)
    {
        TSharedRef<FJsonObject> GroupJson = MakeShared<FJsonObject>();
        GroupJson->SetStringField(
            TEXT("severity"),
            PDSDeveloperTypes::SeverityToString(Group.Severity));
        GroupJson->SetStringField(
            TEXT("origin"),
            PDSDeveloperTypes::AssetOriginToString(Group.Origin));
        GroupJson->SetStringField(TEXT("ruleId"), Group.RuleId);
        GroupJson->SetNumberField(TEXT("count"), Group.Count);

        TArray<TSharedPtr<FJsonValue>> SamplePathsJson;
        for (const FString& SamplePath : Group.SampleAssetPaths)
        {
            SamplePathsJson.Add(MakeShared<FJsonValueString>(SamplePath));
        }
        GroupJson->SetArrayField(TEXT("sampleAssetPaths"), SamplePathsJson);
        return GroupJson;
    }
}

FPDSValidationSummary FPDSProjectDoctorService::ValidateSelectedAssets() const
{
    FPDSValidationSummary Summary = ValidateAssets(
        PDSAssetUtilities::GetSelectedAssets(),
        EPDSValidationProfile::SelectedAssets);
    PersistValidationReports(Summary);
    return Summary;
}

FPDSValidationSummary FPDSProjectDoctorService::ValidatePantheliaCore() const
{
    return ValidateProfile(EPDSValidationProfile::PantheliaCore);
}

FPDSValidationSummary FPDSProjectDoctorService::ValidateGameContent() const
{
    return ValidateProfile(EPDSValidationProfile::GameContent);
}

FPDSValidationSummary FPDSProjectDoctorService::ValidateExternalContent() const
{
    return ValidateProfile(EPDSValidationProfile::ExternalContent);
}

FPDSValidationSummary FPDSProjectDoctorService::ValidateEntireProject() const
{
    return ValidateProfile(EPDSValidationProfile::EntireProject);
}

FPDSValidationSummary FPDSProjectDoctorService::ValidateProject() const
{
    return ValidateEntireProject();
}

FPDSValidationSummary FPDSProjectDoctorService::ValidateProfile(
    const EPDSValidationProfile Profile) const
{
    FPDSAssetQueryStats QueryStats;
    const FPDSAssetSelection Selection =
        PDSAssetUtilities::GetGameAssetSelectionForValidationProfile(
            Profile,
            &QueryStats);

    FPDSValidationSummary Summary = ValidateAssetSelection(Selection, Profile);
    Summary.NumExcludedMaps = QueryStats.NumExcludedMaps;
    Summary.NumExcludedExternalObjects = QueryStats.NumExcludedExternalObjects;
    Summary.NumExcludedByConfiguration = QueryStats.NumExcludedByConfiguration;
    Summary.NumOutsideSelectedProfile = QueryStats.NumOutsideSelectedProfile;

    if (QueryStats.NumExcludedMaps > 0
        || QueryStats.NumExcludedExternalObjects > 0
        || QueryStats.NumExcludedByConfiguration > 0
        || QueryStats.NumOutsideSelectedProfile > 0)
    {
        const FPDSIssue FilteringIssue{
            EPDSIssueSeverity::Info,
            TEXT("PDS.Validation.ProfileFiltering"),
            TEXT("/Game"),
            FString::Printf(
                TEXT("El perfil %s excluyó %d mapas UWorld, %d paquetes de objetos externos, %d assets por configuración y %d assets fuera del perfil."),
                *Summary.ScopeLabel,
                QueryStats.NumExcludedMaps,
                QueryStats.NumExcludedExternalObjects,
                QueryStats.NumExcludedByConfiguration,
                QueryStats.NumOutsideSelectedProfile),
            EPDSAssetOrigin::Unknown
        };
        Summary.Issues.Insert(FilteringIssue, 0);
    }

    PersistValidationReports(Summary);
    return Summary;
}

FPDSValidationSummary FPDSProjectDoctorService::ValidateAssets(
    const TArray<FAssetData>& Assets,
    const EPDSValidationProfile Profile) const
{
    const FPDSOriginResolver OriginResolver =
        FPDSOriginResolver::FromSettings();

    FPDSAssetSelection Selection;
    Selection.Assets = Assets;
    Selection.Origins.Reserve(Assets.Num());

    for (const FAssetData& AssetData : Assets)
    {
        Selection.Origins.Add(
            OriginResolver.Classify(AssetData.PackagePath.ToString()));
    }

    return ValidateAssetSelection(Selection, Profile);
}

FPDSValidationSummary FPDSProjectDoctorService::ValidateAssetSelection(
    const FPDSAssetSelection& Selection,
    const EPDSValidationProfile Profile) const
{
    FPDSValidationSummary Summary;
    Summary.ScopeId = PDSDeveloperTypes::ValidationProfileToId(Profile);
    Summary.ScopeLabel = PDSDeveloperTypes::ValidationProfileToLabel(Profile);
    Summary.NumRequested = Selection.Assets.Num();

    const FPDSOriginResolver OriginResolver =
        FPDSOriginResolver::FromSettings();

    if (!ensureMsgf(
        Selection.HasAlignedOrigins(),
        TEXT("La validación requiere Assets y Origins alineados.")))
    {
        Summary.NumNotValidated = Selection.Assets.Num();
        Summary.NumUnableToValidate = Selection.Assets.Num();
        Summary.Issues.Add({
            EPDSIssueSeverity::Error,
            TEXT("PDS.Validation.OriginAlignmentInvalid"),
            FString(),
            TEXT("La selección interna contiene una cantidad distinta de assets y orígenes. La operación se canceló para evitar resultados clasificados incorrectamente."),
            EPDSAssetOrigin::Unknown
        });
        Summary.GeneratedAtUtc = FDateTime::UtcNow().ToIso8601();
        return Summary;
    }

    TMap<FString, EPDSAssetOrigin> OriginByAssetPath;
    OriginByAssetPath.Reserve(Selection.Assets.Num() * 2);

    for (int32 Index = 0; Index < Selection.Assets.Num(); ++Index)
    {
        const FAssetData& AssetData = Selection.Assets[Index];
        const EPDSAssetOrigin Origin = Selection.Origins[Index];

        switch (Origin)
        {
        case EPDSAssetOrigin::PantheliaCore:
            ++Summary.NumPantheliaCoreAssetsRequested;
            break;
        case EPDSAssetOrigin::GameContent:
            ++Summary.NumGameContentAssetsRequested;
            break;
        case EPDSAssetOrigin::ExternalContent:
            ++Summary.NumExternalContentAssetsRequested;
            break;
        default:
            ++Summary.NumUnknownOriginAssetsRequested;
            break;
        }

        OriginByAssetPath.Add(AssetData.GetObjectPathString(), Origin);
        OriginByAssetPath.Add(AssetData.PackageName.ToString(), Origin);
    }

    const FPDSDirtyPackageState DirtyBefore = CaptureDirtyPackageState();
    const auto FinalizeValidation = [
        &Summary,
        &DirtyBefore,
        &OriginResolver]()
    {
        ApplyReadOnlyVerification(
            Summary,
            DirtyBefore,
            CaptureDirtyPackageState(),
            OriginResolver);
        // GeneratedAtUtc representa el final observable de la validación.
        Summary.GeneratedAtUtc = FDateTime::UtcNow().ToIso8601();
    };

    if (Selection.Assets.IsEmpty())
    {
        Summary.Issues.Add({
            EPDSIssueSeverity::Warning,
            TEXT("PDS.Validation.NoInput"),
            FString(),
            TEXT("No hay assets para validar. Una selección o perfil vacío se trata como ausencia de entrada, no como fallo técnico."),
            EPDSAssetOrigin::Unknown
        });
        FinalizeValidation();
        return Summary;
    }

    if (!GEditor)
    {
        Summary.NumNotValidated = Selection.Assets.Num();
        Summary.NumUnableToValidate = Selection.Assets.Num();
        Summary.Issues.Add({
            EPDSIssueSeverity::Error,
            TEXT("PDS.Validation.EditorUnavailable"),
            FString(),
            TEXT("GEditor no está disponible. La validación requiere una sesión de editor."),
            EPDSAssetOrigin::Unknown
        });
        FinalizeValidation();
        return Summary;
    }

    UEditorValidatorSubsystem* ValidatorSubsystem =
        GEditor->GetEditorSubsystem<UEditorValidatorSubsystem>();

    if (!ValidatorSubsystem)
    {
        Summary.NumNotValidated = Selection.Assets.Num();
        Summary.NumUnableToValidate = Selection.Assets.Num();
        Summary.Issues.Add({
            EPDSIssueSeverity::Error,
            TEXT("PDS.Validation.SubsystemUnavailable"),
            FString(),
            TEXT("UEditorValidatorSubsystem no está disponible. Verifica que Data Validation esté habilitado."),
            EPDSAssetOrigin::Unknown
        });
        FinalizeValidation();
        return Summary;
    }

    FValidateAssetsSettings Settings;
    Settings.ValidationUsecase = EDataValidationUsecase::Manual;
    Settings.bLoadAssetsForValidation = true;
    Settings.bLoadExternalObjectsForValidation = false;
    Settings.bCollectPerAssetDetails = true;
    Settings.bSkipExcludedDirectories = true;
    Settings.bUnloadAssetsLoadedForValidation = true;
    Settings.bShowIfNoFailures = false;
    Settings.bSilent = false;

    FValidateAssetsResults ValidationResults;
    ValidatorSubsystem->ValidateAssetsWithSettings(
        Selection.Assets,
        Settings,
        ValidationResults);

    Summary.NumRequested = ValidationResults.NumRequested;
    Summary.NumChecked = ValidationResults.NumChecked;
    Summary.NumValid = ValidationResults.NumValid;
    Summary.NumInvalid = ValidationResults.NumInvalid;
    Summary.NumWithWarnings = ValidationResults.NumWarnings;
    Summary.NumSkipped = ValidationResults.NumSkipped;
    Summary.NumUnableToValidate = ValidationResults.NumUnableToValidate;
    Summary.NumNotValidated = Summary.NumSkipped + Summary.NumUnableToValidate;
    Summary.bAssetLimitReached = ValidationResults.bAssetLimitReached;

    for (const TPair<FString, FValidateAssetsDetails>& Pair : ValidationResults.AssetsDetails)
    {
        const FString& AssetPath = Pair.Key;
        const FValidateAssetsDetails& Details = Pair.Value;
        const EPDSAssetOrigin Origin = ResolveValidationResultOrigin(
            AssetPath,
            OriginByAssetPath,
            OriginResolver);

        for (const FText& Error : Details.ValidationErrors)
        {
            Summary.Issues.Add({
                EPDSIssueSeverity::Error,
                TEXT("UE.DataValidation.Error"),
                AssetPath,
                Error.ToString(),
                Origin
            });
        }

        for (const FText& Warning : Details.ValidationWarnings)
        {
            Summary.Issues.Add({
                EPDSIssueSeverity::Warning,
                TEXT("UE.DataValidation.Warning"),
                AssetPath,
                Warning.ToString(),
                Origin
            });
        }

        if (Details.Result == EDataValidationResult::Invalid
            && Details.ValidationErrors.IsEmpty())
        {
            Summary.Issues.Add({
                EPDSIssueSeverity::Error,
                TEXT("UE.DataValidation.InvalidWithoutText"),
                AssetPath,
                TEXT("El subsystem marcó el asset como inválido, pero no devolvió un texto de error plano. Consulta también el Message Log de Data Validation."),
                Origin
            });
        }
    }

    if (!ValidationResults.ValidatorMessages.IsEmpty())
    {
        Summary.Issues.Add({
            EPDSIssueSeverity::Info,
            TEXT("UE.DataValidation.AdditionalMessages"),
            FString(),
            FString::Printf(
                TEXT("Data Validation produjo %d mensajes globales adicionales. Consulta el Message Log para conservar sus tokens y enlaces enriquecidos."),
                ValidationResults.ValidatorMessages.Num()),
            EPDSAssetOrigin::Unknown
        });
    }

    if (Summary.bAssetLimitReached)
    {
        Summary.Issues.Add({
            EPDSIssueSeverity::Warning,
            TEXT("PDS.Validation.AssetLimitReached"),
            FString(),
            TEXT("La operación alcanzó MaxAssetsToValidate antes de completar todos los assets solicitados."),
            EPDSAssetOrigin::Unknown
        });
    }

    FinalizeValidation();
    return Summary;
}

void FPDSProjectDoctorService::PersistValidationReports(
    FPDSValidationSummary& InOutSummary) const
{
    const FString ReportsDirectory = FPaths::Combine(
        PDSAssetUtilities::GetSuiteSavedDirectory(),
        TEXT("Reports"));
    PDSAssetUtilities::EnsureDirectoryExists(ReportsDirectory);

    const FString Timestamp = PDSAssetUtilities::MakeTimestampForFileName();
    const FString TimestampedMarkdownPath = FPaths::Combine(
        ReportsDirectory,
        FString::Printf(TEXT("ValidationReport_%s.md"), *Timestamp));
    const FString TimestampedJsonPath = FPaths::Combine(
        ReportsDirectory,
        FString::Printf(TEXT("ValidationReport_%s.json"), *Timestamp));
    const FString LatestMarkdownPath = FPaths::Combine(
        ReportsDirectory,
        TEXT("latest_validation.md"));
    const FString LatestJsonPath = FPaths::Combine(
        ReportsDirectory,
        TEXT("latest_validation.json"));

    // Las rutas públicas empiezan vacías y solo se asignan cuando el archivo
    // timestamped correspondiente existe realmente.
    InOutSummary.OutputPath.Reset();
    InOutSummary.OutputJsonPath.Reset();

    const FString Markdown = BuildValidationReportMarkdown(InOutSummary);
    const bool bSavedTimestampedMarkdown =
        FFileHelper::SaveStringToFile(Markdown, *TimestampedMarkdownPath);
    const bool bSavedLatestMarkdown =
        PDSAssetUtilities::SaveStringAtomically(Markdown, LatestMarkdownPath);

    if (bSavedTimestampedMarkdown)
    {
        InOutSummary.OutputPath = TimestampedMarkdownPath;
    }

    bool bAnyWriteFailed = !bSavedTimestampedMarkdown || !bSavedLatestMarkdown;

    FString SerializedJson;
    const bool bJsonSerialized =
        BuildValidationReportJson(InOutSummary, SerializedJson);

    if (!bJsonSerialized)
    {
        InOutSummary.Issues.Add({
            EPDSIssueSeverity::Error,
            TEXT("PDS.IO.ValidationJsonSerializationFailed"),
            TimestampedJsonPath,
            TEXT("No fue posible serializar el informe JSON de validación. No se intentó escribir ningún JSON."),
            EPDSAssetOrigin::Unknown
        });
    }
    else
    {
        const bool bSavedTimestampedJson =
            FFileHelper::SaveStringToFile(SerializedJson, *TimestampedJsonPath);
        const bool bSavedLatestJson =
            PDSAssetUtilities::SaveStringAtomically(SerializedJson, LatestJsonPath);

        if (bSavedTimestampedJson)
        {
            InOutSummary.OutputJsonPath = TimestampedJsonPath;
        }

        bAnyWriteFailed = bAnyWriteFailed
            || !bSavedTimestampedJson
            || !bSavedLatestJson;
    }

    if (bAnyWriteFailed)
    {
        InOutSummary.Issues.Add({
            EPDSIssueSeverity::Error,
            TEXT("PDS.IO.ValidationReportSaveFailed"),
            ReportsDirectory,
            TEXT("La serialización disponible no pudo escribirse por completo en los informes timestamped o en latest_validation.md/json. Las rutas del summary solo se asignan para archivos timestamped guardados con éxito."),
            EPDSAssetOrigin::Unknown
        });
    }
}

FString FPDSProjectDoctorService::BuildValidationReportMarkdown(
    const FPDSValidationSummary& Summary)
{
    FString Markdown = TEXT("# Panthelia Developer Suite — Validation Report\n\n");
    Markdown += TEXT("- Schema: `pds.validation.v0.2-alpha3`\n");
    Markdown += FString::Printf(TEXT("- Scope: `%s`\n"), *EscapeMarkdownInline(Summary.ScopeLabel));
    Markdown += FString::Printf(TEXT("- Scope ID: `%s`\n"), *EscapeMarkdownInline(Summary.ScopeId));
    Markdown += FString::Printf(
        TEXT("- Generated UTC: `%s`\n"),
        Summary.GeneratedAtUtc.IsEmpty() ? TEXT("<unknown>") : *Summary.GeneratedAtUtc);
    Markdown += FString::Printf(TEXT("- Requested: `%d`\n"), Summary.NumRequested);
    Markdown += FString::Printf(TEXT("- Checked: `%d`\n"), Summary.NumChecked);
    Markdown += FString::Printf(TEXT("- Valid: `%d`\n"), Summary.NumValid);
    Markdown += FString::Printf(TEXT("- Invalid: `%d`\n"), Summary.NumInvalid);
    Markdown += FString::Printf(TEXT("- With warnings: `%d`\n"), Summary.NumWithWarnings);
    Markdown += FString::Printf(TEXT("- Skipped: `%d`\n"), Summary.NumSkipped);
    Markdown += FString::Printf(
        TEXT("- Unable to validate: `%d`\n"),
        Summary.NumUnableToValidate);
    Markdown += FString::Printf(
        TEXT("- Asset limit reached: `%s`\n"),
        Summary.bAssetLimitReached ? TEXT("true") : TEXT("false"));
    Markdown += FString::Printf(
        TEXT("- Observable dirty-package side effects: `%s`\n"),
        Summary.bReadOnlyVerificationPassed ? TEXT("NONE") : TEXT("DETECTED"));
    Markdown += TEXT("- Read-only contract violation confirmed: `false`\n\n");

    Markdown += TEXT("## Asset origins requested\n\n");
    Markdown += FString::Printf(
        TEXT("- Panthelia Core: `%d`\n"),
        Summary.NumPantheliaCoreAssetsRequested);
    Markdown += FString::Printf(
        TEXT("- Game Content: `%d`\n"),
        Summary.NumGameContentAssetsRequested);
    Markdown += FString::Printf(
        TEXT("- External Content: `%d`\n"),
        Summary.NumExternalContentAssetsRequested);
    Markdown += FString::Printf(
        TEXT("- Unknown: `%d`\n\n"),
        Summary.NumUnknownOriginAssetsRequested);

    Markdown += TEXT("## Filtering\n\n");
    Markdown += FString::Printf(TEXT("- Maps excluded: `%d`\n"), Summary.NumExcludedMaps);
    Markdown += FString::Printf(
        TEXT("- External objects excluded: `%d`\n"),
        Summary.NumExcludedExternalObjects);
    Markdown += FString::Printf(
        TEXT("- Excluded by configuration: `%d`\n"),
        Summary.NumExcludedByConfiguration);
    Markdown += FString::Printf(
        TEXT("- Outside selected profile: `%d`\n\n"),
        Summary.NumOutsideSelectedProfile);

    Markdown += TEXT("## Observable dirty-package side effects\n\n");
    Markdown += FString::Printf(
        TEXT("- Dirty packages before: `%d`\n"),
        Summary.NumDirtyPackagesBefore);
    Markdown += FString::Printf(
        TEXT("- Dirty packages after: `%d`\n"),
        Summary.NumDirtyPackagesAfter);
    Markdown += FString::Printf(
        TEXT("- Newly dirtied packages observed: `%d`\n"),
        Summary.NewlyDirtiedPackages.Num());
    Markdown += TEXT("- Interpretation: this comparison reports correlation with the operation, not exclusive causation by the plugin. Unloaded packages may also escape the after snapshot.\n");
    for (const FString& PackageName : Summary.NewlyDirtiedPackages)
    {
        Markdown += FString::Printf(TEXT("  - `%s`\n"), *EscapeMarkdownInline(PackageName));
    }
    Markdown += TEXT("\n");

    Markdown += TEXT("## Issue groups\n\n");
    const TArray<FPDSIssueGroup> Groups = Summary.BuildIssueGroups();
    if (Groups.IsEmpty())
    {
        Markdown += TEXT("No issue groups were reported.\n\n");
    }
    else
    {
        Markdown += TEXT("| Severity | Origin | Rule | Count | Samples |\n");
        Markdown += TEXT("|---|---|---|---:|---|\n");
        for (const FPDSIssueGroup& Group : Groups)
        {
            Markdown += FString::Printf(
                TEXT("| %s | %s | `%s` | %d | %s |\n"),
                *PDSDeveloperTypes::SeverityToString(Group.Severity),
                *PDSDeveloperTypes::AssetOriginToString(Group.Origin),
                *EscapeMarkdownInline(Group.RuleId),
                Group.Count,
                *EscapeMarkdownInline(FString::Join(Group.SampleAssetPaths, TEXT(", "))));
        }
        Markdown += TEXT("\n");
    }

    Markdown += TEXT("## Issues\n\n");
    if (Summary.Issues.IsEmpty())
    {
        Markdown += TEXT("No issues were reported.\n");
        return Markdown;
    }

    for (int32 IssueIndex = 0; IssueIndex < Summary.Issues.Num(); ++IssueIndex)
    {
        const FPDSIssue& Issue = Summary.Issues[IssueIndex];
        Markdown += FString::Printf(
            TEXT("### %d. %s — `%s`\n\n"),
            IssueIndex + 1,
            *PDSDeveloperTypes::SeverityToString(Issue.Severity),
            *EscapeMarkdownInline(Issue.RuleId));
        Markdown += FString::Printf(
            TEXT("- Origin: `%s`\n"),
            *PDSDeveloperTypes::AssetOriginToString(Issue.Origin));
        Markdown += FString::Printf(
            TEXT("- Asset: `%s`\n\n"),
            Issue.AssetPath.IsEmpty()
                ? TEXT("<no asset>")
                : *EscapeMarkdownInline(Issue.AssetPath));
        Markdown += TEXT("```text\n");
        Markdown += EscapeMarkdownCodeBlock(Issue.Message);
        Markdown += TEXT("\n```\n\n");
    }

    return Markdown;
}

bool FPDSProjectDoctorService::BuildValidationReportJson(
    const FPDSValidationSummary& Summary,
    FString& OutSerializedJson)
{
    TSharedRef<FJsonObject> RootJson = MakeShared<FJsonObject>();
    RootJson->SetStringField(TEXT("schemaVersion"), TEXT("pds.validation.v0.2-alpha3"));
    RootJson->SetStringField(TEXT("generatedAtUtc"), Summary.GeneratedAtUtc);

    TSharedRef<FJsonObject> ScopeJson = MakeShared<FJsonObject>();
    ScopeJson->SetStringField(TEXT("id"), Summary.ScopeId);
    ScopeJson->SetStringField(TEXT("label"), Summary.ScopeLabel);
    RootJson->SetObjectField(TEXT("scope"), ScopeJson);

    TSharedRef<FJsonObject> CountsJson = MakeShared<FJsonObject>();
    CountsJson->SetNumberField(TEXT("requested"), Summary.NumRequested);
    CountsJson->SetNumberField(TEXT("checked"), Summary.NumChecked);
    CountsJson->SetNumberField(TEXT("valid"), Summary.NumValid);
    CountsJson->SetNumberField(TEXT("invalid"), Summary.NumInvalid);
    CountsJson->SetNumberField(TEXT("withWarnings"), Summary.NumWithWarnings);
    CountsJson->SetNumberField(TEXT("notValidated"), Summary.NumNotValidated);
    CountsJson->SetNumberField(TEXT("skipped"), Summary.NumSkipped);
    CountsJson->SetNumberField(
        TEXT("unableToValidate"),
        Summary.NumUnableToValidate);
    CountsJson->SetBoolField(TEXT("assetLimitReached"), Summary.bAssetLimitReached);
    CountsJson->SetBoolField(TEXT("cancelled"), Summary.bCancelled);
    RootJson->SetObjectField(TEXT("counts"), CountsJson);

    TSharedRef<FJsonObject> OriginsJson = MakeShared<FJsonObject>();
    OriginsJson->SetNumberField(
        TEXT("pantheliaCore"),
        Summary.NumPantheliaCoreAssetsRequested);
    OriginsJson->SetNumberField(
        TEXT("gameContent"),
        Summary.NumGameContentAssetsRequested);
    OriginsJson->SetNumberField(
        TEXT("externalContent"),
        Summary.NumExternalContentAssetsRequested);
    OriginsJson->SetNumberField(
        TEXT("unknown"),
        Summary.NumUnknownOriginAssetsRequested);
    RootJson->SetObjectField(TEXT("assetOriginsRequested"), OriginsJson);

    TSharedRef<FJsonObject> FilteringJson = MakeShared<FJsonObject>();
    FilteringJson->SetNumberField(TEXT("excludedMaps"), Summary.NumExcludedMaps);
    FilteringJson->SetNumberField(
        TEXT("excludedExternalObjects"),
        Summary.NumExcludedExternalObjects);
    FilteringJson->SetNumberField(
        TEXT("excludedByConfiguration"),
        Summary.NumExcludedByConfiguration);
    FilteringJson->SetNumberField(
        TEXT("outsideSelectedProfile"),
        Summary.NumOutsideSelectedProfile);
    RootJson->SetObjectField(TEXT("filtering"), FilteringJson);

    TSharedRef<FJsonObject> ReadOnlyJson = MakeShared<FJsonObject>();
    // "passed" se conserva por compatibilidad: significa que no se observaron
    // paquetes nuevos dirty, no que exista una prueba absoluta de causalidad.
    ReadOnlyJson->SetBoolField(TEXT("passed"), Summary.bReadOnlyVerificationPassed);
    ReadOnlyJson->SetBoolField(
        TEXT("observableSideEffectsDetected"),
        !Summary.bReadOnlyVerificationPassed);
    ReadOnlyJson->SetBoolField(TEXT("contractViolationConfirmed"), false);
    ReadOnlyJson->SetStringField(
        TEXT("interpretation"),
        TEXT("Newly dirty packages are observable side effects correlated with the operation. They may originate from PostLoad, upgrades, Blueprint recompilation or external validators; unloaded packages may not remain visible in the after snapshot."));
    ReadOnlyJson->SetNumberField(
        TEXT("dirtyPackagesBefore"),
        Summary.NumDirtyPackagesBefore);
    ReadOnlyJson->SetNumberField(
        TEXT("dirtyPackagesAfter"),
        Summary.NumDirtyPackagesAfter);

    TArray<TSharedPtr<FJsonValue>> NewlyDirtiedJson;
    for (const FString& PackageName : Summary.NewlyDirtiedPackages)
    {
        NewlyDirtiedJson.Add(MakeShared<FJsonValueString>(PackageName));
    }
    ReadOnlyJson->SetArrayField(TEXT("newlyDirtiedPackages"), NewlyDirtiedJson);
    RootJson->SetObjectField(TEXT("readOnlyVerification"), ReadOnlyJson);

    TSharedRef<FJsonObject> OutputsJson = MakeShared<FJsonObject>();
    OutputsJson->SetStringField(TEXT("markdownReportPath"), Summary.OutputPath);
    OutputsJson->SetStringField(TEXT("jsonReportPath"), Summary.OutputJsonPath);
    RootJson->SetObjectField(TEXT("outputs"), OutputsJson);

    TArray<TSharedPtr<FJsonValue>> GroupsJson;
    for (const FPDSIssueGroup& Group : Summary.BuildIssueGroups())
    {
        GroupsJson.Add(MakeShared<FJsonValueObject>(BuildIssueGroupJson(Group)));
    }
    RootJson->SetArrayField(TEXT("issueGroups"), GroupsJson);

    TArray<TSharedPtr<FJsonValue>> IssuesJson;
    for (const FPDSIssue& Issue : Summary.Issues)
    {
        IssuesJson.Add(MakeShared<FJsonValueObject>(BuildIssueJson(Issue)));
    }
    RootJson->SetArrayField(TEXT("issues"), IssuesJson);

    OutSerializedJson.Reset();
    const TSharedRef<TJsonWriter<>> Writer =
        TJsonWriterFactory<>::Create(&OutSerializedJson);
    return FJsonSerializer::Serialize(RootJson, Writer);
}
