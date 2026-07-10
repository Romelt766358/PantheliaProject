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

	// Clase de payload. DamageOverTime está implementado. BurstDamage y
	// AttributeDebuff quedan preparados para Electrocución y Saturación.
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
