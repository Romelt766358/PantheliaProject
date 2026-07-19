#pragma once

#include "CoreMinimal.h"
#include "PDSDeveloperTypes.h"

/** Registro mínimo y estable de un asset dentro de un snapshot. */
struct FPDSSnapshotAssetRecord
{
    FString ObjectPath;
    FString PackageName;
    FString PackagePath;
    FString ClassPath;
    EPDSAssetOrigin Origin = EPDSAssetOrigin::Unknown;
};

/** Registro estable de un montage. Fingerprint resume secciones y notifies. */
struct FPDSSnapshotMontageRecord
{
    FString Path;
    FString Fingerprint;
};

/** Resumen de validación opcional embebido en un snapshot. */
struct FPDSSnapshotValidationRecord
{
    bool bIncluded = false;
    FString ScopeId;
    int32 NumValid = 0;
    int32 NumInvalid = 0;
    int32 NumWithWarnings = 0;
};

/** Documento normalizado para comparar snapshots de esquemas compatibles. */
struct FPDSSnapshotDocument
{
    FString SourcePath;
    FString SchemaVersion;
    FString GeneratedAtUtc;
    FString ProjectName;
    FString EngineVersion;

    TMap<FString, FPDSSnapshotAssetRecord> AssetsByObjectPath;
    TSet<FString> GameplayTags;
    TMap<FString, FPDSSnapshotMontageRecord> MontagesByPath;
    FPDSSnapshotValidationRecord Validation;
};

/** Cambio de metadatos para un asset presente en ambos snapshots. */
struct FPDSSnapshotAssetChange
{
    FString ObjectPath;
    FString PreviousClassPath;
    FString CurrentClassPath;
    EPDSAssetOrigin PreviousOrigin = EPDSAssetOrigin::Unknown;
    EPDSAssetOrigin CurrentOrigin = EPDSAssetOrigin::Unknown;
};

/** Diff determinista entre dos snapshots. */
struct FPDSSnapshotDiff
{
    FString PreviousSnapshotPath;
    FString CurrentSnapshotPath;
    FString PreviousGeneratedAtUtc;
    FString CurrentGeneratedAtUtc;
    FString PreviousSchemaVersion;
    FString CurrentSchemaVersion;
    FString ProjectName;

    int32 PreviousAssetCount = 0;
    int32 CurrentAssetCount = 0;
    int32 PreviousGameplayTagCount = 0;
    int32 CurrentGameplayTagCount = 0;
    int32 PreviousMontageCount = 0;
    int32 CurrentMontageCount = 0;

    /** Assets emparejados cuyo snapshot anterior no tenía un origin comparable. */
    int32 NumAssetsWithOriginNotComparable = 0;

    TArray<FPDSSnapshotAssetRecord> AddedAssets;
    TArray<FPDSSnapshotAssetRecord> RemovedAssets;
    TArray<FPDSSnapshotAssetChange> ChangedAssets;
    TArray<FString> AddedGameplayTags;
    TArray<FString> RemovedGameplayTags;
    TArray<FString> AddedMontages;
    TArray<FString> RemovedMontages;
    TArray<FString> ChangedMontages;

    bool bPreviousValidationIncluded = false;
    bool bCurrentValidationIncluded = false;
    int32 PreviousInvalidCount = 0;
    int32 CurrentInvalidCount = 0;
    int32 PreviousWarningCount = 0;
    int32 CurrentWarningCount = 0;

    TArray<FPDSIssue> Issues;

    bool HasChanges() const;
    FString ToDashboardText(int32 MaxEntriesPerCategory) const;
};

namespace PDSSnapshotDiff
{
    /** Parsea un snapshot JSON sin cargar ningún asset. */
    bool ParseSnapshotJson(
        const FString& JsonText,
        const FString& SourcePath,
        FPDSSnapshotDocument& OutDocument,
        TArray<FPDSIssue>& OutIssues);

    /** Compara documentos ya normalizados. Previous siempre representa el baseline temporal. */
    FPDSSnapshotDiff Compare(
        const FPDSSnapshotDocument& Previous,
        const FPDSSnapshotDocument& Current);
}
