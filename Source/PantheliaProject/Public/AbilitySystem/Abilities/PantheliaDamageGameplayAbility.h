// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PantheliaGameplayAbility.h"
#include "GameplayTagContainer.h"
#include "ScalableFloat.h"
#include "Interfaces/CombatInterface.h"
#include "AbilitySystem/PantheliaAbilityTypes.h"
#include "PantheliaDamageGameplayAbility.generated.h"

/**
 * UPantheliaDamageGameplayAbility
 *
 * Clase base para todas las abilities que infligen daño.
 *
 * Jerarquía:
 *   UGameplayAbility
 *   └── UPantheliaGameplayAbility (StartupInputTag)
 *       └── UPantheliaDamageGameplayAbility (DamageTypes, PoiseDamage, AttributeScalings,
 *           DamageEffectClass, BuildupAmounts, DeathImpulseMagnitude,
 *           KnockbackChance/ForceMagnitude, LaunchChance/ForceMagnitude/PitchOverride)
 *           └── UPantheliaProjectileSpell
 *
 * ESCALADO POR ATRIBUTOS (spec §1.7):
 *   Daño bruto = DañoBase + Σ(Ratio × Atributo).
 *   ApplyDamageScalingToSpec centraliza el cálculo para proyectiles, melee y armas;
 *   el ExecCalc recibe el daño YA escalado y solo aplica defensa/afinidades/crítico.
 *   Máximo 2 entradas en AttributeScalings (regla de diseño).
 */
UCLASS()
class PANTHELIAPROJECT_API UPantheliaDamageGameplayAbility : public UPantheliaGameplayAbility
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	TSubclassOf<UGameplayEffect> GetDamageEffectClassForEditor() const
	{
		return DamageEffectClass;
	}
#endif

	// Mapa de tipo de daño → curva de daño base por nivel.
	// Nota: En la spec se llama "Damages" — aquí "DamageTypes" por compatibilidad
	// con los Blueprints ya configurados (GA_Firebolt, etc.).
	// Añadir una entrada por cada tipo de daño que este ability aplica.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	TMap<FGameplayTag, FScalableFloat> DamageTypes;

	// Política de esta ability frente a los i-frames de evasión. Las damage abilities
	// representan por defecto ataques leíbles: se anulan durante un dodge y pueden
	// confirmar un esquive perfecto. Fuentes ambientales/secundarias usan el default
	// conservador AvoidableNoReward de FDamageEffectParams/context.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage|Dodge")
	EPantheliaDodgeResponse DodgeResponse = EPantheliaDodgeResponse::Dodgeable;

	// Regla defensiva del ataque. Normal es el comportamiento base; Heavy multiplica
	// el coste de guardia; Fury/Ultimate no puede bloquearse y atraviesa los i-frames
	// normales salvo que el target tenga Capability.Dodge.Fury.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage|Defense")
	EPantheliaDefenseAttackType DefenseAttackType = EPantheliaDefenseAttackType::Normal;

	// Escalados por atributos secundarios/vitales del caster (spec §1.7).
	// Máximo 2 entradas. Ver FAbilityAttributeScaling para detalles y restricciones.
	// Ej: { Attributes.Secondary.MagicDamage, 0.5 } → +50% MagicDamage al daño total.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage",
		meta = (TitleProperty = "AttributeTag"))
	TArray<FAbilityAttributeScaling> AttributeScalings;

	// Daño a la postura del target (independiente del daño a vida).
	// Armas/habilidades pesadas tienen valor alto. 0 = no aplica daño de postura.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	FScalableFloat PoiseDamage;

	// Los parámetros del estado elemental (daño, duración y frecuencia) NO viven
	// en la ability. Se definen una sola vez en DA_ElementalStatusConfig y se
	// escalan con los atributos Status Power del source. Así, cualquier ataque
	// que llene la misma barra dispara el mismo estado base.

	// --- BUILDUP ELEMENTAL (sistema de umbral — el disparador real de los estados) ---
	//
	// CUÁNTO llena ESTA ability la barra de estado de la víctima, POR TIPO DE DAÑO
	// (misma clave que DamageTypes — coherencia total con el sistema multi-tipo).
	// La ability lo colapsa a nivel de elemento vía DamageTypeToElement y lo
	// transporta como SetByCaller (ver ApplyDamageScalingToSpec, PASO 11); el
	// ExecCalc aplica resistencia/crítico/parry y lo deposita en la barra del target.
	//
	// DECISIÓN DE DISEÑO (cerrada): daño y buildup son INDEPENDIENTES a propósito —
	// aquí vive la identidad de cada ataque: un tajo pesado de fuego puede hacer
	// mucho daño y poco buildup, y una llovizna de chispas lo contrario (poco daño,
	// mucho buildup). Escala 0-100: la barra se llena en 100, así que un valor de 25
	// significa "4 golpes limpios llenan la barra" (antes de resistencias).
	//
	// SIN ENTRADA para un tipo de daño = 0 buildup de ese tipo (daño "puro" que no
	// acumula estado). FScalableFloat: escalable por nivel de ability vía curva,
	// como todo parámetro de combate del proyecto.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage|Buildup")
	TMap<FGameplayTag, FScalableFloat> BuildupAmounts;

	// --- HERIDAS GRAVES INMEDIATAS ---
	// Cualquier golpe o tick de esta ability que cause daño aplica el antiheal sin
	// barra de buildup. 0 desactiva la mecánica. La duración runtime nunca baja de
	// 4 segundos y además recibe los bonuses del árbol/equipamiento del source.
	// Parry, armaduras reactivas u otros sistemas que no hereden de esta ability
	// pueden usar UPantheliaAbilitySystemLibrary::ApplyGrievousWounds directamente.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage|Grievous Wounds", meta = (ClampMin = "0.0", ClampMax = "100.0"))
	FScalableFloat GrievousWoundsPercent = FScalableFloat(0.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage|Grievous Wounds", meta = (ClampMin = "0.0"))
	FScalableFloat GrievousWoundsDuration = FScalableFloat(4.f);

	// --- IMPULSO DE MUERTE (clase 312) ---
	// Magnitud del impulso físico que se aplica al ragdoll cuando ESTA ability da el
	// golpe que mata. Da más "peso" a la muerte (el cuerpo sale despedido/rebota en vez
	// de simplemente desplomarse) — puramente cosmético, no afecta ninguna mecánica.
	//
	// FScalableFloat en vez de float simple: sin curva asignada se comporta
	// igual que un float normal (solo rellenas "Value"), pero queda listo para que el
	// árbol de habilidades lo escale por nivel el día que se quiera, sin tocar C++.
	//
	// Este escalar viaja por SetByCaller dentro del spec. El proyectil o el
	// WeaponTrace, que sí conocen la dirección real del impacto, lo convierten en
	// el vector final de impulso y lo guardan en el Effect Context antes del daño.

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	FScalableFloat DeathImpulseMagnitude = FScalableFloat(60.f);

	// --- KNOCKBACK (clase 315) ---
	// Mismo mecanismo que el impulso de muerte, pero para golpes NO fatales: en vez de
	// que el cuerpo se desplome con un empujón físico (ragdoll), el personaje sigue
	// controlable pero es "lanzado" por el aire un instante (LaunchCharacter, no
	// AddImpulse — ver la nota completa en HandleIncomingDamage sobre por qué).
	//
	// KnockbackChance: probabilidad (0-100) de que ESTE golpe concreto dispare el
	// knockback. (Nota: knockback/launch SÍ conservan su dado — son reacciones físicas
	// puntuales, no efectos de estado; el buildup solo jubiló el azar de los estados
	// elementales.) Por defecto 0 — a diferencia del viejo DebuffChance
	// (que algunas abilities querrán con probabilidad por defecto), el knockback es algo
	// que se "activa a propósito" por ability, no algo que casi todas las abilities
	// deberían tener un poco. La transcripción original lo señala explícitamente: la
	// mayoría de tus enemigos deberían tener 0% (sin knockback en absoluto), y solo
	// algunos ataques/enemigos especiales lo activan subiendo este valor por encima de 0.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	FScalableFloat KnockbackChance = FScalableFloat(0.f);

	// Magnitud de la fuerza de knockback. 500 como valor de partida (la transcripción
	// original probó 1000 y lo bajó a 500 tras ver que se veía demasiado violento) —
	// ajusta jugando, como con cualquier otro número de sensación de combate.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	FScalableFloat KnockbackForceMagnitude = FScalableFloat(500.f);

	// Nivel 2 de knockback ("empujón fuerte", a petición). false (por defecto): el
	// knockback normal de arriba, que convive con GA_HitReact. true: este knockback
	// bloquea GA_HitReact y dispara GA_HeavyKnockback en su lugar — pensado para
	// ataques deliberadamente pesados, no como algo que la mayoría de abilities deba
	// activar.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage")
	bool bKnockbackIsHeavy = false;

	// --- LAUNCH / NIVEL 3 (post-315, a petición) ---
	// Sistema INDEPENDIENTE del Knockback de arriba, no una variación suya. Reservado a
	// propósito para fuentes de daño específicas (explosiones, golpes desde abajo) — la
	// gran mayoría de abilities deben dejar esto en 0 (LaunchChance por defecto).
	// Cuando se activa: bloquea GA_HitReact (vía State.Airborne), lanza al personaje con
	// un ángulo pronunciado hacia arriba, y termina en GA_GetUp (con i-frames) al
	// aterrizar — ver Landed() en PantheliaCharacterBase.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage|Launch")
	FScalableFloat LaunchChance = FScalableFloat(0.f);

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage|Launch")
	FScalableFloat LaunchForceMagnitude = FScalableFloat(1200.f);

	// Ángulo del pitch override — más pronunciado que el de Knockback (45°) por
	// defecto, porque este sí busca un lanzamiento vertical "de verdad", no un simple
	// empujón horizontal.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Damage|Launch")
	FScalableFloat LaunchPitchOverride = FScalableFloat(65.f);

	// Aplica el daño de esta ability al TargetActor.
	// Crea un spec de GE desde DamageEffectClass, asigna SetByCaller para cada
	// tipo de daño en DamageTypes (evaluados al nivel actual de la ability),
	// añade el daño de postura si existe, y aplica el spec al ASC del target.
	// Llamar desde el Blueprint en el LoopBody del ForEach de GetLivePlayersWithinRadius.
	UFUNCTION(BlueprintCallable, Category = "Damage")
	void CauseDamage(AActor* TargetActor);

	// Construye un spec de daño completo (DamageEffectClass + escalado por atributos
	// + daño de postura) al nivel actual de la ability y lo devuelve SIN aplicarlo.
	//
	// Pensado para el Weapon Trace System: GA_MeleeAttack llama esto al activarse y
	// entrega el spec resultante al UWeaponTraceComponent vía SetDamageSpec(). El
	// componente luego aplica ese spec a cada actor que la hoja golpee durante el swing.
	//
	// Es el equivalente "diferido" de CauseDamage: CauseDamage construye Y aplica de
	// inmediato a un target conocido; MakeDamageSpec solo construye, para aplicar después
	// a targets que aún no se conocen (los que el sweep detecte).
	UFUNCTION(BlueprintCallable, Category = "Damage")
	FGameplayEffectSpecHandle MakeDamageSpec();

	// Selecciona un FTaggedMontage al azar del array recibido.
	// Devuelve un FTaggedMontage vacío (Montage=null, MontageTag=inválido) si el
	// array está vacío — PlayMontageAndWait manejará el montage null sin crashear.
	//
	// BlueprintPure: sin exec pin, úsalo como nodo de datos en Blueprint.
	//
	// Reemplaza los 7 nodos manuales en GA_MeleeAttack:
	//   Array_Length → Subtract(-1) → RandomIntegerInRange → GetArrayItem
	//   + Array_Length → Greater(0) → Branch → EndAbility si vacío
	// Con un único nodo limpio.
	UFUNCTION(BlueprintPure, Category = "Combat")
	FTaggedMontage GetRandomTaggedMontageFromArray(const TArray<FTaggedMontage>& TaggedMontages) const;

protected:

	// El GE de daño que contiene el ExecCalc_Damage. Asignar GE_Damage en los BPs.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Damage")
	TSubclassOf<UGameplayEffect> DamageEffectClass;

	// Aplica el escalado de daño completo (spec §1.7) a un spec de GE ya creado.
	// Centraliza los 5 pasos del cálculo de daño para que TODAS las fuentes de daño
	// (proyectiles, melee, weapon trace) usen exactamente la misma lógica:
	//   1. Calcular el daño base total (Σ daños base por tipo en DamageTypes)
	//   2. Calcular el escalado por atributos (Σ ratio × atributo del caster)
	//   3. Multiplicador = (DañoBase + Escalado) / DañoBase
	//   4. Distribuir el multiplicador proporcionalmente entre los tipos de daño
	//   5. Asignar cada tipo de daño escalado + el daño de postura al SetByCaller
	//   6. Aplicar multiplicadores independientes de daño, postura y buildup
	//      (para armas: Daño bruto × multiplicador Light/Heavy/Charged)
	//
	// El ExecCalc_Damage recibe valores YA escalados — no conoce el escalado.
	//
	// SpecHandle: el spec de GE creado desde DamageEffectClass (se modifica in-place).
	// SourceASC:  el ASC del caster, de donde se leen los valores de atributos.
	//
	// Antes esta lógica estaba duplicada en SpawnProjectile; ahora vive aquí en la
	// clase base de daño para que CauseDamage (melee) también la aplique (hallazgo D1).
	void ApplyDamageScalingToSpec(
		FGameplayEffectSpecHandle& SpecHandle,
		UAbilitySystemComponent* SourceASC,
		float DamageMultiplier = 1.0f,
		float PoiseDamageMultiplier = 1.0f,
		float BuildupMultiplier = 1.0f) const;

	// Variante C++ para construir payloads independientes a partir de los mismos datos
	// de la ability. La usa el proyectil de área para separar impacto directo, explosión,
	// postura y buildup sin duplicar el pipeline de escalado.
	FGameplayEffectSpecHandle MakeDamageSpecWithMultipliers(
		float DamageMultiplier,
		float PoiseDamageMultiplier,
		float BuildupMultiplier) const;
};
