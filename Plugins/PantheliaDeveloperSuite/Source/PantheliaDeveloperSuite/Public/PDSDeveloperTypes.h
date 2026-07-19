#pragma once

#include "CoreMinimal.h"

/** Severidad común para Project Doctor, Snapshot y Montage Inspector. */
enum class EPDSIssueSeverity : uint8
{
    Info,
    Warning,
    Error
};

/** Procedencia aproximada del asset, determinada mediante raíces configurables. */
enum class EPDSAssetOrigin : uint8
{
    Unknown,
    PantheliaCore,
    GameContent,
    ExternalContent
};

/** Perfiles masivos disponibles en Project Doctor. */
enum class EPDSValidationProfile : uint8
{
    SelectedAssets,
    PantheliaCore,
    GameContent,
    ExternalContent,
    EntireProject
};

/** Hallazgo serializable/presentable sin depender de un tipo de asset concreto. */
struct FPDSIssue
{
    EPDSIssueSeverity Severity = EPDSIssueSeverity::Info;
    FString RuleId;
    FString AssetPath;
    FString Message;
    EPDSAssetOrigin Origin = EPDSAssetOrigin::Unknown;

    FString ToDisplayString(bool bCompactMessage = false) const;
};

/** Agrupación estable para resumir muchos hallazgos sin perder el detalle completo. */
struct FPDSIssueGroup
{
    EPDSIssueSeverity Severity = EPDSIssueSeverity::Info;
    EPDSAssetOrigin Origin = EPDSAssetOrigin::Unknown;
    FString RuleId;
    int32 Count = 0;
    TArray<FString> SampleAssetPaths;
};

/** Resultado agregado de una validación. */
struct FPDSValidationSummary
{
    int32 NumRequested = 0;
    int32 NumChecked = 0;
    int32 NumValid = 0;
    int32 NumInvalid = 0;
    int32 NumWithWarnings = 0;
    int32 NumNotValidated = 0;
    int32 NumSkipped = 0;
    int32 NumUnableToValidate = 0;
    bool bAssetLimitReached = false;
    bool bCancelled = false;
    FString GeneratedAtUtc;
    FString ScopeId;
    FString ScopeLabel;
    FString OutputPath;
    FString OutputJsonPath;

    int32 NumPantheliaCoreAssetsRequested = 0;
    int32 NumGameContentAssetsRequested = 0;
    int32 NumExternalContentAssetsRequested = 0;
    int32 NumUnknownOriginAssetsRequested = 0;

    int32 NumExcludedMaps = 0;
    int32 NumExcludedExternalObjects = 0;
    int32 NumExcludedByConfiguration = 0;
    int32 NumOutsideSelectedProfile = 0;

    int32 NumDirtyPackagesBefore = 0;
    int32 NumDirtyPackagesAfter = 0;
    bool bReadOnlyVerificationPassed = true;
    TArray<FString> NewlyDirtiedPackages;

    TArray<FPDSIssue> Issues;

    bool HasErrors() const;
    TArray<FPDSIssueGroup> BuildIssueGroups(int32 SampleAssetLimit = 3) const;

    /**
     * Convierte el resultado a texto legible. MaxIssues limita únicamente la vista
     * del dashboard; los informes persistidos conservan todos los hallazgos.
     */
    FString ToMultilineText(int32 MaxIssues = INDEX_NONE) const;
};

/** Resultado sencillo para exportaciones e inspecciones. */
struct FPDSOperationResult
{
    bool bSuccess = false;
    bool bCancelled = false;
    FString Summary;
    FString OutputPath;
    TArray<FPDSIssue> Issues;

    FString ToMultilineText() const;
};

namespace PDSDeveloperTypes
{
    FString SeverityToString(EPDSIssueSeverity Severity);
    FString AssetOriginToString(EPDSAssetOrigin Origin);
    FString ValidationProfileToId(EPDSValidationProfile Profile);
    FString ValidationProfileToLabel(EPDSValidationProfile Profile);
    FString CompactMessage(FString Message, int32 MaxCharacters = 240);
}
