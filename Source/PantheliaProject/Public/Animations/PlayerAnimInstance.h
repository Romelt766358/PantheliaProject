// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "PlayerAnimInstance.generated.h"

/**
 *
 */
UCLASS()
class PANTHELIAPROJECT_API UPlayerAnimInstance : public UAnimInstance
{
	GENERATED_BODY()

protected:

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float CurrentSpeed{ 0.0f };

	UFUNCTION(BlueprintCallable)
	void UpdateSpeed();

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsInCombat{ false };

	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	float CurrentDirection{ 0.0f };

	// True mientras el jugador mantiene la GUARDIA SOSTENIDA (bloqueo, no la ventana de
	// parry). El AnimGraph lo usa para activar un Layered Blend Per Bone que pone la pose
	// de guardia solo en el tren superior, dejando las piernas con la locomocion (caminar).
	// Lo actualiza UpdateGuardState() leyendo los tags State.Block.* del ASC del jugador.
	UPROPERTY(BlueprintReadWrite, EditAnywhere)
	bool bIsGuarding{ false };

public:

	UFUNCTION(BlueprintCallable)
	void HandleUpdatedTarget(AActor* NewTargetActorRef);

	UFUNCTION(BlueprintCallable)
	void UpdateDirection();

	// Actualiza bIsGuarding leyendo los tags State.Block.Physical/Magic del ASC del jugador.
	// Llamar desde el Event Blueprint Update Animation del AnimBlueprint, igual que UpdateSpeed.
	UFUNCTION(BlueprintCallable)
	void UpdateGuardState();
};