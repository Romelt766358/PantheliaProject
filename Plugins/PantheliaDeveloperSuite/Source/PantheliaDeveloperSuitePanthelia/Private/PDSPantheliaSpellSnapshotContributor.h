#pragma once

#include "PDSSemanticSnapshotTypes.h"

struct FAssetData;
class UPantheliaProjectileSpell;

class FPDSPantheliaSpellSnapshotContributor final
    : public IPDSSnapshotDomainContributor
{
public:
    virtual FString GetDomainId() const override;
    virtual FString GetSchemaVersion() const override;
    virtual void GatherDomainSnapshot(
        FPDSSemanticDomainSnapshot& OutSnapshot) const override;

private:
    bool BuildRecord(
        const FAssetData& AssetData,
        const UPantheliaProjectileSpell& Spell,
        FPDSSemanticRecord& OutRecord,
        TArray<FPDSIssue>& OutIssues) const;
};
