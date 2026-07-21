#pragma once

#include "CoreMinimal.h"
#include "PDSAutomationTypes.generated.h"

/** Perfiles de validación que una IA puede solicitar mediante la Automation API. */
UENUM(BlueprintType)
enum class EPDSAutomationValidationProfile : uint8
{
    PantheliaCore UMETA(DisplayName = "Panthelia Core"),
    GameContent UMETA(DisplayName = "Game Content"),
    ExternalContent UMETA(DisplayName = "External Content"),
    EntireProject UMETA(DisplayName = "Entire Project")
};

/** Hallazgo reflejado y estable para respuestas estructuradas de Toolset Registry/MCP. */
USTRUCT(BlueprintType)
struct PANTHELIADEVELOPERSUITE_API FPDSAutomationIssue
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString Severity;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString RuleId;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString AssetPath;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString Message;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString Origin;
};

/** Resultado común para operaciones sencillas como exportar o establecer baseline. */
USTRUCT(BlueprintType)
struct PANTHELIADEVELOPERSUITE_API FPDSAutomationOperationResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bSuccess = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bCancelled = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString Summary;

    /** Ruta del informe Markdown producido por la operación, cuando exista. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString MarkdownReportPath;

    /** Ruta del informe JSON producido por la operación, cuando exista. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString JsonReportPath;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 TotalIssueCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bIssuesTruncated = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    TArray<FPDSAutomationIssue> Issues;
};

/** Descriptor pequeño de un snapshot disponible bajo Saved/PantheliaDeveloperSuite. */
USTRUCT(BlueprintType)
struct PANTHELIADEVELOPERSUITE_API FPDSAutomationSnapshotDescriptor
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString FilePath;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString FileName;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString DisplayLabel;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString GeneratedAtUtc;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString SchemaVersion;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString ProjectName;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 AssetCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bIsBaseline = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bIsLatest = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bIsTimestamped = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bIsValid = false;
};

/** Metadata normalizada de un snapshot persistido y parseado. */
USTRUCT(BlueprintType)
struct PANTHELIADEVELOPERSUITE_API FPDSAutomationSnapshotMetadata
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString FilePath;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString GeneratedAtUtc;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString SchemaVersion;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString ProjectName;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString EngineVersion;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 AssetCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 GameplayTagCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 MontageCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 SemanticDomainCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 SemanticRecordCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 SpellCount = 0;

    /** True cuando esta metadata procede de un snapshot parseado correctamente. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bValid = false;
};

/** Resultado estructurado de la exportación de un snapshot. */
USTRUCT(BlueprintType)
struct PANTHELIADEVELOPERSUITE_API FPDSAutomationSnapshotExportResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bSuccess = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bCancelled = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString Summary;

    /** Ruta exacta del snapshot timestamped producido por esta operación. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString TimestampedSnapshotPath;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FPDSAutomationSnapshotMetadata TimestampedSnapshot;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString LatestSnapshotPath;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bTimestampedSnapshotReadable = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bLatestSnapshotExists = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    double DurationMilliseconds = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 TotalIssueCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bIssuesTruncated = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    TArray<FPDSAutomationIssue> Issues;
};

/** Resultado estructurado del reemplazo de baseline.json. */
USTRUCT(BlueprintType)
struct PANTHELIADEVELOPERSUITE_API FPDSAutomationBaselineUpdateResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bSuccess = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bCancelled = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString Summary;

    /** Ruta canónica de baseline.json reemplazada por esta operación. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString BaselinePath;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bPreviousBaselineExisted = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bPreviousBaselineReadable = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FPDSAutomationSnapshotMetadata PreviousBaseline;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bNewBaselineReadable = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FPDSAutomationSnapshotMetadata NewBaseline;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    double DurationMilliseconds = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 TotalIssueCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bIssuesTruncated = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    TArray<FPDSAutomationIssue> Issues;
};

/** Lista acotada de snapshots para evitar respuestas MCP excesivamente grandes. */
USTRUCT(BlueprintType)
struct PANTHELIADEVELOPERSUITE_API FPDSAutomationSnapshotHistoryResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bSuccess = false;

    /** Valor crudo recibido; <= 0 solicita el default de 20. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 RequestedLimit = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 AppliedLimit = 20;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 MaximumAllowedLimit = 500;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    double DurationMilliseconds = 0.0;

    /** Número real de archivos snapshot presentes, incluidos los omitidos por settings. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 AvailableSnapshotCount = 0;

    /** Descriptores inspeccionados tras aplicar el cap de historial de los settings. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 InspectedSnapshotCount = 0;

    /** Descriptores incluidos finalmente en esta respuesta. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 ReturnedSnapshotCount = 0;

    /** Snapshots válidos dentro del conjunto inspeccionado. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 ValidSnapshotCount = 0;

    /** Snapshots inválidos dentro del conjunto inspeccionado. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 InvalidSnapshotCount = 0;

    /** True cuando los settings impidieron inspeccionar todo el historial físico. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bDescriptorEnumerationCappedBySettings = false;

    /** True cuando esta respuesta no incluye todos los snapshots físicos disponibles. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bSnapshotsTruncated = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 TotalIssueCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bIssuesTruncated = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    TArray<FPDSAutomationSnapshotDescriptor> Snapshots;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    TArray<FPDSAutomationIssue> Issues;
};

/** Estado ligero de la suite para que Codex compruebe disponibilidad antes de operar. */
USTRUCT(BlueprintType)
struct PANTHELIADEVELOPERSUITE_API FPDSAutomationStatusResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bSuccess = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString AutomationApiVersion;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    double DurationMilliseconds = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString ProjectName;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString EngineVersion;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString SnapshotDirectory;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bBaselineExists = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bLatestExists = false;

    /** Número real de archivos snapshot presentes en disco. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 AvailableSnapshotCount = 0;

    /**
     * GetStatus no parsea snapshots para mantener una latencia baja. Este flag permanece
     * false y los dos conteos siguientes valen -1; ListSnapshots obtiene los conteos reales
     * del conjunto inspeccionado.
     */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bSnapshotValidityKnown = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 ValidSnapshotCount = -1;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 InvalidSnapshotCount = -1;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 TotalIssueCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bIssuesTruncated = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    TArray<FPDSAutomationIssue> Issues;
};

/** Resumen estructurado de una validación Project Doctor. */
USTRUCT(BlueprintType)
struct PANTHELIADEVELOPERSUITE_API FPDSAutomationValidationResult
{
    GENERATED_BODY()

    /**
     * True cuando la validación terminó y no encontró errores bloqueantes.
     * Un false no implica necesariamente fallo de ejecución; consulta bValidationCompleted.
     */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bSuccess = false;

    /** True únicamente cuando ExecutionState vale Completed. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bValidationCompleted = false;

    /**
     * Estado para logging: NotStarted, Completed, Cancelled o InfrastructureFailure.
     * Los tres booleanos son el contrato primario para lógica de agentes.
     * Cancelled está reservado; Project Doctor de UE 5.8 no lo produce actualmente.
     */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString ExecutionState;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bCancelled = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bInfrastructureFailure = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString ProfileId;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString ProfileLabel;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString GeneratedAtUtc;

    /** Valor crudo recibido; <= 0 solicita el default de 100. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 RequestedIssueLimit = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 AppliedIssueLimit = 100;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 MaximumAllowedIssueLimit = 500;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    double DurationMilliseconds = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 NumRequested = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 NumChecked = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 NumValid = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 NumInvalid = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 NumWithWarnings = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 NumNotValidated = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 NumSkipped = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 NumUnableToValidate = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bAssetLimitReached = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bReadOnlyVerificationPassed = true;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 TotalNewlyDirtiedPackageCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bNewlyDirtiedPackagesTruncated = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    TArray<FString> NewlyDirtiedPackages;

    /** Ruta del informe Markdown persistido para esta respuesta. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString MarkdownReportPath;

    /** Ruta del informe JSON persistido para esta respuesta. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString JsonReportPath;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 TotalIssueCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bIssuesTruncated = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    TArray<FPDSAutomationIssue> Issues;
};

/** Cambio de asset serializable y limitado para una respuesta MCP. */
USTRUCT(BlueprintType)
struct PANTHELIADEVELOPERSUITE_API FPDSAutomationAssetChange
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString ChangeKind;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString ObjectPath;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString PreviousClassPath;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString CurrentClassPath;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString PreviousOrigin;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString CurrentOrigin;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bExistsInCurrentSnapshot = false;
};

/** Cambio semántico serializable y limitado para una respuesta MCP. */
USTRUCT(BlueprintType)
struct PANTHELIADEVELOPERSUITE_API FPDSAutomationSemanticChange
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString DomainId;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString RecordId;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString DisplayName;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString SourceAssetPath;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString ChangeKind;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString FieldName;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString PreviousValue;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString CurrentValue;
};

/** Diff estructurado y acotado para consumo directo por agentes MCP. */
USTRUCT(BlueprintType)
struct PANTHELIADEVELOPERSUITE_API FPDSAutomationDiffResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bSuccess = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bHasChanges = false;

    /** Valor crudo recibido; <= 0 solicita el default de 100. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 RequestedEntryLimit = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 AppliedEntryLimit = 100;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 MaximumAllowedEntryLimit = 500;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    double DurationMilliseconds = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString ComparisonLabel;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString PreviousSnapshotPath;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString CurrentSnapshotPath;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString PreviousGeneratedAtUtc;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString CurrentGeneratedAtUtc;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString PreviousSchemaVersion;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString CurrentSchemaVersion;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 PreviousAssetCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 CurrentAssetCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 AddedAssetCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 RemovedAssetCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 ModifiedAssetCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 TotalAssetChangeCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 ReturnedAssetChangeCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bAssetChangesTruncated = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    TArray<FPDSAutomationAssetChange> AssetChanges;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 PreviousGameplayTagCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 CurrentGameplayTagCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 AddedGameplayTagCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 RemovedGameplayTagCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    TArray<FString> AddedGameplayTags;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    TArray<FString> RemovedGameplayTags;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 PreviousMontageCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 CurrentMontageCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 AddedMontageCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 RemovedMontageCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 ModifiedMontageCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    TArray<FString> AddedMontages;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    TArray<FString> RemovedMontages;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    TArray<FString> ModifiedMontages;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 OriginNotComparableAssetCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 PreviousSemanticRecordCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 CurrentSemanticRecordCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 AddedSemanticRecordCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 RemovedSemanticRecordCount = 0;

    /** Número de records únicos con al menos un field modificado. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 ModifiedSemanticRecordCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 ChangedSemanticFieldCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 SemanticDomainNotComparableCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 TotalSemanticChangeCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 ReturnedSemanticChangeCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bSemanticChangesTruncated = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    TArray<FPDSAutomationSemanticChange> SemanticChanges;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bEntriesTruncated = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString Summary;

    /** Ruta del informe Markdown persistido para esta respuesta. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString MarkdownReportPath;

    /** Ruta del informe JSON persistido para esta respuesta. */
    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    FString JsonReportPath;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    int32 TotalIssueCount = 0;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    bool bIssuesTruncated = false;

    UPROPERTY(BlueprintReadOnly, Category = "PDS Automation")
    TArray<FPDSAutomationIssue> Issues;
};
