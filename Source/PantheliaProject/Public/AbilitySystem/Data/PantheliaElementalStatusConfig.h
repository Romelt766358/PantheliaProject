// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PantheliaElementTypes.h"
#include "PantheliaElementalStatusConfig.generated.h"

/**
 * Tipo de payload que ejecuta un estado elemental al llenarse su barra.
 *
 * El buildup solo decide CUÁNDO se dispara. Esta enumeración decide QUÉ hace
 * el estado, de modo que la lógica de cada elemento no dependa de la ability
 * concreta que dio el último golpe.
 */
UENUM(BlueprintType)
enum class EPantheliaElementalStatusPayload : uint8
{
	DamageOverTime UMETA(DisplayName = "Damage Over Time"),
	BurstDamage UMETA(DisplayName = "Burst Damage"),
	AttributeDebuff UMETA(DisplayName = "Attribute Debuff")
};

/**
 * Definición global de UN estado elemental.
 *
 * La ability solo aporta BuildupAmounts. Cuando la barra llega a 100, el
 * AttributeSet consulta esta definición central y calcula el payload usando
 * el Status Power del source. Por tanto, Firebolt, un arma o cualquier otra
 * fuente de Fuego producen la misma Quemadura base.
 */
USTRUCT(BlueprintType)
struct FPantheliaElementalStatusDefinition
{
	GENERATED_BODY()

	// Elemento al que pertenece esta definición. Debe existir una sola entrada
	// por elemento dentro del Data Asset.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Identity")
	EPantheliaElement Element = EPantheliaElement::None;

	// Clase de payload. DamageOverTime y BurstDamage están implementados.
	// AttributeDebuff queda preparado para Saturación.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Payload")
	EPantheliaElementalStatusPayload PayloadType = EPantheliaElementalStatusPayload::DamageOverTime;

	// Magnitud base del payload. Para DamageOverTime es el daño de CADA tick.
	// Para futuros payloads será la magnitud principal (daño de detonación o
	// reducción de atributo, según PayloadType).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Payload", meta = (ClampMin = "0.0"))
	float BaseMagnitude = 0.f;

	// Duración total del estado. En un BurstDamage puede permanecer en 0 porque
	// el payload ocurre una sola vez.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.0"))
	float BaseDuration = 0.f;

	// Frecuencia de ticks para DamageOverTime. No se usa en los otros payloads.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Timing", meta = (ClampMin = "0.01"))
	float TickFrequency = 1.f;

	// Cada punto de Status Power del elemento aporta este porcentaje a la
	// magnitud. Con 1.0: +25 Fire Status Power = +25% de daño de Quemadura.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scaling")
	float MagnitudePercentPerStatusPower = 1.f;

	// Coeficiente opcional para escalar la duración con Status Power.
	// Default 0: el poder aumenta la magnitud, no la duración.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scaling")
	float DurationPercentPerStatusPower = 0.f;

	// Todo payload ofensivo elemental es daño mágico y debe poder escalar con
	// Attributes.Secondary.MagicDamage. Este ratio se suma al daño plano ANTES
	// del multiplicador de Status Power: 0.25 = +0.25 de daño por cada punto de
	// MagicDamage. Se configura por estado en el Data Asset, nunca por ability.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scaling|Magic Damage", meta = (ClampMin = "0.0"))
	float FlatDamagePerMagicDamage = 0.f;

	// ===== DAÑO POR PORCENTAJE DE VIDA DESBLOQUEABLE =====
	// Estos coeficientes NO activan la mecánica por sí solos. El árbol o el
	// equipamiento deben aportar un porcentaje base mayor que 0 mediante los
	// atributos Attributes.StatusDamage.* del source. Solo entonces Status Power
	// añade puntos porcentuales extra usando estos coeficientes.
	//
	// Ejemplo: el perk da FireMaxHealthDamagePercent = 0.5 y este campo vale
	// 0.005. Con 100 FireStatusPower, cada tick de Quemadura suma 1.0% de la
	// vida máxima: 0.5 + (100 * 0.005).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health Percentage Damage", meta = (ClampMin = "0.0"))
	float MaxHealthPercentPerStatusPower = 0.f;

	// Usado principalmente por Electrocución. El árbol puede habilitar daño
	// basado en vida ACTUAL, vida FALTANTE, o ambos, concediendo los atributos
	// correspondientes. Cada canal se calcula y suma de forma independiente.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health Percentage Damage", meta = (ClampMin = "0.0"))
	float CurrentHealthPercentPerStatusPower = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health Percentage Damage", meta = (ClampMin = "0.0"))
	float MissingHealthPercentPerStatusPower = 0.f;

	// Escalado AP de los componentes porcentuales. Los valores son PUNTOS
	// PORCENTUALES añadidos por cada punto de MagicDamage. Ejemplo: 0.005 y
	// 100 MagicDamage añaden 0.5% a la rama ya desbloqueada por el árbol.
	// Igual que con Status Power, estos coeficientes no desbloquean la rama solos.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health Percentage Damage|Magic Damage", meta = (ClampMin = "0.0"))
	float MaxHealthPercentPerMagicDamage = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health Percentage Damage|Magic Damage", meta = (ClampMin = "0.0"))
	float CurrentHealthPercentPerMagicDamage = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health Percentage Damage|Magic Damage", meta = (ClampMin = "0.0"))
	float MissingHealthPercentPerMagicDamage = 0.f;

	// ===== DAÑO DE POSTURA AL DETONAR =====
	// Se aplica UNA sola vez cuando la barra llega a 100, nunca en cada tick del
	// DoT. El resultado entra por IncomingPoiseDamage para reutilizar flinch,
	// stagger y el reinicio del timer de regeneración de postura.
	//
	// Los tres campos existen en todas las definiciones: Electrocución y
	// Saturación los usan desde ahora, pero cualquier estado futuro puede hacerlo
	// sin añadir ramas hardcodeadas nuevas al AttributeSet.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Payload|Poise", meta = (ClampMin = "0.0"))
	float BasePoiseDamage = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Payload|Poise", meta = (ClampMin = "0.0"))
	float PoiseDamagePerStatusPower = 0.f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Payload|Poise", meta = (ClampMin = "0.0"))
	float PoiseDamagePerMagicDamage = 0.f;

	// ===== HERIDAS GRAVES =====
	// Veneno puede aplicar un segundo Gameplay Effect de duración que reduce la
	// curación recibida. La reducción real se procesa por IncomingHealing; futuras
	// curaciones deben usar ese meta atributo y no escribir Health directamente.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grievous Wounds")
	bool bAppliesGrievousWounds = false;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grievous Wounds", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	float GrievousWoundsPercent = 30.f;

	// Escalado opcional de la intensidad con Status Power. Default 0: el Veneno
	// siempre niega el porcentaje configurado, independientemente de su daño.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grievous Wounds", meta = (ClampMin = "0.0"))
	float GrievousWoundsPercentPerStatusPower = 0.f;
};

/**
 * Base de datos global de los cuatro estados elementales de Panthelia.
 *
 * Se crea un único Data Asset (DA_ElementalStatusConfig) y se asigna dentro de
 * DA_CharacterClassInfo. El árbol y el equipamiento NO modifican este asset en
 * runtime: aplican Gameplay Effects Infinite sobre los atributos Status Power
 * del source, y el cálculo consulta esos atributos al disparar el estado.
 */
UCLASS()
class PANTHELIAPROJECT_API UPantheliaElementalStatusConfig : public UDataAsset
{
	GENERATED_BODY()

public:

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Elemental Status")
	TArray<FPantheliaElementalStatusDefinition> StatusDefinitions;

	// Devuelve la entrada exacta del elemento o nullptr si el asset está mal
	// configurado. No devuelve una copia para evitar copiar el array/struct.
	const FPantheliaElementalStatusDefinition* FindStatusDefinition(
		EPantheliaElement Element,
		bool bLogNotFound = false) const;
};
