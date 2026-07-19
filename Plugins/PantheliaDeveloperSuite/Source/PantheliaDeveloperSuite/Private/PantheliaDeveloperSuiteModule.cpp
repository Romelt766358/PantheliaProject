#include "PantheliaDeveloperSuiteModule.h"

#include "Framework/Docking/TabManager.h"
#include "PantheliaDeveloperSuiteToolset.h"
#include "SPDSDashboard.h"
#include "SPDSSnapshotDiffBrowser.h"
#include "ToolMenus.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "PantheliaDeveloperSuiteModule"

const FName FPantheliaDeveloperSuiteModule::DashboardTabName(
    TEXT("PantheliaDeveloperSuite.Dashboard"));
const FName FPantheliaDeveloperSuiteModule::SnapshotDiffBrowserTabName(
    TEXT("PantheliaDeveloperSuite.SnapshotDiffBrowser"));

void FPantheliaDeveloperSuiteModule::StartupModule()
{
    // UE 5.8 registra sus toolsets oficiales explícitamente desde StartupModule.
    UToolsetRegistry::RegisterToolsetClass(
        UPantheliaDeveloperSuiteToolset::StaticClass());

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        DashboardTabName,
        FOnSpawnTab::CreateRaw(this, &FPantheliaDeveloperSuiteModule::SpawnDashboardTab))
        .SetDisplayName(LOCTEXT("DashboardTabTitle", "Panthelia Developer Suite"))
        .SetMenuType(ETabSpawnerMenuType::Hidden);

    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        SnapshotDiffBrowserTabName,
        FOnSpawnTab::CreateRaw(
            this,
            &FPantheliaDeveloperSuiteModule::SpawnSnapshotDiffBrowserTab))
        .SetDisplayName(LOCTEXT("SnapshotDiffTabTitle", "PDS Snapshot Diff Browser"))
        .SetMenuType(ETabSpawnerMenuType::Hidden);

    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(
            this,
            &FPantheliaDeveloperSuiteModule::RegisterMenus));
}

void FPantheliaDeveloperSuiteModule::ShutdownModule()
{
    // Mismo guard usado por GASToolsets en UE 5.8 durante el cierre del editor.
    if (UObjectInitialized())
    {
        UToolsetRegistry::UnregisterToolsetClass(
            UPantheliaDeveloperSuiteToolset::StaticClass());
    }

    if (UToolMenus::IsToolMenuUIEnabled())
    {
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
    }

    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(SnapshotDiffBrowserTabName);
    FGlobalTabmanager::Get()->UnregisterNomadTabSpawner(DashboardTabName);
}

void FPantheliaDeveloperSuiteModule::RegisterMenus()
{
    FToolMenuOwnerScoped OwnerScoped(this);

    UToolMenu* ToolsMenu = UToolMenus::Get()->ExtendMenu(TEXT("LevelEditor.MainMenu.Tools"));
    FToolMenuSection& Section = ToolsMenu->FindOrAddSection(
        TEXT("PantheliaDeveloperSuite"),
        LOCTEXT("PantheliaDeveloperSuiteSection", "Panthelia"),
        FToolMenuInsert());

    Section.AddMenuEntry(
        TEXT("OpenPantheliaDeveloperSuite"),
        LOCTEXT("OpenDashboardLabel", "Panthelia Developer Suite"),
        LOCTEXT(
            "OpenDashboardTooltip",
            "Open Project Doctor, snapshots, diffs and Montage Inspector. Structured read-only automation is also exposed to Unreal MCP through Toolset Registry."),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateRaw(
            this,
            &FPantheliaDeveloperSuiteModule::OpenDashboard)));

    Section.AddMenuEntry(
        TEXT("OpenPantheliaSnapshotDiffBrowser"),
        LOCTEXT("OpenSnapshotDiffBrowserLabel", "PDS Snapshot Diff Browser"),
        LOCTEXT(
            "OpenSnapshotDiffBrowserTooltip",
            "Open the read-only browser for selecting, filtering and inspecting snapshot differences."),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateRaw(
            this,
            &FPantheliaDeveloperSuiteModule::OpenSnapshotDiffBrowser)));
}

void FPantheliaDeveloperSuiteModule::OpenDashboard()
{
    FGlobalTabmanager::Get()->TryInvokeTab(DashboardTabName);
}

void FPantheliaDeveloperSuiteModule::OpenSnapshotDiffBrowser()
{
    FGlobalTabmanager::Get()->TryInvokeTab(SnapshotDiffBrowserTabName);
}

TSharedRef<SDockTab> FPantheliaDeveloperSuiteModule::SpawnDashboardTab(
    const FSpawnTabArgs& SpawnTabArgs)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SPDSDashboard)
        ];
}

TSharedRef<SDockTab> FPantheliaDeveloperSuiteModule::SpawnSnapshotDiffBrowserTab(
    const FSpawnTabArgs& SpawnTabArgs)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SPDSSnapshotDiffBrowser)
        ];
}

IMPLEMENT_MODULE(FPantheliaDeveloperSuiteModule, PantheliaDeveloperSuite)

#undef LOCTEXT_NAMESPACE
