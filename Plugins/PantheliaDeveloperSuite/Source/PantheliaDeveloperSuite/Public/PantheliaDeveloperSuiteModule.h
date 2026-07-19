#pragma once

#include "Modules/ModuleManager.h"

class SDockTab;
class SPDSDashboard;
class FSpawnTabArgs;

class FPantheliaDeveloperSuiteModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    void RegisterMenus();
    void OpenDashboard();
    TSharedRef<SDockTab> SpawnDashboardTab(const FSpawnTabArgs& SpawnTabArgs);

    static const FName DashboardTabName;
};
