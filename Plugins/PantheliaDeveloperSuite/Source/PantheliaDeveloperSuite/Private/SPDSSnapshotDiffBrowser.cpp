#include "SPDSSnapshotDiffBrowser.h"

#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "ContentBrowserModule.h"
#include "Editor.h"
#include "HAL/PlatformApplicationMisc.h"
#include "IContentBrowserSingleton.h"
#include "Modules/ModuleManager.h"
#include "PDSDeveloperTypes.h"
#include "PDSProjectSnapshotDiffService.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "UObject/SoftObjectPath.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Input/SMultiLineEditableTextBox.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SHeaderRow.h"
#include "Widgets/Views/SListView.h"
#include "Widgets/Views/STableRow.h"

#define LOCTEXT_NAMESPACE "SPDSSnapshotDiffBrowser"

namespace
{
    const FName ChangeColumnName(TEXT("Change"));
    const FName OriginColumnName(TEXT("Origin"));
    const FName ClassColumnName(TEXT("Class"));
    const FName PathColumnName(TEXT("Path"));

    FString JoinIssueMessages(const TArray<FPDSIssue>& Issues)
    {
        FString Output;
        for (const FPDSIssue& Issue : Issues)
        {
            if (!Output.IsEmpty())
            {
                Output += LINE_TERMINATOR;
            }
            Output += Issue.ToDisplayString(false);
        }
        return Output;
    }

    class SPDSSnapshotDiffBrowserRow final
        : public SMultiColumnTableRow<TSharedPtr<FPDSSnapshotDiffBrowserEntry>>
    {
    public:
        SLATE_BEGIN_ARGS(SPDSSnapshotDiffBrowserRow) {}
            SLATE_ARGUMENT(TSharedPtr<FPDSSnapshotDiffBrowserEntry>, Item)
        SLATE_END_ARGS()

        void Construct(
            const FArguments& InArgs,
            const TSharedRef<STableViewBase>& OwnerTable)
        {
            Item = InArgs._Item;
            SMultiColumnTableRow<TSharedPtr<FPDSSnapshotDiffBrowserEntry>>::Construct(
                FSuperRowType::FArguments().Padding(FMargin(2.0f)),
                OwnerTable);
        }

        virtual TSharedRef<SWidget> GenerateWidgetForColumn(
            const FName& ColumnName) override
        {
            FString Value;
            if (!Item.IsValid())
            {
                Value = TEXT("<invalid>");
            }
            else if (ColumnName == ChangeColumnName)
            {
                Value = PDSSnapshotDiffBrowser::ChangeKindToString(Item->ChangeKind);
            }
            else if (ColumnName == OriginColumnName)
            {
                Value = PDSDeveloperTypes::AssetOriginToString(
                    Item->GetDisplayOrigin());
            }
            else if (ColumnName == ClassColumnName)
            {
                Value = Item->GetDisplayClassPath();
            }
            else if (ColumnName == PathColumnName)
            {
                Value = Item->ObjectPath;
            }

            return SNew(STextBlock)
                .Text(FText::FromString(Value))
                .ToolTipText(FText::FromString(Value));
        }

    private:
        TSharedPtr<FPDSSnapshotDiffBrowserEntry> Item;
    };
}

void SPDSSnapshotDiffBrowser::Construct(const FArguments& InArgs)
{
    BuildDefaultFilterOptions();
    RefreshSnapshotOptions(false);

    ChildSlot
    [
        SNew(SBorder)
        .Padding(12.0f)
        [
            SNew(SVerticalBox)

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 6.0f)
            [
                SNew(STextBlock)
                .Text(LOCTEXT(
                    "Title",
                    "Snapshot Diff Browser — v0.3 Alpha 4 Read-Only"))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                SNew(STextBlock)
                .AutoWrapText(true)
                .Text(LOCTEXT(
                    "Description",
                    "Selecciona dos snapshots, compara cambios y filtra assets. Locate no carga el asset; Open Asset puede cargarlo, pero esta herramienta nunca guarda paquetes."))
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                SNew(STextBlock)
                .Text(this, &SPDSSnapshotDiffBrowser::GetBaselineSummaryText)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(0.0f, 0.0f, 4.0f, 0.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("PreviousLabel", "Previous snapshot"))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(PreviousSnapshotCombo, SComboBox<FSnapshotOption>)
                        .OptionsSource(&SnapshotOptions)
                        .OnGenerateWidget(this, &SPDSSnapshotDiffBrowser::GenerateSnapshotOptionWidget)
                        .OnSelectionChanged(this, &SPDSSnapshotDiffBrowser::OnPreviousSnapshotChanged)
                        [
                            SNew(STextBlock)
                            .Text(this, &SPDSSnapshotDiffBrowser::GetPreviousSnapshotText)
                        ]
                    ]
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(4.0f, 0.0f, 4.0f, 0.0f)
                [
                    SNew(SVerticalBox)
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SNew(STextBlock)
                        .Text(LOCTEXT("CurrentLabel", "Current snapshot"))
                    ]
                    + SVerticalBox::Slot()
                    .AutoHeight()
                    [
                        SAssignNew(CurrentSnapshotCombo, SComboBox<FSnapshotOption>)
                        .OptionsSource(&SnapshotOptions)
                        .OnGenerateWidget(this, &SPDSSnapshotDiffBrowser::GenerateSnapshotOptionWidget)
                        .OnSelectionChanged(this, &SPDSSnapshotDiffBrowser::OnCurrentSnapshotChanged)
                        [
                            SNew(STextBlock)
                            .Text(this, &SPDSSnapshotDiffBrowser::GetCurrentSnapshotText)
                        ]
                    ]
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Bottom)
                .Padding(4.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("RefreshSnapshots", "Refresh"))
                    .OnClicked(this, &SPDSSnapshotDiffBrowser::OnRefreshSnapshotsClicked)
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .VAlign(VAlign_Bottom)
                .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("CompareSnapshots", "Compare"))
                    .OnClicked(this, &SPDSSnapshotDiffBrowser::OnCompareClicked)
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                SNew(SSeparator)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .FillWidth(1.5f)
                .Padding(0.0f, 0.0f, 4.0f, 0.0f)
                [
                    SNew(SSearchBox)
                    .HintText(LOCTEXT("SearchHint", "Search asset path or class"))
                    .OnTextChanged(this, &SPDSSnapshotDiffBrowser::OnSearchTextChanged)
                ]

                + SHorizontalBox::Slot()
                .FillWidth(0.8f)
                .Padding(4.0f, 0.0f)
                [
                    SAssignNew(ChangeFilterCombo, SComboBox<FStringOption>)
                    .OptionsSource(&ChangeFilterOptions)
                    .OnGenerateWidget(this, &SPDSSnapshotDiffBrowser::GenerateStringOptionWidget)
                    .OnSelectionChanged(this, &SPDSSnapshotDiffBrowser::OnChangeFilterChanged)
                    [
                        SNew(STextBlock)
                        .Text(this, &SPDSSnapshotDiffBrowser::GetChangeFilterText)
                    ]
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .Padding(4.0f, 0.0f)
                [
                    SAssignNew(OriginFilterCombo, SComboBox<FStringOption>)
                    .OptionsSource(&OriginFilterOptions)
                    .OnGenerateWidget(this, &SPDSSnapshotDiffBrowser::GenerateStringOptionWidget)
                    .OnSelectionChanged(this, &SPDSSnapshotDiffBrowser::OnOriginFilterChanged)
                    [
                        SNew(STextBlock)
                        .Text(this, &SPDSSnapshotDiffBrowser::GetOriginFilterText)
                    ]
                ]

                + SHorizontalBox::Slot()
                .FillWidth(1.4f)
                .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                [
                    SAssignNew(ClassFilterCombo, SComboBox<FStringOption>)
                    .OptionsSource(&ClassFilterOptions)
                    .OnGenerateWidget(this, &SPDSSnapshotDiffBrowser::GenerateStringOptionWidget)
                    .OnSelectionChanged(this, &SPDSSnapshotDiffBrowser::OnClassFilterChanged)
                    [
                        SNew(STextBlock)
                        .Text(this, &SPDSSnapshotDiffBrowser::GetClassFilterText)
                    ]
                ]
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 6.0f)
            [
                SNew(SHorizontalBox)

                + SHorizontalBox::Slot()
                .FillWidth(1.0f)
                .VAlign(VAlign_Center)
                [
                    SNew(STextBlock)
                    .Text(this, &SPDSSnapshotDiffBrowser::GetResultCountText)
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(4.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("LocateAsset", "Locate in Content Browser"))
                    .IsEnabled(this, &SPDSSnapshotDiffBrowser::CanActOnSelectedAsset)
                    .OnClicked(this, &SPDSSnapshotDiffBrowser::OnLocateSelectedClicked)
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(4.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("OpenAsset", "Open Asset"))
                    .IsEnabled(this, &SPDSSnapshotDiffBrowser::CanActOnSelectedAsset)
                    .OnClicked(this, &SPDSSnapshotDiffBrowser::OnOpenSelectedClicked)
                ]

                + SHorizontalBox::Slot()
                .AutoWidth()
                .Padding(4.0f, 0.0f, 0.0f, 0.0f)
                [
                    SNew(SButton)
                    .Text(LOCTEXT("CopyAssetPath", "Copy Asset Path"))
                    .IsEnabled(this, &SPDSSnapshotDiffBrowser::HasSelectedEntry)
                    .OnClicked(this, &SPDSSnapshotDiffBrowser::OnCopySelectedPathClicked)
                ]
            ]

            + SVerticalBox::Slot()
            .FillHeight(1.0f)
            .Padding(0.0f, 0.0f, 0.0f, 8.0f)
            [
                SAssignNew(EntryListView, SListView<FEntryItem>)
                .ListItemsSource(&FilteredEntries)
                .SelectionMode(ESelectionMode::Single)
                .OnGenerateRow(this, &SPDSSnapshotDiffBrowser::GenerateEntryRow)
                .HeaderRow
                (
                    SNew(SHeaderRow)
                    + SHeaderRow::Column(ChangeColumnName)
                    .DefaultLabel(LOCTEXT("ChangeColumn", "Change"))
                    .FixedWidth(90.0f)
                    + SHeaderRow::Column(OriginColumnName)
                    .DefaultLabel(LOCTEXT("OriginColumn", "Origin"))
                    .FixedWidth(130.0f)
                    + SHeaderRow::Column(ClassColumnName)
                    .DefaultLabel(LOCTEXT("ClassColumn", "Class"))
                    .FillWidth(0.35f)
                    + SHeaderRow::Column(PathColumnName)
                    .DefaultLabel(LOCTEXT("PathColumn", "Asset Path"))
                    .FillWidth(0.65f)
                )
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            .Padding(0.0f, 0.0f, 0.0f, 4.0f)
            [
                SNew(STextBlock)
                .AutoWrapText(true)
                .Text(this, &SPDSSnapshotDiffBrowser::GetStatusText)
            ]

            + SVerticalBox::Slot()
            .AutoHeight()
            [
                SNew(SBox)
                .HeightOverride(140.0f)
                [
                    SAssignNew(SummaryTextBox, SMultiLineEditableTextBox)
                    .IsReadOnly(true)
                    .AutoWrapText(false)
                    .Text(LOCTEXT(
                        "InitialSummary",
                        "Selecciona dos snapshots y pulsa Compare."))
                ]
            ]
        ]
    ];

    if (PreviousSnapshotCombo.IsValid() && SelectedPreviousSnapshot.IsValid())
    {
        PreviousSnapshotCombo->SetSelectedItem(SelectedPreviousSnapshot);
    }
    if (CurrentSnapshotCombo.IsValid() && SelectedCurrentSnapshot.IsValid())
    {
        CurrentSnapshotCombo->SetSelectedItem(SelectedCurrentSnapshot);
    }
    if (ChangeFilterCombo.IsValid())
    {
        ChangeFilterCombo->SetSelectedItem(SelectedChangeFilter);
    }
    if (OriginFilterCombo.IsValid())
    {
        OriginFilterCombo->SetSelectedItem(SelectedOriginFilter);
    }
    if (ClassFilterCombo.IsValid())
    {
        ClassFilterCombo->SetSelectedItem(SelectedClassFilter);
    }
}

void SPDSSnapshotDiffBrowser::RefreshSnapshotOptions(const bool bPreserveSelection)
{
    const FString PreviousPath = bPreserveSelection && SelectedPreviousSnapshot.IsValid()
        ? SelectedPreviousSnapshot->FilePath
        : FString();
    const FString CurrentPath = bPreserveSelection && SelectedCurrentSnapshot.IsValid()
        ? SelectedCurrentSnapshot->FilePath
        : FString();

    TArray<FPDSIssue> Issues;
    const TArray<FPDSSnapshotFileDescriptor> Descriptors =
        PDSSnapshotDiffBrowser::EnumerateSnapshotFiles(Issues, DescriptorCache);

    SnapshotOptions.Reset();
    for (const FPDSSnapshotFileDescriptor& Descriptor : Descriptors)
    {
        SnapshotOptions.Add(MakeShared<FPDSSnapshotFileDescriptor>(Descriptor));
    }

    SelectedPreviousSnapshot.Reset();
    SelectedCurrentSnapshot.Reset();

    for (const FSnapshotOption& Option : SnapshotOptions)
    {
        if (!Option.IsValid() || !Option->bIsValid)
        {
            continue;
        }
        if (!PreviousPath.IsEmpty() && Option->FilePath == PreviousPath)
        {
            SelectedPreviousSnapshot = Option;
        }
        if (!CurrentPath.IsEmpty() && Option->FilePath == CurrentPath)
        {
            SelectedCurrentSnapshot = Option;
        }
    }

    if (!SelectedCurrentSnapshot.IsValid())
    {
        for (const FSnapshotOption& Option : SnapshotOptions)
        {
            if (Option.IsValid() && Option->bIsValid && Option->bIsLatest)
            {
                SelectedCurrentSnapshot = Option;
                break;
            }
        }
    }
    if (!SelectedCurrentSnapshot.IsValid())
    {
        for (const FSnapshotOption& Option : SnapshotOptions)
        {
            if (Option.IsValid() && Option->bIsValid && Option->bIsTimestamped)
            {
                SelectedCurrentSnapshot = Option;
                break;
            }
        }
    }

    if (!SelectedPreviousSnapshot.IsValid())
    {
        for (const FSnapshotOption& Option : SnapshotOptions)
        {
            if (Option.IsValid() && Option->bIsValid && Option->bIsBaseline)
            {
                SelectedPreviousSnapshot = Option;
                break;
            }
        }
    }
    if (!SelectedPreviousSnapshot.IsValid())
    {
        for (const FSnapshotOption& Option : SnapshotOptions)
        {
            if (!Option.IsValid() || !Option->bIsValid)
            {
                continue;
            }
            if (!SelectedCurrentSnapshot.IsValid()
                || Option->FilePath != SelectedCurrentSnapshot->FilePath)
            {
                SelectedPreviousSnapshot = Option;
                break;
            }
        }
    }

    if (PreviousSnapshotCombo.IsValid())
    {
        PreviousSnapshotCombo->RefreshOptions();
        PreviousSnapshotCombo->SetSelectedItem(SelectedPreviousSnapshot);
    }
    if (CurrentSnapshotCombo.IsValid())
    {
        CurrentSnapshotCombo->RefreshOptions();
        CurrentSnapshotCombo->SetSelectedItem(SelectedCurrentSnapshot);
    }

    int32 NumValidSnapshots = 0;
    int32 NumInvalidSnapshots = 0;
    for (const FSnapshotOption& Option : SnapshotOptions)
    {
        if (Option.IsValid() && Option->bIsValid)
        {
            ++NumValidSnapshots;
        }
        else
        {
            ++NumInvalidSnapshots;
        }
    }

    FString RefreshStatus = FString::Printf(
        TEXT("Snapshots disponibles: %d válidos, %d inválidos"),
        NumValidSnapshots,
        NumInvalidSnapshots);
    if (!Issues.IsEmpty())
    {
        RefreshStatus += LINE_TERMINATOR;
        RefreshStatus += JoinIssueMessages(Issues);
    }
    SetStatus(RefreshStatus);
}

void SPDSSnapshotDiffBrowser::BuildDefaultFilterOptions()
{
    ChangeFilterOptions = {
        MakeShared<FString>(TEXT("All Changes")),
        MakeShared<FString>(TEXT("Added")),
        MakeShared<FString>(TEXT("Removed")),
        MakeShared<FString>(TEXT("Modified"))
    };
    SelectedChangeFilter = ChangeFilterOptions[0];

    OriginFilterOptions = {
        MakeShared<FString>(TEXT("All Origins")),
        MakeShared<FString>(TEXT("Panthelia Core")),
        MakeShared<FString>(TEXT("Game Content")),
        MakeShared<FString>(TEXT("External Content")),
        MakeShared<FString>(TEXT("Unknown"))
    };
    SelectedOriginFilter = OriginFilterOptions[0];

    ClassFilterOptions = {
        MakeShared<FString>(TEXT("All Classes"))
    };
    SelectedClassFilter = ClassFilterOptions[0];
}

void SPDSSnapshotDiffBrowser::RebuildClassOptions()
{
    const FString PreviousSelection = SelectedClassFilter.IsValid()
        ? *SelectedClassFilter
        : TEXT("All Classes");

    TSet<FString> UniqueClasses;
    for (const FEntryItem& Item : AllEntries)
    {
        if (Item.IsValid() && !Item->GetDisplayClassPath().IsEmpty())
        {
            UniqueClasses.Add(Item->GetDisplayClassPath());
        }
    }

    TArray<FString> SortedClasses = UniqueClasses.Array();
    SortedClasses.Sort();

    ClassFilterOptions.Reset();
    ClassFilterOptions.Add(MakeShared<FString>(TEXT("All Classes")));
    for (const FString& ClassPath : SortedClasses)
    {
        ClassFilterOptions.Add(MakeShared<FString>(ClassPath));
    }

    SelectedClassFilter = ClassFilterOptions[0];
    for (const FStringOption& Option : ClassFilterOptions)
    {
        if (Option.IsValid()
            && Option->Equals(PreviousSelection, ESearchCase::IgnoreCase))
        {
            SelectedClassFilter = Option;
            break;
        }
    }

    if (ClassFilterCombo.IsValid())
    {
        ClassFilterCombo->RefreshOptions();
        ClassFilterCombo->SetSelectedItem(SelectedClassFilter);
    }
}

void SPDSSnapshotDiffBrowser::ApplyCurrentFilters()
{
    FPDSSnapshotDiffBrowserFilter Filter;
    Filter.SearchText = SearchText;

    if (SelectedChangeFilter.IsValid())
    {
        if (SelectedChangeFilter->Equals(TEXT("Added"), ESearchCase::IgnoreCase))
        {
            Filter.ChangeKind = EPDSSnapshotDiffChangeKind::Added;
        }
        else if (SelectedChangeFilter->Equals(TEXT("Removed"), ESearchCase::IgnoreCase))
        {
            Filter.ChangeKind = EPDSSnapshotDiffChangeKind::Removed;
        }
        else if (SelectedChangeFilter->Equals(TEXT("Modified"), ESearchCase::IgnoreCase))
        {
            Filter.ChangeKind = EPDSSnapshotDiffChangeKind::Modified;
        }
    }

    if (SelectedOriginFilter.IsValid())
    {
        if (SelectedOriginFilter->Equals(TEXT("Panthelia Core"), ESearchCase::IgnoreCase))
        {
            Filter.Origin = EPDSAssetOrigin::PantheliaCore;
        }
        else if (SelectedOriginFilter->Equals(TEXT("Game Content"), ESearchCase::IgnoreCase))
        {
            Filter.Origin = EPDSAssetOrigin::GameContent;
        }
        else if (SelectedOriginFilter->Equals(TEXT("External Content"), ESearchCase::IgnoreCase))
        {
            Filter.Origin = EPDSAssetOrigin::ExternalContent;
        }
        else if (SelectedOriginFilter->Equals(TEXT("Unknown"), ESearchCase::IgnoreCase))
        {
            Filter.Origin = EPDSAssetOrigin::Unknown;
        }
    }

    if (SelectedClassFilter.IsValid()
        && !SelectedClassFilter->Equals(TEXT("All Classes"), ESearchCase::IgnoreCase))
    {
        Filter.ClassPath = *SelectedClassFilter;
    }

    FilteredEntries.Reset();
    for (const FEntryItem& Item : AllEntries)
    {
        if (Item.IsValid() && Filter.Matches(*Item))
        {
            FilteredEntries.Add(Item);
        }
    }

    if (EntryListView.IsValid())
    {
        EntryListView->ClearSelection();
        EntryListView->RequestListRefresh();
    }
}

void SPDSSnapshotDiffBrowser::SetStatus(const FString& NewStatus)
{
    StatusText = NewStatus;
}

FReply SPDSSnapshotDiffBrowser::OnRefreshSnapshotsClicked()
{
    RefreshSnapshotOptions(true);
    return FReply::Handled();
}

FReply SPDSSnapshotDiffBrowser::OnCompareClicked()
{
    if (!SelectedPreviousSnapshot.IsValid()
        || !SelectedCurrentSnapshot.IsValid())
    {
        SetStatus(TEXT("Selecciona un snapshot Previous y uno Current."));
        return FReply::Handled();
    }

    if (!SelectedPreviousSnapshot->bIsValid
        || !SelectedCurrentSnapshot->bIsValid)
    {
        SetStatus(TEXT("Uno de los snapshots seleccionados no es válido."));
        return FReply::Handled();
    }

    if (SelectedPreviousSnapshot->FilePath == SelectedCurrentSnapshot->FilePath)
    {
        SetStatus(TEXT("Previous y Current deben ser archivos distintos."));
        return FReply::Handled();
    }

    FPDSSnapshotDocument PreviousDocument;
    FPDSSnapshotDocument CurrentDocument;
    TArray<FPDSIssue> Issues;
    const bool bLoadedPrevious = PDSSnapshotDiffBrowser::LoadSnapshotDocument(
        SelectedPreviousSnapshot->FilePath,
        PreviousDocument,
        Issues);
    const bool bLoadedCurrent = PDSSnapshotDiffBrowser::LoadSnapshotDocument(
        SelectedCurrentSnapshot->FilePath,
        CurrentDocument,
        Issues);

    if (!bLoadedPrevious || !bLoadedCurrent)
    {
        SetStatus(JoinIssueMessages(Issues));
        return FReply::Handled();
    }

    LastDiff = PDSSnapshotDiff::Compare(PreviousDocument, CurrentDocument);

    TArray<FPDSSnapshotDiffBrowserEntry> BuiltEntries;
    PDSSnapshotDiffBrowser::BuildAssetEntries(LastDiff, BuiltEntries);
    AllEntries.Reset();
    AllEntries.Reserve(BuiltEntries.Num());
    for (const FPDSSnapshotDiffBrowserEntry& Entry : BuiltEntries)
    {
        AllEntries.Add(MakeShared<FPDSSnapshotDiffBrowserEntry>(Entry));
    }

    RebuildClassOptions();
    ApplyCurrentFilters();

    const FPDSProjectSnapshotDiffService Service;
    const FPDSOperationResult ReportResult = Service.PersistDiffReports(
        LastDiff,
        TEXT("Manual Snapshot Selection"));

    if (SummaryTextBox.IsValid())
    {
        SummaryTextBox->SetText(FText::FromString(ReportResult.ToMultilineText()));
    }

    SetStatus(FString::Printf(
        TEXT("Comparación lista: %d cambios de assets; %d visibles con los filtros actuales."),
        AllEntries.Num(),
        FilteredEntries.Num()));
    return FReply::Handled();
}

FReply SPDSSnapshotDiffBrowser::OnLocateSelectedClicked()
{
    FAssetData AssetData;
    if (!ResolveSelectedAssetData(AssetData))
    {
        SetStatus(TEXT("No se encontró el asset actual en Asset Registry."));
        return FReply::Handled();
    }

    TArray<FAssetData> AssetsToSync;
    AssetsToSync.Add(AssetData);
    FContentBrowserModule& ContentBrowserModule =
        FModuleManager::LoadModuleChecked<FContentBrowserModule>(TEXT("ContentBrowser"));
    ContentBrowserModule.Get().SyncBrowserToAssets(
        AssetsToSync,
        false,
        true,
        NAME_None,
        false);

    SetStatus(TEXT("Asset localizado en Content Browser."));
    return FReply::Handled();
}

FReply SPDSSnapshotDiffBrowser::OnOpenSelectedClicked()
{
    FAssetData AssetData;
    if (!ResolveSelectedAssetData(AssetData))
    {
        SetStatus(TEXT("No se encontró el asset actual en Asset Registry."));
        return FReply::Handled();
    }

    UObject* Asset = AssetData.GetAsset();
    if (!Asset)
    {
        SetStatus(TEXT("El asset no pudo cargarse para abrir su editor."));
        return FReply::Handled();
    }

    UAssetEditorSubsystem* AssetEditorSubsystem = GEditor
        ? GEditor->GetEditorSubsystem<UAssetEditorSubsystem>()
        : nullptr;
    if (!AssetEditorSubsystem
        || !AssetEditorSubsystem->OpenEditorForAsset(Asset))
    {
        SetStatus(TEXT("Unreal no pudo abrir el editor del asset."));
        return FReply::Handled();
    }

    SetStatus(TEXT("Asset abierto. La suite no guardó ningún paquete."));
    return FReply::Handled();
}

FReply SPDSSnapshotDiffBrowser::OnCopySelectedPathClicked()
{
    const FEntryItem SelectedEntry = GetSelectedEntry();
    if (!SelectedEntry.IsValid())
    {
        SetStatus(TEXT("Selecciona una fila antes de copiar."));
        return FReply::Handled();
    }

    FPlatformApplicationMisc::ClipboardCopy(*SelectedEntry->ObjectPath);
    SetStatus(FString::Printf(
        TEXT("Ruta copiada: %s"),
        *SelectedEntry->ObjectPath));
    return FReply::Handled();
}

void SPDSSnapshotDiffBrowser::OnPreviousSnapshotChanged(
    FSnapshotOption NewSelection,
    ESelectInfo::Type SelectInfo)
{
    SelectedPreviousSnapshot = NewSelection;
}

void SPDSSnapshotDiffBrowser::OnCurrentSnapshotChanged(
    FSnapshotOption NewSelection,
    ESelectInfo::Type SelectInfo)
{
    SelectedCurrentSnapshot = NewSelection;
}

void SPDSSnapshotDiffBrowser::OnChangeFilterChanged(
    FStringOption NewSelection,
    ESelectInfo::Type SelectInfo)
{
    SelectedChangeFilter = NewSelection;
    ApplyCurrentFilters();
}

void SPDSSnapshotDiffBrowser::OnOriginFilterChanged(
    FStringOption NewSelection,
    ESelectInfo::Type SelectInfo)
{
    SelectedOriginFilter = NewSelection;
    ApplyCurrentFilters();
}

void SPDSSnapshotDiffBrowser::OnClassFilterChanged(
    FStringOption NewSelection,
    ESelectInfo::Type SelectInfo)
{
    SelectedClassFilter = NewSelection;
    ApplyCurrentFilters();
}

void SPDSSnapshotDiffBrowser::OnSearchTextChanged(const FText& NewText)
{
    SearchText = NewText.ToString();
    ApplyCurrentFilters();
}

TSharedRef<SWidget> SPDSSnapshotDiffBrowser::GenerateSnapshotOptionWidget(
    FSnapshotOption Option) const
{
    return SNew(STextBlock)
        .Text(FText::FromString(
            Option.IsValid() ? Option->DisplayLabel : FString(TEXT("<invalid>"))));
}

TSharedRef<SWidget> SPDSSnapshotDiffBrowser::GenerateStringOptionWidget(
    FStringOption Option) const
{
    return SNew(STextBlock)
        .Text(FText::FromString(
            Option.IsValid() ? *Option : FString(TEXT("<invalid>"))));
}

TSharedRef<ITableRow> SPDSSnapshotDiffBrowser::GenerateEntryRow(
    FEntryItem Item,
    const TSharedRef<STableViewBase>& OwnerTable) const
{
    return SNew(SPDSSnapshotDiffBrowserRow, OwnerTable)
        .Item(Item);
}

FText SPDSSnapshotDiffBrowser::GetPreviousSnapshotText() const
{
    return FText::FromString(
        SelectedPreviousSnapshot.IsValid()
            ? SelectedPreviousSnapshot->DisplayLabel
            : FString(TEXT("Select previous snapshot")));
}

FText SPDSSnapshotDiffBrowser::GetCurrentSnapshotText() const
{
    return FText::FromString(
        SelectedCurrentSnapshot.IsValid()
            ? SelectedCurrentSnapshot->DisplayLabel
            : FString(TEXT("Select current snapshot")));
}

FText SPDSSnapshotDiffBrowser::GetChangeFilterText() const
{
    return FText::FromString(
        SelectedChangeFilter.IsValid() ? *SelectedChangeFilter : FString(TEXT("All Changes")));
}

FText SPDSSnapshotDiffBrowser::GetOriginFilterText() const
{
    return FText::FromString(
        SelectedOriginFilter.IsValid() ? *SelectedOriginFilter : FString(TEXT("All Origins")));
}

FText SPDSSnapshotDiffBrowser::GetClassFilterText() const
{
    return FText::FromString(
        SelectedClassFilter.IsValid() ? *SelectedClassFilter : FString(TEXT("All Classes")));
}

FText SPDSSnapshotDiffBrowser::GetBaselineSummaryText() const
{
    for (const FSnapshotOption& Option : SnapshotOptions)
    {
        if (Option.IsValid() && Option->bIsBaseline)
        {
            return FText::FromString(FString::Printf(
                TEXT("Current baseline: %s"),
                *Option->DisplayLabel));
        }
    }
    return LOCTEXT("NoBaseline", "Current baseline: <none>");
}

FText SPDSSnapshotDiffBrowser::GetResultCountText() const
{
    return FText::FromString(FString::Printf(
        TEXT("Visible assets: %d / %d"),
        FilteredEntries.Num(),
        AllEntries.Num()));
}

FText SPDSSnapshotDiffBrowser::GetStatusText() const
{
    return FText::FromString(StatusText);
}

SPDSSnapshotDiffBrowser::FEntryItem SPDSSnapshotDiffBrowser::GetSelectedEntry() const
{
    if (!EntryListView.IsValid())
    {
        return nullptr;
    }

    TArray<FEntryItem> SelectedItems;
    EntryListView->GetSelectedItems(SelectedItems);
    return SelectedItems.IsEmpty() ? nullptr : SelectedItems[0];
}

bool SPDSSnapshotDiffBrowser::HasSelectedEntry() const
{
    return GetSelectedEntry().IsValid();
}

bool SPDSSnapshotDiffBrowser::CanActOnSelectedAsset() const
{
    const FEntryItem SelectedEntry = GetSelectedEntry();
    return SelectedEntry.IsValid() && SelectedEntry->CanLocateOrOpen();
}

bool SPDSSnapshotDiffBrowser::ResolveSelectedAssetData(
    FAssetData& OutAssetData) const
{
    const FEntryItem SelectedEntry = GetSelectedEntry();
    if (!SelectedEntry.IsValid() || !SelectedEntry->CanLocateOrOpen())
    {
        return false;
    }

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    OutAssetData = AssetRegistryModule.Get().GetAssetByObjectPath(
        FSoftObjectPath(SelectedEntry->ObjectPath),
        false,
        false);
    return OutAssetData.IsValid();
}

#undef LOCTEXT_NAMESPACE
