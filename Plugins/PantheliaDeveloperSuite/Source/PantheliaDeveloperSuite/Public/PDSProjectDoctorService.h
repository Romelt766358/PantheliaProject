#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "PDSDeveloperTypes.h"

struct FPDSAssetSelection;
struct FValidateAssetsSettings;

class FPDSProjectDoctorService
{
public:
    FPDSValidationSummary ValidateSelectedAssets() const;
    FPDSValidationSummary ValidatePantheliaCore() const;
    FPDSValidationSummary ValidateGameContent() const;
    FPDSValidationSummary ValidateExternalContent() const;
    FPDSValidationSummary ValidateEntireProject() const;

    /** Compatibilidad con v0.1.2: ahora equivale a ValidateEntireProject. */
    FPDSValidationSummary ValidateProject() const;

    FPDSValidationSummary ValidateProfile(
        EPDSValidationProfile Profile,
        bool bNonInteractive = false) const;

    FPDSValidationSummary ValidateAssets(
        const TArray<FAssetData>& Assets,
        EPDSValidationProfile Profile = EPDSValidationProfile::SelectedAssets,
        bool bNonInteractive = false) const;

    /** Configura Data Validation de Project Doctor para el contexto interactivo o no interactivo solicitado. */
    static void ConfigureValidationSettings(
        FValidateAssetsSettings& Settings,
        bool bNonInteractive);

private:
    FPDSValidationSummary ValidateAssetSelection(
        const FPDSAssetSelection& Selection,
        EPDSValidationProfile Profile,
        bool bNonInteractive) const;

    void PersistValidationReports(FPDSValidationSummary& InOutSummary) const;

    static FString BuildValidationReportMarkdown(
        const FPDSValidationSummary& Summary);

    static bool BuildValidationReportJson(
        const FPDSValidationSummary& Summary,
        FString& OutSerializedJson);
};
