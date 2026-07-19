#pragma once

#include "CoreMinimal.h"
#include "PDSDeveloperTypes.h"

class FJsonObject;
class UAnimMontage;

class FPDSMontageInspectorService
{
public:
    FPDSOperationResult InspectSelectedMontages() const;

    /** Añade al JSON los datos de un montage ya cargado. */
    static TSharedRef<FJsonObject> BuildMontageJson(const UAnimMontage& Montage);

private:
    static void InspectMontage(
        const UAnimMontage& Montage,
        FString& InOutMarkdown,
        TArray<FPDSIssue>& InOutIssues);
};
