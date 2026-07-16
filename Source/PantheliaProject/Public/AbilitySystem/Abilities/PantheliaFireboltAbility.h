// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PantheliaProjectileSpell.h"
#include "PantheliaFireboltAbility.generated.h"

/**
 * UPantheliaFireboltAbility
 *
 * Especialización del proyectil genérico para el Firebolt del jugador.
 * La clase base conserva SpawnProjectile() para enemigos y hechizos de un único
 * proyectil; esta clase añade exclusivamente el patrón de abanico del Firebolt.
 *
 * En la clase 316 solo visualizamos el patrón. El spawn múltiple real se añadirá
 * cuando se procese la clase 317, sin romper el Firebolt funcional actual.
 */
UCLASS()
class PANTHELIAPROJECT_API UPantheliaFireboltAbility : public UPantheliaProjectileSpell
{
	GENERATED_BODY()

public:
	/**
	 * Dibuja el vector frontal, los límites del arco y las direcciones calculadas.
	 *
	 * ProjectileCountOverride:
	 *   0 -> usa el nivel actual de la ability, limitado por MaxProjectileCount.
	 *   >0 -> usa ese número para probar visualmente 1, 2, 3, 5... proyectiles.
	 *
	 * Es una herramienta de desarrollo. No spawnea proyectiles ni cambia gameplay.
	 */
	UFUNCTION(BlueprintCallable, Category = "Firebolt|Debug")
	void DrawProjectileSpreadDebug(int32 ProjectileCountOverride = 0) const;

protected:
	// Arco total del abanico, centrado alrededor de la dirección hacia el objetivo.
	// Para una primera prueba del curso puede ponerse en 90°. En un soulslike conviene
	// empezar el balance real alrededor de 35°-45° para no golpear paredes laterales.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Firebolt|Projectile Pattern",
		meta = (ClampMin = "0.0", ClampMax = "360.0"))
	float ProjectileSpreadDegrees = 45.f;

	// Límite defensivo. Por ahora el número efectivo es min(nivel de ability, límite).
	// La fórmula queda encapsulada en GetCurrentProjectileCount() para poder consultar
	// tags o modificadores del árbol más adelante sin reescribir el spawn múltiple.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Firebolt|Projectile Pattern",
		meta = (ClampMin = "1", ClampMax = "32"))
	int32 MaxProjectileCount = 5;

	UFUNCTION(BlueprintPure, Category = "Firebolt|Projectile Pattern")
	int32 GetCurrentProjectileCount() const;

	UFUNCTION(BlueprintPure, Category = "Firebolt|Projectile Pattern")
	float GetCurrentProjectileSpreadDegrees() const;
};
