// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PantheliaPlayerInterface.generated.h"

UINTERFACE(MinimalAPI, BlueprintType)
class UPantheliaPlayerInterface : public UInterface
{
	GENERATED_BODY()
};

/**
 * IPantheliaPlayerInterface
 *
 * Interfaz exclusiva del personaje del jugador (AMainCharacter).
 * Proporciona un contrato desacoplado para que sistemas como el AttributeSet
 * puedan comunicarse con el jugador sin depender directamente de
 * APantheliaPlayerState, evitando dependencias circulares y acoplamiento fuerte.
 *
 * PATRÓN DE USO EN C++:
 *   // Enviar XP al jugador sin saber nada del PlayerState:
 *   if (Props.SourceCharacter->Implements<UPantheliaPlayerInterface>())
 *   {
 *       IPantheliaPlayerInterface::Execute_AddToXP(Props.SourceCharacter, FinalXP);
 *   }
 *
 * FUTURAS FUNCIONES (añadir aquí cuando el sistema correspondiente exista):
 *   - LevelUp()                             — animación/efectos de subida de nivel
 */
class PANTHELIAPROJECT_API IPantheliaPlayerInterface
{
	GENERATED_BODY()

public:
	// Suma InXP a la XP acumulada del jugador y comprueba si sube de nivel.
	// La implementación en AMainCharacter obtiene el PlayerState y llama AddToXP.
	// BlueprintNativeEvent: en C++ llamar vía Execute_AddToXP(Actor, InXP).
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void AddToXP(int32 InXP);

	// Notifica al jugador que acaba de subir de nivel.
	// La implementación base está vacía; se sobreescribe en Blueprint (BP_ThirdPersonCharacter)
	// para reproducir efectos visuales y de sonido de subida de nivel (al estilo Black Myth Wukong:
	// breve pausa, destello, texto de nivel). La lógica de XP/atributos ya ocurrió antes.
	// BlueprintNativeEvent: en C++ llamar vía Execute_LevelUp(Actor).
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void LevelUp();

	// ======== GETTERS ========

	// Devuelve la XP total acumulada del jugador.
	// Usado por el AttributeSet para calcular cuántos niveles va a subir ANTES de sumar la XP.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	int32 GetXP() const;

	// Dado un valor de XP, devuelve el nivel que le correspondería al jugador.
	// Delega a UPantheliaLevelUpInfo::FindLevelForXP() vía PlayerState.
	// Usado para calcular numLevelUps = FindLevelForXP(currentXP + incomingXP) - currentLevel.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	int32 FindLevelForXP(int32 InXP) const;

	// Devuelve los atributos primarios que se otorgan al subir AL nivel indicado.
	// Lee LevelUpInformation[Level].AttributePointAward del DA_LevelUpInfo.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	int32 GetAttributePointsReward(int32 Level) const;

	// Devuelve los puntos de árbol que se otorgan al subir AL nivel indicado.
	// Lee LevelUpInformation[Level].SkillPointAward del DA_LevelUpInfo.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	int32 GetSkillPointsReward(int32 Level) const;

	// Devuelve los puntos de atributo DISPONIBLES actualmente (el saldo, no el premio
	// de un nivel concreto). Usado por el ASC para comprobar si hay saldo antes de
	// gastar un punto al mejorar un atributo primario (ver UpgradeAttribute).
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	int32 GetAttributePoints() const;

	// Devuelve los puntos de árbol de habilidades DISPONIBLES actualmente (el saldo).
	// Análogo a GetAttributePoints, para cuando el árbol de habilidades consuma este saldo.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	int32 GetSkillPoints() const;

	// ======== SETTERS / ADDERS (para uso futuro; el AttributeSet los define pero ========
	// ======== no los llama directamente porque AddToXP → UpdateLevelFromXP ya lo hace) ====

	// Incrementa el nivel del jugador en InLevel unidades.
	// NOTA: en Panthelia AddToXP → UpdateLevelFromXP ya maneja la subida de nivel.
	// Esta función existe para operaciones directas (ej: consola de dev, reembolso de nivel).
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void AddToPlayerLevel(int32 InLevel);

	// Incrementa los puntos de atributo disponibles. Se usa con valores negativos para
	// GASTAR un punto (ver UPantheliaAbilitySystemComponent::UpgradeAttribute).
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void AddToAttributePoints(int32 InAttributePoints);

	// Incrementa los puntos de árbol de habilidades disponibles.
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
	void AddToSkillPoints(int32 InSkillPoints);
};