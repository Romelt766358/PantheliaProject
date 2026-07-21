#pragma once

#include "CoreMinimal.h"
#include "PDSDeveloperTypes.h"

/**
 * Registro semántico genérico.
 *
 * El core no conoce GAS, armas, bosses ni hechizos. Cada contributor transforma
 * su dominio en strings canónicos y deterministas.
 *
 * Convención de contrato:
 * - Los nombres de fields aportados por contributors no deben comenzar con '$'.
 * - El prefijo '$' queda reservado al comparador para pseudo-campos estructurales.
 */
struct PANTHELIADEVELOPERSUITE_API FPDSSemanticRecord
{
    FString RecordId;
    FString DisplayName;
    FString Kind;
    FString SourceAssetPath;
    TMap<FString, FString> Fields;

    void Normalize();
};

/** Snapshot normalizado de un dominio semántico, por ejemplo "spells". */
struct PANTHELIADEVELOPERSUITE_API FPDSSemanticDomainSnapshot
{
    FString DomainId;
    FString SchemaVersion;
    TArray<FPDSSemanticRecord> Records;
    TArray<FPDSIssue> Issues;

    /**
     * Conteo runtime-only de assets que el contributor tuvo que cargar.
     * No se serializa. El exportador usa la suma para decidir una única pasada de GC.
     */
    int32 NewlyLoadedAssetCount = 0;

    void Normalize();
};

/**
 * Extensión de snapshot registrada por un módulo externo.
 *
 * Debe ejecutarse en game thread y no debe modificar assets.
 */
class PANTHELIADEVELOPERSUITE_API IPDSSnapshotDomainContributor
{
public:
    virtual ~IPDSSnapshotDomainContributor() = default;

    virtual FString GetDomainId() const = 0;
    virtual FString GetSchemaVersion() const = 0;
    virtual void GatherDomainSnapshot(FPDSSemanticDomainSnapshot& OutSnapshot) const = 0;
};

/**
 * Registro de contributors. El módulo que registra conserva ownership del shared ref
 * y debe hacer Unregister durante ShutdownModule.
 */
class PANTHELIADEVELOPERSUITE_API FPDSSnapshotDomainRegistry
{
public:
    static FPDSSnapshotDomainRegistry& Get();

    bool RegisterContributor(
        const TSharedRef<IPDSSnapshotDomainContributor>& Contributor,
        FString* OutError = nullptr);

    bool UnregisterContributor(const FString& DomainId);

    TArray<FPDSSemanticDomainSnapshot> GatherAllDomains(
        TArray<FPDSIssue>& OutIssues) const;

private:
    TMap<FString, TSharedRef<IPDSSnapshotDomainContributor>> ContributorsByDomainId;
};
