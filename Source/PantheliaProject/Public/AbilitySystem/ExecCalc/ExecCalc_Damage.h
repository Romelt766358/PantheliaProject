// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectExecutionCalculation.h"
#include "ExecCalc_Damage.generated.h"

/**
 * UExecCalc_Damage
 *
 * Execution Calculation para el cálculo de daño.
 * Es el sistema más poderoso de GAS para modificar atributos — puede capturar
 * atributos tanto del source como del target, leer SetByCaller magnitudes,
 * y modificar múltiples atributos en una sola ejecución.
 *
 * A diferencia de los MMCs, solo funciona con GameplayEffects Instant o Periodic.
 * No soporta predicción — se ejecuta solo en el servidor.
 *
 * Flujo de daño completo (se implementará progresivamente):
 *   1. Leer el daño base (SetByCaller desde GA_ProjectileSpell)
 *   2. Capturar atributos del source: PhysicalDamage, MagicDamage, CritChance, CritDamage, ArmorPen, MagicPen
 *   3. Capturar atributos del target: Armor, MagicResistance
 *   4. Aplicar fórmulas de mitigación, crits, penetración
 *   5. Escribir el resultado final a IncomingDamage del target
 */
UCLASS()
class PANTHELIAPROJECT_API UExecCalc_Damage : public UGameplayEffectExecutionCalculation
{
	GENERATED_BODY()

public:

	UExecCalc_Damage();

	// Aquí ocurre todo el cálculo de daño.
	// ExecutionParams contiene los ASCs del source y target, el EffectSpec, etc.
	// OutExecutionOutput es donde escribimos los cambios de atributos que queremos aplicar.
	virtual void Execute_Implementation(
		const FGameplayEffectCustomExecutionParameters& ExecutionParams,
		FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const override;
};