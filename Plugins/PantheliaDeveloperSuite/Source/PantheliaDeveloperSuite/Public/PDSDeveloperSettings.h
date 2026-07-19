#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "PDSDeveloperSettings.generated.h"

/**
 * Configuración compartida del plugin. Se expone en Project Settings para que
 * Panthelia pueda ajustar perfiles sin recompilar el módulo. Config=Game se mantiene
 * porque fue la variante que registró la sección correctamente en el Panthelia real
 * con UE 5.8; la clase y el módulo continúan siendo exclusivamente Editor-only.
 */
UCLASS(Config = Game, DefaultConfig, meta = (DisplayName = "Panthelia Developer Suite"))
class PANTHELIADEVELOPERSUITE_API UPDSDeveloperSettings final : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UPDSDeveloperSettings();

    virtual FName GetContainerName() const override;
    virtual FName GetCategoryName() const override;
    virtual FName GetSectionName() const override;

    /**
     * Raíces consideradas código/contenido propio de Panthelia.
     * Precedencia: Always Excluded se evalúa antes y External tiene prioridad sobre Core.
     */
    UPROPERTY(Config, EditAnywhere, Category = "Validation Profiles", meta = (
        ToolTip = "Raíces propias de Panthelia. En solapamientos, Always Excluded se evalúa primero y External Content tiene prioridad sobre Panthelia Core."))
    TArray<FString> PantheliaCorePackageRoots;

    /**
     * Raíces consideradas paquetes externos o contenido de terceros.
     * External tiene prioridad sobre Core cuando dos raíces se solapan.
     */
    UPROPERTY(Config, EditAnywhere, Category = "Validation Profiles", meta = (
        ToolTip = "Raíces de contenido externo. En solapamientos, External Content tiene prioridad sobre Panthelia Core."))
    TArray<FString> ExternalContentPackageRoots;

    /**
     * Raíces omitidas en todos los perfiles masivos. La selección manual no se filtra.
     * Esta lista se evalúa antes de clasificar un asset como External o Core.
     */
    UPROPERTY(Config, EditAnywhere, Category = "Validation Profiles", meta = (
        ToolTip = "Raíces omitidas en perfiles masivos. Se evalúan antes que External Content y Panthelia Core. Selected Assets no aplica este filtro."))
    TArray<FString> AlwaysExcludedPackageRoots;

    /** Cantidad máxima de issues mostrados en el cuadro de texto del dashboard. */
    UPROPERTY(Config, EditAnywhere, Category = "Dashboard", meta = (ClampMin = "10", ClampMax = "1000"))
    int32 DashboardIssuePreviewLimit = 100;

    /** Cantidad máxima de entradas por categoría mostradas en el resumen de Snapshot Diff. */
    UPROPERTY(Config, EditAnywhere, Category = "Dashboard", meta = (
        ClampMin = "5", ClampMax = "500",
        ToolTip = "Limita solo la vista del dashboard. Los informes Markdown y JSON conservan el diff completo."))
    int32 SnapshotDiffPreviewLimit = 50;

    /**
     * Cantidad máxima de snapshots timestamped mostrados en el navegador. Baseline y
     * latest se incluyen aparte. Evita que el refresco del selector escale sin límite.
     */
    UPROPERTY(Config, EditAnywhere, Category = "Snapshot History", meta = (
        ClampMin = "10", ClampMax = "500",
        ToolTip = "Máximo de snapshots timestamped enumerados por Snapshot Diff Browser. Baseline y latest siempre se incluyen si existen."))
    int32 SnapshotBrowserMaxTimestampedSnapshots = 20;

    /**
     * Eleva a Error un paquete que aparece dirty después de la operación.
     * La detección es un efecto secundario observable; no demuestra por sí sola que el plugin
     * haya modificado deliberadamente el asset. El default conservador es Warning.
     */
    UPROPERTY(Config, EditAnywhere, Category = "Safety", meta = (
        ToolTip = "Cuando está activo, los paquetes newly dirty se reportan como Error. La señal puede provenir de PostLoad, upgrades o recompilación al cargar; no confirma por sí sola una violación del contrato read-only."))
    bool bTreatNewlyDirtiedPackagesAsError = false;
};
