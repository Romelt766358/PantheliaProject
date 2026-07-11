// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PantheliaDodgeAbility.h"
#include "PantheliaPlayerDodgeAbility.generated.h"

/**
 * UPantheliaPlayerDodgeAbility
 *
 * Implementación del request direccional para el jugador:
 *   - Sin lock-on: con input rota al vector de movimiento y usa el montage frontal;
 *     sin input usa el montage hacia atrás sin rotar.
 *   - Con lock-on: conserva el strafe y clasifica el input en Forward/Backward/Left/Right
 *     usando dot products contra los ejes locales del personaje.
 *
 * La ejecución común (coste, root motion, i-frames y limpieza) permanece en la base.
 */
UCLASS()
class PANTHELIAPROJECT_API UPantheliaPlayerDodgeAbility : public UPantheliaDodgeAbility
{
	GENERATED_BODY()

protected:
	virtual bool BuildDodgeRequest(FPantheliaDodgeRequest& OutRequest) const override;
};
