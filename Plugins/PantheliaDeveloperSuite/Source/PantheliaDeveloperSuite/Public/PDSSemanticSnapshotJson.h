#pragma once

#include "CoreMinimal.h"
#include "PDSSemanticSnapshotTypes.h"

class FJsonObject;

namespace PDSSemanticSnapshotJson
{
    PANTHELIADEVELOPERSUITE_API TSharedRef<FJsonObject> BuildDomainsJson(
        const TArray<FPDSSemanticDomainSnapshot>& Domains);

    PANTHELIADEVELOPERSUITE_API bool ParseDomainsJson(
        const TSharedPtr<FJsonObject>& RootObject,
        TMap<FString, FPDSSemanticDomainSnapshot>& OutDomainsById,
        TArray<FPDSIssue>& OutIssues);
}
