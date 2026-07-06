// Fill out your copyright notice in the Description page of Project Settings.

#include "Animations/PlayerAnimInstance.h"
#include "KismetAnimationLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "PantheliaGameplayTags.h"

void UPlayerAnimInstance::UpdateSpeed()
{
	APawn* PawnRef{ TryGetPawnOwner() };

	if (!IsValid(PawnRef)) { return; }

	FVector Velocity{ PawnRef->GetVelocity() };

	CurrentSpeed = static_cast<float>(Velocity.Length());
}

void UPlayerAnimInstance::HandleUpdatedTarget(AActor* NewTargetActorRef)
{
	bIsInCombat = IsValid(NewTargetActorRef);
}

void UPlayerAnimInstance::UpdateDirection()
{
	APawn* PawnRef{ TryGetPawnOwner() };

	if (!IsValid(PawnRef)) { return; }

	if (!bIsInCombat) { return; }

	CurrentDirection = UKismetAnimationLibrary::CalculateDirection(
		PawnRef->GetVelocity(),
		PawnRef->GetActorRotation()
	);
}

void UPlayerAnimInstance::UpdateGuardState()
{
	APawn* PawnRef{ TryGetPawnOwner() };
	if (!IsValid(PawnRef)) { bIsGuarding = false; return; }

	// Obtener el ASC del jugador. GetAbilitySystemComponent acepta cualquier actor y
	// devuelve null si no tiene ASC, asi que es seguro.
	UAbilitySystemComponent* ASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(PawnRef);
	if (!IsValid(ASC)) { bIsGuarding = false; return; }

	// Guardia sostenida = cualquiera de los dos tags de bloqueo activo. La ventana de parry
	// (State.Parry.*) NO cuenta: solo queremos el blend upper-body durante el bloqueo
	// sostenido, no durante el flash de parry ni los retrocesos (esos son full body).
	const FPantheliaGameplayTags& Tags = FPantheliaGameplayTags::Get();
	bIsGuarding =
		ASC->HasMatchingGameplayTag(Tags.State_Block_Physical) ||
		ASC->HasMatchingGameplayTag(Tags.State_Block_Magic);
}