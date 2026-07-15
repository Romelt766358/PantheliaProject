// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "PantheliaCostAttributeSet.generated.h"

#define PANTHELIA_COST_ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

/**
 * UPantheliaCostAttributeSet
 *
 * AttributeSet pequeño y dedicado a los modificadores globales de costes.
 * Se mantiene separado de UPantheliaAttributeSet para no seguir ampliando el
 * AttributeSet monolítico de combate/vitales y para que el futuro árbol de
 * habilidades pueda modificar costes sin acoplarse al cálculo de daño.
 *
 * MODELO:
 *   CosteFinal = max(0, CosteBase × Multiplicador + ModificadorPlano)
 *
 * Valores neutros:
 *   Multiplicador = 1.0
 *   ModificadorPlano = 0.0
 *
 * Los Gameplay Effects del árbol, equipo, Corazones o buffs pueden acumular
 * modificaciones sobre estos atributos. Los planos pueden ser negativos; el
 * resolvedor común clampa el coste final a cero.
 *
 * Panthelia es single-player: este AttributeSet no añade replicación de red.
 */
UCLASS()
class PANTHELIAPROJECT_API UPantheliaCostAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:
	UPantheliaCostAttributeSet();

	virtual void PreAttributeChange(
		const FGameplayAttribute& Attribute,
		float& NewValue) override;

	// Multiplicador global de todas las acciones que gastan Stamina.
	UPROPERTY(BlueprintReadOnly, Category = "Cost Attributes")
	FGameplayAttributeData StaminaCostMultiplier;
	PANTHELIA_COST_ATTRIBUTE_ACCESSORS(
		UPantheliaCostAttributeSet,
		StaminaCostMultiplier)

	// Modificador plano global de los costes de Stamina. Puede ser negativo.
	UPROPERTY(BlueprintReadOnly, Category = "Cost Attributes")
	FGameplayAttributeData StaminaCostFlat;
	PANTHELIA_COST_ATTRIBUTE_ACCESSORS(
		UPantheliaCostAttributeSet,
		StaminaCostFlat)

	// Multiplicador global de todas las acciones que gastan Mana.
	UPROPERTY(BlueprintReadOnly, Category = "Cost Attributes")
	FGameplayAttributeData ManaCostMultiplier;
	PANTHELIA_COST_ATTRIBUTE_ACCESSORS(
		UPantheliaCostAttributeSet,
		ManaCostMultiplier)

	// Modificador plano global de los costes de Mana. Puede ser negativo.
	UPROPERTY(BlueprintReadOnly, Category = "Cost Attributes")
	FGameplayAttributeData ManaCostFlat;
	PANTHELIA_COST_ATTRIBUTE_ACCESSORS(
		UPantheliaCostAttributeSet,
		ManaCostFlat)
};

#undef PANTHELIA_COST_ATTRIBUTE_ACCESSORS
