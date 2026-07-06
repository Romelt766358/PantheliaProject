// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "PantheliaElementTypes.generated.h"

/**
 * EPantheliaElement
 *
 * Los 4 elementos del juego. Usado para:
 *   - El tipo defensivo de personajes/enemigos (GetDefensiveElement())
 *   - La tabla de afinidades de daño elemental en ExecCalc_Damage
 *   - El corazón elemental equipado por el jugador (sistema elemental, pendiente)
 *
 * Cada elemento agrupa un tipo de daño físico y uno mágico:
 *   Fuego:     solo Damage.Magical.Fire (sin componente físico)
 *   Agua:      Damage.Physical.Ice + Damage.Magical.Water
 *   Tormenta:  Damage.Physical.Air + Damage.Magical.Lightning
 *   Naturaleza:Damage.Physical.Earth + Damage.Magical.Poison
 *
 * None = sin elemento (daño físico genérico, neutro en tabla de afinidades).
 */
UENUM(BlueprintType)
enum class EPantheliaElement : uint8
{
    None        UMETA(DisplayName = "None (Neutral)"),
    Fire        UMETA(DisplayName = "Fire"),
    Water       UMETA(DisplayName = "Water"),
    Storm       UMETA(DisplayName = "Storm"),
    Nature      UMETA(DisplayName = "Nature")
};
