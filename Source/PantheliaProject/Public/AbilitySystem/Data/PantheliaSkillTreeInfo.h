// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
// FScalableFloat por valor en el TMap de magnitudes: el compilador necesita el
// struct completo, no basta un forward declaration (misma lección documentada en
// Code_Review.md para FActiveGameplayEffectHandle).
#include "ScalableFloat.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "PantheliaSkillTreeInfo.generated.h"

class UGameplayAbility;
class UGameplayEffect;

// ============================================================
// ÁRBOL DE HABILIDADES — DEFINICIÓN DE NODOS (Etapa 5)
// ============================================================
//
// FILOSOFÍA (decisión de diseño del proyecto): "el árbol no es código, es data".
// Añadir un nodo nuevo al árbol NUNCA debe requerir tocar C++: es una entrada
// nueva en este Data Asset. El componente USkillTreeComponent (en el PlayerState)
// lee estas definiciones y las traduce a estado de GAS (GiveAbility / GEs Infinite).
//
// Este Data Asset es el "QUÉ existe" (diseño, se edita en el editor).
// El componente guarda el "QUÉ tiene el jugador" (runtime, se guardará en SaveGame).
//
// Convención de nombrado (IMPORTANTE, misma regla que FPantheliaAbilityInfo):
//   El struct se llama FPantheliaSkillNodeInfo y la clase UPantheliaSkillTreeInfo.
//   Tienen bases distintas ("SkillNodeInfo" vs "SkillTreeInfo") así que aquí no hay
//   riesgo de colisión UHT, pero mantenemos el patrón de sufijos del proyecto.

// Struct que define UN nodo del árbol de habilidades.
USTRUCT(BlueprintType)
struct FPantheliaSkillNodeInfo
{
	GENERATED_BODY()

	// Tag que identifica el nodo. Es la CLAVE PRIMARIA de la entrada: el componente
	// busca nodos por este tag, el mapa de rangos del jugador usa este tag, y el
	// SaveGame guardará este tag. Convención sugerida: "SkillTree.Fire.MaxHealth1"
	// o similar — la jerarquía definitiva de tags de nodos se decidirá al poblar
	// el árbol real (los tags de nodos pueden registrarse en el .ini del editor,
	// no necesitan ser nativos: ningún código C++ los referencia por nombre).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Node")
	FGameplayTag NodeTag = FGameplayTag();

	// Nombre del nodo para la UI del árbol (Spell Menu / árbol futuro).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Node")
	FText NodeName = FText();

	// Descripción para la UI. Cuando llegue la clase de Rich Text del curso
	// (sección 24), este texto podrá llevar marcado de estilos.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Node")
	FText NodeDescription = FText();

	// Rango máximo del nodo. 1 = nodo de un solo desbloqueo (típico "desbloquea
	// hechizo"); 3 = nodo mejorable tres veces (típico "+X atributo por rango").
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Node", meta = (ClampMin = "1"))
	int32 MaxRank = 1;

	// Puntos de habilidad que cuesta CADA rango de este nodo.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Node", meta = (ClampMin = "0"))
	int32 CostPerRank = 1;

	// Nivel de personaje mínimo para poder desbloquear este nodo (requisito del
	// curso, sección 24, adaptado). 1 = sin requisito efectivo.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Node", meta = (ClampMin = "1"))
	int32 LevelRequirement = 1;

	// Nodos que deben estar desbloqueados (rango >= 1) ANTES que este.
	// Un FGameplayTagContainer es exactamente la herramienta para esto: "para
	// desbloquear X necesitas Y y Z" es una relación entre tags, no entre punteros.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Node")
	FGameplayTagContainer PrerequisiteNodeTags;

	// --- QUÉ OTORGA EL NODO (las tres piezas son opcionales y combinables) ---

	// (1) Ability que desbloquea este nodo (hechizos, maniobras). Opcional.
	//   - Al comprar el rango 1: se otorga con GiveAbility a nivel 1.
	//   - Al comprar rangos siguientes: se sube su nivel con SetAbilityLevel, y con
	//     él escalan DamageTypes/PoiseDamage vía GetValueAtLevel (Etapa 2).
	// NOTA ANTI-ERRORES: el AbilityTag NO se duplica aquí como campo — el componente
	// lo deriva del propio CDO de esta clase (de sus Ability Tags). Así es imposible
	// que el dato del nodo y el tag real de la ability se desincronicen por un typo.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grants")
	TSubclassOf<UGameplayAbility> GrantedAbility = nullptr;

	// (2) GameplayEffects que aplica este nodo. Deben ser de duración INFINITE y
	// usar Stacking Type=None. El componente guarda sus handles para poder removerlos
	// (cambio de rango, respec, recarga de partida). Un GE Instant sería irrevocable;
	// un GE apilable podría reutilizar el handle del rango anterior y romper el rollback.
	// Ejemplos: GE_Node_MaxHealth (+MaxHealth), GE_Node_FireResist (+FireResistance),
	// o un GE sin modifiers cuyo único trabajo sea CONCEDER UN TAG (Granted Tags)
	// que una ability consulte ("Talent.Firebolt.TripleShot") — el patrón de dos
	// mitades ya acordado para las pasivas elementales en State_Pending.md.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grants")
	TArray<TSubclassOf<UGameplayEffect>> GrantedEffects;

	// (3) Magnitudes SetByCaller que el componente inyecta en los GrantedEffects,
	// evaluadas AL RANGO ACTUAL del nodo. FScalableFloat permite definir la
	// progresión por rangos con una Curve Table sin tocar código:
	//   Key   = tag SetByCaller que el GE espera (p. ej. "Attributes.Secondary.MaxHealth")
	//   Value = curva/valor; GetValueAtLevel(Rank) da la magnitud de ese rango.
	// Ejemplo: nodo de 3 rangos con curva (1→20, 2→35, 3→50) = "+20/+35/+50 MaxHealth".
	// Si un GE no usa SetByCaller (magnitudes fijas en el asset), dejar el mapa vacío.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Grants")
	TMap<FGameplayTag, FScalableFloat> SetByCallerMagnitudes;
};

// Data Asset con la definición de TODOS los nodos del árbol de habilidades.
// Permite buscar un FPantheliaSkillNodeInfo por su NodeTag.
//
// Flujo de uso (cuando se pueble el árbol real):
//   1. En el editor: clic derecho → Miscellaneous → Data Asset → UPantheliaSkillTreeInfo
//      → crear DA_SkillTree.
//   2. Añadir una entrada por nodo (NodeTag, coste, prerequisitos, GEs...).
//   3. Asignar DA_SkillTree a la propiedad SkillTreeInfo del SkillTreeComponent
//      (visible en los Class Defaults de BP_PantheliaPlayerState, dentro del componente).
//
// NOTA: el DA se crea vacío por ahora — esta etapa entrega la INFRAESTRUCTURA.
// Se poblará con la sección 24 del curso y el diseño real del árbol.
UCLASS()
class PANTHELIAPROJECT_API UPantheliaSkillTreeInfo : public UDataAsset
{
	GENERATED_BODY()

public:
	// Array con la definición de todos los nodos del árbol.
	// Se rellena desde el editor del Data Asset.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Skill Tree")
	TArray<FPantheliaSkillNodeInfo> SkillNodes;

	// Busca en el array la entrada cuyo NodeTag coincida EXACTAMENTE con el tag dado.
	// bLogNotFound: si es true y no lo encuentra, loguea un error (útil en flujos
	// donde el tag DEBERÍA existir; falso en consultas especulativas de la UI).
	// Devuelve nullptr si no existe — el caller SIEMPRE debe comprobarlo.
	// Devolvemos puntero (no copia) porque el struct contiene arrays y mapas: copiarlo
	// en cada consulta de la UI sería trabajo innecesario; el DA es inmutable en runtime.
	const FPantheliaSkillNodeInfo* FindNodeInfoForTag(
		const FGameplayTag& NodeTag,
		bool bLogNotFound = false) const;

#if WITH_EDITOR
	// Valida las invariantes del asset mediante Data Validation del editor.
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
