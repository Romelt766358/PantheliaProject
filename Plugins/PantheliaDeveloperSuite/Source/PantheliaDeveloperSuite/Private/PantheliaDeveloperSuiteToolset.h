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
     * @param Limit Máximo de descriptores devueltos. Valores <= 0 usan 20;
     * valores superiores a 500 se limitan a 500.
     */
    UFUNCTION(meta = (AICallable), Category = "Panthelia Developer Suite")
    static FPDSAutomationSnapshotHistoryResult ListSnapshots(int32 Limit);

    /**
     * Exporta un snapshot origin-aware bajo Saved y devuelve metadata estructurada
     * obtenida mediante readback del JSON persistido.
     */
    UFUNCTION(meta = (AICallable), Category = "Panthelia Developer Suite")
    static FPDSAutomationSnapshotExportResult ExportProjectSnapshot();

    /**
     * Ejecuta Project Doctor para un perfil explícito y persiste sus informes.
     * bValidationCompleted indica únicamente ExecutionState=Completed; bSuccess
     * además exige que no existan errores bloqueantes de validación.
     * ExecutionState usa NotStarted, Completed, Cancelled o InfrastructureFailure.
     * Los booleanos son el contrato primario; Cancelled está reservado y Project
     * Doctor de UE 5.8 no lo produce actualmente.
     * @param Profile Perfil read-only que debe validarse.
     * @param MaxIssues Máximo de issues incluidos en la respuesta. Valores <= 0
     * usan 100 y valores superiores a 500 se limitan a 500. El informe conserva todos.
     */
    UFUNCTION(meta = (AICallable), Category = "Panthelia Developer Suite")
    static FPDSAutomationValidationResult ValidateProfile(
        EPDSAutomationValidationProfile Profile,
        int32 MaxIssues);

    /**
     * Reemplaza baseline.json con una copia del latest snapshot bajo Saved y
     * devuelve metadata estructurada del baseline anterior y del nuevo.
     */
    UFUNCTION(meta = (AICallable), Category = "Panthelia Developer Suite")
    static FPDSAutomationBaselineUpdateResult SetLatestSnapshotAsBaseline();

    /**
     * Compara latest.json con baseline.json y devuelve un diff estructurado.
     * @param MaxEntries Máximo de cambios por categoría incluidos en la respuesta.
     * Valores <= 0 usan 100 y valores superiores a 500 se limitan a 500.
     */
    UFUNCTION(meta = (AICallable), Category = "Panthelia Developer Suite")
    static FPDSAutomationDiffResult CompareLatestSnapshotWithBaseline(
        int32 MaxEntries);

    /**
     * Compara los dos snapshots timestamped más recientes.
     * @param MaxEntries Máximo de cambios por categoría incluidos en la respuesta.
     * Valores <= 0 usan 100 y valores superiores a 500 se limitan a 500.
     */
    UFUNCTION(meta = (AICallable), Category = "Panthelia Developer Suite")
    static FPDSAutomationDiffResult CompareLatestTwoSnapshots(
        int32 MaxEntries);
};
