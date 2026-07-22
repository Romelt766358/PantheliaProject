// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "PantheliaElementTypes.h"
#include "PantheliaAreaImpactTypes.generated.h"

class AActor;

/**
 * EPantheliaAreaImpactDamagePolicy
 *
 * Define qué payloads de daño aplica un proyectil de impacto en área.
 * ExplosionOnly es el comportamiento base recomendado para Fireburst: el actor
 * directamente golpeado participa en la explosión, pero no recibe un segundo daño
 * directo implícito.
 */
UENUM(BlueprintType)
enum class EPantheliaAreaImpactDamagePolicy : uint8
{
	ExplosionOnly UMETA(DisplayName = "Explosion Only"),
	DirectOnly UMETA(DisplayName = "Direct Only"),
	DirectAndExplosion UMETA(DisplayName = "Direct + Explosion")
};

/**
 * EPantheliaProjectileAimPointMode
 *
 * Punto lógico que usa una ability de proyectil cuando existe un target explícito.
 * LockonLocation conserva el contrato actual de cámara/proyectiles. GroundUnderTarget
 * resuelve un punto estable bajo la cápsula/bounds y lo proyecta sobre el escenario.
 */
UENUM(BlueprintType)
enum class EPantheliaProjectileAimPointMode : uint8
{
	LockonLocation UMETA(DisplayName = "Logical Lock-on Location"),
	GroundUnderTarget UMETA(DisplayName = "Ground Under Target")
};

/**
 * FPantheliaAreaImpactRuntimeConfig
 *
 * Snapshot plano y ya evaluado que la ability entrega al proyectil antes de
 * FinishSpawning. El actor no consulta el nivel, el corazón ni el árbol después del
 * cast: esos sistemas resuelven modificadores antes y producen esta configuración.
 */
USTRUCT(BlueprintType)
struct FPantheliaAreaImpactRuntimeConfig
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area Impact", meta = (ClampMin = "0.0"))
	float ExplosionRadius = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area Impact", meta = (ClampMin = "1", ClampMax = "128"))
	int32 MaxAffectedTargets = 32;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area Impact")
	EPantheliaAreaImpactDamagePolicy DamagePolicy = EPantheliaAreaImpactDamagePolicy::ExplosionOnly;

	// En DirectAndExplosion, true permite que el actor golpeado reciba ambos payloads.
	// ExplosionOnly siempre lo incluye en la consulta radial si es un target válido.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area Impact")
	bool bDirectTargetReceivesExplosion = true;

	// Si está activo, los candidatos radiales deben tener línea de visión desde el
	// punto de detonación. El actor golpeado directamente se considera visible porque
	// el proyectil ya alcanzó físicamente su superficie.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area Impact|Line of Sight")
	bool bExplosionRequiresLineOfSight = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area Impact|Line of Sight")
	TEnumAsByte<ECollisionChannel> ExplosionLineOfSightTraceChannel = ECC_Visibility;

	// Identidad elemental explícita. El proyectil no interpreta el corazón ni aplica
	// recompensas; solo publica este dato en FPantheliaAreaImpactResult.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area Impact")
	EPantheliaElement Element = EPantheliaElement::None;

	// Identidad de la ability fuente para telemetría, listeners de perks y futuros
	// sistemas de corazones. Puede quedar inválido durante prototipos.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Area Impact")
	FGameplayTag SourceAbilityTag;
};

/**
 * FPantheliaAreaImpactResult
 *
 * Resultado inmutable de una detonación. No concede curación ni otras recompensas:
 * un sistema persistente externo podrá escuchar este resultado y resolver sinergias
 * como "elemento de la explosión == elemento del corazón activo".
 */
USTRUCT(BlueprintType)
struct FPantheliaAreaImpactResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Area Impact")
	FVector ImpactLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Area Impact")
	TObjectPtr<AActor> DirectHitActor = nullptr;

	// Número de candidatos válidos seleccionados después de ordenar y aplicar
	// MaxAffectedTargets. No representa el total crudo devuelto por la consulta física.
	UPROPERTY(BlueprintReadOnly, Category = "Area Impact")
	int32 SelectedTargetCount = 0;

	// Número de targets donde el payload de explosión produjo un efecto real.
	// Accepted y MitigatedBlock cuentan; i-frames y perfect parry no cuentan.
	// No incluye el payload directo adicional de DirectAndExplosion. El término
	// Affected evita confundir este contador con EPantheliaHitOutcome::Accepted.
	UPROPERTY(BlueprintReadOnly, Category = "Area Impact")
	int32 AffectedTargetCount = 0;

	UPROPERTY(BlueprintReadOnly, Category = "Area Impact")
	bool bHadDirectActorImpact = false;

	// True cuando el payload directo produjo Accepted o MitigatedBlock. El nombre
	// Affected es deliberado: Accepted queda reservado al outcome estricto de GAS.
	UPROPERTY(BlueprintReadOnly, Category = "Area Impact")
	bool bDirectImpactAffected = false;

	UPROPERTY(BlueprintReadOnly, Category = "Area Impact")
	bool bWasWorldImpact = false;

	UPROPERTY(BlueprintReadOnly, Category = "Area Impact")
	EPantheliaElement Element = EPantheliaElement::None;

	UPROPERTY(BlueprintReadOnly, Category = "Area Impact")
	FGameplayTag SourceAbilityTag;
};
