// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "PantheliaProjectilePatternLibrary.generated.h"

/**
 * UPantheliaProjectilePatternLibrary
 *
 * Utilidades matemáticas puras para patrones de proyectiles.
 * No spawnea actores, no conoce GAS y no conoce ningún elemento: solo transforma datos.
 *
 * Separar la geometría del patrón de la ability permite reutilizarla en hechizos,
 * ataques de bosses, trampas y patrones radiales sin duplicar el algoritmo.
 */
UCLASS()
class PANTHELIAPROJECT_API UPantheliaProjectilePatternLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/**
	 * Genera direcciones normalizadas distribuidas uniformemente dentro de un arco.
	 *
	 * Ejemplos con Forward=(1,0,0), Axis=Up y Spread=90:
	 *   NumDirections=1 -> [0°]
	 *   NumDirections=2 -> [-45°, +45°]
	 *   NumDirections=3 -> [-45°, 0°, +45°]
	 *
	 * Para arcos menores de 360°, los extremos están incluidos y por eso el delta es
	 * Spread/(NumDirections-1). El caso NumDirections=1 se trata aparte para impedir
	 * división entre cero. Con 360° se usa Spread/NumDirections para no duplicar la
	 * primera dirección en el último elemento.
	 */
	UFUNCTION(BlueprintPure, Category = "Panthelia|Projectile Patterns")
	static TArray<FVector> MakeEvenlySpacedDirections(
		const FVector& ForwardDirection,
		const FVector& RotationAxis,
		float SpreadAngleDegrees,
		int32 NumDirections);
};
