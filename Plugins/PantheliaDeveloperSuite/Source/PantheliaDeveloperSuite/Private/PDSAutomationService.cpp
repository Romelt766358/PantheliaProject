#include "PDSAutomationService.h"

#include "HAL/FileManager.h"
#include "Misc/App.h"
#include "Misc/EngineVersion.h"
#include "Misc/Paths.h"
#include "PDSAssetUtilities.h"
#include "PDSProjectDoctorService.h"
#include "PDSProjectSnapshotService.h"
#include "PDSProjectSnapshotDiffService.h"
#include "PDSSnapshotDiffBrowserTypes.h"

namespace
{
    constexpr int32 DefaultHistoryLimit = 20;
    constexpr int32 DefaultIssueLimit = 100;
    constexpr int32 MaximumResponseEntries = 500;

    struct FSnapshotFileInventory
    {
        int32 TimestampedCount = 0;
        bool bBaselineExists = false;
        bool bLatestExists = false;

        int32 GetTotalCount() const
        {
            return TimestampedCount
                + (bBaselineExists ? 1 : 0)
                + (bLatestExists ? 1 : 0);
        }
    };

    FSnapshotFileInventory CountSnapshotFiles()
    {
        FSnapshotFileInventory Result;
        Result.TimestampedCount =
            FPDSProjectSnapshotDiffService::FindTimestampedSnapshotPathsNewestFirst().Num();

        const FString SnapshotDirectory =
            FPDSProjectSnapshotDiffService::GetSnapshotsDirectory();
        const FString BaselinePath =
            FPDSProjectSnapshotDiffService::GetBaselineSnapshotPath();
        const FString LatestPath = FPaths::Combine(
            SnapshotDirectory,
            TEXT("latest.json"));

        Result.bBaselineExists =
            IFileManager::Get().FileExists(*BaselinePath);
        Result.bLatestExists =
            IFileManager::Get().FileExists(*LatestPath);
        return Result;
    }

    int32 ClampResponseLimit(const int32 RequestedLimit, const int32 DefaultLimit)
    {
        if (RequestedLimit <= 0)
        {
            return DefaultLimit;
        }
        return FMath::Clamp(RequestedLimit, 1, MaximumResponseEntries);
    }

    FPDSAutomationIssue ConvertIssue(const FPDSIssue& Source)
    {
        FPDSAutomationIssue Result;
        Result.Severity = PDSDeveloperTypes::SeverityToString(Source.Severity);
        Result.RuleId = Source.RuleId;
        Result.AssetPath = Source.AssetPath;
        Result.Message = Source.Message;
        Result.Origin = PDSDeveloperTypes::AssetOriginToString(Source.Origin);
        return Result;
    }

    void CopyIssuesLimited(
        const TArray<FPDSIssue>& Source,
        const int32 RequestedLimit,
        int32& OutTotalIssueCount,
        bool& bOutTruncated,
        TArray<FPDSAutomationIssue>& OutIssues)
    {
        const int32 Limit = ClampResponseLimit(RequestedLimit, DefaultIssueLimit);
        OutTotalIssueCount = Source.Num();
        OutIssues.Reset();
        OutIssues.Reserve(FMath::Min(Source.Num(), Limit));

        for (int32 Index = 0; Index < Source.Num() && Index < Limit; ++Index)
        {
            OutIssues.Add(ConvertIssue(Source[Index]));
        }

        bOutTruncated = OutIssues.Num() < Source.Num();
    }

    template <typename ValueType>
    void CopyArrayLimited(
        const TArray<ValueType>& Source,
        const int32 Limit,
        TArray<ValueType>& OutValues,
        bool& bInOutTruncated)
    {
        OutValues.Reset();
        const int32 Count = FMath::Min(Source.Num(), Limit);
        OutValues.Reserve(Count);

        for (int32 Index = 0; Index < Count; ++Index)
        {
            OutValues.Add(Source[Index]);
        }

        bInOutTruncated |= Count < Source.Num();
    }

    FPDSAutomationSnapshotDescriptor ConvertSnapshotDescriptor(
        const FPDSSnapshotFileDescriptor& Source)
    {
        FPDSAutomationSnapshotDescriptor Result;
        Result.FilePath = Source.FilePath;
        Result.FileName = Source.FileName;
        Result.DisplayLabel = Source.DisplayLabel;
        Result.GeneratedAtUtc = Source.GeneratedAtUtc;
        Result.SchemaVersion = Source.SchemaVersion;
        Result.ProjectName = Source.ProjectName;
        Result.AssetCount = Source.AssetCount;
        Result.bIsBaseline = Source.bIsBaseline;
        Result.bIsLatest = Source.bIsLatest;
        Result.bIsTimestamped = Source.bIsTimestamped;
        Result.bIsValid = Source.bIsValid;
        return Result;
    }

    TMap<FString, FPDSSnapshotDescriptorCacheEntry>& GetAutomationDescriptorCache()
    {
        // Unreal MCP ejecuta tools serialmente en el game thread. Esta caché está
        // aislada del browser Slate y reutiliza la misma invalidación por size + mtime.
        static TMap<FString, FPDSSnapshotDescriptorCacheEntry> Cache;
        return Cache;
    }

    FPDSAutomationDiffResult BuildLoadFailure(
        const FString& PreviousPath,
        const FString& CurrentPath,
        const FString& ComparisonLabel,
        const TArray<FPDSIssue>& LoadIssues,
        const int32 MaxEntries)
    {
        FPDSSnapshotDiff EmptyDiff;
        EmptyDiff.PreviousSnapshotPath = PreviousPath;
        EmptyDiff.CurrentSnapshotPath = CurrentPath;

        FPDSOperationResult Failure;
        Failure.bSuccess = false;
        Failure.Summary = TEXT("No fue posible cargar ambos snapshots para comparación.");
        Failure.Issues = LoadIssues;

        return PDSAutomation::BuildDiffResult(
            EmptyDiff,
            Failure,
            ComparisonLabel,
            MaxEntries,
            DefaultIssueLimit);
    }

    FPDSAutomationDiffResult CompareSnapshotPaths(
        const FString& PreviousPath,
        const FString& CurrentPath,
        const FString& ComparisonLabel,
        const int32 MaxEntries)
    {
        FPDSSnapshotDocument Previous;
        FPDSSnapshotDocument Current;
        TArray<FPDSIssue> LoadIssues;

        const bool bPreviousLoaded = PDSSnapshotDiffBrowser::LoadSnapshotDocument(
            PreviousPath,
            Previous,
            LoadIssues);
        const bool bCurrentLoaded = PDSSnapshotDiffBrowser::LoadSnapshotDocument(
            CurrentPath,
            Current,
            LoadIssues);

        if (!bPreviousLoaded || !bCurrentLoaded)
        {
            return BuildLoadFailure(
                PreviousPath,
                CurrentPath,
                ComparisonLabel,
                LoadIssues,
                MaxEntries);
        }

        const FPDSSnapshotDiff Diff = PDSSnapshotDiff::Compare(Previous, Current);
        const FPDSProjectSnapshotDiffService DiffService;
        FPDSOperationResult PersistResult = DiffService.PersistDiffReports(
            Diff,
            ComparisonLabel);

        // Los fallos de lectura ocurrieron antes que cualquier hallazgo de persistencia.
        if (!LoadIssues.IsEmpty())
        {
            TArray<FPDSIssue> OrderedIssues = MoveTemp(LoadIssues);
            OrderedIssues.Append(PersistResult.Issues);
            PersistResult.Issues = MoveTemp(OrderedIssues);
        }

        return PDSAutomation::BuildDiffResult(
            Diff,
            PersistResult,
            ComparisonLabel,
            MaxEntries,
            DefaultIssueLimit);
    }
}

FString FPDSAutomationService::GetAutomationApiVersion()
{
    return TEXT("0.4.0-alpha2");
}

FPDSAutomationStatusResult FPDSAutomationService::GetStatus() const
{
    // GetStatus funciona como ping barato para clientes MCP. Deliberadamente no
    // parsea snapshots ni calienta la caché; ListSnapshots realiza esa inspección.
    const FSnapshotFileInventory Inventory = CountSnapshotFiles();

    FPDSAutomationStatusResult Result;
    Result.bSuccess = true;
    Result.AutomationApiVersion = GetAutomationApiVersion();
    Result.ProjectName = FApp::GetProjectName();
    Result.EngineVersion = FEngineVersion::Current().ToString();
    Result.SnapshotDirectory =
        FPDSProjectSnapshotDiffService::GetSnapshotsDirectory();
    Result.bBaselineExists = Inventory.bBaselineExists;
    Result.bLatestExists = Inventory.bLatestExists;
    Result.AvailableSnapshotCount = Inventory.GetTotalCount();
    Result.bSnapshotValidityKnown = false;
    Result.ValidSnapshotCount = -1;
    Result.InvalidSnapshotCount = -1;
    return Result;
}

FPDSAutomationSnapshotHistoryResult FPDSAutomationService::ListSnapshots(
    const int32 Limit) const
{
    const FSnapshotFileInventory Inventory = CountSnapshotFiles();

    TArray<FPDSIssue> Issues;
    TArray<FPDSSnapshotFileDescriptor> Descriptors =
        PDSSnapshotDiffBrowser::EnumerateSnapshotFiles(
            Issues,
            GetAutomationDescriptorCache());

    FPDSAutomationSnapshotHistoryResult Result;
    Result.bSuccess = true;
    Result.AvailableSnapshotCount = Inventory.GetTotalCount();
    Result.InspectedSnapshotCount = Descriptors.Num();
    Result.bDescriptorEnumerationCappedBySettings =
        Result.InspectedSnapshotCount < Result.AvailableSnapshotCount;

    for (const FPDSSnapshotFileDescriptor& Descriptor : Descriptors)
    {
        if (Descriptor.bIsValid)
        {
            ++Result.ValidSnapshotCount;
        }
        else
        {
            ++Result.InvalidSnapshotCount;
        }
    }

    const int32 SafeLimit = ClampResponseLimit(Limit, DefaultHistoryLimit);
    const int32 ReturnCount = FMath::Min(Descriptors.Num(), SafeLimit);
    Result.Snapshots.Reserve(ReturnCount);

    for (int32 Index = 0; Index < ReturnCount; ++Index)
    {
        Result.Snapshots.Add(ConvertSnapshotDescriptor(Descriptors[Index]));
    }

    Result.ReturnedSnapshotCount = Result.Snapshots.Num();
    // Incluye tanto el límite de respuesta como el cap de historial de settings.
    Result.bSnapshotsTruncated =
        Result.ReturnedSnapshotCount < Result.AvailableSnapshotCount;

    CopyIssuesLimited(
        Issues,
        DefaultIssueLimit,
        Result.TotalIssueCount,
        Result.bIssuesTruncated,
        Result.Issues);
    return Result;
}

FPDSAutomationOperationResult FPDSAutomationService::ExportProjectSnapshot() const
{
    const FPDSProjectSnapshotService Service;
    return PDSAutomation::ConvertOperationResult(
        Service.ExportProjectSnapshot(nullptr),
        DefaultIssueLimit);
}

FPDSAutomationValidationResult FPDSAutomationService::ValidateProfile(
    const EPDSAutomationValidationProfile Profile,
    const int32 MaxIssues) const
{
    const FPDSProjectDoctorService Service;
    const FPDSValidationSummary Summary = Service.ValidateProfile(
        PDSAutomation::ToNativeValidationProfile(Profile));

    return PDSAutomation::ConvertValidationSummary(Summary, MaxIssues);
}

FPDSAutomationOperationResult
FPDSAutomationService::SetLatestSnapshotAsBaseline() const
{
    const FPDSProjectSnapshotDiffService Service;
    return PDSAutomation::ConvertOperationResult(
        Service.SetLatestSnapshotAsBaseline(),
        DefaultIssueLimit);
}

FPDSAutomationDiffResult
FPDSAutomationService::CompareLatestSnapshotWithBaseline(
    const int32 MaxEntries) const
{
    const FString PreviousPath =
        FPDSProjectSnapshotDiffService::GetBaselineSnapshotPath();
    const FString CurrentPath = FPaths::Combine(
        FPDSProjectSnapshotDiffService::GetSnapshotsDirectory(),
        TEXT("latest.json"));

    return CompareSnapshotPaths(
        PreviousPath,
        CurrentPath,
        TEXT("MCP Latest vs Baseline"),
        MaxEntries);
}

FPDSAutomationDiffResult FPDSAutomationService::CompareLatestTwoSnapshots(
    const int32 MaxEntries) const
{
    const TArray<FString> SnapshotPaths =
        FPDSProjectSnapshotDiffService::FindTimestampedSnapshotPathsNewestFirst();

    if (SnapshotPaths.Num() < 2)
    {
        TArray<FPDSIssue> Issues;
        FPDSIssue Issue;
        Issue.Severity = EPDSIssueSeverity::Error;
        Issue.RuleId = TEXT("PDS.Automation.NotEnoughSnapshots");
        Issue.AssetPath =
            FPDSProjectSnapshotDiffService::GetSnapshotsDirectory();
        Issue.Message =
            TEXT("Se requieren al menos dos snapshots timestamped para comparar.");
        Issues.Add(MoveTemp(Issue));

        FPDSSnapshotDiff EmptyDiff;
        EmptyDiff.CurrentSnapshotPath = SnapshotPaths.IsEmpty()
            ? FString()
            : SnapshotPaths[0];

        FPDSOperationResult Failure;
        Failure.bSuccess = false;
        Failure.Summary =
            TEXT("Se requieren al menos dos snapshots timestamped para comparar.");
        Failure.Issues = MoveTemp(Issues);

        return PDSAutomation::BuildDiffResult(
            EmptyDiff,
            Failure,
            TEXT("MCP Latest Two Snapshots"),
            MaxEntries,
            DefaultIssueLimit);
    }

    return CompareSnapshotPaths(
        SnapshotPaths[1],
        SnapshotPaths[0],
        TEXT("MCP Latest Two Snapshots"),
        MaxEntries);
}

namespace PDSAutomation
{
    FPDSAutomationOperationResult ConvertOperationResult(
        const FPDSOperationResult& Source,
        const int32 IssueLimit)
    {
        FPDSAutomationOperationResult Result;
        Result.bSuccess = Source.bSuccess;
        Result.bCancelled = Source.bCancelled;
        Result.Summary = Source.Summary;
        Result.OutputPath = Source.OutputPath;
        Result.OutputJsonPath = Source.OutputJsonPath;

        CopyIssuesLimited(
            Source.Issues,
            IssueLimit,
            Result.TotalIssueCount,
            Result.bIssuesTruncated,
            Result.Issues);
        return Result;
    }

    FPDSAutomationValidationResult ConvertValidationSummary(
        const FPDSValidationSummary& Source,
        const int32 IssueLimit)
    {
        FPDSAutomationValidationResult Result;
        Result.bValidationCompleted = !Source.bCancelled;
        Result.bSuccess =
            Result.bValidationCompleted && !Source.HasErrors();
        Result.bCancelled = Source.bCancelled;
        Result.ProfileId = Source.ScopeId;
        Result.ProfileLabel = Source.ScopeLabel;
        Result.GeneratedAtUtc = Source.GeneratedAtUtc;
        Result.NumRequested = Source.NumRequested;
        Result.NumChecked = Source.NumChecked;
        Result.NumValid = Source.NumValid;
        Result.NumInvalid = Source.NumInvalid;
        Result.NumWithWarnings = Source.NumWithWarnings;
        Result.NumNotValidated = Source.NumNotValidated;
        Result.NumSkipped = Source.NumSkipped;
        Result.NumUnableToValidate = Source.NumUnableToValidate;
        Result.bAssetLimitReached = Source.bAssetLimitReached;
        Result.bReadOnlyVerificationPassed =
            Source.bReadOnlyVerificationPassed;
        Result.OutputPath = Source.OutputPath;
        Result.OutputJsonPath = Source.OutputJsonPath;

        const int32 SafeLimit = ClampResponseLimit(
            IssueLimit,
            DefaultIssueLimit);
        Result.TotalNewlyDirtiedPackageCount =
            Source.NewlyDirtiedPackages.Num();
        const int32 DirtyPackageCount = FMath::Min(
            Result.TotalNewlyDirtiedPackageCount,
            SafeLimit);
        Result.NewlyDirtiedPackages.Reserve(DirtyPackageCount);
        for (int32 Index = 0; Index < DirtyPackageCount; ++Index)
        {
            Result.NewlyDirtiedPackages.Add(
                Source.NewlyDirtiedPackages[Index]);
        }
        Result.bNewlyDirtiedPackagesTruncated =
            DirtyPackageCount < Result.TotalNewlyDirtiedPackageCount;

        // CopyIssuesLimited aplica su propio clamp; se conserva IssueLimit original.
        CopyIssuesLimited(
            Source.Issues,
            IssueLimit,
            Result.TotalIssueCount,
            Result.bIssuesTruncated,
            Result.Issues);
        return Result;
    }

    FPDSAutomationDiffResult BuildDiffResult(
        const FPDSSnapshotDiff& Diff,
        const FPDSOperationResult& PersistResult,
        const FString& ComparisonLabel,
        const int32 MaxEntries,
        const int32 IssueLimit)
    {
        const int32 SafeEntryLimit = ClampResponseLimit(
            MaxEntries,
            100);

        FPDSAutomationDiffResult Result;
        Result.bSuccess = PersistResult.bSuccess;
        Result.bHasChanges = Diff.HasChanges();
        Result.ComparisonLabel = ComparisonLabel;
        Result.PreviousSnapshotPath = Diff.PreviousSnapshotPath;
        Result.CurrentSnapshotPath = Diff.CurrentSnapshotPath;
        Result.PreviousGeneratedAtUtc = Diff.PreviousGeneratedAtUtc;
        Result.CurrentGeneratedAtUtc = Diff.CurrentGeneratedAtUtc;
        Result.PreviousSchemaVersion = Diff.PreviousSchemaVersion;
        Result.CurrentSchemaVersion = Diff.CurrentSchemaVersion;
        Result.PreviousAssetCount = Diff.PreviousAssetCount;
        Result.CurrentAssetCount = Diff.CurrentAssetCount;
        Result.AddedAssetCount = Diff.AddedAssets.Num();
        Result.RemovedAssetCount = Diff.RemovedAssets.Num();
        Result.ModifiedAssetCount = Diff.ChangedAssets.Num();
        Result.TotalAssetChangeCount =
            Result.AddedAssetCount
            + Result.RemovedAssetCount
            + Result.ModifiedAssetCount;
        Result.PreviousGameplayTagCount =
            Diff.PreviousGameplayTagCount;
        Result.CurrentGameplayTagCount =
            Diff.CurrentGameplayTagCount;
        Result.AddedGameplayTagCount =
            Diff.AddedGameplayTags.Num();
        Result.RemovedGameplayTagCount =
            Diff.RemovedGameplayTags.Num();
        Result.PreviousMontageCount = Diff.PreviousMontageCount;
        Result.CurrentMontageCount = Diff.CurrentMontageCount;
        Result.AddedMontageCount = Diff.AddedMontages.Num();
        Result.RemovedMontageCount = Diff.RemovedMontages.Num();
        Result.ModifiedMontageCount = Diff.ChangedMontages.Num();
        Result.OriginNotComparableAssetCount =
            Diff.NumAssetsWithOriginNotComparable;
        Result.Summary = PersistResult.Summary;
        Result.OutputPath = PersistResult.OutputPath;
        Result.OutputJsonPath = PersistResult.OutputJsonPath;

        Result.AssetChanges.Reserve(
            FMath::Min(Result.TotalAssetChangeCount, SafeEntryLimit));

        const auto TryAppendAssetChange =
            [&Result, SafeEntryLimit](
                const FString& ChangeKind,
                const FString& ObjectPath,
                const FString& PreviousClassPath,
                const FString& CurrentClassPath,
                const EPDSAssetOrigin PreviousOrigin,
                const EPDSAssetOrigin CurrentOrigin,
                const bool bExistsInCurrentSnapshot)
            {
                if (Result.AssetChanges.Num() >= SafeEntryLimit)
                {
                    return;
                }

                FPDSAutomationAssetChange Entry;
                Entry.ChangeKind = ChangeKind;
                Entry.ObjectPath = ObjectPath;
                Entry.PreviousClassPath = PreviousClassPath;
                Entry.CurrentClassPath = CurrentClassPath;
                Entry.PreviousOrigin =
                    PDSDeveloperTypes::AssetOriginToString(
                        PreviousOrigin);
                Entry.CurrentOrigin =
                    PDSDeveloperTypes::AssetOriginToString(
                        CurrentOrigin);
                Entry.bExistsInCurrentSnapshot =
                    bExistsInCurrentSnapshot;
                Result.AssetChanges.Add(MoveTemp(Entry));
            };

        for (const FPDSSnapshotAssetRecord& Asset : Diff.AddedAssets)
        {
            TryAppendAssetChange(
                TEXT("Added"),
                Asset.ObjectPath,
                FString(),
                Asset.ClassPath,
                EPDSAssetOrigin::Unknown,
                Asset.Origin,
                true);
        }

        for (const FPDSSnapshotAssetRecord& Asset : Diff.RemovedAssets)
        {
            TryAppendAssetChange(
                TEXT("Removed"),
                Asset.ObjectPath,
                Asset.ClassPath,
                FString(),
                Asset.Origin,
                EPDSAssetOrigin::Unknown,
                false);
        }

        for (const FPDSSnapshotAssetChange& Change : Diff.ChangedAssets)
        {
            TryAppendAssetChange(
                TEXT("Modified"),
                Change.ObjectPath,
                Change.PreviousClassPath,
                Change.CurrentClassPath,
                Change.PreviousOrigin,
                Change.CurrentOrigin,
                true);
        }

        Result.ReturnedAssetChangeCount = Result.AssetChanges.Num();
        Result.bAssetChangesTruncated =
            Result.ReturnedAssetChangeCount
            < Result.TotalAssetChangeCount;
        Result.bEntriesTruncated = Result.bAssetChangesTruncated;

        CopyArrayLimited(
            Diff.AddedGameplayTags,
            SafeEntryLimit,
            Result.AddedGameplayTags,
            Result.bEntriesTruncated);
        CopyArrayLimited(
            Diff.RemovedGameplayTags,
            SafeEntryLimit,
            Result.RemovedGameplayTags,
            Result.bEntriesTruncated);
        CopyArrayLimited(
            Diff.AddedMontages,
            SafeEntryLimit,
            Result.AddedMontages,
            Result.bEntriesTruncated);
        CopyArrayLimited(
            Diff.RemovedMontages,
            SafeEntryLimit,
            Result.RemovedMontages,
            Result.bEntriesTruncated);
        CopyArrayLimited(
            Diff.ChangedMontages,
            SafeEntryLimit,
            Result.ModifiedMontages,
            Result.bEntriesTruncated);

        CopyIssuesLimited(
            PersistResult.Issues,
            IssueLimit,
            Result.TotalIssueCount,
            Result.bIssuesTruncated,
            Result.Issues);
        return Result;
    }

    EPDSValidationProfile ToNativeValidationProfile(
        const EPDSAutomationValidationProfile Profile)
    {
        switch (Profile)
        {
        case EPDSAutomationValidationProfile::PantheliaCore:
            return EPDSValidationProfile::PantheliaCore;
        case EPDSAutomationValidationProfile::GameContent:
            return EPDSValidationProfile::GameContent;
        case EPDSAutomationValidationProfile::ExternalContent:
            return EPDSValidationProfile::ExternalContent;
        case EPDSAutomationValidationProfile::EntireProject:
        default:
            return EPDSValidationProfile::EntireProject;
        }
    }
}
