// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "PantheliaGameplayAbility.generated.h"

/**
 * FAbilityAttributeScaling
 *
 * Define un escalado opcional de daño basado en un atributo secundario o vital del caster.
 * Modelo LoL: cada habilidad puede escalar con 1-2 atributos del propio personaje.
 *
 * Ejemplo: Firebolt con +50% MagicDamage
 *   AttributeTag = Attributes.Secondary.MagicDamage
 *   Ratio        = 0.5
 *
 * REGLAS (diseño, no técnicas):
 *   - Máximo 2 entradas por habilidad.
 *   - Solo atributos SECUNDARIOS y VITALES del caster (no primarios).
 *   - Los vitales (Health, Mana, Stamina) habilitan habilidades cuyo daño depende
 *     del estado actual del personaje.
 *   - Los ratios son fijos (no escalan por nivel — la curva de daño base ya lo hace).
 *
 * ASIMETRÍA PhysicalDamage vs MagicDamage (modelo LoL, spec §1.7):
 *   - PhysicalDamage (AD): también se suma como addend genérico en ExecCalc_Damage.
 *     Si se usa aquí como ratio, se aplica dos veces para esa habilidad (intencional).
 *   - MagicDamage (AP): NO se suma en el ExecCalc — solo existe como ratio aquí.
 */
USTRUCT(BlueprintType)
struct FAbilityAttributeScaling
{
	GENERATED_BODY()

	// Tag del atributo del caster que escala el daño.
	// Ej: Attributes.Secondary.MagicDamage, Attributes.Vital.Health
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scaling")
	FGameplayTag AttributeTag;

	// Fracción del atributo que se añade como daño extra.
	// 0.5 = 50% del atributo. 1.0 = 100%.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scaling", meta = (ClampMin = "0.0"))
	float Ratio = 0.f;
};

/**
 * UPantheliaGameplayAbility
 *
 * Clase base de todas las abilities del proyecto.
 * Solo contiene lo común a TODAS las abilities: el tag de input.
 *
 * Las abilities que infligen daño heredan de UPantheliaDamageGameplayAbility,
 * que añade DamageTypes (TMap de tipo→curva), PoiseDamage, AttributeScalings y DamageEffectClass.
 */
UCLASS()
class PANTHELIAPROJECT_API UPantheliaGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:

	// Input tag con el que esta ability empieza el juego asociada.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	FGameplayTag StartupInputTag;
};
