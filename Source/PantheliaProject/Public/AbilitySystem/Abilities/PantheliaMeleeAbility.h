// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PantheliaDamageGameplayAbility.h"
#include "PantheliaMeleeAbility.generated.h"

/**
 * UPantheliaMeleeAbility
 *
 * Clase base C++ para todas las abilities de ataque cuerpo a cuerpo de los enemigos.
 * Toda la lógica de animación y daño va en el Blueprint hijo (GA_MeleeAttack).
 *
 * Hereda de UPantheliaDamageGameplayAbility para tener acceso a:
 * - DamageTypes (mapa de tipo de daño → curva)
 * - DamageEffectClass (GE de daño con ExecCalc)
 * - CauseDamage() (función que aplica el daño al target)
 *
 * El Behavior Tree activa esta ability mediante el tag "Abilities.Attack".
 */
UCLASS()
class PANTHELIAPROJECT_API UPantheliaMeleeAbility : public UPantheliaDamageGameplayAbility
{
	GENERATED_BODY()
};