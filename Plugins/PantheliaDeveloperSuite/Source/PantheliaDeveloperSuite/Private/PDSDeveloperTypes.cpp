#include "PDSDeveloperTypes.h"

namespace
{
    int32 SeveritySortRank(const EPDSIssueSeverity Severity)
    {
        switch (Severity)
        {
        case EPDSIssueSeverity::Error:
            return 0;
        case EPDSIssueSeverity::Warning:
            return 1;
        default:
            return 2;
        }
    }
}

namespace PDSDeveloperTypes
{
    FString SeverityToString(const EPDSIssueSeverity Severity)
    {
        switch (Severity)
        {
        case EPDSIssueSeverity::Error:
            return TEXT("ERROR");
        case EPDSIssueSeverity::Warning:
            return TEXT("WARNING");
        default:
            return TEXT("INFO");
        }
    }

    FString AssetOriginToString(const EPDSAssetOrigin Origin)
    {
        switch (Origin)
        {
        case EPDSAssetOrigin::PantheliaCore:
            return TEXT("Panthelia Core");
        case EPDSAssetOrigin::GameContent:
            return TEXT("Game Content");
        case EPDSAssetOrigin::ExternalContent:
            return TEXT("External Content");
        default:
            return TEXT("Unknown");
        }
    }

    FString ValidationProfileToId(const EPDSValidationProfile Profile)
    {
        switch (Profile)
        {
        case EPDSValidationProfile::SelectedAssets:
            return TEXT("selected-assets");
        case EPDSValidationProfile::PantheliaCore:
            return TEXT("panthelia-core");
        case EPDSValidationProfile::GameContent:
            return TEXT("game-content");
        case EPDSValidationProfile::ExternalContent:
            return TEXT("external-content");
        case EPDSValidationProfile::EntireProject:
            return TEXT("entire-project");
        default:
            return TEXT("unknown");
        }
    }

    FString ValidationProfileToLabel(const EPDSValidationProfile Profile)
    {
        switch (Profile)
        {
        case EPDSValidationProfile::SelectedAssets:
            return TEXT("Selected Assets");
        case EPDSValidationProfile::PantheliaCore:
            return TEXT("Panthelia Core");
        case EPDSValidationProfile::GameContent:
            return TEXT("Game Content");
        case EPDSValidationProfile::ExternalContent:
            return TEXT("External Content");
        case EPDSValidationProfile::EntireProject:
            return TEXT("Entire Project");
        default:
            return TEXT("Unknown");
        }
    }

    FString CompactMessage(FString Message, const int32 MaxCharacters)
    {
        Message.ReplaceInline(TEXT("\r"), TEXT(" "));
        Message.ReplaceInline(TEXT("\n"), TEXT(" "));
        while (Message.Contains(TEXT("  ")))
        {
            Message.ReplaceInline(TEXT("  "), TEXT(" "));
        }
        Message.TrimStartAndEndInline();

        const int32 SafeLimit = FMath::Max(MaxCharacters, 16);
        if (Message.Len() > SafeLimit)
        {
            Message = Message.Left(SafeLimit - 3) + TEXT("...");
        }

        return Message;
    }
}

FString FPDSIssue::ToDisplayString(const bool bCompactMessage) const
{
    const FString DisplayMessage = bCompactMessage
        ? PDSDeveloperTypes::CompactMessage(Message)
        : Message;

    return FString::Printf(
        TEXT("[%s] [%s] [%s] %s — %s"),
        *PDSDeveloperTypes::SeverityToString(Severity),
        RuleId.IsEmpty() ? TEXT("PDS") : *RuleId,
        *PDSDeveloperTypes::AssetOriginToString(Origin),
        AssetPath.IsEmpty() ? TEXT("<sin asset>") : *AssetPath,
        *DisplayMessage);
}

bool FPDSValidationSummary::HasErrors() const
{
    if (NumInvalid > 0)
    {
        return true;
    }

    return Issues.ContainsByPredicate([](const FPDSIssue& Issue)
    {
        return Issue.Severity == EPDSIssueSeverity::Error;
    });
}

TArray<FPDSIssueGroup> FPDSValidationSummary::BuildIssueGroups(
    const int32 SampleAssetLimit) const
{
    TArray<FPDSIssueGroup> Groups;
    TMap<FString, int32> GroupIndexByKey;
    const int32 SafeSampleLimit = FMath::Max(SampleAssetLimit, 0);

    for (const FPDSIssue& Issue : Issues)
    {
        const FString Key = FString::Printf(
            TEXT("%d|%d|%s"),
            static_cast<int32>(Issue.Severity),
            static_cast<int32>(Issue.Origin),
            *Issue.RuleId);

        int32* ExistingIndex = GroupIndexByKey.Find(Key);
        if (!ExistingIndex)
        {
            FPDSIssueGroup& NewGroup = Groups.AddDefaulted_GetRef();
            NewGroup.Severity = Issue.Severity;
            NewGroup.Origin = Issue.Origin;
            NewGroup.RuleId = Issue.RuleId;
            ExistingIndex = &GroupIndexByKey.Add(Key, Groups.Num() - 1);
        }

        FPDSIssueGroup& Group = Groups[*ExistingIndex];
        ++Group.Count;

        if (!Issue.AssetPath.IsEmpty()
            && Group.SampleAssetPaths.Num() < SafeSampleLimit
            && !Group.SampleAssetPaths.Contains(Issue.AssetPath))
        {
            Group.SampleAssetPaths.Add(Issue.AssetPath);
        }
    }

    Groups.Sort([](const FPDSIssueGroup& A, const FPDSIssueGroup& B)
    {
        const int32 SeverityComparison = SeveritySortRank(A.Severity)
            - SeveritySortRank(B.Severity);
        if (SeverityComparison != 0)
        {
            return SeverityComparison < 0;
        }

        if (A.Origin != B.Origin)
        {
            return static_cast<int32>(A.Origin) < static_cast<int32>(B.Origin);
        }

        return A.RuleId < B.RuleId;
    });

    return Groups;
}

FString FPDSValidationSummary::ToMultilineText(const int32 MaxIssues) const
{
    FString Output = FString::Printf(
        TEXT("Validación completada\nPerfil: %s\nGenerada UTC: %s\nSolicitados: %d\nComprobados: %d\nVálidos: %d\nInválidos: %d\nCon warnings: %d\nNo validados: %d\nOmitidos: %d\nNo se pudieron validar: %d\nLímite alcanzado: %s\nCancelado: %s\n"),
        ScopeLabel.IsEmpty() ? TEXT("<desconocido>") : *ScopeLabel,
        GeneratedAtUtc.IsEmpty() ? TEXT("<desconocida>") : *GeneratedAtUtc,
        NumRequested,
        NumChecked,
        NumValid,
        NumInvalid,
        NumWithWarnings,
        NumNotValidated,
        NumSkipped,
        NumUnableToValidate,
        bAssetLimitReached ? TEXT("Sí") : TEXT("No"),
        bCancelled ? TEXT("Sí") : TEXT("No"));

    Output += FString::Printf(
        TEXT("\nOrigen de assets solicitados:\n- Panthelia Core: %d\n- Game Content: %d\n- External Content: %d\n- Unknown: %d\n"),
        NumPantheliaCoreAssetsRequested,
        NumGameContentAssetsRequested,
        NumExternalContentAssetsRequested,
        NumUnknownOriginAssetsRequested);

    Output += FString::Printf(
        TEXT("\nFiltrado:\n- Mapas excluidos: %d\n- External Objects excluidos: %d\n- Excluidos por configuración: %d\n- Fuera del perfil: %d\n"),
        NumExcludedMaps,
        NumExcludedExternalObjects,
        NumExcludedByConfiguration,
        NumOutsideSelectedProfile);

    Output += FString::Printf(
        TEXT("\nEfectos dirty observables: %s\n- Paquetes dirty antes: %d\n- Paquetes dirty después: %d\n- Paquetes nuevos dirty observados: %d\n- Violación read-only confirmada: No\n"),
        bReadOnlyVerificationPassed ? TEXT("NINGUNO") : TEXT("DETECTADOS"),
        NumDirtyPackagesBefore,
        NumDirtyPackagesAfter,
        NewlyDirtiedPackages.Num());

    if (!OutputPath.IsEmpty())
    {
        Output += FString::Printf(TEXT("Informe Markdown: %s\n"), *OutputPath);
    }

    if (!OutputJsonPath.IsEmpty())
    {
        Output += FString::Printf(TEXT("Informe JSON: %s\n"), *OutputJsonPath);
    }

    const TArray<FPDSIssueGroup> Groups = BuildIssueGroups();
    if (!Groups.IsEmpty())
    {
        Output += TEXT("\nGrupos de hallazgos:\n");
        for (const FPDSIssueGroup& Group : Groups)
        {
            Output += FString::Printf(
                TEXT("- [%s] [%s] %s: %d\n"),
                *PDSDeveloperTypes::SeverityToString(Group.Severity),
                *PDSDeveloperTypes::AssetOriginToString(Group.Origin),
                Group.RuleId.IsEmpty() ? TEXT("PDS") : *Group.RuleId,
                Group.Count);
        }
    }

    if (!Issues.IsEmpty())
    {
        Output += TEXT("\nVista previa de hallazgos:\n");
        const int32 IssueLimit = MaxIssues == INDEX_NONE
            ? Issues.Num()
            : FMath::Clamp(MaxIssues, 0, Issues.Num());

        for (int32 IssueIndex = 0; IssueIndex < IssueLimit; ++IssueIndex)
        {
            Output += Issues[IssueIndex].ToDisplayString(true);
            Output += LINE_TERMINATOR;
        }

        if (IssueLimit < Issues.Num())
        {
            Output += FString::Printf(
                TEXT("... %d hallazgos adicionales. Consulta los informes completos.\n"),
                Issues.Num() - IssueLimit);
        }
    }

    return Output;
}

FString FPDSOperationResult::ToMultilineText() const
{
    FString Output = Summary;

    if (bCancelled)
    {
        Output += TEXT("\n\nEstado: cancelado por el usuario.");
    }

    if (!OutputPath.IsEmpty())
    {
        Output += FString::Printf(TEXT("\n\nSalida: %s"), *OutputPath);
    }

    if (!Issues.IsEmpty())
    {
        Output += TEXT("\n\nHallazgos:\n");
        for (const FPDSIssue& Issue : Issues)
        {
            Output += Issue.ToDisplayString();
            Output += LINE_TERMINATOR;
        }
    }

    return Output;
}
