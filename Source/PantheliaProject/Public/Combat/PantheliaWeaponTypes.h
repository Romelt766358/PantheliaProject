// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PantheliaWeaponTypes.generated.h"

/**
 * EWeaponType
 *
 * Categoría de arma del jugador. Determina el set de animaciones de movimiento/idle
 * (cómo el personaje sostiene y se mueve con el arma) y sirve para lógica de gameplay
 * que dependa de la familia de arma (requisitos, infusiones, moveset base).
 *
 * El MOVESET de ataque concreto (qué montages de combo) NO lo define este enum, sino
 * el UWeaponDefinition de cada arma — dos espadas pueden compartir WeaponType pero
 * tener combos distintos. Este enum es la familia, no el arma individual.
 *
 * Empezamos solo con Sword. Se expande añadiendo entradas aquí conforme el juego
 * incorpore nuevas familias de arma (el orden no importa, son valores nombrados).
 */
UENUM(BlueprintType)
enum class EWeaponType : uint8
{
	Sword       UMETA(DisplayName = "Sword"),
	// Expansión futura: descomentar/añadir conforme se implementen movesets.
	// GreatSword  UMETA(DisplayName = "Great Sword"),
	// Axe         UMETA(DisplayName = "Axe"),
	// Spear       UMETA(DisplayName = "Spear"),
	// Dagger      UMETA(DisplayName = "Dagger"),
};
