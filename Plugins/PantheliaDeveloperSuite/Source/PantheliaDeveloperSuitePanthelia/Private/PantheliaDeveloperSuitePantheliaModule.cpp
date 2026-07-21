#include "PantheliaDeveloperSuitePantheliaModule.h"

#include "PDSPantheliaSpellSnapshotContributor.h"
#include "PDSSemanticSnapshotTypes.h"

DEFINE_LOG_CATEGORY_STATIC(LogPDSPanthelia, Log, All);

void FPantheliaDeveloperSuitePantheliaModule::StartupModule()
{
    SpellContributor = MakeShared<FPDSPantheliaSpellSnapshotContributor>();

    FString Error;
    if (!FPDSSnapshotDomainRegistry::Get().RegisterContributor(
            SpellContributor.ToSharedRef(),
            &Error))
    {
        UE_LOG(
            LogPDSPanthelia,
            Error,
            TEXT("PDS Panthelia adapter no pudo registrar spells: %s"),
            *Error);
        SpellContributor.Reset();
    }
}

void FPantheliaDeveloperSuitePantheliaModule::ShutdownModule()
{
    if (SpellContributor.IsValid())
    {
        FPDSSnapshotDomainRegistry::Get().UnregisterContributor(
            SpellContributor->GetDomainId());
        SpellContributor.Reset();
    }
}

IMPLEMENT_MODULE(
    FPantheliaDeveloperSuitePantheliaModule,
    PantheliaDeveloperSuitePanthelia)
