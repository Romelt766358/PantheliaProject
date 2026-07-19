#include "PDSMontageInspectorService.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/ScopedSlowTask.h"
#include "PDSAssetUtilities.h"

namespace
{
    constexpr float DuplicateNotifyToleranceSeconds = 0.001f;

    struct FSeenNotify
    {
        FString ClassOrName;
        float TriggerTime = 0.0f;
        float Duration = 0.0f;
    };

    FString GetNotifyClassName(const FAnimNotifyEvent& NotifyEvent)
    {
        if (NotifyEvent.NotifyStateClass)
        {
            return NotifyEvent.NotifyStateClass->GetClass()->GetPathName();
        }

        if (NotifyEvent.Notify)
        {
            return NotifyEvent.Notify->GetClass()->GetPathName();
        }

        if (!NotifyEvent.NotifyName.IsNone())
        {
            return NotifyEvent.NotifyName.ToString();
        }

        return TEXT("<Notify sin clase ni nombre>");
    }

    bool IsDuplicateNotify(
        const FSeenNotify& SeenNotify,
        const FString& ClassOrName,
        const float TriggerTime,
        const float Duration)
    {
        return SeenNotify.ClassOrName == ClassOrName
            && FMath::IsNearlyEqual(
                SeenNotify.TriggerTime,
                TriggerTime,
                DuplicateNotifyToleranceSeconds)
            && FMath::IsNearlyEqual(
                SeenNotify.Duration,
                Duration,
                DuplicateNotifyToleranceSeconds);
    }
}

FPDSOperationResult FPDSMontageInspectorService::InspectSelectedMontages() const
{
    FPDSOperationResult Result;
    const TArray<FAssetData> SelectedAssets = PDSAssetUtilities::GetSelectedAssets();

    if (SelectedAssets.IsEmpty())
    {
        Result.Summary = TEXT("No hay assets seleccionados en el Content Browser.");
        Result.Issues.Add({
            EPDSIssueSeverity::Warning,
            TEXT("PDS.Montage.NoInput"),
            FString(),
            TEXT("La selección vacía se trata como ausencia de entrada, no como fallo técnico.")
        });
        return Result;
    }

    TArray<FAssetData> MontageAssets;
    MontageAssets.Reserve(SelectedAssets.Num());
    int32 IgnoredNonMontageCount = 0;

    for (const FAssetData& AssetData : SelectedAssets)
    {
        if (AssetData.IsInstanceOf(UAnimMontage::StaticClass(), EResolveClass::No))
        {
            MontageAssets.Add(AssetData);
        }
        else
        {
            ++IgnoredNonMontageCount;
        }
    }

    if (MontageAssets.IsEmpty())
    {
        Result.Summary = TEXT("No había Animation Montages entre los assets seleccionados.");
        Result.Issues.Add({
            EPDSIssueSeverity::Warning,
            TEXT("PDS.Montage.NoMontagesSelected"),
            FString(),
            TEXT("Selecciona uno o más UAnimMontage en el Content Browser.")
        });
        return Result;
    }

    FString Markdown = TEXT("# Panthelia Developer Suite — Montage Inspection\n\n");
    Markdown += FString::Printf(
        TEXT("Generated UTC: `%s`\n\n"),
        *FDateTime::UtcNow().ToIso8601());

    int32 MontageCount = 0;
    FScopedSlowTask SlowTask(
        static_cast<float>(MontageAssets.Num()),
        NSLOCTEXT(
            "PantheliaDeveloperSuite",
            "InspectMontagesProgress",
            "Inspecting selected Animation Montages..."));
    SlowTask.MakeDialog(true);

    for (const FAssetData& AssetData : MontageAssets)
    {
        SlowTask.EnterProgressFrame(
            1.0f,
            FText::FromString(AssetData.AssetName.ToString()));

        if (SlowTask.ShouldCancel())
        {
            Result.bCancelled = true;
            Result.Summary = FString::Printf(
                TEXT("Inspección cancelada después de procesar %d de %d montages. No se guardó un informe parcial."),
                MontageCount,
                MontageAssets.Num());
            Result.Issues.Add({
                EPDSIssueSeverity::Warning,
                TEXT("PDS.Montage.Cancelled"),
                FString(),
                TEXT("La operación fue cancelada por el usuario antes de completar todos los montages.")
            });
            return Result;
        }

        UAnimMontage* Montage = Cast<UAnimMontage>(AssetData.GetAsset());
        if (!Montage)
        {
            Result.Issues.Add({
                EPDSIssueSeverity::Warning,
                TEXT("PDS.Montage.LoadFailed"),
                AssetData.GetSoftObjectPath().ToString(),
                TEXT("Asset Registry lo identificó como UAnimMontage o subclase, pero no pudo cargarse como tal.")
            });
            continue;
        }

        ++MontageCount;
        InspectMontage(*Montage, Markdown, Result.Issues);
    }

    if (IgnoredNonMontageCount > 0)
    {
        Result.Issues.Add({
            EPDSIssueSeverity::Info,
            TEXT("PDS.Montage.NonMontagesIgnored"),
            FString(),
            FString::Printf(
                TEXT("Se ignoraron %d assets seleccionados que no eran UAnimMontage ni subclases."),
                IgnoredNonMontageCount)
        });
    }

    const FString ReportsDirectory = FPaths::Combine(
        PDSAssetUtilities::GetSuiteSavedDirectory(),
        TEXT("Reports"));
    PDSAssetUtilities::EnsureDirectoryExists(ReportsDirectory);

    const FString TimestampedPath = FPaths::Combine(
        ReportsDirectory,
        FString::Printf(
            TEXT("MontageReport_%s.md"),
            *PDSAssetUtilities::MakeTimestampForFileName()));
    const FString LatestPath = FPaths::Combine(
        ReportsDirectory,
        TEXT("latest_montages.md"));

    const bool bSavedTimestamped =
        FFileHelper::SaveStringToFile(Markdown, *TimestampedPath);
    const bool bSavedLatest =
        PDSAssetUtilities::SaveStringAtomically(Markdown, LatestPath);

    Result.bSuccess = bSavedTimestamped && bSavedLatest;
    Result.OutputPath = TimestampedPath;
    Result.Summary = FString::Printf(
        TEXT("Se inspeccionaron %d montages. Hallazgos: %d. Guardado: %s"),
        MontageCount,
        Result.Issues.Num(),
        Result.bSuccess ? TEXT("Sí") : TEXT("No"));

    if (!Result.bSuccess)
    {
        Result.Issues.Add({
            EPDSIssueSeverity::Error,
            TEXT("PDS.IO.SaveFailed"),
            TimestampedPath,
            TEXT("No fue posible guardar el informe timestamped o reemplazar latest_montages.md de forma atómica.")
        });
    }

    return Result;
}

void FPDSMontageInspectorService::InspectMontage(
    const UAnimMontage& Montage,
    FString& InOutMarkdown,
    TArray<FPDSIssue>& InOutIssues)
{
    const FString MontagePath = Montage.GetPathName();
    const float PlayLength = Montage.GetPlayLength();

    InOutMarkdown += FString::Printf(TEXT("## `%s`\n\n"), *MontagePath);
    InOutMarkdown += FString::Printf(TEXT("- Duration: `%.4f s`\n"), PlayLength);
    InOutMarkdown += FString::Printf(TEXT("- Slot tracks: `%d`\n"), Montage.SlotAnimTracks.Num());
    InOutMarkdown += FString::Printf(TEXT("- Sections: `%d`\n"), Montage.CompositeSections.Num());
    InOutMarkdown += FString::Printf(TEXT("- Notify tracks: `%d`\n"), Montage.AnimNotifyTracks.Num());
    InOutMarkdown += FString::Printf(TEXT("- Notify events: `%d`\n\n"), Montage.Notifies.Num());

    if (PlayLength <= 0.0f)
    {
        InOutIssues.Add({
            EPDSIssueSeverity::Error,
            TEXT("PDS.Montage.ZeroLength"),
            MontagePath,
            TEXT("El montage no tiene una duración positiva.")
        });
    }

    if (Montage.SlotAnimTracks.IsEmpty())
    {
        InOutIssues.Add({
            EPDSIssueSeverity::Error,
            TEXT("PDS.Montage.NoSlotTracks"),
            MontagePath,
            TEXT("El montage no contiene SlotAnimTracks.")
        });
    }

    if (Montage.CompositeSections.IsEmpty())
    {
        InOutIssues.Add({
            EPDSIssueSeverity::Warning,
            TEXT("PDS.Montage.NoSections"),
            MontagePath,
            TEXT("El montage no contiene Composite Sections.")
        });
    }

    InOutMarkdown += TEXT("| Index | Track | Trigger | Duration | End | Notify |\n");
    InOutMarkdown += TEXT("|---:|---:|---:|---:|---:|---|\n");

    TArray<FSeenNotify> SeenNotifies;
    SeenNotifies.Reserve(Montage.Notifies.Num());

    for (int32 NotifyIndex = 0; NotifyIndex < Montage.Notifies.Num(); ++NotifyIndex)
    {
        const FAnimNotifyEvent& NotifyEvent = Montage.Notifies[NotifyIndex];
        const float TriggerTime = NotifyEvent.GetTriggerTime();
        const float Duration = NotifyEvent.GetDuration();
        const float EndTime = TriggerTime + Duration;
        const FString NotifyClassName = GetNotifyClassName(NotifyEvent);

        InOutMarkdown += FString::Printf(
            TEXT("| %d | %d | %.4f | %.4f | %.4f | `%s` |\n"),
            NotifyIndex,
            NotifyEvent.TrackIndex,
            TriggerTime,
            Duration,
            EndTime,
            *NotifyClassName);

        if (TriggerTime < -KINDA_SMALL_NUMBER
            || TriggerTime > PlayLength + KINDA_SMALL_NUMBER)
        {
            InOutIssues.Add({
                EPDSIssueSeverity::Error,
                TEXT("PDS.Montage.NotifyOutOfRange"),
                MontagePath,
                FString::Printf(
                    TEXT("Notify %d (%s) dispara en %.4f fuera de [0, %.4f]."),
                    NotifyIndex,
                    *NotifyClassName,
                    TriggerTime,
                    PlayLength)
            });
        }

        if (Duration > 0.0f && EndTime > PlayLength + KINDA_SMALL_NUMBER)
        {
            InOutIssues.Add({
                EPDSIssueSeverity::Error,
                TEXT("PDS.Montage.NotifyStateOutOfRange"),
                MontagePath,
                FString::Printf(
                    TEXT("Notify State %d (%s) termina en %.4f después de %.4f."),
                    NotifyIndex,
                    *NotifyClassName,
                    EndTime,
                    PlayLength)
            });
        }

        const bool bIsDuplicate = SeenNotifies.ContainsByPredicate(
            [&NotifyClassName, TriggerTime, Duration](const FSeenNotify& SeenNotify)
            {
                return IsDuplicateNotify(
                    SeenNotify,
                    NotifyClassName,
                    TriggerTime,
                    Duration);
            });

        if (bIsDuplicate)
        {
            InOutIssues.Add({
                EPDSIssueSeverity::Warning,
                TEXT("PDS.Montage.DuplicateNotify"),
                MontagePath,
                FString::Printf(
                    TEXT("Notify duplicado dentro de una tolerancia de %.3f s: %s en %.4f, duración %.4f (track %d)."),
                    DuplicateNotifyToleranceSeconds,
                    *NotifyClassName,
                    TriggerTime,
                    Duration,
                    NotifyEvent.TrackIndex)
            });
        }
        else
        {
            SeenNotifies.Add(FSeenNotify{ NotifyClassName, TriggerTime, Duration });
        }
    }

    InOutMarkdown += TEXT("\n");
}

TSharedRef<FJsonObject> FPDSMontageInspectorService::BuildMontageJson(
    const UAnimMontage& Montage)
{
    TSharedRef<FJsonObject> MontageJson = MakeShared<FJsonObject>();
    MontageJson->SetStringField(TEXT("path"), Montage.GetPathName());
    MontageJson->SetNumberField(TEXT("playLengthSeconds"), Montage.GetPlayLength());
    MontageJson->SetNumberField(TEXT("slotTrackCount"), Montage.SlotAnimTracks.Num());
    MontageJson->SetNumberField(TEXT("sectionCount"), Montage.CompositeSections.Num());
    MontageJson->SetNumberField(TEXT("notifyTrackCount"), Montage.AnimNotifyTracks.Num());

    TArray<TSharedPtr<FJsonValue>> SectionsJson;
    for (const FCompositeSection& Section : Montage.CompositeSections)
    {
        TSharedRef<FJsonObject> SectionJson = MakeShared<FJsonObject>();
        SectionJson->SetStringField(TEXT("name"), Section.SectionName.ToString());
        SectionJson->SetNumberField(TEXT("startTimeSeconds"), Section.GetTime());
        SectionsJson.Add(MakeShared<FJsonValueObject>(SectionJson));
    }
    MontageJson->SetArrayField(TEXT("sections"), SectionsJson);

    TArray<TSharedPtr<FJsonValue>> NotifiesJson;
    for (int32 NotifyIndex = 0; NotifyIndex < Montage.Notifies.Num(); ++NotifyIndex)
    {
        const FAnimNotifyEvent& NotifyEvent = Montage.Notifies[NotifyIndex];
        const float TriggerTime = NotifyEvent.GetTriggerTime();
        const float Duration = NotifyEvent.GetDuration();

        TSharedRef<FJsonObject> NotifyJson = MakeShared<FJsonObject>();
        NotifyJson->SetNumberField(TEXT("index"), NotifyIndex);
        NotifyJson->SetNumberField(TEXT("trackIndex"), NotifyEvent.TrackIndex);
        NotifyJson->SetStringField(TEXT("classOrName"), GetNotifyClassName(NotifyEvent));
        NotifyJson->SetNumberField(TEXT("triggerTimeSeconds"), TriggerTime);
        NotifyJson->SetNumberField(TEXT("durationSeconds"), Duration);
        NotifyJson->SetNumberField(TEXT("endTimeSeconds"), TriggerTime + Duration);
        NotifiesJson.Add(MakeShared<FJsonValueObject>(NotifyJson));
    }
    MontageJson->SetArrayField(TEXT("notifies"), NotifiesJson);

    return MontageJson;
}
