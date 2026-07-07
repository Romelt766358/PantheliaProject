// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "PantheliaElementTypes.h"
#include "PantheliaBossProfile.generated.h"

UENUM(BlueprintType)
enum class EPantheliaBossActionType : uint8
{
	Melee UMETA(DisplayName = "Melee"),
	Ranged UMETA(DisplayName = "Ranged"),
	GapCloser UMETA(DisplayName = "Gap Closer"),
	Reposition UMETA(DisplayName = "Reposition"),
	Punish UMETA(DisplayName = "Punish"),
	Special UMETA(DisplayName = "Special")
};

USTRUCT(BlueprintType)
struct FPantheliaBossStatsPreset
{
	GENERATED_BODY()

	// Para la demo usaremos normalmente un único preset: DemoDefault.
	// El array existe como gancho futuro para elegir stats por avance de historia,
	// NO por nivel del jugador.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Stats")
	FName PresetID = FName("DemoDefault");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Stats", meta = (ClampMin = "1.0"))
	float MaxHealth = 1000.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Stats", meta = (ClampMin = "0.0"))
	float Armor = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Stats", meta = (ClampMin = "0.0"))
	float MagicResistance = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Stats", meta = (ClampMin = "1.0"))
	float MaxPoise = 100.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Stats", meta = (ClampMin = "0.0"))
	float FireResistance = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Stats", meta = (ClampMin = "0.0"))
	float WaterResistance = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Stats", meta = (ClampMin = "0.0"))
	float StormResistance = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Stats", meta = (ClampMin = "0.0"))
	float NatureResistance = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Stats", meta = (ClampMin = "0.0"))
	float BaseWalkSpeed = 250.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Stats")
	EPantheliaElement DefensiveElement = EPantheliaElement::None;
};

USTRUCT(BlueprintType)
struct FPantheliaBossPhaseDefinition
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Phase")
	FName PhaseID = FName("Phase1");

	// La fase se considera activa cuando HealthPercent <= EnterHealthPercent.
	// Phase1 normalmente queda en 1.0; Phase2 puede quedar en 0.6 o 0.5.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Phase", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float EnterHealthPercent = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Phase", meta = (ClampMin = "0.0"))
	float WeightMultiplier = 1.f;

	// Acciones permitidas por esta fase. Si está vacío, el selector usa todas las
	// acciones que declaren esta fase en ValidPhases o que no declaren fases.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Phase")
	TArray<FGameplayTag> ExplicitActionPool;
};

USTRUCT(BlueprintType)
struct FPantheliaBossActionDefinition
{
	GENERATED_BODY()

	// Identidad interna de la acción. Ej: BossAction.Melee.ShortSlash.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Action")
	FGameplayTag ActionTag;

	// Ability real que se activa en GAS. Ej: Abilities.Attack.Melee.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Action")
	FGameplayTag AbilityTag;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Action")
	EPantheliaBossActionType ActionType = EPantheliaBossActionType::Melee;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Action")
	TArray<FName> ValidPhases;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Action", meta = (ClampMin = "0.0"))
	float MinDistance = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Action", meta = (ClampMin = "0.0"))
	float MaxDistance = 350.f;

	// Ángulo absoluto contra el forward del boss. 0 = justo delante, 180 = detrás.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Action", meta = (ClampMin = "0.0", ClampMax = "180.0"))
	float MaxAngle = 60.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Action", meta = (ClampMin = "0.0"))
	float Weight = 1.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Action", meta = (ClampMin = "0.0"))
	float Cooldown = 2.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Action")
	bool bRequiresLineOfSight = true;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Action")
	bool bCanExtendCombo = false;

	// El boss debe tener todos estos tags activos para poder usar la acción.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Action")
	FGameplayTagContainer RequiredOwnerTags;

	// Si el boss tiene alguno de estos tags, la acción queda bloqueada.
	// Útil para Effects.Stagger, Effects.HitReact, State.Recovering, etc.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Action")
	FGameplayTagContainer BlockedOwnerTags;
};

/**
 * Data Asset central de un boss.
 *
 * No escala por nivel del jugador. Para la demo, los stats son manuales y fijos.
 * El array de presets queda como gancho futuro para seleccionar variantes por
 * punto de historia, cuando exista esa capa de progresión.
 */
UCLASS(BlueprintType)
class PANTHELIAPROJECT_API UPantheliaBossProfile : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Identity")
	FName BossID = NAME_None;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Identity")
	FText BossDisplayName;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Stats")
	FName DefaultStatsPresetID = FName("DemoDefault");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Stats")
	TArray<FPantheliaBossStatsPreset> StatsPresets;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Phases")
	TArray<FPantheliaBossPhaseDefinition> Phases;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Boss|Actions")
	TArray<FPantheliaBossActionDefinition> Actions;

	const FPantheliaBossStatsPreset* FindStatsPreset(const FName PresetID) const;
	const FPantheliaBossPhaseDefinition* FindPhase(const FName PhaseID) const;
	const FPantheliaBossActionDefinition* FindAction(const FGameplayTag& ActionTag) const;
};
