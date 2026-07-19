#pragma once

#include "CoreMinimal.h"
#include "PDSDeveloperTypes.h"

class FPDSProjectSnapshotService
{
public:
    FPDSOperationResult ExportProjectSnapshot(
        const FPDSValidationSummary* OptionalValidationSummary = nullptr) const;
};
