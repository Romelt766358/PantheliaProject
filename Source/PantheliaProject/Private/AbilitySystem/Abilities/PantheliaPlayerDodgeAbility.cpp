// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/PantheliaPlayerDodgeAbility.h"

#include "AbilitySystem/PantheliaAbilitySystemComponent.h"
#include "Combat/LockonComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "PantheliaGameplayTags.h"
#include "PantheliaLogChannels.h"
#include "Engine/World.h"
#include "TimerManager.h"

bool UPantheliaPlayerDodgeAbility::TryBufferFollowupInput(const FGameplayTag& InputTag)
{
	if (!IsActive() ||
		!IsFollowupWindowOpen() ||
		BufferedFollowup != EPantheliaDodgeBufferedAction::None)
	{
		return false;
	}

	const EPantheliaDodgeBufferedAction RequestedAction =
		GetBufferedActionFromInputTag(InputTag);
	if (RequestedAction == EPantheliaDodgeBufferedAction::None)
	{
		return false;
	}

	// Primer input válido gana. Desde este punto, otras pulsaciones no pueden sustituirlo.
	BufferedFollowup = RequestedAction;

	UE_LOG(LogPanthelia, Log,
		TEXT("[DODGE] Input de follow-up aceptado: %s | Modo inmediato: %s"),
		RequestedAction == EPantheliaDodgeBufferedAction::HeavyAttack
			? TEXT("Heavy")
			: TEXT("Light"),
		bChainImmediatelyOnFollowupInput ? TEXT("Sí") : TEXT("No"));

	if (!bChainImmediatelyOnFollowupInput)
	{
		// Modo comprometido: conservar el input hasta que el montage llegue a OnCompleted.
		return true;
	}

	// Modo inmediato: copiamos la acción a una variable local porque EndAbility limpia
	// siempre el buffer. El dodge se termina a sí mismo, retira State.Dodge.Active y solo
	// después intenta activar el ataque; el ataque nunca cancela externamente al dodge.
	const EPantheliaDodgeBufferedAction FollowupToExecute = BufferedFollowup;
	UPantheliaAbilitySystemComponent* PantheliaASC =
		Cast<UPantheliaAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	BufferedFollowup = EPantheliaDodgeBufferedAction::None;
	CloseFollowupWindow();

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	ExecuteBufferedFollowup(FollowupToExecute, PantheliaASC);
	return true;
}

bool UPantheliaPlayerDodgeAbility::BuildDodgeRequest(FPantheliaDodgeRequest& OutRequest) const
{
	const ACharacter* Character = Cast<ACharacter>(GetAvatarActorFromActorInfo());
	const UCharacterMovementComponent* MovementComponent =
		Character ? Character->GetCharacterMovement() : nullptr;

	if (!Character || !MovementComponent)
	{
		return false;
	}

	FVector InputDirection = MovementComponent->GetLastInputVector();
	InputDirection.Z = 0.f;
	const bool bHasMovementInput = InputDirection.Normalize(0.1f);

	const FVector ActorForward = Character->GetActorForwardVector().GetSafeNormal2D();
	const FVector ActorRight = Character->GetActorRightVector().GetSafeNormal2D();

	const ULockonComponent* LockonComponent =
		Character->FindComponentByClass<ULockonComponent>();
	const bool bHasLockon =
		LockonComponent && IsValid(LockonComponent->CurrentTargetActor);

	const FPantheliaDodgeMontageData* SelectedMontageData = nullptr;

	if (!bHasLockon)
	{
		if (bHasMovementInput)
		{
			// Movimiento libre: el personaje gira hacia el input y el montage frontal
			// produce el dash en esa dirección sin necesitar cuatro animaciones.
			OutRequest.Direction = EPantheliaDodgeDirection::Forward;
			OutRequest.WorldDirection = InputDirection;
			OutRequest.DesiredRotation = InputDirection.Rotation();
			OutRequest.bApplyRotation = true;
			SelectedMontageData = &DodgeForward;
		}
		else
		{
			// Sin input: retroceso respecto a la orientación actual.
			OutRequest.Direction = EPantheliaDodgeDirection::Backward;
			OutRequest.WorldDirection = -ActorForward;
			OutRequest.DesiredRotation = Character->GetActorRotation();
			OutRequest.bApplyRotation = false;
			SelectedMontageData = &DodgeBackward;
		}
	}
	else
	{
		// Con lock-on no rotamos al input: se conserva la orientación hacia el objetivo
		// y se elige uno de ocho montages en sectores locales de 45 grados.
		OutRequest.bApplyRotation = false;
		OutRequest.DesiredRotation = Character->GetActorRotation();

		if (!bHasMovementInput)
		{
			OutRequest.Direction = EPantheliaDodgeDirection::Backward;
			OutRequest.WorldDirection = -ActorForward;
			SelectedMontageData = &DodgeBackward;
		}
		else
		{
			const float ForwardDot = FVector::DotProduct(InputDirection, ActorForward);
			const float RightDot = FVector::DotProduct(InputDirection, ActorRight);
			const float DirectionAngleDegrees = FMath::RadiansToDegrees(
				FMath::Atan2(RightDot, ForwardDot));

			const FVector ForwardRightDirection = (ActorForward + ActorRight).GetSafeNormal();
			const FVector BackwardRightDirection = (-ActorForward + ActorRight).GetSafeNormal();
			const FVector BackwardLeftDirection = (-ActorForward - ActorRight).GetSafeNormal();
			const FVector ForwardLeftDirection = (ActorForward - ActorRight).GetSafeNormal();

			if (DirectionAngleDegrees >= -22.5f && DirectionAngleDegrees < 22.5f)
			{
				OutRequest.Direction = EPantheliaDodgeDirection::Forward;
				OutRequest.WorldDirection = ActorForward;
				SelectedMontageData = &DodgeForward;
			}
			else if (DirectionAngleDegrees >= 22.5f && DirectionAngleDegrees < 67.5f)
			{
				OutRequest.Direction = EPantheliaDodgeDirection::ForwardRight;
				OutRequest.WorldDirection = ForwardRightDirection;
				SelectedMontageData = &DodgeForwardRight;
			}
			else if (DirectionAngleDegrees >= 67.5f && DirectionAngleDegrees < 112.5f)
			{
				OutRequest.Direction = EPantheliaDodgeDirection::Right;
				OutRequest.WorldDirection = ActorRight;
				SelectedMontageData = &DodgeRight;
			}
			else if (DirectionAngleDegrees >= 112.5f && DirectionAngleDegrees < 157.5f)
			{
				OutRequest.Direction = EPantheliaDodgeDirection::BackwardRight;
				OutRequest.WorldDirection = BackwardRightDirection;
				SelectedMontageData = &DodgeBackwardRight;
			}
			else if (DirectionAngleDegrees >= 157.5f || DirectionAngleDegrees < -157.5f)
			{
				OutRequest.Direction = EPantheliaDodgeDirection::Backward;
				OutRequest.WorldDirection = -ActorForward;
				SelectedMontageData = &DodgeBackward;
			}
			else if (DirectionAngleDegrees >= -157.5f && DirectionAngleDegrees < -112.5f)
			{
				OutRequest.Direction = EPantheliaDodgeDirection::BackwardLeft;
				OutRequest.WorldDirection = BackwardLeftDirection;
				SelectedMontageData = &DodgeBackwardLeft;
			}
			else if (DirectionAngleDegrees >= -112.5f && DirectionAngleDegrees < -67.5f)
			{
				OutRequest.Direction = EPantheliaDodgeDirection::Left;
				OutRequest.WorldDirection = -ActorRight;
				SelectedMontageData = &DodgeLeft;
			}
			else
			{
				OutRequest.Direction = EPantheliaDodgeDirection::ForwardLeft;
				OutRequest.WorldDirection = ForwardLeftDirection;
				SelectedMontageData = &DodgeForwardLeft;
			}
		}
	}

	if (!SelectedMontageData ||
		!IsValid(SelectedMontageData->Montage) ||
		SelectedMontageData->AuthoredTravelDistance <= KINDA_SMALL_NUMBER)
	{
		return false;
	}

	OutRequest.Montage = SelectedMontageData->Montage;
	OutRequest.AuthoredTravelDistance = SelectedMontageData->AuthoredTravelDistance;
	return true;
}

void UPantheliaPlayerDodgeAbility::HandleDodgeMontageCompleted()
{
	if (BufferedFollowup == EPantheliaDodgeBufferedAction::None)
	{
		Super::HandleDodgeMontageCompleted();
		return;
	}

	// Modo no inmediato: el montage completó su desplazamiento/recovery. Conservamos la
	// acción localmente, terminamos la ability para retirar el tag bloqueante y activamos.
	const EPantheliaDodgeBufferedAction FollowupToExecute = BufferedFollowup;
	UPantheliaAbilitySystemComponent* PantheliaASC =
		Cast<UPantheliaAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	BufferedFollowup = EPantheliaDodgeBufferedAction::None;

	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	ExecuteBufferedFollowup(FollowupToExecute, PantheliaASC);
}

void UPantheliaPlayerDodgeAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Toda salida por interrupción, muerte o cancelación descarta el input. Los únicos
	// caminos que ejecutan un follow-up copian la acción a una variable local antes.
	BufferedFollowup = EPantheliaDodgeBufferedAction::None;
	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
}

EPantheliaDodgeBufferedAction UPantheliaPlayerDodgeAbility::GetBufferedActionFromInputTag(
	const FGameplayTag& InputTag) const
{
	const FPantheliaGameplayTags& Tags = FPantheliaGameplayTags::Get();

	if (InputTag.MatchesTagExact(Tags.InputTag_LightAttack))
	{
		return EPantheliaDodgeBufferedAction::LightAttack;
	}

	if (InputTag.MatchesTagExact(Tags.InputTag_HeavyAttack))
	{
		return EPantheliaDodgeBufferedAction::HeavyAttack;
	}

	return EPantheliaDodgeBufferedAction::None;
}

FGameplayTag UPantheliaPlayerDodgeAbility::GetInputTagFromBufferedAction(
	EPantheliaDodgeBufferedAction BufferedAction) const
{
	const FPantheliaGameplayTags& Tags = FPantheliaGameplayTags::Get();

	switch (BufferedAction)
	{
	case EPantheliaDodgeBufferedAction::LightAttack:
		return Tags.InputTag_LightAttack;

	case EPantheliaDodgeBufferedAction::HeavyAttack:
		return Tags.InputTag_HeavyAttack;

	default:
		return FGameplayTag();
	}
}

void UPantheliaPlayerDodgeAbility::ExecuteBufferedFollowup(
	EPantheliaDodgeBufferedAction BufferedAction,
	UPantheliaAbilitySystemComponent* PantheliaASC)
{
	const FGameplayTag BufferedInputTag =
		GetInputTagFromBufferedAction(BufferedAction);

	if (!PantheliaASC || !BufferedInputTag.IsValid())
	{
		return;
	}

	PantheliaASC->SetPendingAttackEntryContext(
		EPantheliaAttackEntryContext::DodgeFollowup);

	UWorld* World = PantheliaASC->GetWorld();
	if (!World)
	{
		PantheliaASC->ResetPendingAttackEntryContext();
		return;
	}

	// El dodge acaba de terminar y retirar sus ActivationOwnedTags. Activar el ataque
	// en el mismo call stack puede competir con la limpieza interna de GAS, especialmente
	// con State.Dodge.Active como ActivationBlockedTag de los ataques. Diferimos un tick
	// para que la terminación del dodge quede completamente asentada antes de revalidar
	// costes, tags y requisitos del follow-up.
	const TWeakObjectPtr<UPantheliaAbilitySystemComponent> WeakASC(PantheliaASC);
	World->GetTimerManager().SetTimerForNextTick(
		[WeakASC, BufferedInputTag]()
		{
			UPantheliaAbilitySystemComponent* ResolvedASC = WeakASC.Get();
			if (!ResolvedASC)
			{
				return;
			}

			const bool bActivationAccepted =
				ResolvedASC->TryActivateAbilityByInputTag(BufferedInputTag);
			const bool bContextWasConsumed =
				ResolvedASC->GetPendingAttackEntryContext() ==
				EPantheliaAttackEntryContext::Normal;

			if (!bActivationAccepted || !bContextWasConsumed)
			{
				// TryActivateAbility puede devolver un falso positivo si la activación falla
				// más tarde. La prueba definitiva es que la ability de ataque haya consumido
				// el contexto al entrar en ActivateAbility. Si sigue pendiente, lo descartamos.
				ResolvedASC->ResetPendingAttackEntryContext();

				UE_LOG(LogPanthelia, Warning,
					TEXT("[DODGE] Follow-up rechazado para InputTag '%s' | TryActivate=%s | Contexto consumido=%s"),
					*BufferedInputTag.ToString(),
					bActivationAccepted ? TEXT("Sí") : TEXT("No"),
					bContextWasConsumed ? TEXT("Sí") : TEXT("No"));
				return;
			}

			UE_LOG(LogPanthelia, Log,
				TEXT("[DODGE] Follow-up activado correctamente para InputTag '%s'."),
				*BufferedInputTag.ToString());
		});
}

