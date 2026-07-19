#include "PDSSnapshotDiffBrowserTypes.h"

#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "PDSDeveloperSettings.h"
#include "PDSDeveloperTypes.h"
#include "PDSProjectSnapshotDiffService.h"

namespace
{
    int32 GetDescriptorSortRank(const FPDSSnapshotFileDescriptor& Descriptor)
    {
        if (Descriptor.bIsBaseline)
        {
            return 0;
        }
        if (Descriptor.bIsLatest)
        {
            return 1;
        }
        return 2;
    }

    FString NormalizeSnapshotPath(FString SnapshotPath)
    {
        SnapshotPath = FPaths::ConvertRelativePathToFull(SnapshotPath);
        FPaths::NormalizeFilename(SnapshotPath);
        return SnapshotPath;
    }

    void InitializeDescriptorIdentity(
        const FString& NormalizedPath,
        FPDSSnapshotFileDescriptor& OutDescriptor)
    {
        OutDescriptor = FPDSSnapshotFileDescriptor();
        OutDescriptor.FilePath = NormalizedPath;
        OutDescriptor.FileName = FPaths::GetCleanFilename(NormalizedPath);
        OutDescriptor.bIsBaseline = OutDescriptor.FileName.Equals(
            TEXT("baseline.json"),
            ESearchCase::IgnoreCase);
        OutDescriptor.bIsLatest = OutDescriptor.FileName.Equals(
            TEXT("latest.json"),
            ESearchCase::IgnoreCase);
        OutDescriptor.bIsTimestamped = OutDescriptor.FileName.StartsWith(
            TEXT("PantheliaSnapshot_"),
            ESearchCase::IgnoreCase);
    }

    FString BuildDescriptorLabel(const FPDSSnapshotFileDescriptor& Descriptor)
    {
        FString Prefix;
        if (Descriptor.bIsBaseline)
        {
            Prefix = TEXT("Baseline");
        }
        else if (Descriptor.bIsLatest)
        {
            Prefix = TEXT("Latest");
        }
        else
        {
            Prefix = FPaths::GetBaseFilename(Descriptor.FileName);
        }

        if (!Descriptor.bIsValid)
        {
            return FString::Printf(TEXT("%s — INVALID"), *Prefix);
        }

        return FString::Printf(
            TEXT("%s — %s — %d assets — schema %s"),
            *Prefix,
            Descriptor.GeneratedAtUtc.IsEmpty() ? TEXT("fecha desconocida") : *Descriptor.GeneratedAtUtc,
            Descriptor.AssetCount,
            Descriptor.SchemaVersion.IsEmpty() ? TEXT("desconocido") : *Descriptor.SchemaVersion);
    }
}

FString FPDSSnapshotDiffBrowserEntry::GetDisplayClassPath() const
{
    return ChangeKind == EPDSSnapshotDiffChangeKind::Removed
        ? PreviousClassPath
        : CurrentClassPath;
}

EPDSAssetOrigin FPDSSnapshotDiffBrowserEntry::GetDisplayOrigin() const
{
    return ChangeKind == EPDSSnapshotDiffChangeKind::Removed
        ? PreviousOrigin
        : CurrentOrigin;
}

bool FPDSSnapshotDiffBrowserEntry::CanLocateOrOpen() const
{
    return bExistsInCurrentSnapshot && !ObjectPath.IsEmpty();
}

bool FPDSSnapshotDiffBrowserFilter::Matches(
    const FPDSSnapshotDiffBrowserEntry& Entry) const
{
    if (ChangeKind.IsSet() && Entry.ChangeKind != ChangeKind.GetValue())
    {
        return false;
    }

    if (Origin.IsSet() && Entry.GetDisplayOrigin() != Origin.GetValue())
    {
        return false;
    }

    const FString DisplayClass = Entry.GetDisplayClassPath();
    if (!ClassPath.IsEmpty()
        && !DisplayClass.Equals(ClassPath, ESearchCase::IgnoreCase))
    {
        return false;
    }

    const FString TrimmedSearch = SearchText.TrimStartAndEnd();
    if (!TrimmedSearch.IsEmpty())
    {
        const bool bMatchesPath = Entry.ObjectPath.Contains(
            TrimmedSearch,
            ESearchCase::IgnoreCase);
        const bool bMatchesPreviousClass = Entry.PreviousClassPath.Contains(
            TrimmedSearch,
            ESearchCase::IgnoreCase);
        const bool bMatchesCurrentClass = Entry.CurrentClassPath.Contains(
            TrimmedSearch,
            ESearchCase::IgnoreCase);
        if (!bMatchesPath && !bMatchesPreviousClass && !bMatchesCurrentClass)
        {
            return false;
        }
    }

    return true;
}

FString PDSSnapshotDiffBrowser::ChangeKindToString(
    const EPDSSnapshotDiffChangeKind ChangeKind)
{
    switch (ChangeKind)
    {
    case EPDSSnapshotDiffChangeKind::Added:
        return TEXT("Added");
    case EPDSSnapshotDiffChangeKind::Removed:
        return TEXT("Removed");
    case EPDSSnapshotDiffChangeKind::Modified:
        return TEXT("Modified");
    default:
        return TEXT("Unknown");
    }
}

bool PDSSnapshotDiffBrowser::LoadSnapshotDocument(
    const FString& SnapshotPath,
    FPDSSnapshotDocument& OutDocument,
    TArray<FPDSIssue>& OutIssues)
{
    FString JsonText;
    if (!FFileHelper::LoadFileToString(JsonText, *SnapshotPath))
    {
        OutIssues.Add({
            EPDSIssueSeverity::Error,
            TEXT("PDS.SnapshotDiffBrowser.LoadFailed"),
            SnapshotPath,
            TEXT("No fue posible leer el snapshot JSON.")
        });
        return false;
    }

    return PDSSnapshotDiff::ParseSnapshotJson(
        JsonText,
        SnapshotPath,
        OutDocument,
        OutIssues);
}

bool PDSSnapshotDiffBrowser::ResolveSnapshotFileDescriptor(
    const FString& SnapshotPath,
    TMap<FString, FPDSSnapshotDescriptorCacheEntry>& DescriptorCache,
    FPDSSnapshotFileDescriptor& OutDescriptor,
    TArray<FPDSIssue>& OutIssues,
    bool* bOutCacheHit)
{
    if (bOutCacheHit)
    {
        *bOutCacheHit = false;
    }

    const FString NormalizedPath = NormalizeSnapshotPath(SnapshotPath);
    const FFileStatData StatData = IFileManager::Get().GetStatData(*NormalizedPath);
    if (!StatData.bIsValid || StatData.bIsDirectory)
    {
        DescriptorCache.Remove(NormalizedPath);
        OutIssues.Add({
            EPDSIssueSeverity::Error,
            TEXT("PDS.SnapshotDiffBrowser.StatFailed"),
            NormalizedPath,
            TEXT("El snapshot desapareció o no pudo consultarse como archivo.")
        });
        return false;
    }

    if (const FPDSSnapshotDescriptorCacheEntry* CachedEntry =
        DescriptorCache.Find(NormalizedPath))
    {
        if (CachedEntry->FileSize == StatData.FileSize
            && CachedEntry->ModificationTime == StatData.ModificationTime)
        {
            OutDescriptor = CachedEntry->Descriptor;
            OutIssues.Append(CachedEntry->Issues);
            if (bOutCacheHit)
            {
                *bOutCacheHit = true;
            }
            return true;
        }
    }

    FPDSSnapshotFileDescriptor Descriptor;
    InitializeDescriptorIdentity(NormalizedPath, Descriptor);

    FPDSSnapshotDocument Document;
    TArray<FPDSIssue> DescriptorIssues;
    Descriptor.bIsValid = LoadSnapshotDocument(
        NormalizedPath,
        Document,
        DescriptorIssues);

    if (Descriptor.bIsValid)
    {
        Descriptor.GeneratedAtUtc = Document.GeneratedAtUtc;
        Descriptor.SchemaVersion = Document.SchemaVersion;
        Descriptor.ProjectName = Document.ProjectName;
        Descriptor.AssetCount = Document.AssetsByObjectPath.Num();
    }

    Descriptor.DisplayLabel = BuildDescriptorLabel(Descriptor);

    FPDSSnapshotDescriptorCacheEntry CacheEntry;
    CacheEntry.FileSize = StatData.FileSize;
    CacheEntry.ModificationTime = StatData.ModificationTime;
    CacheEntry.Descriptor = Descriptor;
    CacheEntry.Issues = DescriptorIssues;
    DescriptorCache.Add(NormalizedPath, MoveTemp(CacheEntry));

    OutDescriptor = MoveTemp(Descriptor);
    OutIssues.Append(DescriptorIssues);
    return true;
}

TArray<FPDSSnapshotFileDescriptor> PDSSnapshotDiffBrowser::EnumerateSnapshotFiles(
    TArray<FPDSIssue>& OutIssues)
{
    TMap<FString, FPDSSnapshotDescriptorCacheEntry> TemporaryCache;
    return EnumerateSnapshotFiles(OutIssues, TemporaryCache);
}

TArray<FPDSSnapshotFileDescriptor> PDSSnapshotDiffBrowser::EnumerateSnapshotFiles(
    TArray<FPDSIssue>& OutIssues,
    TMap<FString, FPDSSnapshotDescriptorCacheEntry>& DescriptorCache)
{
    TArray<FString> CandidatePaths;
    const FString SnapshotsDirectory =
        FPDSProjectSnapshotDiffService::GetSnapshotsDirectory();
    const FString BaselinePath =
        FPDSProjectSnapshotDiffService::GetBaselineSnapshotPath();
    const FString LatestPath = FPaths::Combine(
        SnapshotsDirectory,
        TEXT("latest.json"));

    if (IFileManager::Get().FileExists(*BaselinePath))
    {
        CandidatePaths.Add(BaselinePath);
    }
    if (IFileManager::Get().FileExists(*LatestPath))
    {
        CandidatePaths.Add(LatestPath);
    }

    const TArray<FString> TimestampedPaths =
        FPDSProjectSnapshotDiffService::FindTimestampedSnapshotPathsNewestFirst();
    const UPDSDeveloperSettings* Settings = GetDefault<UPDSDeveloperSettings>();

    // Protege también contra un valor inválido escrito manualmente en DefaultGame.ini.
    const int32 MaxTimestampedSnapshots = FMath::Max(
        Settings ? Settings->SnapshotBrowserMaxTimestampedSnapshots : 20,
        1);
    const int32 NumTimestampedToInclude = FMath::Min(
        TimestampedPaths.Num(),
        MaxTimestampedSnapshots);
    for (int32 Index = 0; Index < NumTimestampedToInclude; ++Index)
    {
        CandidatePaths.Add(TimestampedPaths[Index]);
    }
    if (TimestampedPaths.Num() > NumTimestampedToInclude)
    {
        OutIssues.Add({
            EPDSIssueSeverity::Info,
            TEXT("PDS.SnapshotDiffBrowser.HistoryTruncated"),
            SnapshotsDirectory,
            FString::Printf(
                TEXT("Se muestran los %d snapshots timestamped más recientes de %d. Ajusta SnapshotBrowserMaxTimestampedSnapshots en Project Settings para ampliar el historial visible."),
                NumTimestampedToInclude,
                TimestampedPaths.Num())
        });
    }

    TSet<FString> SeenPaths;
    TArray<FPDSSnapshotFileDescriptor> Descriptors;
    Descriptors.Reserve(CandidatePaths.Num());

    for (const FString& CandidatePath : CandidatePaths)
    {
        const FString NormalizedPath = NormalizeSnapshotPath(CandidatePath);
        if (SeenPaths.Contains(NormalizedPath))
        {
            continue;
        }
        SeenPaths.Add(NormalizedPath);

        FPDSSnapshotFileDescriptor Descriptor;
        if (ResolveSnapshotFileDescriptor(
            NormalizedPath,
            DescriptorCache,
            Descriptor,
            OutIssues))
        {
            Descriptors.Add(MoveTemp(Descriptor));
        }
    }

    for (auto CacheIterator = DescriptorCache.CreateIterator(); CacheIterator; ++CacheIterator)
    {
        if (!SeenPaths.Contains(CacheIterator.Key()))
        {
            CacheIterator.RemoveCurrent();
        }
    }

    Descriptors.Sort([](
        const FPDSSnapshotFileDescriptor& A,
        const FPDSSnapshotFileDescriptor& B)
    {
        const int32 RankA = GetDescriptorSortRank(A);
        const int32 RankB = GetDescriptorSortRank(B);
        if (RankA != RankB)
        {
            return RankA < RankB;
        }

        if (A.GeneratedAtUtc != B.GeneratedAtUtc)
        {
            return A.GeneratedAtUtc > B.GeneratedAtUtc;
        }
        return A.FilePath < B.FilePath;
    });

    return Descriptors;
}

void PDSSnapshotDiffBrowser::BuildAssetEntries(
    const FPDSSnapshotDiff& Diff,
    TArray<FPDSSnapshotDiffBrowserEntry>& OutEntries)
{
    OutEntries.Reset();
    OutEntries.Reserve(
        Diff.AddedAssets.Num()
        + Diff.RemovedAssets.Num()
        + Diff.ChangedAssets.Num());

    for (const FPDSSnapshotAssetRecord& Asset : Diff.AddedAssets)
    {
        FPDSSnapshotDiffBrowserEntry Entry;
        Entry.ChangeKind = EPDSSnapshotDiffChangeKind::Added;
        Entry.ObjectPath = Asset.ObjectPath;
        Entry.CurrentClassPath = Asset.ClassPath;
        Entry.CurrentOrigin = Asset.Origin;
        Entry.bExistsInCurrentSnapshot = true;
        OutEntries.Add(MoveTemp(Entry));
    }

    for (const FPDSSnapshotAssetRecord& Asset : Diff.RemovedAssets)
    {
        FPDSSnapshotDiffBrowserEntry Entry;
        Entry.ChangeKind = EPDSSnapshotDiffChangeKind::Removed;
        Entry.ObjectPath = Asset.ObjectPath;
        Entry.PreviousClassPath = Asset.ClassPath;
        Entry.PreviousOrigin = Asset.Origin;
        Entry.bExistsInCurrentSnapshot = false;
        OutEntries.Add(MoveTemp(Entry));
    }

    for (const FPDSSnapshotAssetChange& Change : Diff.ChangedAssets)
    {
        FPDSSnapshotDiffBrowserEntry Entry;
        Entry.ChangeKind = EPDSSnapshotDiffChangeKind::Modified;
        Entry.ObjectPath = Change.ObjectPath;
        Entry.PreviousClassPath = Change.PreviousClassPath;
        Entry.CurrentClassPath = Change.CurrentClassPath;
        Entry.PreviousOrigin = Change.PreviousOrigin;
        Entry.CurrentOrigin = Change.CurrentOrigin;
        Entry.bExistsInCurrentSnapshot = true;
        OutEntries.Add(MoveTemp(Entry));
    }

    OutEntries.Sort([](
        const FPDSSnapshotDiffBrowserEntry& A,
        const FPDSSnapshotDiffBrowserEntry& B)
    {
        if (A.ChangeKind != B.ChangeKind)
        {
            return static_cast<uint8>(A.ChangeKind)
                < static_cast<uint8>(B.ChangeKind);
        }
        return A.ObjectPath < B.ObjectPath;
    });
}

void PDSSnapshotDiffBrowser::ApplyFilter(
    const TArray<FPDSSnapshotDiffBrowserEntry>& SourceEntries,
    const FPDSSnapshotDiffBrowserFilter& Filter,
    TArray<FPDSSnapshotDiffBrowserEntry>& OutEntries)
{
    OutEntries.Reset();
    OutEntries.Reserve(SourceEntries.Num());

    for (const FPDSSnapshotDiffBrowserEntry& Entry : SourceEntries)
    {
        if (Filter.Matches(Entry))
        {
            OutEntries.Add(Entry);
        }
    }
}
