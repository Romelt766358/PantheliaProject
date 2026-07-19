#pragma once

#include "Modules/ModuleManager.h"

class SDockTab;
class FSpawnTabArgs;

class FPantheliaDeveloperSuiteModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

    /** Abre o enfoca el navegador accionable de Snapshot Diff. */
    void OpenSnapshotDiffBrowser();

private:
    void RegisterMenus();
    void OpenDashboard();
    TSharedRef<SDockTab> SpawnDashboardTab(const FSpawnTabArgs& SpawnTabArgs);
    TSharedRef<SDockTab> SpawnSnapshotDiffBrowserTab(const FSpawnTabArgs& SpawnTabArgs);

    static const FName DashboardTabName;
    static const FName SnapshotDiffBrowserTabName;
};
