#pragma once

#include "CoreMinimal.h"
#include "PDSSnapshotDiffBrowserTypes.h"
#include "Widgets/SCompoundWidget.h"

struct FAssetData;
class ITableRow;
class STableViewBase;
class SSearchBox;
class SMultiLineEditableTextBox;
template <typename OptionType> class SComboBox;
template <typename ItemType> class SListView;

/**
 * Navegador read-only para seleccionar dos snapshots, filtrar cambios de assets
 * y localizar o abrir assets que todavía existen en el snapshot actual.
 */
class SPDSSnapshotDiffBrowser final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SPDSSnapshotDiffBrowser) {}
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    using FSnapshotOption = TSharedPtr<FPDSSnapshotFileDescriptor>;
    using FEntryItem = TSharedPtr<FPDSSnapshotDiffBrowserEntry>;
    using FStringOption = TSharedPtr<FString>;

    void RefreshSnapshotOptions(bool bPreserveSelection);
    void BuildDefaultFilterOptions();
    void RebuildClassOptions();
    void ApplyCurrentFilters();
    void SetStatus(const FString& NewStatus);

    FReply OnRefreshSnapshotsClicked();
    FReply OnCompareClicked();
    FReply OnLocateSelectedClicked();
    FReply OnOpenSelectedClicked();
    FReply OnCopySelectedPathClicked();

    void OnPreviousSnapshotChanged(FSnapshotOption NewSelection, ESelectInfo::Type SelectInfo);
    void OnCurrentSnapshotChanged(FSnapshotOption NewSelection, ESelectInfo::Type SelectInfo);
    void OnChangeFilterChanged(FStringOption NewSelection, ESelectInfo::Type SelectInfo);
    void OnOriginFilterChanged(FStringOption NewSelection, ESelectInfo::Type SelectInfo);
    void OnClassFilterChanged(FStringOption NewSelection, ESelectInfo::Type SelectInfo);
    void OnSearchTextChanged(const FText& NewText);

    TSharedRef<SWidget> GenerateSnapshotOptionWidget(FSnapshotOption Option) const;
    TSharedRef<SWidget> GenerateStringOptionWidget(FStringOption Option) const;
    TSharedRef<ITableRow> GenerateEntryRow(
        FEntryItem Item,
        const TSharedRef<STableViewBase>& OwnerTable) const;

    FText GetPreviousSnapshotText() const;
    FText GetCurrentSnapshotText() const;
    FText GetChangeFilterText() const;
    FText GetOriginFilterText() const;
    FText GetClassFilterText() const;
    FText GetBaselineSummaryText() const;
    FText GetResultCountText() const;
    FText GetStatusText() const;

    FEntryItem GetSelectedEntry() const;
    bool HasSelectedEntry() const;
    bool CanActOnSelectedAsset() const;
    bool ResolveSelectedAssetData(FAssetData& OutAssetData) const;

    TArray<FSnapshotOption> SnapshotOptions;
    TMap<FString, FPDSSnapshotDescriptorCacheEntry> DescriptorCache;
    FSnapshotOption SelectedPreviousSnapshot;
    FSnapshotOption SelectedCurrentSnapshot;

    TArray<FStringOption> ChangeFilterOptions;
    TArray<FStringOption> OriginFilterOptions;
    TArray<FStringOption> ClassFilterOptions;
    FStringOption SelectedChangeFilter;
    FStringOption SelectedOriginFilter;
    FStringOption SelectedClassFilter;

    TArray<FEntryItem> AllEntries;
    TArray<FEntryItem> FilteredEntries;
    FPDSSnapshotDiff LastDiff;
    FString SearchText;
    FString StatusText;

    TSharedPtr<SComboBox<FSnapshotOption>> PreviousSnapshotCombo;
    TSharedPtr<SComboBox<FSnapshotOption>> CurrentSnapshotCombo;
    TSharedPtr<SComboBox<FStringOption>> ChangeFilterCombo;
    TSharedPtr<SComboBox<FStringOption>> OriginFilterCombo;
    TSharedPtr<SComboBox<FStringOption>> ClassFilterCombo;
    TSharedPtr<SListView<FEntryItem>> EntryListView;
    TSharedPtr<SMultiLineEditableTextBox> SummaryTextBox;
};
