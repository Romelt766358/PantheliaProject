#pragma once

#include "CoreMinimal.h"
#include "PDSDeveloperTypes.h"
#include "PDSSnapshotDiffTypes.h"

/** Tipo de cambio mostrado por el Snapshot Diff Browser. */
enum class EPDSSnapshotDiffChangeKind : uint8
{
    Added,
    Removed,
    Modified
};

/** Metadatos legibles de un snapshot disponible para comparación manual. */
struct FPDSSnapshotFileDescriptor
{
    FString FilePath;
    FString FileName;
    FString DisplayLabel;
    FString GeneratedAtUtc;
    FString SchemaVersion;
    FString ProjectName;
    int32 AssetCount = 0;
    bool bIsBaseline = false;
    bool bIsLatest = false;
    bool bIsTimestamped = false;
    bool bIsValid = false;
};

/**
 * Descriptor cacheado junto con la firma mínima del archivo que lo produjo.
 * Los issues se conservan para que un cache hit mantenga el mismo diagnóstico.
 */
struct FPDSSnapshotDescriptorCacheEntry
{
    int64 FileSize = -1;
    FDateTime ModificationTime = FDateTime::MinValue();
    FPDSSnapshotFileDescriptor Descriptor;
    TArray<FPDSIssue> Issues;
};

/** Una fila de asset accionable derivada de FPDSSnapshotDiff. */
struct FPDSSnapshotDiffBrowserEntry
{
    EPDSSnapshotDiffChangeKind ChangeKind = EPDSSnapshotDiffChangeKind::Added;
    FString ObjectPath;
    FString PreviousClassPath;
    FString CurrentClassPath;
    EPDSAssetOrigin PreviousOrigin = EPDSAssetOrigin::Unknown;
    EPDSAssetOrigin CurrentOrigin = EPDSAssetOrigin::Unknown;
    bool bExistsInCurrentSnapshot = false;

    FString GetDisplayClassPath() const;
    EPDSAssetOrigin GetDisplayOrigin() const;
    bool CanLocateOrOpen() const;
};

/** Estado puro de filtros. Se prueba sin Slate ni acceso a disco. */
struct FPDSSnapshotDiffBrowserFilter
{
    TOptional<EPDSSnapshotDiffChangeKind> ChangeKind;
    TOptional<EPDSAssetOrigin> Origin;
    FString ClassPath;
    FString SearchText;

    bool Matches(const FPDSSnapshotDiffBrowserEntry& Entry) const;
};

namespace PDSSnapshotDiffBrowser
{
    FString ChangeKindToString(EPDSSnapshotDiffChangeKind ChangeKind);

    /** Lee y normaliza un snapshot sin cargar ningún asset. */
    bool LoadSnapshotDocument(
        const FString& SnapshotPath,
        FPDSSnapshotDocument& OutDocument,
        TArray<FPDSIssue>& OutIssues);

    /**
     * Resuelve un descriptor mediante caché por ruta, tamaño y fecha de modificación.
     * Devuelve false solo si el archivo ya no puede stat-earse; un JSON inválido se
     * representa mediante OutDescriptor.bIsValid = false y conserva sus issues.
     */
    bool ResolveSnapshotFileDescriptor(
        const FString& SnapshotPath,
        TMap<FString, FPDSSnapshotDescriptorCacheEntry>& DescriptorCache,
        FPDSSnapshotFileDescriptor& OutDescriptor,
        TArray<FPDSIssue>& OutIssues,
        bool* bOutCacheHit = nullptr);

    /** Enumera sin reutilizar caché. Útil para callers puntuales y compatibilidad. */
    TArray<FPDSSnapshotFileDescriptor> EnumerateSnapshotFiles(
        TArray<FPDSIssue>& OutIssues);

    /**
     * Enumera baseline, latest y snapshots timestamped con orden determinista,
     * reutilizando descriptores cuyo archivo no cambió.
     */
    TArray<FPDSSnapshotFileDescriptor> EnumerateSnapshotFiles(
        TArray<FPDSIssue>& OutIssues,
        TMap<FString, FPDSSnapshotDescriptorCacheEntry>& DescriptorCache);

    /** Convierte únicamente los cambios de assets en filas accionables. */
    void BuildAssetEntries(
        const FPDSSnapshotDiff& Diff,
        TArray<FPDSSnapshotDiffBrowserEntry>& OutEntries);

    /** Aplica filtros sin mutar la colección fuente. */
    void ApplyFilter(
        const TArray<FPDSSnapshotDiffBrowserEntry>& SourceEntries,
        const FPDSSnapshotDiffBrowserFilter& Filter,
        TArray<FPDSSnapshotDiffBrowserEntry>& OutEntries);
}
