#include "Misc/AutomationTest.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "PDSSnapshotDiffBrowserTypes.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    FString MakeCacheTestSnapshotJson(const bool bIncludeSecondAsset)
    {
        FString Assets =
            TEXT("[{\"objectPath\":\"/Game/A.A\",\"packageName\":\"/Game/A\",\"packagePath\":\"/Game\",\"classPath\":\"/Script/Engine.DataAsset\",\"origin\":\"Panthelia Core\"}");
        if (bIncludeSecondAsset)
        {
            Assets +=
                TEXT(",{\"objectPath\":\"/Game/B.B\",\"packageName\":\"/Game/B\",\"packagePath\":\"/Game\",\"classPath\":\"/Script/Engine.DataAsset\",\"origin\":\"Game Content\"}");
        }
        Assets += TEXT("]");

        return FString::Printf(
            TEXT("{\"schemaVersion\":\"0.3.0-alpha4\",\"generatedAtUtc\":\"2026-07-19T12:00:00Z\","
                 "\"project\":{\"name\":\"PantheliaProject\",\"engineVersion\":\"5.8\"},"
                 "\"assetInventory\":%s,\"gameplayTags\":[],\"montages\":[],"
                 "\"validation\":{\"included\":false}}"),
            *Assets);
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSSnapshotDiffBrowserEntryProjectionTest,
    "Panthelia.DeveloperSuite.SnapshotDiffBrowser.EntryProjection",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSSnapshotDiffBrowserEntryProjectionTest::RunTest(const FString& Parameters)
{
    FPDSSnapshotDiff Diff;

    FPDSSnapshotAssetRecord AddedAsset;
    AddedAsset.ObjectPath = TEXT("/Game/New.New");
    AddedAsset.PackageName = TEXT("/Game/New");
    AddedAsset.PackagePath = TEXT("/Game");
    AddedAsset.ClassPath = TEXT("/Script/Engine.DataAsset");
    AddedAsset.Origin = EPDSAssetOrigin::PantheliaCore;
    Diff.AddedAssets.Add(MoveTemp(AddedAsset));

    FPDSSnapshotAssetRecord RemovedAsset;
    RemovedAsset.ObjectPath = TEXT("/Game/Old.Old");
    RemovedAsset.PackageName = TEXT("/Game/Old");
    RemovedAsset.PackagePath = TEXT("/Game");
    RemovedAsset.ClassPath = TEXT("/Script/Engine.Blueprint");
    RemovedAsset.Origin = EPDSAssetOrigin::GameContent;
    Diff.RemovedAssets.Add(MoveTemp(RemovedAsset));

    FPDSSnapshotAssetChange ChangedAsset;
    ChangedAsset.ObjectPath = TEXT("/Game/Changed.Changed");
    ChangedAsset.PreviousClassPath = TEXT("/Script/Engine.DataAsset");
    ChangedAsset.CurrentClassPath = TEXT("/Script/Engine.PrimaryDataAsset");
    ChangedAsset.PreviousOrigin = EPDSAssetOrigin::PantheliaCore;
    ChangedAsset.CurrentOrigin = EPDSAssetOrigin::PantheliaCore;
    Diff.ChangedAssets.Add(MoveTemp(ChangedAsset));

    TArray<FPDSSnapshotDiffBrowserEntry> Entries;
    PDSSnapshotDiffBrowser::BuildAssetEntries(Diff, Entries);

    TestEqual(TEXT("Three asset rows projected"), Entries.Num(), 3);
    if (Entries.Num() == 3)
    {
        TestEqual(
            TEXT("Added row first by deterministic change ordering"),
            static_cast<uint8>(Entries[0].ChangeKind),
            static_cast<uint8>(EPDSSnapshotDiffChangeKind::Added));
        TestTrue(TEXT("Added asset is actionable"), Entries[0].CanLocateOrOpen());

        TestEqual(
            TEXT("Removed row second"),
            static_cast<uint8>(Entries[1].ChangeKind),
            static_cast<uint8>(EPDSSnapshotDiffChangeKind::Removed));
        TestFalse(TEXT("Removed asset is not actionable"), Entries[1].CanLocateOrOpen());

        TestEqual(
            TEXT("Modified row third"),
            static_cast<uint8>(Entries[2].ChangeKind),
            static_cast<uint8>(EPDSSnapshotDiffChangeKind::Modified));
        TestEqual(
            TEXT("Modified row exposes current class"),
            Entries[2].GetDisplayClassPath(),
            FString(TEXT("/Script/Engine.PrimaryDataAsset")));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSSnapshotDiffBrowserFilteringTest,
    "Panthelia.DeveloperSuite.SnapshotDiffBrowser.Filtering",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSSnapshotDiffBrowserFilteringTest::RunTest(const FString& Parameters)
{
    TArray<FPDSSnapshotDiffBrowserEntry> Entries;

    FPDSSnapshotDiffBrowserEntry AddedEntry;
    AddedEntry.ChangeKind = EPDSSnapshotDiffChangeKind::Added;
    AddedEntry.ObjectPath = TEXT("/Game/Characters/Hero.Hero");
    AddedEntry.CurrentClassPath = TEXT("/Script/Engine.Blueprint");
    AddedEntry.CurrentOrigin = EPDSAssetOrigin::PantheliaCore;
    AddedEntry.bExistsInCurrentSnapshot = true;
    Entries.Add(MoveTemp(AddedEntry));

    FPDSSnapshotDiffBrowserEntry RemovedEntry;
    RemovedEntry.ChangeKind = EPDSSnapshotDiffChangeKind::Removed;
    RemovedEntry.ObjectPath = TEXT("/Game/ARPG_Pack/Legacy.Legacy");
    RemovedEntry.PreviousClassPath = TEXT("/Script/Engine.DataAsset");
    RemovedEntry.PreviousOrigin = EPDSAssetOrigin::ExternalContent;
    RemovedEntry.bExistsInCurrentSnapshot = false;
    Entries.Add(MoveTemp(RemovedEntry));

    FPDSSnapshotDiffBrowserEntry ModifiedEntry;
    ModifiedEntry.ChangeKind = EPDSSnapshotDiffChangeKind::Modified;
    ModifiedEntry.ObjectPath = TEXT("/Game/UI/WBP_HUD.WBP_HUD");
    ModifiedEntry.PreviousClassPath = TEXT("/Script/UMGEditor.WidgetBlueprint");
    ModifiedEntry.CurrentClassPath = TEXT("/Script/UMGEditor.WidgetBlueprint");
    ModifiedEntry.PreviousOrigin = EPDSAssetOrigin::PantheliaCore;
    ModifiedEntry.CurrentOrigin = EPDSAssetOrigin::PantheliaCore;
    ModifiedEntry.bExistsInCurrentSnapshot = true;
    Entries.Add(MoveTemp(ModifiedEntry));

    FPDSSnapshotDiffBrowserFilter Filter;
    Filter.ChangeKind = EPDSSnapshotDiffChangeKind::Modified;
    Filter.Origin = EPDSAssetOrigin::PantheliaCore;
    Filter.ClassPath = TEXT("/script/umgeditor.widgetblueprint");
    Filter.SearchText = TEXT("hud");

    TArray<FPDSSnapshotDiffBrowserEntry> Filtered;
    PDSSnapshotDiffBrowser::ApplyFilter(Entries, Filter, Filtered);
    TestEqual(TEXT("Combined filters select one row"), Filtered.Num(), 1);
    if (Filtered.Num() == 1)
    {
        TestEqual(
            TEXT("Expected HUD row selected"),
            Filtered[0].ObjectPath,
            FString(TEXT("/Game/UI/WBP_HUD.WBP_HUD")));
    }

    Filter = FPDSSnapshotDiffBrowserFilter();
    Filter.SearchText = TEXT("ARPG_pack");
    PDSSnapshotDiffBrowser::ApplyFilter(Entries, Filter, Filtered);
    TestEqual(TEXT("Search is case-insensitive"), Filtered.Num(), 1);
    if (Filtered.Num() == 1)
    {
        TestFalse(TEXT("Removed search result remains non-actionable"), Filtered[0].CanLocateOrOpen());
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPDSSnapshotDiffBrowserDescriptorCacheTest,
    "Panthelia.DeveloperSuite.SnapshotDiffBrowser.DescriptorCache",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FPDSSnapshotDiffBrowserDescriptorCacheTest::RunTest(const FString& Parameters)
{
    const FString TestDirectory = FPaths::Combine(
        FPaths::ProjectSavedDir(),
        TEXT("PantheliaDeveloperSuite"),
        TEXT("Automation"),
        FString(TEXT("DescriptorCache_")) + FGuid::NewGuid().ToString(EGuidFormats::Digits));
    const FString SnapshotPath = FPaths::Combine(
        TestDirectory,
        TEXT("PantheliaSnapshot_CacheTest.json"));

    IFileManager::Get().MakeDirectory(*TestDirectory, true);
    TestTrue(
        TEXT("Initial cache test snapshot saved"),
        FFileHelper::SaveStringToFile(MakeCacheTestSnapshotJson(false), *SnapshotPath));

    TMap<FString, FPDSSnapshotDescriptorCacheEntry> Cache;
    FPDSSnapshotFileDescriptor Descriptor;
    TArray<FPDSIssue> Issues;
    bool bCacheHit = true;

    TestTrue(
        TEXT("First descriptor resolution succeeds"),
        PDSSnapshotDiffBrowser::ResolveSnapshotFileDescriptor(
            SnapshotPath,
            Cache,
            Descriptor,
            Issues,
            &bCacheHit));
    TestFalse(TEXT("First descriptor resolution is a cache miss"), bCacheHit);
    TestTrue(TEXT("First descriptor is valid"), Descriptor.bIsValid);
    TestEqual(TEXT("First descriptor has one asset"), Descriptor.AssetCount, 1);

    Issues.Reset();
    TestTrue(
        TEXT("Second descriptor resolution succeeds"),
        PDSSnapshotDiffBrowser::ResolveSnapshotFileDescriptor(
            SnapshotPath,
            Cache,
            Descriptor,
            Issues,
            &bCacheHit));
    TestTrue(TEXT("Unchanged file produces a cache hit"), bCacheHit);

    TestTrue(
        TEXT("Modified cache test snapshot saved"),
        FFileHelper::SaveStringToFile(MakeCacheTestSnapshotJson(true), *SnapshotPath));
    Issues.Reset();
    TestTrue(
        TEXT("Modified descriptor resolution succeeds"),
        PDSSnapshotDiffBrowser::ResolveSnapshotFileDescriptor(
            SnapshotPath,
            Cache,
            Descriptor,
            Issues,
            &bCacheHit));
    TestFalse(TEXT("Size change invalidates the cache"), bCacheHit);
    TestEqual(TEXT("Invalidated descriptor has two assets"), Descriptor.AssetCount, 2);

    IFileManager::Get().DeleteDirectory(*TestDirectory, false, true);
    return true;
}

#endif
