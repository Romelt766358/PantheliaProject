#pragma once

#include "CoreMinimal.h"
#include "AssetRegistry/AssetData.h"
#include "PDSDeveloperTypes.h"

struct FPDSAssetSelection;

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

    FPDSValidationSummary ValidateProfile(EPDSValidationProfile Profile) const;

    FPDSValidationSummary ValidateAssets(
        const TArray<FAssetData>& Assets,
        EPDSValidationProfile Profile = EPDSValidationProfile::SelectedAssets) const;

private:
    FPDSValidationSummary ValidateAssetSelection(
        const FPDSAssetSelection& Selection,
        EPDSValidationProfile Profile) const;

    void PersistValidationReports(FPDSValidationSummary& InOutSummary) const;

    static FString BuildValidationReportMarkdown(
        const FPDSValidationSummary& Summary);

    static bool BuildValidationReportJson(
        const FPDSValidationSummary& Summary,
        FString& OutSerializedJson);
};
