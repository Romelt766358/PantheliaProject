// Fill out your copyright notice in the Description page of Project Settings.

#include "Animations/DodgeFollowupWindowNotifyState.h"

#include "AbilitySystem/Abilities/PantheliaDodgeAbility.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	// Nombre específico para que Unity Build no colisione con helpers equivalentes de
	// DodgeIFrameNotify o DodgePerfectWindowNotify al unir varios .cpp en un solo módulo.
	UPantheliaDodgeAbility* FindActiveDodgeAbilityForFollowupWindow(
		USkeletalMeshComponent* MeshComp)
	{
		if (!MeshComp) return nullptr;

		AActor* OwnerActor = MeshComp->GetOwner();
		if (!OwnerActor) return nullptr;

		UAbilitySystemComponent* ASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor);
		if (!ASC) return nullptr;

		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (UPantheliaDodgeAbility* DodgeAbility =
				Cast<UPantheliaDodgeAbility>(Spec.GetPrimaryInstance()))
			{
				if (DodgeAbility->IsActive())
				{
					return DodgeAbility;
				}
			}
		}

		return nullptr;
	}
}

void UDodgeFollowupWindowNotifyState::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (UPantheliaDodgeAbility* DodgeAbility =
		FindActiveDodgeAbilityForFollowupWindow(MeshComp))
	{
		DodgeAbility->OpenFollowupWindow();
	}
}

void UDodgeFollowupWindowNotifyState::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (UPantheliaDodgeAbility* DodgeAbility =
		FindActiveDodgeAbilityForFollowupWindow(MeshComp))
	{
		DodgeAbility->CloseFollowupWindow();
	}
}
