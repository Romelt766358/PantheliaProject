// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/PantheliaPlayerDodgeAbility.h"

#include "Combat/LockonComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"

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
		// y se elige un montage cardinal en el espacio local del personaje.
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

			if (FMath::Abs(ForwardDot) >= FMath::Abs(RightDot))
			{
				if (ForwardDot >= 0.f)
				{
					OutRequest.Direction = EPantheliaDodgeDirection::Forward;
					OutRequest.WorldDirection = ActorForward;
					SelectedMontageData = &DodgeForward;
				}
				else
				{
					OutRequest.Direction = EPantheliaDodgeDirection::Backward;
					OutRequest.WorldDirection = -ActorForward;
					SelectedMontageData = &DodgeBackward;
				}
			}
			else if (RightDot >= 0.f)
			{
				OutRequest.Direction = EPantheliaDodgeDirection::Right;
				OutRequest.WorldDirection = ActorRight;
				SelectedMontageData = &DodgeRight;
			}
			else
			{
				OutRequest.Direction = EPantheliaDodgeDirection::Left;
				OutRequest.WorldDirection = -ActorRight;
				SelectedMontageData = &DodgeLeft;
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
