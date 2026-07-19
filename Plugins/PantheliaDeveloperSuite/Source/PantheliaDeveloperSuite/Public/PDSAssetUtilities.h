#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "PDSDeveloperTypes.h"

/** Estadísticas del filtrado aplicado antes de invocar Data Validation. */
struct FPDSAssetQueryStats
{
    int32 NumExcludedMaps = 0;
    int32 NumExcludedExternalObjects = 0;
    int32 NumExcludedByConfiguration = 0;
    int32 NumOutsideSelectedProfile = 0;
};

/**
 * Assets y orígenes paralelos de una consulta de validación.
 * Mantener ambos arrays alineados evita reclasificar miles de assets después del filtrado.
 */
struct FPDSAssetSelection
{
    TArray<FAssetData> Assets;
    TArray<EPDSAssetOrigin> Origins;

    bool HasAlignedOrigins() const
    {
        return Assets.Num() == Origins.Num();
    }
};

/**
 * Resolver inmutable construido una vez por operación.
 * Normaliza y cachea las raíces de Project Settings para evitar trabajo repetido por asset.
 */
struct FPDSOriginResolver
{
public:
    FPDSOriginResolver() = default;

    FPDSOriginResolver(
        const TArray<FString>& InPantheliaCorePackageRoots,
        const TArray<FString>& InExternalContentPackageRoots,
        const TArray<FString>& InAlwaysExcludedPackageRoots = TArray<FString>());

    /** Construye el resolver desde UPDSDeveloperSettings. */
    static FPDSOriginResolver FromSettings();

    /** Normaliza la ruta una vez y devuelve su origen. */
    EPDSAssetOrigin Classify(const FString& AssetOrPackagePath) const;

    /** Clasifica una ruta que ya fue normalizada con NormalizePackageRoot. */
    EPDSAssetOrigin ClassifyNormalizedPath(const FString& NormalizedPath) const;

    /** Normaliza la ruta una vez y comprueba Always Excluded. */
    bool IsExcluded(const FString& AssetOrPackagePath) const;

    /** Comprueba Always Excluded sobre una ruta ya normalizada. */
    bool IsExcludedNormalizedPath(const FString& NormalizedPath) const;

private:
    TArray<FString> PantheliaCorePackageRoots;
    TArray<FString> ExternalContentPackageRoots;
    TArray<FString> AlwaysExcludedPackageRoots;
};

namespace PDSAssetUtilities
{
    /** Obtiene la selección actual del Content Browser. */
    TArray<FAssetData> GetSelectedAssets();

    /** Obtiene todos los assets registrados bajo /Game sin cargarlos. */
    TArray<FAssetData> GetAllGameAssets();

    /** Devuelve true para paquetes World Partition bajo ExternalActors/ExternalObjects. */
    bool IsExternalObjectPackagePath(FName PackagePath);

    /** Normaliza una raíz configurable a formato /Game/... sin slash final. */
    FString NormalizePackageRoot(const FString& PackageRoot);

    /** Comprueba límites de carpeta; /Game/Fab no coincide con /Game/Fabricated. */
    bool IsPathUnderPackageRoot(const FString& Path, const FString& PackageRoot);

    /** Comprueba una ruta contra una colección de raíces configurables. */
    bool IsPathUnderAnyPackageRoot(
        const FString& Path,
        const TArray<FString>& PackageRoots);

    /**
     * Clasifica usando raíces explícitas. External tiene prioridad sobre Core cuando se solapan.
     * Esta variante mantiene el contrato comprobable sin mutar Project Settings en tests.
     */
    EPDSAssetOrigin ClassifyAssetOriginWithRoots(
        const FString& AssetOrPackagePath,
        const TArray<FString>& PantheliaCorePackageRoots,
        const TArray<FString>& ExternalContentPackageRoots);

    /** Clasifica la procedencia con las raíces definidas en Project Settings. */
    EPDSAssetOrigin ClassifyAssetOrigin(const FString& AssetOrPackagePath);

    /**
     * Obtiene assets y orígenes ya clasificados para el perfil solicitado.
     * Es la ruta preferida para operaciones masivas porque construye un solo resolver.
     */
    FPDSAssetSelection GetGameAssetSelectionForValidationProfile(
        EPDSValidationProfile Profile,
        FPDSAssetQueryStats* OutStats = nullptr);

    /**
     * Compatibilidad: devuelve solo los assets del perfil solicitado.
     * Internamente usa GetGameAssetSelectionForValidationProfile.
     */
    TArray<FAssetData> GetGameAssetsForValidationProfile(
        EPDSValidationProfile Profile,
        FPDSAssetQueryStats* OutStats = nullptr);

    /**
     * Compatibilidad con v0.1.2: equivale al perfil Entire Project.
     * Excluye mapas UWorld y paquetes de actores/objetos externos para evitar
     * cargas costosas o efectos secundarios durante una validación global.
     */
    TArray<FAssetData> GetAllGameAssetsForValidation(
        int32* OutExcludedMaps = nullptr,
        int32* OutExcludedExternalObjects = nullptr);

    /**
     * Obtiene el inventario del snapshot. Conserva mapas, pero excluye paquetes
     * World Partition externos para no inundar la salida destinada a una IA.
     */
    TArray<FAssetData> GetAllGameAssetsForSnapshot(
        int32* OutExcludedExternalObjects = nullptr);

    /** Directorio raíz persistente de la suite. */
    FString GetSuiteSavedDirectory();

    /** Crea el directorio si todavía no existe. */
    bool EnsureDirectoryExists(const FString& Directory);

    /** Sufijo UTC con milisegundos para evitar colisiones de evidencia. */
    FString MakeTimestampForFileName();

    /**
     * Escribe a un temporal en el mismo directorio y después reemplaza el destino.
     * Se usa para rutas estables como latest.json, que pueden ser leídas por otra IA.
     */
    bool SaveStringAtomically(const FString& Contents, const FString& DestinationPath);
}
