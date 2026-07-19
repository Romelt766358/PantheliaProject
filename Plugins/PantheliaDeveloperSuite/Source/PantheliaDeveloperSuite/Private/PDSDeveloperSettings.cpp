#include "PDSDeveloperSettings.h"

UPDSDeveloperSettings::UPDSDeveloperSettings()
{
    PantheliaCorePackageRoots = {
        TEXT("/Game/Blueprints"),
        TEXT("/Game/Characters"),
        TEXT("/Game/UI")
    };

    ExternalContentPackageRoots = {
        TEXT("/Game/ARPG_Pack"),
        TEXT("/Game/Fab")
    };

    AlwaysExcludedPackageRoots = {
        TEXT("/Game/Developers")
    };
}

FName UPDSDeveloperSettings::GetContainerName() const
{
    return TEXT("Project");
}

FName UPDSDeveloperSettings::GetCategoryName() const
{
    return TEXT("Plugins");
}

FName UPDSDeveloperSettings::GetSectionName() const
{
    return TEXT("PantheliaDeveloperSuite");
}
