// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/PantheliaPlayerState.h"
#include "AbilitySystem/PantheliaAttributeSet.h"
#include "AbilitySystem/PantheliaCostAttributeSet.h"
#include "AbilitySystem/PantheliaAbilitySystemComponent.h"
#include "AbilitySystem/Data/PantheliaLevelUpInfo.h"
#include "AbilitySystem/PantheliaSkillTreeComponent.h"
#include "PantheliaLogChannels.h"

APantheliaPlayerState::APantheliaPlayerState()
{
	AbilitySystemComponent = CreateDefaultSubobject<UPantheliaAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);

	AttributeSet = CreateDefaultSubobject<UPantheliaAttributeSet>("AttributeSet");

	// Dominio separado de costes: permite que árbol/equipo/buffs modifiquen Stamina
	// y Mana sin seguir ampliando el AttributeSet monolítico de combate.
	CostAttributeSet = CreateDefaultSubobject<UPantheliaCostAttributeSet>("CostAttributeSet");

	// Componente del árbol de habilidades (Etapa 5). Vive junto al ASC por la misma
	// razón de persistencia (sobrevive al respawn del Pawn). Ver PantheliaSkillTreeComponent.h.
	SkillTreeComponent = CreateDefaultSubobject<UPantheliaSkillTreeComponent>("SkillTreeComponent");

	SetNetUpdateFrequency(100.f);

	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);
}

void APantheliaPlayerState::PostInitializeComponents()
{
	Super::PostInitializeComponents();

	// El saldo inicial se resuelve desde una propiedad de configuración. Un futuro
	// SaveGame puede restaurar otro valor después de esta inicialización sin que el
	// PlayerState vuelva a sobrescribirlo durante el respawn del Pawn.
	Level = FMath::Max(1, Level);
	XP = FMath::Max(0, XP);
	AttributePoints = FMath::Max(0, AttributePoints);
	SkillPoints = FMath::Max(0, InitialSkillPoints);
}

UAbilitySystemComponent* APantheliaPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void APantheliaPlayerState::AddToXP(int32 InXP)
{
	GrantXP(InXP);
}

bool APantheliaPlayerState::GrantXP(int32 InXP)
{
	if (InXP <= 0)
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("[Level] GrantXP rechazó una cantidad no positiva: %d."),
			InXP);
		return false;
	}

	const int64 NewXP64 = static_cast<int64>(XP) + static_cast<int64>(InXP);
	XP = static_cast<int32>(FMath::Clamp<int64>(NewXP64, 0LL, static_cast<int64>(MAX_int32)));
	OnXPChangedDelegate.Broadcast(XP);

	UE_LOG(LogPanthelia, Log, TEXT("[Level] +%d XP. XP total acumulada: %d"), InXP, XP);

	// Tras sumar XP, comprobamos si se ha cruzado uno o más umbrales de nivel.
	UpdateLevelFromXP();
	return true;
}

void APantheliaPlayerState::GrantAttributePoints(int32 InPoints)
{
	if (InPoints <= 0)
	{
		return;
	}

	const int64 NewValue = static_cast<int64>(AttributePoints) + static_cast<int64>(InPoints);
	AttributePoints = static_cast<int32>(FMath::Clamp<int64>(NewValue, 0LL, static_cast<int64>(MAX_int32)));
	OnAttributePointsChangedDelegate.Broadcast(AttributePoints);
}

void APantheliaPlayerState::GrantSkillPoints(int32 InPoints)
{
	if (InPoints <= 0)
	{
		return;
	}

	const int64 NewValue = static_cast<int64>(SkillPoints) + static_cast<int64>(InPoints);
	SkillPoints = static_cast<int32>(FMath::Clamp<int64>(NewValue, 0LL, static_cast<int64>(MAX_int32)));
	OnSkillPointsChangedDelegate.Broadcast(SkillPoints);
}

bool APantheliaPlayerState::TrySpendAttributePoints(int32 InPoints)
{
	if (InPoints < 0 || AttributePoints < InPoints)
	{
		return false;
	}

	if (InPoints == 0)
	{
		return true;
	}

	AttributePoints -= InPoints;
	OnAttributePointsChangedDelegate.Broadcast(AttributePoints);
	return true;
}

bool APantheliaPlayerState::TrySpendSkillPoints(int32 InPoints)
{
	if (InPoints < 0 || SkillPoints < InPoints)
	{
		return false;
	}

	if (InPoints == 0)
	{
		return true;
	}

	SkillPoints -= InPoints;
	OnSkillPointsChangedDelegate.Broadcast(SkillPoints);
	return true;
}

void APantheliaPlayerState::AddToLevel(int32 InLevel)
{
	const int64 RequestedLevel = static_cast<int64>(Level) + static_cast<int64>(InLevel);
	SetLevel(static_cast<int32>(FMath::Clamp<int64>(RequestedLevel, 1LL, static_cast<int64>(MAX_int32))));
}

void APantheliaPlayerState::AddToAttributePoints(int32 InPoints)
{
	if (InPoints >= 0)
	{
		GrantAttributePoints(InPoints);
		return;
	}

	const int64 Spend64 = -static_cast<int64>(InPoints);
	TrySpendAttributePoints(static_cast<int32>(FMath::Min<int64>(Spend64, static_cast<int64>(MAX_int32))));
}

void APantheliaPlayerState::AddToSkillPoints(int32 InPoints)
{
	if (InPoints >= 0)
	{
		GrantSkillPoints(InPoints);
		return;
	}

	const int64 Spend64 = -static_cast<int64>(InPoints);
	TrySpendSkillPoints(static_cast<int32>(FMath::Min<int64>(Spend64, static_cast<int64>(MAX_int32))));
}

void APantheliaPlayerState::SetXP(int32 InXP)
{
	XP = FMath::Max(0, InXP);
	OnXPChangedDelegate.Broadcast(XP);

	// SetXP es una ruta de restauración, no una concesión de gameplay: reconcilia el
	// nivel con la XP, pero no vuelve a acreditar premios. Los saldos se restauran
	// mediante sus propios setters dentro de la misma transacción de carga.
	if (LevelUpInfo)
	{
		const int32 ReconciledLevel = FMath::Max(1, LevelUpInfo->FindLevelForXP(XP));
		if (Level != ReconciledLevel)
		{
			Level = ReconciledLevel;
			OnLevelChangedDelegate.Broadcast(Level);
		}
	}
}

void APantheliaPlayerState::SetLevel(int32 InLevel)
{
	int32 MaxLevel = MAX_int32;
	if (LevelUpInfo && LevelUpInfo->LevelUpInformation.Num() > 1)
	{
		MaxLevel = LevelUpInfo->LevelUpInformation.Num() - 1;
	}

	const int32 NewLevel = FMath::Clamp(InLevel, 1, MaxLevel);
	if (Level == NewLevel)
	{
		return;
	}

	Level = NewLevel;
	OnLevelChangedDelegate.Broadcast(Level);
}

void APantheliaPlayerState::SetAttributePoints(int32 InPoints)
{
	const int32 NewValue = FMath::Max(0, InPoints);
	if (AttributePoints == NewValue)
	{
		return;
	}

	AttributePoints = NewValue;
	OnAttributePointsChangedDelegate.Broadcast(AttributePoints);
}

void APantheliaPlayerState::SetSkillPoints(int32 InPoints)
{
	const int32 NewValue = FMath::Max(0, InPoints);
	if (SkillPoints == NewValue)
	{
		return;
	}

	SkillPoints = NewValue;
	OnSkillPointsChangedDelegate.Broadcast(SkillPoints);
}

void APantheliaPlayerState::UpdateLevelFromXP()
{
	// Sin tabla de niveles no podemos calcular nada. Avisamos y salimos limpio.
	if (LevelUpInfo == nullptr)
	{
		UE_LOG(LogPanthelia, Error, TEXT("[Level] LevelUpInfo no asignado en BP_PantheliaPlayerState. No se puede subir de nivel."));
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
		const int32 SafeAttributeAward = FMath::Max(0, Info.AttributePointAward);
		const int32 SafeSkillAward = FMath::Max(0, Info.SkillPointAward);

		if (Info.AttributePointAward < 0 || Info.SkillPointAward < 0)
		{
			UE_LOG(LogPanthelia, Error,
				TEXT("[Level] Premio negativo rechazado en %s, nivel %d. Attribute=%d Skill=%d."),
				*GetNameSafe(LevelUpInfo), NextLevel,
				Info.AttributePointAward, Info.SkillPointAward);
		}

		// Aplicamos la subida: nivel +1 y se acreditan únicamente premios válidos.
		Level = NextLevel;
		GrantAttributePoints(SafeAttributeAward);
		GrantSkillPoints(SafeSkillAward);

		UE_LOG(LogPanthelia, Log,
			TEXT("[Level] ¡Subiste a nivel %d! +%d puntos de atributo (total %d), +%d puntos de arbol (total %d)."),
			Level, SafeAttributeAward, AttributePoints, SafeSkillAward, SkillPoints);

		OnLevelChangedDelegate.Broadcast(Level);
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
