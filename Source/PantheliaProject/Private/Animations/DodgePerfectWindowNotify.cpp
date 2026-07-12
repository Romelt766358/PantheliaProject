// Fill out your copyright notice in the Description page of Project Settings.

#include "Animations/DodgePerfectWindowNotify.h"

#include "AbilitySystem/Abilities/PantheliaDodgeAbility.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	// Localiza la instancia activa de la ability de dodge perteneciente al actor que
	// reproduce el montage. Mantiene el mismo patrón que DodgeIFrameNotify y
	// ComboWindowNotifyState para que el asset no necesite referencias directas.
	UPantheliaDodgeAbility* FindActiveDodgeAbilityForPerfectWindow(USkeletalMeshComponent* MeshComp)
	{
		if (!MeshComp)
		{
			return nullptr;
		}

		AActor* OwnerActor = MeshComp->GetOwner();
		if (!OwnerActor)
		{
			return nullptr;
		}

		UAbilitySystemComponent* ASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor);
		if (!ASC)
		{
			return nullptr;
		}

		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			UPantheliaDodgeAbility* DodgeAbility =
				Cast<UPantheliaDodgeAbility>(Spec.GetPrimaryInstance());

			if (DodgeAbility && DodgeAbility->IsActive())
			{
				return DodgeAbility;
			}
		}

		return nullptr;
	}
}

void UDodgePerfectWindowNotify::Notify(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::Notify(MeshComp, Animation, EventReference);

	if (UPantheliaDodgeAbility* DodgeAbility = FindActiveDodgeAbilityForPerfectWindow(MeshComp))
	{
		DodgeAbility->StartPerfectDodgeWindow();
	}
}
