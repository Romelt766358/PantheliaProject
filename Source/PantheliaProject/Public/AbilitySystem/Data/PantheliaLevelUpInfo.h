// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PantheliaLevelUpInfo.generated.h"

// Información de un único nivel de la tabla de progresión.
USTRUCT(BlueprintType)
struct FPantheliaLevelUpEntry
{
	GENERATED_BODY()

	// XP INCREMENTAL necesaria para alcanzar este nivel desde el nivel anterior.
	// Es INCREMENTAL, no acumulada: si el nivel 2 requiere 500 y el nivel 3 requiere 548,
	// el jugador necesita 500 XP en total para llegar a nivel 2, y 500+548=1048 para nivel 3.
	// El código acumula estos valores internamente para calcular el nivel correcto.
	// La entrada del nivel 1 (índice 1) debe quedar en 0: el jugador empieza en nivel 1.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 LevelUpRequirement = 0;

	// Puntos de atributo otorgados al alcanzar este nivel.
	// Default 5 (modelo estilo Dragon Age: 5 puntos de atributo por nivel).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 AttributePointAward = 5;

	// Puntos de árbol de habilidades otorgados al alcanzar este nivel.
	// Default 1 (el gasto real, con nodos que cuestan 1-5, llega con el árbol).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	int32 SkillPointAward = 1;
};

// Data Asset con la tabla de progresión de niveles del jugador.
//
// CONVENCIÓN DE ÍNDICES (importante):
//   El índice del array ES el nivel. LevelUpInformation[1] = datos del nivel 1,
//   LevelUpInformation[2] = nivel 2, etc.
//   La entrada [0] es un PLACEHOLDER (no existe el "nivel 0"); déjala por defecto.
//   La LONGITUD del array define el NIVEL MÁXIMO del juego:
//   para un nivel máximo N, el array tiene N+1 entradas (índices 0..N).
//
// CONVENCIÓN DE XP (importante):
//   LevelUpRequirement es INCREMENTAL: cuánta XP hace falta para pasar del nivel
//   anterior a este. El código los acumula internamente para comparar con la XP total
//   del jugador. Ejemplo: si nivel 2 = 500 y nivel 3 = 548, el jugador necesita
//   1048 XP acumuladas para ser nivel 3 (500 + 548).
UCLASS()
class PANTHELIAPROJECT_API UPantheliaLevelUpInfo : public UDataAsset
{
	GENERATED_BODY()

public:

	// Tabla de niveles. El índice = nivel. Se rellena en el editor (DA_LevelUpInfo).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FPantheliaLevelUpEntry> LevelUpInformation;

	// Devuelve el nivel que corresponde a una cantidad de XP acumulada total.
	// Acumula los requisitos incrementales de cada nivel para determinar cuándo
	// se cruza el umbral. Si la XP supera todos los niveles definidos, devuelve
	// el nivel máximo (no se sale del array).
	int32 FindLevelForXP(int32 InXP) const;
};