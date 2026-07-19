#include "PDSAssetUtilities.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "ContentBrowserModule.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "IContentBrowserSingleton.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "PDSDeveloperSettings.h"

namespace
{
    bool IsNormalizedPathUnderNormalizedRoot(
        const FString& NormalizedPath,
        const FString& NormalizedRoot)
    {
        if (NormalizedPath.IsEmpty() || NormalizedRoot.IsEmpty())
        {
            return false;
        }

        return NormalizedPath.Equals(NormalizedRoot, ESearchCase::IgnoreCase)
            || NormalizedPath.StartsWith(
                NormalizedRoot + TEXT("/"),
                ESearchCase::IgnoreCase);
    }

    bool IsNormalizedPathUnderAnyNormalizedRoot(
        const FString& NormalizedPath,
        const TArray<FString>& NormalizedRoots)
    {
        return NormalizedRoots.ContainsByPredicate(
            [&NormalizedPath](const FString& NormalizedRoot)
            {
                return IsNormalizedPathUnderNormalizedRoot(
                    NormalizedPath,
                    NormalizedRoot);
            });
    }

    TArray<FString> NormalizePackageRoots(const TArray<FString>& PackageRoots)
    {
        TArray<FString> NormalizedRoots;
        NormalizedRoots.Reserve(PackageRoots.Num());

        for (const FString& PackageRoot : PackageRoots)
        {
            const FString NormalizedRoot =
                PDSAssetUtilities::NormalizePackageRoot(PackageRoot);

            if (NormalizedRoot.IsEmpty())
            {
                continue;
            }

            const bool bAlreadyPresent = NormalizedRoots.ContainsByPredicate(
                [&NormalizedRoot](const FString& ExistingRoot)
                {
                    return ExistingRoot.Equals(
                        NormalizedRoot,
                        ESearchCase::IgnoreCase);
                });

            if (!bAlreadyPresent)
            {
                NormalizedRoots.Add(NormalizedRoot);
            }
        }

        return NormalizedRoots;
    }

    bool DoesOriginMatchProfile(
        const EPDSAssetOrigin Origin,
        const EPDSValidationProfile Profile)
    {
        switch (Profile)
        {
        case EPDSValidationProfile::PantheliaCore:
            return Origin == EPDSAssetOrigin::PantheliaCore;
        case EPDSValidationProfile::GameContent:
            return Origin == EPDSAssetOrigin::PantheliaCore
                || Origin == EPDSAssetOrigin::GameContent;
        case EPDSValidationProfile::ExternalContent:
            return Origin == EPDSAssetOrigin::ExternalContent;
        case EPDSValidationProfile::EntireProject:
            return true;
        case EPDSValidationProfile::SelectedAssets:
        default:
            return false;
        }
    }
}

FPDSOriginResolver::FPDSOriginResolver(
    const TArray<FString>& InPantheliaCorePackageRoots,
    const TArray<FString>& InExternalContentPackageRoots,
    const TArray<FString>& InAlwaysExcludedPackageRoots)
    : PantheliaCorePackageRoots(NormalizePackageRoots(InPantheliaCorePackageRoots))
    , ExternalContentPackageRoots(NormalizePackageRoots(InExternalContentPackageRoots))
    , AlwaysExcludedPackageRoots(NormalizePackageRoots(InAlwaysExcludedPackageRoots))
{
}

FPDSOriginResolver FPDSOriginResolver::FromSettings()
{
    const UPDSDeveloperSettings* Settings = GetDefault<UPDSDeveloperSettings>();
    if (!Settings)
    {
        return FPDSOriginResolver();
    }

    return FPDSOriginResolver(
        Settings->PantheliaCorePackageRoots,
        Settings->ExternalContentPackageRoots,
        Settings->AlwaysExcludedPackageRoots);
}

EPDSAssetOrigin FPDSOriginResolver::Classify(
    const FString& AssetOrPackagePath) const
{
    return ClassifyNormalizedPath(
        PDSAssetUtilities::NormalizePackageRoot(AssetOrPackagePath));
}

EPDSAssetOrigin FPDSOriginResolver::ClassifyNormalizedPath(
    const FString& NormalizedPath) const
{
    // Contrato deliberado: External tiene prioridad sobre Core en solapamientos.
    if (IsNormalizedPathUnderAnyNormalizedRoot(
        NormalizedPath,
        ExternalContentPackageRoots))
    {
        return EPDSAssetOrigin::ExternalContent;
    }

    if (IsNormalizedPathUnderAnyNormalizedRoot(
        NormalizedPath,
        PantheliaCorePackageRoots))
    {
        return EPDSAssetOrigin::PantheliaCore;
    }

    if (IsNormalizedPathUnderNormalizedRoot(
        NormalizedPath,
        TEXT("/Game")))
    {
        return EPDSAssetOrigin::GameContent;
    }

    return EPDSAssetOrigin::Unknown;
}

bool FPDSOriginResolver::IsExcluded(
    const FString& AssetOrPackagePath) const
{
    return IsExcludedNormalizedPath(
        PDSAssetUtilities::NormalizePackageRoot(AssetOrPackagePath));
}

bool FPDSOriginResolver::IsExcludedNormalizedPath(
    const FString& NormalizedPath) const
{
    return IsNormalizedPathUnderAnyNormalizedRoot(
        NormalizedPath,
        AlwaysExcludedPackageRoots);
}

TArray<FAssetData> PDSAssetUtilities::GetSelectedAssets()
{
    TArray<FAssetData> SelectedAssets;

    FContentBrowserModule& ContentBrowserModule =
        FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
    ContentBrowserModule.Get().GetSelectedAssets(SelectedAssets);

    return SelectedAssets;
}

TArray<FAssetData> PDSAssetUtilities::GetAllGameAssets()
{
    TArray<FAssetData> Assets;

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();
    AssetRegistry.WaitForCompletion();
    AssetRegistry.GetAssetsByPath(FName(TEXT("/Game")), Assets, true, false);

    return Assets;
}

bool PDSAssetUtilities::IsExternalObjectPackagePath(const FName PackagePath)
{
    const FString Path = PackagePath.ToString();
    return Path.StartsWith(TEXT("/Game/__ExternalActors__"))
        || Path.StartsWith(TEXT("/Game/__ExternalObjects__"));
}

FString PDSAssetUtilities::NormalizePackageRoot(const FString& PackageRoot)
{
    FString Normalized = PackageRoot;
    Normalized.TrimStartAndEndInline();
    Normalized.ReplaceInline(TEXT("\\"), TEXT("/"));

    if (Normalized.IsEmpty())
    {
        return Normalized;
    }

    if (!Normalized.StartsWith(TEXT("/")))
    {
        if (Normalized.StartsWith(TEXT("Game/")))
        {
            Normalized = TEXT("/") + Normalized;
        }
        else
        {
            Normalized = TEXT("/Game/") + Normalized;
        }
    }

    while (Normalized.Len() > 1 && Normalized.EndsWith(TEXT("/")))
    {
        Normalized = Normalized.LeftChop(1);
    }

    return Normalized;
}

bool PDSAssetUtilities::IsPathUnderPackageRoot(
    const FString& Path,
    const FString& PackageRoot)
{
    const FString NormalizedPath = NormalizePackageRoot(Path);
    const FString NormalizedRoot = NormalizePackageRoot(PackageRoot);

    return IsNormalizedPathUnderNormalizedRoot(
        NormalizedPath,
        NormalizedRoot);
}

bool PDSAssetUtilities::IsPathUnderAnyPackageRoot(
    const FString& Path,
    const TArray<FString>& PackageRoots)
{
    const FString NormalizedPath = NormalizePackageRoot(Path);
    const TArray<FString> NormalizedRoots = NormalizePackageRoots(PackageRoots);

    return IsNormalizedPathUnderAnyNormalizedRoot(
        NormalizedPath,
        NormalizedRoots);
}

EPDSAssetOrigin PDSAssetUtilities::ClassifyAssetOriginWithRoots(
    const FString& AssetOrPackagePath,
    const TArray<FString>& PantheliaCorePackageRoots,
    const TArray<FString>& ExternalContentPackageRoots)
{
    const FPDSOriginResolver Resolver(
        PantheliaCorePackageRoots,
        ExternalContentPackageRoots);
    return Resolver.Classify(AssetOrPackagePath);
}

EPDSAssetOrigin PDSAssetUtilities::ClassifyAssetOrigin(
    const FString& AssetOrPackagePath)
{
    return FPDSOriginResolver::FromSettings().Classify(AssetOrPackagePath);
}

FPDSAssetSelection PDSAssetUtilities::GetGameAssetSelectionForValidationProfile(
    const EPDSValidationProfile Profile,
    FPDSAssetQueryStats* OutStats)
{
    FPDSAssetQueryStats LocalStats;
    FPDSAssetQueryStats& Stats = OutStats ? *OutStats : LocalStats;
    Stats = FPDSAssetQueryStats();

    if (Profile == EPDSValidationProfile::SelectedAssets)
    {
        ensureMsgf(
            false,
            TEXT("SelectedAssets debe obtenerse desde GetSelectedAssets(), no mediante un perfil masivo."));
        return FPDSAssetSelection();
    }

    const FPDSOriginResolver OriginResolver =
        FPDSOriginResolver::FromSettings();

    FPDSAssetSelection Selection;
    const TArray<FAssetData> AllAssets = GetAllGameAssets();
    Selection.Assets.Reserve(AllAssets.Num());
    Selection.Origins.Reserve(AllAssets.Num());

    for (const FAssetData& AssetData : AllAssets)
    {
        if (AssetData.IsInstanceOf(UWorld::StaticClass(), EResolveClass::No))
        {
            ++Stats.NumExcludedMaps;
            continue;
        }

        if (IsExternalObjectPackagePath(AssetData.PackagePath))
        {
            ++Stats.NumExcludedExternalObjects;
            continue;
        }

        // Se normaliza una sola vez por asset y se reutiliza para exclusión y clasificación.
        const FString NormalizedPackagePath =
            NormalizePackageRoot(AssetData.PackagePath.ToString());

        if (OriginResolver.IsExcludedNormalizedPath(NormalizedPackagePath))
        {
            ++Stats.NumExcludedByConfiguration;
            continue;
        }

        const EPDSAssetOrigin Origin =
            OriginResolver.ClassifyNormalizedPath(NormalizedPackagePath);

        if (!DoesOriginMatchProfile(Origin, Profile))
        {
            ++Stats.NumOutsideSelectedProfile;
            continue;
        }

        Selection.Assets.Add(AssetData);
        Selection.Origins.Add(Origin);
    }

    ensureMsgf(
        Selection.HasAlignedOrigins(),
        TEXT("La selección de validación debe conservar Assets y Origins alineados."));

    return Selection;
}

TArray<FAssetData> PDSAssetUtilities::GetGameAssetsForValidationProfile(
    const EPDSValidationProfile Profile,
    FPDSAssetQueryStats* OutStats)
{
    FPDSAssetSelection Selection =
        GetGameAssetSelectionForValidationProfile(Profile, OutStats);
    return MoveTemp(Selection.Assets);
}

TArray<FAssetData> PDSAssetUtilities::GetAllGameAssetsForValidation(
    int32* OutExcludedMaps,
    int32* OutExcludedExternalObjects)
{
    FPDSAssetQueryStats Stats;
    TArray<FAssetData> Assets = GetGameAssetsForValidationProfile(
        EPDSValidationProfile::EntireProject,
        &Stats);

    if (OutExcludedMaps)
    {
        *OutExcludedMaps = Stats.NumExcludedMaps;
    }

    if (OutExcludedExternalObjects)
    {
        *OutExcludedExternalObjects = Stats.NumExcludedExternalObjects;
    }

    return Assets;
}

TArray<FAssetData> PDSAssetUtilities::GetAllGameAssetsForSnapshot(
    int32* OutExcludedExternalObjects)
{
    if (OutExcludedExternalObjects)
    {
        *OutExcludedExternalObjects = 0;
    }

    TArray<FAssetData> SnapshotAssets;
    const TArray<FAssetData> AllAssets = GetAllGameAssets();
    SnapshotAssets.Reserve(AllAssets.Num());

    for (const FAssetData& AssetData : AllAssets)
    {
        if (IsExternalObjectPackagePath(AssetData.PackagePath))
        {
            if (OutExcludedExternalObjects)
            {
                ++(*OutExcludedExternalObjects);
            }
            continue;
        }

        SnapshotAssets.Add(AssetData);
    }

    return SnapshotAssets;
}

FString PDSAssetUtilities::GetSuiteSavedDirectory()
{
    return FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("PantheliaDeveloperSuite"));
}

bool PDSAssetUtilities::EnsureDirectoryExists(const FString& Directory)
{
    return IFileManager::Get().MakeDirectory(*Directory, true);
}

FString PDSAssetUtilities::MakeTimestampForFileName()
{
    const FDateTime Now = FDateTime::UtcNow();
    return FString::Printf(
        TEXT("%s_%03d"),
        *Now.ToString(TEXT("%Y%m%d_%H%M%S")),
        Now.GetMillisecond());
}

bool PDSAssetUtilities::SaveStringAtomically(
    const FString& Contents,
    const FString& DestinationPath)
{
    const FString DestinationDirectory = FPaths::GetPath(DestinationPath);
    if (!EnsureDirectoryExists(DestinationDirectory))
    {
        return false;
    }

    const FString TemporaryPath = DestinationPath + TEXT(".tmp");
    IFileManager& FileManager = IFileManager::Get();
    FileManager.Delete(*TemporaryPath, false, true, true);

    if (!FFileHelper::SaveStringToFile(Contents, *TemporaryPath))
    {
        return false;
    }

    const bool bMoved = FileManager.Move(
        *DestinationPath,
        *TemporaryPath,
        true,
        true,
        false,
        true);

    if (!bMoved)
    {
        FileManager.Delete(*TemporaryPath, false, true, true);
    }

    return bMoved;
}
