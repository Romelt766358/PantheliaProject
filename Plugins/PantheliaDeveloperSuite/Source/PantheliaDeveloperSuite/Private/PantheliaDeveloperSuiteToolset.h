#pragma once

#include "CoreMinimal.h"
#include "PDSAutomationTypes.h"
#include "ToolsetRegistry/ToolsetDefinition.h"
#include "PantheliaDeveloperSuiteToolset.generated.h"

/**
 * Herramientas estructuradas de Panthelia Developer Suite para Unreal MCP y
 * cualquier otra superficie que consuma Toolset Registry.
 */
UCLASS(BlueprintType, Hidden)
class UPantheliaDeveloperSuiteToolset final : public UToolsetDefinition
{
    GENERATED_BODY()

public:
    virtual FString GetToolsetVersion() const override;

    /**
     * Devuelve un estado ligero sin parsear JSON de snapshots. Los conteos de
     * validez permanecen desconocidos hasta llamar ListSnapshots.
     */
    UFUNCTION(meta = (AICallable), Category = "Panthelia Developer Suite")
    static FPDSAutomationStatusResult GetStatus();

    /**
     * Enumera snapshots disponibles con respuesta acotada. AvailableSnapshotCount
     * representa todos los archivos físicos; la enumeración de descriptores también
     * respeta SnapshotBrowserMaxTimestampedSnapshots de los settings del plugin.
     * @param Limit Máximo de descriptores devueltos. Valores <= 0 usan 20.
     */
    UFUNCTION(meta = (AICallable), Category = "Panthelia Developer Suite")
    static FPDSAutomationSnapshotHistoryResult ListSnapshots(int32 Limit);

    /**
     * Exporta un snapshot origin-aware del proyecto bajo Saved.
     */
    UFUNCTION(meta = (AICallable), Category = "Panthelia Developer Suite")
    static FPDSAutomationOperationResult ExportProjectSnapshot();

    /**
     * Ejecuta Project Doctor para un perfil explícito y persiste sus informes.
     * bValidationCompleted indica que la operación terminó; bSuccess además exige
     * que no existan errores bloqueantes de validación.
     * @param Profile Perfil read-only que debe validarse.
     * @param MaxIssues Máximo de issues incluidos en la respuesta. El informe conserva todos.
     */
    UFUNCTION(meta = (AICallable), Category = "Panthelia Developer Suite")
    static FPDSAutomationValidationResult ValidateProfile(
        EPDSAutomationValidationProfile Profile,
        int32 MaxIssues);

    /**
     * Reemplaza baseline.json con una copia del latest snapshot bajo Saved.
     */
    UFUNCTION(meta = (AICallable), Category = "Panthelia Developer Suite")
    static FPDSAutomationOperationResult SetLatestSnapshotAsBaseline();

    /**
     * Compara latest.json con baseline.json y devuelve un diff estructurado.
     * @param MaxEntries Máximo de cambios por categoría incluidos en la respuesta.
     */
    UFUNCTION(meta = (AICallable), Category = "Panthelia Developer Suite")
    static FPDSAutomationDiffResult CompareLatestSnapshotWithBaseline(
        int32 MaxEntries);

    /**
     * Compara los dos snapshots timestamped más recientes.
     * @param MaxEntries Máximo de cambios por categoría incluidos en la respuesta.
     */
    UFUNCTION(meta = (AICallable), Category = "Panthelia Developer Suite")
    static FPDSAutomationDiffResult CompareLatestTwoSnapshots(
        int32 MaxEntries);
};
