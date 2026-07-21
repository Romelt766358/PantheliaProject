#pragma once

#include "Modules/ModuleManager.h"

class FPDSPantheliaSpellSnapshotContributor;

class FPantheliaDeveloperSuitePantheliaModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override;
    virtual void ShutdownModule() override;

private:
    TSharedPtr<FPDSPantheliaSpellSnapshotContributor> SpellContributor;
};
