#include "PantheliaDeveloperSuiteModule.h"

#include "Framework/Docking/TabManager.h"
#include "SPDSDashboard.h"
#include "ToolMenus.h"
#include "Widgets/Docking/SDockTab.h"

#define LOCTEXT_NAMESPACE "PantheliaDeveloperSuiteModule"

const FName FPantheliaDeveloperSuiteModule::DashboardTabName(
    TEXT("PantheliaDeveloperSuite.Dashboard"));

void FPantheliaDeveloperSuiteModule::StartupModule()
{
    FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
        DashboardTabName,
        FOnSpawnTab::CreateRaw(this, &FPantheliaDeveloperSuiteModule::SpawnDashboardTab))
        .SetDisplayName(LOCTEXT("DashboardTabTitle", "Panthelia Developer Suite"))
        .SetMenuType(ETabSpawnerMenuType::Hidden);

    UToolMenus::RegisterStartupCallback(
        FSimpleMulticastDelegate::FDelegate::CreateRaw(
            this,
            &FPantheliaDeveloperSuiteModule::RegisterMenus));
}

void FPantheliaDeveloperSuiteModule::ShutdownModule()
{
    if (UToolMenus::IsToolMenuUIEnabled())
    {
        UToolMenus::UnRegisterStartupCallback(this);
        UToolMenus::UnregisterOwner(this);
    }

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
        LOCTEXT("OpenDashboardTooltip", "Open Project Doctor profiles, Project Snapshot and Montage Inspector."),
        FSlateIcon(),
        FUIAction(FExecuteAction::CreateRaw(this, &FPantheliaDeveloperSuiteModule::OpenDashboard)));
}

void FPantheliaDeveloperSuiteModule::OpenDashboard()
{
    FGlobalTabmanager::Get()->TryInvokeTab(DashboardTabName);
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

IMPLEMENT_MODULE(FPantheliaDeveloperSuiteModule, PantheliaDeveloperSuite)

#undef LOCTEXT_NAMESPACE
