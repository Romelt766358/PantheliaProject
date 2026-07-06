// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/ComboWindowNotifyState.h"
#include "AbilitySystem/Abilities/PantheliaPlayerAttackAbility.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

namespace
{
	// Helper: encuentra la ability de ataque del jugador que esta activa en el ASC
	// del actor dueno del mesh que reproduce el montage. Devuelve null si no la halla.
	UPantheliaPlayerAttackAbility* FindActiveAttackAbility(USkeletalMeshComponent* MeshComp)
	{
		if (!MeshComp) return nullptr;

		AActor* OwnerActor = MeshComp->GetOwner();
		if (!OwnerActor) return nullptr;

		UAbilitySystemComponent* ASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OwnerActor);
		if (!ASC) return nullptr;

		// Recorrer las abilities activas y quedarnos con la de ataque del jugador.
		for (const FGameplayAbilitySpec& Spec : ASC->GetActivatableAbilities())
		{
			if (UPantheliaPlayerAttackAbility* AttackAbility =
				Cast<UPantheliaPlayerAttackAbility>(Spec.GetPrimaryInstance()))
			{
				// Solo nos interesa si esta activa ahora mismo (reproduciendo el combo).
				if (AttackAbility->IsActive())
				{
					return AttackAbility;
				}
			}
		}
		return nullptr;
	}
}

void UComboWindowNotifyState::NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	float TotalDuration, const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	// Abrir la ventana de combo: a partir de aqui la ability acepta input buffered.
	if (UPantheliaPlayerAttackAbility* AttackAbility = FindActiveAttackAbility(MeshComp))
	{
		AttackAbility->OpenComboWindow();
	}
}

void UComboWindowNotifyState::NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	// Cerrar la ventana: la ability decide encadenar (si hay buffer) o terminar.
	if (UPantheliaPlayerAttackAbility* AttackAbility = FindActiveAttackAbility(MeshComp))
	{
		AttackAbility->CloseComboWindow();
	}
}