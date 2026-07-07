// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "Enemy.generated.h"

// BlueprintType permite usar IEnemy como tipo de variable en Blueprint y
// llamar a sus funciones directamente desde actores sin necesidad de cast explícito.
UINTERFACE(MinimalAPI, BlueprintType)
class UEnemy : public UInterface
{
	GENERATED_BODY()
};

/**
 * Interfaz para actores controlados como enemigos.
 * Agrupa funciones de highlight (para el lockon visual) y de combate.
 */
class PANTHELIAPROJECT_API IEnemy
{
	GENERATED_BODY()

	// Add interface functions to this class. This is the class that will be inherited to implement this interface.
public:
	UFUNCTION(BlueprintImplementableEvent)
	void OnSelect();

	UFUNCTION(BlueprintImplementableEvent)
	void OnDeselect();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void HighlightActor();

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void UnHighlightActor();

	// El actor al que este enemigo está intentando atacar actualmente.
	// Se setea desde BTT_Attack justo antes de activar la ability de ataque,
	// para que la ability pueda leer el target y pasarlo al Motion Warping
	// (UpdateFacingTarget). Separado de TargetToFollow del Blackboard:
	// ese se actualiza cada 0.5s; este se setea puntualmente al atacar.

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void SetCombatTarget(AActor* InCombatTarget);

	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	AActor* GetCombatTarget() const;

	// Devuelve si este enemigo puede ser seleccionado por el lock-on en este momento.
	// Gancho para enemigos ocultos, invulnerables, en cinemática, muertos, etc.
	// Default: true. Los enemigos C++ pueden sobrescribirlo y los Blueprints también.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	bool IsLockonTargetable() const;

	// Punto lógico al que apunta el lock-on. Por ahora suele ser la ubicación del actor,
	// pero más adelante puede devolver un socket/componente dedicado (pecho/cabeza)
	// para bosses grandes o enemigos con siluetas raras.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	FVector GetLockonLocation() const;
};