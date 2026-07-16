// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PantheliaMultiProjectileSpell.h"
#include "PantheliaFireboltAbility.generated.h"

/**
 * UPantheliaFireboltAbility
 *
 * Alias de migración conservado para que una referencia residual del parche de la
 * clase 316 no rompa el proyecto. GA_Firebolt ya volvió a UPantheliaProjectileSpell
 * y NO debe usar esta clase.
 *
 * Crear los hechizos múltiples nuevos directamente desde
 * UPantheliaMultiProjectileSpell. Cuando se confirme que ningún asset referencia
 * esta clase, estos dos archivos podrán eliminarse en una limpieza posterior.
 */
UCLASS(Abstract)
class PANTHELIAPROJECT_API UPantheliaFireboltAbility : public UPantheliaMultiProjectileSpell
{
	GENERATED_BODY()
};
