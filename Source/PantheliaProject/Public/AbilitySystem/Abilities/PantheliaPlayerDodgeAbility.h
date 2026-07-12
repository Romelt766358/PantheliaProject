// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PantheliaDodgeAbility.h"
#include "GameplayTagContainer.h"
#include "PantheliaPlayerDodgeAbility.generated.h"

class UPantheliaAbilitySystemComponent;

/**
 * EPantheliaDodgeBufferedAction
 *
 * Único input ofensivo que puede quedar almacenado durante una activación del dodge.
 * Es un enum en vez de dos bools para garantizar por estructura la regla
 * "el primer input válido gana" y evitar estados imposibles (Light + Heavy a la vez).
 */
UENUM()
enum class EPantheliaDodgeBufferedAction : uint8
{
	None,
	LightAttack,
	HeavyAttack
};

/**
 * UPantheliaPlayerDodgeAbility
 *
 * Implementación del request direccional para el jugador:
 *   - Sin lock-on: con input rota al vector de movimiento y usa el montage frontal;
 *     sin input usa el montage hacia atrás sin rotar.
 *   - Con lock-on: conserva el strafe y clasifica el input en ocho sectores de 45 grados
 *     usando el ángulo local respecto a los ejes del personaje.
 *
 * También contiene la parte específica del jugador del follow-up:
 *   - Acepta un único input Light/Heavy durante la ventana marcada en el montage.
 *   - Puede encadenar inmediatamente o esperar a OnCompleted según el toggle de la base.
 *   - Termina primero el dodge para retirar State.Dodge.Active y después activa el ataque.
 *   - Si el dash se interrumpe, EndAbility limpia el buffer y nunca regala el follow-up.
 *
 * La ejecución común (coste, root motion, i-frames y limpieza) permanece en la base.
 */
UCLASS()
class PANTHELIAPROJECT_API UPantheliaPlayerDodgeAbility : public UPantheliaDodgeAbility
{
	GENERATED_BODY()

public:
	// Llamado por el ASC una vez por pulsación real. Solo acepta Light/Heavy si la
	// ventana está abierta y no existe ya una acción bufferizada.
	bool TryBufferFollowupInput(const FGameplayTag& InputTag);

protected:
	virtual bool BuildDodgeRequest(FPantheliaDodgeRequest& OutRequest) const override;
	virtual void HandleDodgeMontageCompleted() override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

private:
	// Buffer máximo de una acción. Se limpia al activar, terminar o interrumpir el dodge.
	EPantheliaDodgeBufferedAction BufferedFollowup =
		EPantheliaDodgeBufferedAction::None;

	EPantheliaDodgeBufferedAction GetBufferedActionFromInputTag(
		const FGameplayTag& InputTag) const;

	FGameplayTag GetInputTagFromBufferedAction(
		EPantheliaDodgeBufferedAction BufferedAction) const;

	// Termina con el contexto DodgeFollowup en el ASC y activa el ataque por su InputTag.
	// Si GAS rechaza la activación, resetea el contexto inmediatamente.
	void ExecuteBufferedFollowup(
		EPantheliaDodgeBufferedAction BufferedAction,
		UPantheliaAbilitySystemComponent* PantheliaASC);
};
