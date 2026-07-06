// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/PantheliaPlayerState.h"
#include "AbilitySystem/PantheliaAttributeSet.h"
#include "AbilitySystem/PantheliaAbilitySystemComponent.h"
#include "AbilitySystem/Data/PantheliaLevelUpInfo.h"
#include "AbilitySystem/PantheliaSkillTreeComponent.h"

APantheliaPlayerState::APantheliaPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UPantheliaAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);

	AttributeSet = CreateDefaultSubobject<UPantheliaAttributeSet>("AttributeSet");

	// Componente del árbol de habilidades (Etapa 5). Vive junto al ASC por la misma
	// razón de persistencia (sobrevive al respawn del Pawn). Ver PantheliaSkillTreeComponent.h.
	SkillTreeComponent = CreateDefaultSubobject<UPantheliaSkillTreeComponent>("SkillTreeComponent");

	SetNetUpdateFrequency(100.f);

	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

UAbilitySystemComponent* APantheliaPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void APantheliaPlayerState::AddToXP(int32 InXP)
{
	XP += InXP;
	OnXPChangedDelegate.Broadcast(XP);

	UE_LOG(LogTemp, Log, TEXT("[Level] +%d XP. XP total acumulada: %d"), InXP, XP);

	// Tras sumar XP, comprobamos si se ha cruzado uno o más umbrales de nivel.
	UpdateLevelFromXP();
}

void APantheliaPlayerState::AddToLevel(int32 InLevel)
{
	Level += InLevel;
	OnLevelChangedDelegate.Broadcast(Level);
}

void APantheliaPlayerState::AddToAttributePoints(int32 InPoints)
{
	AttributePoints += InPoints;
	OnAttributePointsChangedDelegate.Broadcast(AttributePoints);
}

void APantheliaPlayerState::AddToSkillPoints(int32 InPoints)
{
	SkillPoints += InPoints;
	OnSkillPointsChangedDelegate.Broadcast(SkillPoints);
}

void APantheliaPlayerState::SetXP(int32 InXP)
{
	XP = InXP;
	OnXPChangedDelegate.Broadcast(XP);
}

void APantheliaPlayerState::SetLevel(int32 InLevel)
{
	Level = InLevel;
	OnLevelChangedDelegate.Broadcast(Level);
}

void APantheliaPlayerState::SetAttributePoints(int32 InPoints)
{
	AttributePoints = InPoints;
	OnAttributePointsChangedDelegate.Broadcast(AttributePoints);
}

void APantheliaPlayerState::SetSkillPoints(int32 InPoints)
{
	SkillPoints = InPoints;
	OnSkillPointsChangedDelegate.Broadcast(SkillPoints);
}

void APantheliaPlayerState::UpdateLevelFromXP()
{
	// Sin tabla de niveles no podemos calcular nada. Avisamos y salimos limpio.
	if (LevelUpInfo == nullptr)
	{
		UE_LOG(LogTemp, Error, TEXT("[Level] LevelUpInfo no asignado en BP_PantheliaPlayerState. No se puede subir de nivel."));
		return;
	}

	// Nivel que corresponde a la XP acumulada actual, según la tabla.
	const int32 TargetLevel = LevelUpInfo->FindLevelForXP(XP);

	// Subimos uno a uno hasta el nivel objetivo, otorgando los premios de cada
	// nivel cruzado. Hacerlo en bucle permite subir varios niveles de golpe
	// (p. ej. al matar un boss que da mucha XP) sin perder ningún premio.
	while (Level < TargetLevel)
	{
		const int32 NextLevel = Level + 1;

		// Guarda de seguridad: nunca leer fuera del array de la tabla.
		if (!LevelUpInfo->LevelUpInformation.IsValidIndex(NextLevel))
		{
			break;
		}

		const FPantheliaLevelUpEntry& Info = LevelUpInfo->LevelUpInformation[NextLevel];

		// Aplicamos la subida: nivel +1 y se acreditan los puntos del nivel.
		Level = NextLevel;
		AttributePoints += Info.AttributePointAward;
		SkillPoints += Info.SkillPointAward;

		UE_LOG(LogTemp, Log,
			TEXT("[Level] ¡Subiste a nivel %d! +%d puntos de atributo (total %d), +%d puntos de arbol (total %d)."),
			Level, Info.AttributePointAward, AttributePoints, Info.SkillPointAward, SkillPoints);

		// Avisamos a la UI de cada cambio (Fase 2 escuchará estos delegates).
		OnLevelChangedDelegate.Broadcast(Level);
		OnAttributePointsChangedDelegate.Broadcast(AttributePoints);
		OnSkillPointsChangedDelegate.Broadcast(SkillPoints);
	}
}

// ============================================================
// SISTEMA DE RENDIMIENTOS DECRECIENTES DE XP
// ============================================================

int32 APantheliaPlayerState::GetEnemyKillCount(FName EnemyID) const
{
	// Si el ID es vacío (bosses/minibosses sin tracking), devolvemos 0 siempre.
	// Eso hace que GetXPMultiplierForKillCount(0) devuelva 1.0 (100% XP).
	if (EnemyID.IsNone()) return 0;

	const int32* Count = EnemyKillCounts.Find(EnemyID);
	return Count ? *Count : 0;
}

void APantheliaPlayerState::RecordEnemyKill(FName EnemyID)
{
	// Ignoramos silenciosamente IDs vacíos — corresponden a bosses/minibosses
	// que no usan el sistema de rendimientos decrecientes.
	if (EnemyID.IsNone()) return;

	int32& Count = EnemyKillCounts.FindOrAdd(EnemyID);
	++Count;

	UE_LOG(LogTemp, Verbose,
		TEXT("[XP] RecordEnemyKill: '%s' → %d muertes totales."),
		*EnemyID.ToString(), Count);
}