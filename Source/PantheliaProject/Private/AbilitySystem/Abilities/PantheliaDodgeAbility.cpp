// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/PantheliaDodgeAbility.h"

#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystem/PantheliaAbilitySystemLibrary.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PantheliaGameplayTags.h"

UPantheliaDodgeAbility::UPantheliaDodgeAbility()
{
	// El dodge mantiene estado runtime propio (task + handle de i-frames), por lo que
	// necesita una instancia persistente por avatar y no puede ejecutarse sobre el CDO.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
	InputActivationPolicy = EPantheliaAbilityInputActivationPolicy::OnInputTriggered;

	BaseIFrameDuration = FScalableFloat(0.30f);
	MaxIFrameDuration = FScalableFloat(1.00f);
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

	// Orden deliberado: primero sabemos que el dash puede ejecutarse; después cobramos.
	// Si no hay estamina suficiente, CommitAbility falla y no se reproduce nada.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Estado limpio para esta activación. EndAbility también lo limpia como red de
	// seguridad, pero se reinicia aquí antes de que el montage pueda disparar notifies.
	bIFramesStarted = false;
	IFramesEffectHandle = FActiveGameplayEffectHandle();

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
	// Limpiar primero el GE exacto de esta activación. Nunca removemos por tag porque
	// eso podría borrar i-frames legítimos concedidos por otra fuente.
	ClearIFrames();
	ClearMontageTask();

	bIFramesStarted = false;

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

	bIFramesStarted = true;

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		return;
	}

	IFramesEffectHandle = UPantheliaAbilitySystemLibrary::GrantTemporaryGameplayTag(
		ASC,
		FPantheliaGameplayTags::Get().State_Invulnerable_Dodge,
		GetFinalIFrameDuration());
}

float UPantheliaDodgeAbility::GetFinalIFrameDuration() const
{
	const float AbilityLevel = static_cast<float>(FMath::Max(1, GetAbilityLevel()));
	const float BaseDuration = BaseIFrameDuration.GetValueAtLevel(AbilityLevel);
	const float ConfiguredCap = MaxIFrameDuration.GetValueAtLevel(AbilityLevel);
	const float SafeCap = FMath::Max(MinIFrameDuration, ConfiguredCap);

	return FMath::Clamp(BaseDuration, MinIFrameDuration, SafeCap);
}

float UPantheliaDodgeAbility::GetFinalDashDistance() const
{
	const float AbilityLevel = static_cast<float>(FMath::Max(1, GetAbilityLevel()));
	return FMath::Max(1.f, BaseDashDistance.GetValueAtLevel(AbilityLevel));
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
