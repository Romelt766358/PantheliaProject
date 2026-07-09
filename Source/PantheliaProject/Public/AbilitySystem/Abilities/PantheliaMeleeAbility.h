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

public:
	// Devuelve el CombatTarget guardado en el enemigo que ejecuta esta ability.
	// Lo setea BossBrain/BTT_Attack justo antes de activar la ability.
	UFUNCTION(BlueprintPure, Category = "Panthelia|Enemy Melee|Targeting")
	AActor* GetCombatTargetFromAvatar() const;

	// Distancia 2D entre el avatar y su CombatTarget. Devuelve -1 si no hay target válido.
	UFUNCTION(BlueprintPure, Category = "Panthelia|Enemy Melee|Targeting")
	float GetDistanceToCombatTarget2D() const;

	// Helper simple para gap closers: impulsa al enemigo hacia su CombatTarget,
	// frenando virtualmente en StopDistance para no intentar ocupar el mismo espacio.
	//
	// Uso recomendado en una GA hija de GA_MeleeAttack:
	//   ActivateAbility → LaunchAvatarTowardCombatTarget → PlayMontageAndWait.
	//
	// Esto NO reemplaza la locomoción táctica futura del StateTree. Es solo un dash
	// controlado por la ability para ataques de cierre de distancia.
	// No debe ser const: Unreal puede exponer funciones const con retorno como nodos puros,
	// y este helper sí tiene efecto lateral real sobre el CharacterMovement.
	UFUNCTION(BlueprintCallable, Category = "Panthelia|Enemy Melee|Movement", meta = (ExpandBoolAsExecs = "ReturnValue"))
	bool LaunchAvatarTowardCombatTarget(
		float StopDistance = 180.f,
		float TravelTime = 0.35f,
		float MaxHorizontalSpeed = 1400.f,
		float VerticalSpeed = 0.f,
		bool bOverrideXY = true,
		bool bOverrideZ = false);
};
