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
#if WITH_EDITOR

#include "Validation/PantheliaDataValidationUtils.h"

EDataValidationResult UPantheliaLevelUpInfo::IsDataValid(FDataValidationContext& Context) const
{
	using namespace PantheliaDataValidation;

	EDataValidationResult Result = MakeInitialResult(Super::IsDataValid(Context));

	if (LevelUpInformation.Num() < 2)
	{
		AddError(Context, Result, FString::Printf(
			TEXT("'%s' necesita al menos las entradas [0] placeholder y [1] nivel inicial."),
			*GetNameSafe(this)));
		return Result;
	}

	if (LevelUpInformation[0].LevelUpRequirement != 0)
	{
		AddError(Context, Result,
			TEXT("LevelUpInformation[0] es un placeholder y debe tener LevelUpRequirement=0."));
	}

	if (LevelUpInformation[1].LevelUpRequirement != 0)
	{
		AddError(Context, Result,
			TEXT("LevelUpInformation[1] representa el nivel inicial y debe tener LevelUpRequirement=0."));
	}

	int64 AccumulatedXP = 0;
	for (int32 LevelIndex = 0; LevelIndex < LevelUpInformation.Num(); ++LevelIndex)
	{
		const FPantheliaLevelUpEntry& Entry = LevelUpInformation[LevelIndex];

		if (Entry.LevelUpRequirement < 0)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("Nivel %d: LevelUpRequirement no puede ser negativo (%d)."),
				LevelIndex,
				Entry.LevelUpRequirement));
		}
		else if (LevelIndex >= 2 && Entry.LevelUpRequirement == 0)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("Nivel %d: el requisito incremental debe ser mayor que 0."),
				LevelIndex));
		}

		if (Entry.AttributePointAward < 0)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("Nivel %d: AttributePointAward no puede ser negativo (%d)."),
				LevelIndex,
				Entry.AttributePointAward));
		}

		if (Entry.SkillPointAward < 0)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("Nivel %d: SkillPointAward no puede ser negativo (%d)."),
				LevelIndex,
				Entry.SkillPointAward));
		}

		if (LevelIndex >= 2 && Entry.LevelUpRequirement > 0)
		{
			AccumulatedXP += static_cast<int64>(Entry.LevelUpRequirement);
			if (AccumulatedXP > static_cast<int64>(TNumericLimits<int32>::Max()))
			{
				AddError(Context, Result, FString::Printf(
					TEXT("Nivel %d: la XP acumulada (%lld) excede int32_MAX y desbordaría FindLevelForXP."),
					LevelIndex,
					AccumulatedXP));
				break;
			}
		}
	}

	return Result;
}

#endif // WITH_EDITOR
