// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Data/PantheliaLevelUpInfo.h"

int32 UPantheliaLevelUpInfo::FindLevelForXP(int32 InXP) const
{
	// Empezamos en nivel 1 (nivel mínimo siempre garantizado) y vamos acumulando
	// los requisitos incrementales de cada nivel para saber cuándo subiríamos.
	int32 Level = 1;

	// XPAccumulated es la XP TOTAL que el jugador necesita para alcanzar el
	// nivel que estamos evaluando en cada iteración del bucle.
	// Empieza en 0 porque el nivel 1 no requiere XP (el jugador empieza en él).
	int32 XPAccumulated = 0;

	while (true)
	{
		// ¿Existe un nivel por encima del actual en la tabla?
		// Si no, ya estamos en el nivel máximo — no podemos subir más.
		if (!LevelUpInformation.IsValidIndex(Level + 1))
		{
			return Level;
		}

		// Acumulamos el coste incremental del siguiente nivel.
		// Ejemplo: si nivel 2 cuesta 500 y nivel 3 cuesta 548:
		//   Iteración 1: XPAccumulated = 0 + 500 = 500  (umbral para nivel 2)
		//   Iteración 2: XPAccumulated = 500 + 548 = 1048 (umbral para nivel 3)
		XPAccumulated += LevelUpInformation[Level + 1].LevelUpRequirement;

		if (InXP >= XPAccumulated)
		{
			// La XP total del jugador supera el umbral de este nivel: sube.
			++Level;
		}
		else
		{
			// La XP no llega al siguiente umbral: el nivel actual es correcto.
			return Level;
		}
	}
}