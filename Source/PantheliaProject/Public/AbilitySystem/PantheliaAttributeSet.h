// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AttributeSet.h"
#include "GameplayTagContainer.h"
#include "PantheliaAttributeSet.generated.h"

// Forward declaration para el caché de definiciones dinámicas de debuff (ver
// CachedDebuffEffects más abajo) — solo guardamos punteros, no hace falta el header.
class UGameplayEffect;

#define ATTRIBUTE_ACCESSORS(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_PROPERTY_GETTER(ClassName, PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_GETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_SETTER(PropertyName) \
	GAMEPLAYATTRIBUTE_VALUE_INITTER(PropertyName)

USTRUCT()
struct FEffectProperties
{
	GENERATED_BODY()

	FEffectProperties() {}

	FGameplayEffectContextHandle EffectContextHandle;

	UPROPERTY() UAbilitySystemComponent* SourceASC = nullptr;
	UPROPERTY() AActor* SourceAvatarActor = nullptr;
	UPROPERTY() AController* SourceController = nullptr;
	UPROPERTY() ACharacter* SourceCharacter = nullptr;

	UPROPERTY() UAbilitySystemComponent* TargetASC = nullptr;
	UPROPERTY() AActor* TargetAvatarActor = nullptr;
	UPROPERTY() AController* TargetController = nullptr;
	UPROPERTY() ACharacter* TargetCharacter = nullptr;
};

template<class T>
using TStaticFuncPtr = typename TBaseStaticDelegateInstance<T, FDefaultDelegateUserPolicy>::FFuncPtr;

UCLASS()
class PANTHELIAPROJECT_API UPantheliaAttributeSet : public UAttributeSet
{
	GENERATED_BODY()

public:

	UPantheliaAttributeSet();

	virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
	virtual void PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue) override;

	// === POR QUÉ EXISTE PostAttributeChange ADEMÁS DE PreAttributeChange ===
	//
	// PreAttributeChange clampea un atributo cuando ESE atributo cambia (ej: Health
	// intenta subir por encima de MaxHealth → se recorta). Pero NO se dispara cuando
	// cambia el atributo Max asociado. Ejemplo del problema que esto deja abierto:
	//
	//   Health = 350, MaxHealth = 350. Un nodo del árbol de habilidades (GE Infinite
	//   con +50 MaxHealth) se REMUEVE en un respec → MaxHealth baja a 300. Nadie ha
	//   tocado Health, así que PreAttributeChange(Health) nunca se ejecuta y Health
	//   queda en 350: por encima del máximo, y persistido en el BaseValue.
	//
	// PostAttributeChange es el hook simétrico: se dispara DESPUÉS de que cualquier
	// atributo cambie su CurrentValue, con el valor viejo y el nuevo. Aquí vigilamos
	// los cuatro Max (MaxHealth/MaxMana/MaxStamina/MaxPoise) y, si el vital asociado
	// quedó por encima del nuevo máximo, lo recortamos al máximo.
	//
	// DECISIÓN DE DISEÑO (soulslike, estilo Elden Ring): al bajar el Max se CLAMPEA
	// el vital (300/300), no se escala proporcionalmente. Al SUBIR el Max no se toca
	// el vital (sigues con la misma vida actual, ahora con más techo) — ganar vida
	// actual gratis al comprar un nodo de MaxHealth sería un regalo no soulslike.
	//
	// Este método es el prerequisito silencioso del árbol de habilidades y del futuro
	// equipamiento: ambos moverán los Max hacia arriba Y hacia abajo (respec, cambiar
	// una pieza por otra peor) y este clamp evita estados corruptos de vitales.
	virtual void PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue) override;

	virtual void PostGameplayEffectExecute(const struct FGameplayEffectModCallbackData& Data) override;

	// Mapa tag → función estática GetXAttribute.
	// Usado por el WidgetController para saber qué atributo leer dado un tag.
	TMap<FGameplayTag, TStaticFuncPtr<FGameplayAttribute()>> TagsToAttributes;

	// ===== ATRIBUTOS PRIMARIOS =====
	UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Hardness, Category = "Primary Attributes") FGameplayAttributeData Hardness;   ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, Hardness)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Resonance, Category = "Primary Attributes") FGameplayAttributeData Resonance;  ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, Resonance)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Resilience, Category = "Primary Attributes") FGameplayAttributeData Resilience; ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, Resilience)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Endurance, Category = "Primary Attributes") FGameplayAttributeData Endurance;  ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, Endurance)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Spirit, Category = "Primary Attributes") FGameplayAttributeData Spirit;     ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, Spirit)

		// ===== ATRIBUTOS SECUNDARIOS =====
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxHealth, Category = "Secondary Attributes") FGameplayAttributeData MaxHealth;        ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, MaxHealth)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Armor, Category = "Secondary Attributes") FGameplayAttributeData Armor;            ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, Armor)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxMana, Category = "Secondary Attributes") FGameplayAttributeData MaxMana;          ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, MaxMana)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MagicResistance, Category = "Secondary Attributes") FGameplayAttributeData MagicResistance;  ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, MagicResistance)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxStamina, Category = "Secondary Attributes") FGameplayAttributeData MaxStamina;       ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, MaxStamina)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MaxPoise, Category = "Secondary Attributes") FGameplayAttributeData MaxPoise;         ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, MaxPoise)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Tenacity, Category = "Secondary Attributes") FGameplayAttributeData Tenacity;         ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, Tenacity)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_PhysicalDamage, Category = "Secondary Attributes") FGameplayAttributeData PhysicalDamage;   ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, PhysicalDamage)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MagicDamage, Category = "Secondary Attributes") FGameplayAttributeData MagicDamage;      ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, MagicDamage)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_ArmorPenetration, Category = "Secondary Attributes") FGameplayAttributeData ArmorPenetration; ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, ArmorPenetration)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_MagicPenetration, Category = "Secondary Attributes") FGameplayAttributeData MagicPenetration; ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, MagicPenetration)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CritChance, Category = "Secondary Attributes") FGameplayAttributeData CritChance;       ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, CritChance)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_CritDamage, Category = "Secondary Attributes") FGameplayAttributeData CritDamage;       ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, CritDamage)

		// ===== RESISTENCIAS ELEMENTALES =====
		// Una resistencia cubre los dos tipos de su elemento (físico y mágico).
		// Placeholder derivado de Resilience en el GE de secundarios.
		// Cuando existan equipamiento y árbol de habilidades, serán esos sistemas
		// los que modifiquen estos atributos directamente.
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_FireResistance, Category = "Resistance Attributes") FGameplayAttributeData FireResistance;   ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, FireResistance)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_WaterResistance, Category = "Resistance Attributes") FGameplayAttributeData WaterResistance;  ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, WaterResistance)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_StormResistance, Category = "Resistance Attributes") FGameplayAttributeData StormResistance;  ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, StormResistance)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_NatureResistance, Category = "Resistance Attributes") FGameplayAttributeData NatureResistance; ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, NatureResistance)

		// ===== ATRIBUTOS VITALES =====
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Health, Category = "Vital Attributes") FGameplayAttributeData Health;   ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, Health)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Mana, Category = "Vital Attributes") FGameplayAttributeData Mana;     ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, Mana)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Stamina, Category = "Vital Attributes") FGameplayAttributeData Stamina;  ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, Stamina)
		UPROPERTY(BlueprintReadOnly, ReplicatedUsing = OnRep_Poise, Category = "Combat Attributes") FGameplayAttributeData Poise;   ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, Poise)

		// ===== META ATRIBUTOS (no replicados, temporales en servidor) =====
		// Los meta atributos se zeroan en PostGameplayEffectExecute tras procesar su valor.
		// No tienen OnRep porque no se replican — viven solo en el servidor.
		UPROPERTY(BlueprintReadOnly, Category = "Meta Attributes") FGameplayAttributeData IncomingDamage;      ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, IncomingDamage)
		UPROPERTY(BlueprintReadOnly, Category = "Meta Attributes") FGameplayAttributeData IncomingPoiseDamage; ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, IncomingPoiseDamage)

		// IncomingXP: XP que el jugador va a recibir por matar a un enemigo.
		// Se aplica vía GE Instant desde la ability pasiva ListenForXPEvents (próximas clases).
		// El AttributeSet lo consume aquí, aplica el multiplicador de rendimientos decrecientes
		// y lo suma al PlayerState via AddToXP. No se muestra en la UI como atributo: no se
		// añade al TagsToAttributes ni tiene OnRep.
		UPROPERTY(BlueprintReadOnly, Category = "Meta Attributes") FGameplayAttributeData IncomingXP; ATTRIBUTE_ACCESSORS(UPantheliaAttributeSet, IncomingXP)

		// --- OnRep Primarios ---
		UFUNCTION() void OnRep_Hardness(const FGameplayAttributeData& OldHardness) const;
	UFUNCTION() void OnRep_Resonance(const FGameplayAttributeData& OldResonance) const;
	UFUNCTION() void OnRep_Resilience(const FGameplayAttributeData& OldResilience) const;
	UFUNCTION() void OnRep_Endurance(const FGameplayAttributeData& OldEndurance) const;
	UFUNCTION() void OnRep_Spirit(const FGameplayAttributeData& OldSpirit) const;

	// --- OnRep Secundarios ---
	UFUNCTION() void OnRep_MaxHealth(const FGameplayAttributeData& OldMaxHealth) const;
	UFUNCTION() void OnRep_Armor(const FGameplayAttributeData& OldArmor) const;
	UFUNCTION() void OnRep_MaxMana(const FGameplayAttributeData& OldMaxMana) const;
	UFUNCTION() void OnRep_MagicResistance(const FGameplayAttributeData& OldMagicResistance) const;
	UFUNCTION() void OnRep_MaxStamina(const FGameplayAttributeData& OldMaxStamina) const;
	UFUNCTION() void OnRep_MaxPoise(const FGameplayAttributeData& OldMaxPoise) const;
	UFUNCTION() void OnRep_Tenacity(const FGameplayAttributeData& OldTenacity) const;
	UFUNCTION() void OnRep_PhysicalDamage(const FGameplayAttributeData& OldPhysicalDamage) const;
	UFUNCTION() void OnRep_MagicDamage(const FGameplayAttributeData& OldMagicDamage) const;
	UFUNCTION() void OnRep_ArmorPenetration(const FGameplayAttributeData& OldArmorPenetration) const;
	UFUNCTION() void OnRep_MagicPenetration(const FGameplayAttributeData& OldMagicPenetration) const;
	UFUNCTION() void OnRep_CritChance(const FGameplayAttributeData& OldCritChance) const;
	UFUNCTION() void OnRep_CritDamage(const FGameplayAttributeData& OldCritDamage) const;

	// --- OnRep Resistencias ---
	UFUNCTION() void OnRep_FireResistance(const FGameplayAttributeData& OldFireResistance) const;
	UFUNCTION() void OnRep_WaterResistance(const FGameplayAttributeData& OldWaterResistance) const;
	UFUNCTION() void OnRep_StormResistance(const FGameplayAttributeData& OldStormResistance) const;
	UFUNCTION() void OnRep_NatureResistance(const FGameplayAttributeData& OldNatureResistance) const;

	// --- OnRep Vitales ---
	UFUNCTION() void OnRep_Health(const FGameplayAttributeData& OldHealth) const;
	UFUNCTION() void OnRep_Mana(const FGameplayAttributeData& OldMana) const;
	UFUNCTION() void OnRep_Stamina(const FGameplayAttributeData& OldStamina) const;
	UFUNCTION() void OnRep_Poise(const FGameplayAttributeData& OldPoise) const;

private:
	void SetEffectProperties(const FGameplayEffectModCallbackData& Data, FEffectProperties& Props) const;
	void HandleParryReaction(const FEffectProperties& Props);

	// Extraído de PostGameplayEffectExecute (clase 309): toda la reacción al meta atributo
	// IncomingDamage (parry/bloqueo, reducción de vida, muerte, XP) vivía inline en un solo
	// bloque enorme. Se separa en su propia función por la misma razón que Health/Mana/Stamina
	// ya tenían la suya: cada meta atributo hace UNA cosa clara, no una maraña de ifs.
	void HandleIncomingDamage(const FEffectProperties& Props);

	// Extraído de PostGameplayEffectExecute (clase 309): toda la reacción al meta atributo
	// IncomingXP (detección de subida de nivel, relleno de vida/maná, AddToXP).
	void HandleIncomingXP(const FEffectProperties& Props);

	// Llamada desde HandleIncomingDamage cuando el context indica un debuff exitoso
	// (ExecCalc_Damage → DetermineDebuff, clases 307-309 escriben ese resultado en el
	// context). Construye un UGameplayEffect DINÁMICAMENTE en C++ (no un asset de
	// Blueprint) con la duración/período/daño de este golpe concreto, le concede el tag
	// de debuff correspondiente al elemento (Fuego→Burn, etc. — ver ElementToDebuff en
	// FPantheliaGameplayTags) y lo aplica al target. Implementación completa en el .cpp
	// (clase 310) — ahí se explican las adaptaciones de API frente al curso original
	// (tags de GE deprecados desde 5.3, stacking en API inestable en 5.7+).
	void Debuff(const FEffectProperties& Props);

	// === CACHÉ DE DEFINICIONES DINÁMICAS DE DEBUFF (fix auditoría post-315) ===
	//
	// POR QUÉ EXISTE — dos bugs de la versión anterior de Debuff() que este caché
	// resuelve A LA VEZ:
	//
	// 1. EL STACKING NO FUNCIONABA. Debuff() creaba un UGameplayEffect NUEVO con
	//    NewObject en cada debuff exitoso y le configuraba StackingType =
	//    AggregateBySource + StackLimitCount = 1. Pero el stacking de GAS agrupa por
	//    DEFINICIÓN de efecto (el objeto UGameplayEffect concreto): dos Quemaduras
	//    seguidas creaban DOS definiciones distintas, GAS no las consideraba "el
	//    mismo efecto", y corrían EN PARALELO — exactamente lo que el stacking
	//    intentaba evitar. Con una ÚNICA definición cacheada por tipo de debuff,
	//    las aplicaciones repetidas SÍ agregan: la segunda Quemadura refresca la
	//    duración de la primera en vez de sumarse.
	//
	// 2. COLISIÓN DE NOMBRES. NewObject con el MISMO FName en el MISMO outer
	//    (GetTransientPackage) mientras el objeto anterior con ese nombre sigue vivo
	//    (su debuff de 5s aún activo) es terreno indefinido del motor — puede
	//    reemplazar/reconstruir el objeto in situ con un efecto activo apuntándole.
	//    Con el caché, el objeto se crea UNA vez y se reutiliza: nunca se recrea un
	//    nombre ocupado.
	//
	// La clave del mapa incluye el tag de debuff Y la frecuencia (Period): el Period
	// es una propiedad fija de la definición (no puede ser SetByCaller), así que dos
	// abilities con frecuencias distintas del mismo debuff necesitan definiciones
	// distintas — y NO deben stackear entre sí de todas formas (tiquean a ritmos
	// diferentes). Daño y duración, en cambio, SÍ viajan como SetByCaller en el spec
	// (ver Debuff() en el .cpp), así que no fragmentan el caché.
	//
	// UPROPERTY(Transient): Transient porque es puro estado runtime (jamás se
	// serializa a disco); UPROPERTY porque sin él el garbage collector podría
	// llevarse una definición cacheada cuando ningún efecto activo la referencie —
	// y la siguiente Quemadura usaría un puntero muerto.
	UPROPERTY(Transient)
	TMap<FName, TObjectPtr<UGameplayEffect>> CachedDebuffEffects;

	// Llamada cuando IncomingDamage es fatal. Obtiene la XP del enemigo muerto,
	// aplica el multiplicador de rendimientos decrecientes según EnemyID y kill count
	// del PlayerState del atacante, y envía un GameplayEvent al atacante con la XP final.
	// El GameplayEvent lo recibe GA_ListenForXPEvents, que aplica GE_EventBasedEffect
	// sobre IncomingXP del AttributeSet del atacante. PostGameplayEffectExecute lo
	// consume y lo suma al PlayerState vía AddToXP.
	void SendXPEvent(const FEffectProperties& Props);
};