// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/PantheliaDodgeAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitDelay.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AbilitySystem/PantheliaAbilitySystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PantheliaGameplayTags.h"
#include "PantheliaLogChannels.h"

UPantheliaDodgeAbility::UPantheliaDodgeAbility()
{
	// El dodge mantiene estado runtime propio (task + handle de i-frames), por lo que
	// necesita una instancia persistente por avatar y no puede ejecutarse sobre el CDO.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	InputActivationPolicy = EPantheliaAbilityInputActivationPolicy::OnInputTriggered;

	BaseIFrameDuration = FScalableFloat(0.30f);
	MaxIFrameDuration = FScalableFloat(1.00f);
	BasePerfectDodgeWindowDuration = FScalableFloat(0.12f);
	MaxPerfectDodgeWindowDuration = FScalableFloat(0.30f);
	BaseDashDistance = FScalableFloat(300.f);
}

bool UPantheliaDodgeAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	// Aunque GAS normalmente impide reactivar una ability InstancedPerActor activa,
	// este guard hace explícita la regla: una pulsación durante el mismo dash no crea
	// otra ejecución ni reinicia el montage.
	if (IsActive())
	{
		return false;
	}

	return Super::CanActivateAbility(
		Handle,
		ActorInfo,
		SourceTags,
		TargetTags,
		OptionalRelevantTags);
}

void UPantheliaDodgeAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	UCharacterMovementComponent* MovementComponent = Character ? Character->GetCharacterMovement() : nullptr;
	USkeletalMeshComponent* MeshComponent = Character ? Character->GetMesh() : nullptr;

	FPantheliaDodgeRequest DodgeRequest;
	const bool bValidRequest =
		Character &&
		MovementComponent &&
		MeshComponent &&
		MeshComponent->GetAnimInstance() &&
		BuildDodgeRequest(DodgeRequest) &&
		IsValid(DodgeRequest.Montage) &&
		DodgeRequest.AuthoredTravelDistance > KINDA_SMALL_NUMBER;

	const float FinalDashDistance = GetFinalDashDistance();
	if (!bValidRequest || FinalDashDistance <= KINDA_SMALL_NUMBER)
	{
		// Un request/montage mal configurado no debe cobrar estamina ni conceder estado.
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Creamos la escucha antes de cobrar y antes del montage. Crear la task no produce
	// gameplay ni activa nada; solo valida que la infraestructura del Perfect Dodge
	// está disponible. OnlyTriggerOnce=false es intencional: un HitAvoided temprano
	// fuera de la ventana no debe consumir la escucha de un impacto posterior válido.
	const FPantheliaGameplayTags& DodgeTags = FPantheliaGameplayTags::Get();
	UAbilityTask_WaitGameplayEvent* PendingHitAvoidedEventTask =
		UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(
			this,
			DodgeTags.Event_Dodge_HitAvoided,
			/*OptionalExternalTarget=*/nullptr,
			/*OnlyTriggerOnce=*/false,
			/*OnlyMatchExact=*/true);

	if (!PendingHitAvoidedEventTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Orden deliberado: primero sabemos que el dash puede ejecutarse; después cobramos.
	// Si no hay estamina suficiente, CommitAbility falla y no se reproduce nada.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		PendingHitAvoidedEventTask->EndTask();
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Estado limpio para esta activación. EndAbility también lo limpia como red de
	// seguridad, pero se reinicia aquí antes de que el montage pueda disparar notifies.
	bIFramesStarted = false;
	bPerfectDodgeWindowRequested = false;
	bPerfectDodgeWindowStarted = false;
	bPerfectDodgeWindowOpen = false;
	bPerfectDodgeTriggered = false;
	IFramesStartTimeSeconds = 0.f;
	IFramesEffectHandle = FActiveGameplayEffectHandle();

	HitAvoidedEventTask = PendingHitAvoidedEventTask;
	HitAvoidedEventTask->EventReceived.AddDynamic(
		this, &UPantheliaDodgeAbility::OnHitAvoidedEventReceived);
	HitAvoidedEventTask->ReadyForActivation();

	// Evita sumar la velocidad de carrera o el input acumulado al root motion del dash.
	// No desactivamos CharacterMovement: el montage sigue controlando el desplazamiento.
	MovementComponent->StopMovementImmediately();
	Character->ConsumeMovementInputVector();

	if (DodgeRequest.bApplyRotation)
	{
		const FRotator DesiredYawOnly(0.f, DodgeRequest.DesiredRotation.Yaw, 0.f);
		Character->SetActorRotation(DesiredYawOnly);
	}

	const float RootMotionTranslationScale =
		FinalDashDistance / DodgeRequest.AuthoredTravelDistance;

	if (!FMath::IsFinite(RootMotionTranslationScale) || RootMotionTranslationScale <= 0.f)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// UE 5.8 expone AnimRootMotionTranslationScale directamente en esta task. La escala
	// pertenece a esta ejecución: la task la aplica y la restaura sin estado global.
	CurrentMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		DodgeRequest.Montage,
		/*Rate=*/1.f,
		/*StartSection=*/NAME_None,
		/*bStopWhenAbilityEnds=*/true,
		/*AnimRootMotionTranslationScale=*/RootMotionTranslationScale,
		/*StartTimeSeconds=*/0.f);

	if (!CurrentMontageTask)
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// No terminamos en OnBlendOut: puede ocurrir antes de los últimos notifies. Solo el
	// fin real o una interrupción/cancelación cierran esta activación.
	CurrentMontageTask->OnCompleted.AddDynamic(this, &UPantheliaDodgeAbility::OnDodgeMontageCompleted);
	CurrentMontageTask->OnInterrupted.AddDynamic(this, &UPantheliaDodgeAbility::OnDodgeMontageInterruptedOrCancelled);
	CurrentMontageTask->OnCancelled.AddDynamic(this, &UPantheliaDodgeAbility::OnDodgeMontageInterruptedOrCancelled);
	CurrentMontageTask->ReadyForActivation();
}

void UPantheliaDodgeAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Cerramos primero las escuchas/ventanas para impedir callbacks tardíos durante la
	// limpieza de la ability. Después retiramos el GE exacto de esta activación.
	ClearPerfectDodgeWindowTask();
	ClearHitAvoidedEventTask();
	ClearIFrames();
	ClearMontageTask();

	bIFramesStarted = false;
	bPerfectDodgeWindowRequested = false;
	bPerfectDodgeWindowStarted = false;
	bPerfectDodgeWindowOpen = false;
	bPerfectDodgeTriggered = false;
	IFramesStartTimeSeconds = 0.f;

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
}

void UPantheliaDodgeAbility::StartIFrames()
{
	if (!IsActive() || bIFramesStarted)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	UWorld* World = GetWorld();
	if (!ASC || !World)
	{
		return;
	}

	IFramesEffectHandle = UPantheliaAbilitySystemLibrary::GrantTemporaryGameplayTag(
		ASC,
		FPantheliaGameplayTags::Get().State_Invulnerable_Dodge,
		GetFinalIFrameDuration());

	if (!IFramesEffectHandle.IsValid())
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("[DODGE] No se pudo conceder State.Invulnerable.Dodge."));
		return;
	}

	bIFramesStarted = true;
	IFramesStartTimeSeconds = World->GetTimeSeconds();

	// Si Unreal procesó primero el notify de Perfect Window en el mismo timestamp,
	// la petición quedó guardada y se resuelve ahora, ya dentro de los i-frames.
	if (bPerfectDodgeWindowRequested)
	{
		OpenPerfectDodgeWindow();
	}
}

void UPantheliaDodgeAbility::StartPerfectDodgeWindow()
{
	if (!IsActive() || bPerfectDodgeWindowStarted)
	{
		return;
	}

	bPerfectDodgeWindowRequested = true;

	// La ventana perfecta nunca puede existir antes que la invulnerabilidad real.
	// Si los notifies comparten frame y este llega primero, StartIFrames la abrirá.
	if (bIFramesStarted)
	{
		OpenPerfectDodgeWindow();
	}
}

float UPantheliaDodgeAbility::GetFinalIFrameDuration() const
{
	const float AbilityLevel = static_cast<float>(FMath::Max(1, GetAbilityLevel()));
	const float BaseDuration = BaseIFrameDuration.GetValueAtLevel(AbilityLevel);
	const float ConfiguredCap = MaxIFrameDuration.GetValueAtLevel(AbilityLevel);
	const float SafeCap = FMath::Max(MinIFrameDuration, ConfiguredCap);

	return FMath::Clamp(BaseDuration, MinIFrameDuration, SafeCap);
}

float UPantheliaDodgeAbility::GetFinalPerfectDodgeWindowDuration() const
{
	const float AbilityLevel = static_cast<float>(FMath::Max(1, GetAbilityLevel()));
	const float BaseDuration = BasePerfectDodgeWindowDuration.GetValueAtLevel(AbilityLevel);
	const float ConfiguredCap = MaxPerfectDodgeWindowDuration.GetValueAtLevel(AbilityLevel);

	// La ventana perfecta nunca puede ser más larga que los i-frames totales.
	const float IFrameCap = GetFinalIFrameDuration();
	const float SafeCap = FMath::Max(
		MinPerfectDodgeWindowDuration,
		FMath::Min(ConfiguredCap, IFrameCap));

	return FMath::Clamp(
		BaseDuration,
		MinPerfectDodgeWindowDuration,
		SafeCap);
}

float UPantheliaDodgeAbility::GetFinalDashDistance() const
{
	const float AbilityLevel = static_cast<float>(FMath::Max(1, GetAbilityLevel()));
	return FMath::Max(1.f, BaseDashDistance.GetValueAtLevel(AbilityLevel));
}

void UPantheliaDodgeAbility::OpenPerfectDodgeWindow()
{
	if (!IsActive() || !bIFramesStarted || bPerfectDodgeWindowStarted)
	{
		return;
	}

	UWorld* World = GetWorld();
	if (!World)
	{
		return;
	}

	const float ElapsedIFrameTime =
		FMath::Max(0.f, World->GetTimeSeconds() - IFramesStartTimeSeconds);
	const float RemainingIFrameTime =
		FMath::Max(0.f, GetFinalIFrameDuration() - ElapsedIFrameTime);
	const float WindowDuration = FMath::Min(
		GetFinalPerfectDodgeWindowDuration(),
		RemainingIFrameTime);

	if (WindowDuration <= KINDA_SMALL_NUMBER)
	{
		return;
	}

	UAbilityTask_WaitDelay* PendingWindowTask =
		UAbilityTask_WaitDelay::WaitDelay(this, WindowDuration);
	if (!PendingWindowTask)
	{
		return;
	}

	bPerfectDodgeWindowStarted = true;
	bPerfectDodgeWindowOpen = true;
	PerfectDodgeWindowTask = PendingWindowTask;
	PerfectDodgeWindowTask->OnFinish.AddDynamic(
		this, &UPantheliaDodgeAbility::OnPerfectDodgeWindowFinished);
	PerfectDodgeWindowTask->ReadyForActivation();

	UE_LOG(LogPanthelia, Verbose,
		TEXT("[DODGE] Ventana perfecta abierta durante %.3f s."),
		WindowDuration);
}

void UPantheliaDodgeAbility::OnPerfectDodgeWindowFinished()
{
	bPerfectDodgeWindowOpen = false;

	if (PerfectDodgeWindowTask)
	{
		PerfectDodgeWindowTask->OnFinish.RemoveAll(this);
		PerfectDodgeWindowTask = nullptr;
	}

	UE_LOG(LogPanthelia, Verbose, TEXT("[DODGE] Ventana perfecta cerrada."));
}

void UPantheliaDodgeAbility::OnHitAvoidedEventReceived(FGameplayEventData Payload)
{
	if (!IsActive() ||
		bPerfectDodgeTriggered ||
		!bPerfectDodgeWindowOpen ||
		!bIFramesStarted)
	{
		return;
	}

	ConfirmPerfectDodge(Payload);
}

void UPantheliaDodgeAbility::ConfirmPerfectDodge(const FGameplayEventData& RawPayload)
{
	AActor* Avatar = GetAvatarActorFromActorInfo();
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!Avatar || !ASC)
	{
		return;
	}

	// El ExecCalc envía el evento al defensor. Esta comprobación evita confirmar un
	// payload ajeno si otro sistema reutilizara por error el tag interno.
	if (RawPayload.Target && RawPayload.Target != Avatar)
	{
		return;
	}

	bPerfectDodgeTriggered = true;

	const FPantheliaGameplayTags& Tags = FPantheliaGameplayTags::Get();

	// Copiamos el payload crudo para conservar Instigator, ContextHandle, objetos
	// opcionales y tags capturados. Solo normalizamos el contrato público.
	FGameplayEventData PerfectPayload = RawPayload;
	PerfectPayload.EventTag = Tags.Event_Dodge_Perfect;
	PerfectPayload.Target = Avatar;
	PerfectPayload.EventMagnitude = 1.f;

	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
		Avatar,
		Tags.Event_Dodge_Perfect,
		PerfectPayload);

	// El Gameplay Event es el contrato de gameplay. El Cue solo presenta el resultado
	// mediante VFX/SFX/cámara y puede no tener asset todavía sin romper la simulación.
	FGameplayCueParameters CueParams;
	CueParams.Location = Avatar->GetActorLocation();

	// En UE 5.8, FGameplayEventData expone Instigator como const AActor*, mientras
	// que FGameplayCueParameters conserva TWeakObjectPtr<AActor>. El Cue no modifica
	// al actor: solo necesita una referencia no const para completar sus parámetros.
	AActor* CueInstigator = RawPayload.Instigator
		? const_cast<AActor*>(RawPayload.Instigator.Get())
		: Avatar;
	CueParams.Instigator = CueInstigator;
	CueParams.EffectCauser = CueInstigator;
	CueParams.EffectContext = RawPayload.ContextHandle;
	CueParams.RawMagnitude = 1.f;

	if (RawPayload.Instigator)
	{
		CueParams.Normal =
			(Avatar->GetActorLocation() - RawPayload.Instigator->GetActorLocation())
			.GetSafeNormal();
	}
	else
	{
		CueParams.Normal = Avatar->GetActorForwardVector();
	}

	ASC->ExecuteGameplayCue(Tags.GameplayCue_Dodge_Perfect, CueParams);

	UE_LOG(LogPanthelia, Log,
		TEXT("[DODGE] Perfect confirmado | Atacante: %s | Objetivo: %s"),
		RawPayload.Instigator ? *RawPayload.Instigator->GetName() : TEXT("None"),
		*Avatar->GetName());
}

bool UPantheliaDodgeAbility::BuildDodgeRequest(FPantheliaDodgeRequest& OutRequest) const
{
	const AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		return false;
	}

	OutRequest.Direction = EPantheliaDodgeDirection::Backward;
	OutRequest.WorldDirection = -AvatarActor->GetActorForwardVector();
	OutRequest.DesiredRotation = AvatarActor->GetActorRotation();
	OutRequest.bApplyRotation = false;
	OutRequest.Montage = DodgeBackward.Montage;
	OutRequest.AuthoredTravelDistance = DodgeBackward.AuthoredTravelDistance;

	return IsValid(OutRequest.Montage) &&
		OutRequest.AuthoredTravelDistance > KINDA_SMALL_NUMBER;
}

void UPantheliaDodgeAbility::OnDodgeMontageCompleted()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UPantheliaDodgeAbility::OnDodgeMontageInterruptedOrCancelled()
{
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

void UPantheliaDodgeAbility::ClearIFrames()
{
	if (IFramesEffectHandle.IsValid())
	{
		if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
		{
			ASC->RemoveActiveGameplayEffect(IFramesEffectHandle);
		}
	}

	IFramesEffectHandle = FActiveGameplayEffectHandle();
}

void UPantheliaDodgeAbility::ClearMontageTask()
{
	if (!CurrentMontageTask)
	{
		return;
	}

	CurrentMontageTask->OnCompleted.RemoveAll(this);
	CurrentMontageTask->OnInterrupted.RemoveAll(this);
	CurrentMontageTask->OnCancelled.RemoveAll(this);
	CurrentMontageTask->EndTask();
	CurrentMontageTask = nullptr;
}


void UPantheliaDodgeAbility::ClearHitAvoidedEventTask()
{
	if (!HitAvoidedEventTask)
	{
		return;
	}

	HitAvoidedEventTask->EventReceived.RemoveAll(this);
	HitAvoidedEventTask->EndTask();
	HitAvoidedEventTask = nullptr;
}

void UPantheliaDodgeAbility::ClearPerfectDodgeWindowTask()
{
	bPerfectDodgeWindowOpen = false;

	if (!PerfectDodgeWindowTask)
	{
		return;
	}

	PerfectDodgeWindowTask->OnFinish.RemoveAll(this);
	PerfectDodgeWindowTask->EndTask();
	PerfectDodgeWindowTask = nullptr;
}
