// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/PantheliaCostAttributeSet.h"

UPantheliaCostAttributeSet::UPantheliaCostAttributeSet()
{
	// Valores neutros: sin cambios de coste hasta que un GE del build los modifique.
	InitStaminaCostMultiplier(1.f);
	InitStaminaCostFlat(0.f);
	InitManaCostMultiplier(1.f);
	InitManaCostFlat(0.f);
}

void UPantheliaCostAttributeSet::PreAttributeChange(
	const FGameplayAttribute& Attribute,
	float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	// Un multiplicador negativo invertiría el signo del coste. Los modificadores
	// planos sí pueden ser negativos porque representan reducciones planas.
	if (Attribute == GetStaminaCostMultiplierAttribute() ||
		Attribute == GetManaCostMultiplierAttribute())
	{
		NewValue = FMath::Max(0.f, NewValue);
	}
}
