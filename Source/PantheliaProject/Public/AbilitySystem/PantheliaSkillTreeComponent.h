// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
// FActiveGameplayEffectHandle por valor (dentro del TMap de handles): el compilador
// necesita el struct completo — un forward declaration no basta (lección documentada
// en Code_Review.md).
#include "GameplayEffectTypes.h"
#include "PantheliaSkillTreeComponent.generated.h"

class UPantheliaSkillTreeInfo;
class UPantheliaAbilitySystemComponent;
class APantheliaPlayerState;
struct FPantheliaSkillNodeInfo;

// Delegate para avisar a la UI de que un nodo cambió de rango (se desbloqueó o
// se mejoró). Pasa el tag del nodo y su nuevo rango. NO es dinámico porque el
// binding se hará en C++ desde el futuro SpellMenuWidgetController, igual que
// los delegates del PlayerState (mismo patrón del proyecto).
DECLARE_MULTICAST_DELEGATE_TwoParams(FOnSkillNodeChanged, const FGameplayTag& /*NodeTag*/, int32 /*NewRank*/);

// ============================================================
// ÁRBOL DE HABILIDADES — COMPONENTE DE ESTADO DEL JUGADOR (Etapa 5)
// ============================================================
//
// CLASE PADRE: UActorComponent.
//
// Este componente es el ÚNICO DUEÑO del estado del árbol del jugador, y la única
// pieza que traduce datos (UPantheliaSkillTreeInfo) a estado de GAS (abilities +
// GEs Infinite). La UI nunca llama a GiveAbility ni aplica GEs directamente:
// pide TryUnlockNode y este componente valida y ejecuta.
//
// DÓNDE VIVE Y POR QUÉ: se crea en el constructor de APantheliaPlayerState,
// junto al ASC. El PlayerState sobrevive a la muerte/respawn del Pawn, así que
// los nodos comprados sobreviven también (misma razón de la Etapa 4).
//
// PRINCIPIO DE PERSISTENCIA (para el futuro SaveGame): "GAS nunca se guarda;
// GAS se reconstruye desde datos guardados". Lo ÚNICO serializable de este
// componente es NodeRanks (tag → rango) — al cargar partida, ReapplyAllNodes()
// re-deriva todo el estado de GAS desde el Data Asset. Los handles de GEs son
// runtime puro y jamás tocan el disco. Ventaja extra: si se rebalancea un nodo
// en el Data Asset, los saves viejos cargan con los valores nuevos automáticamente.
//
// TIPOS DE NODO SOPORTADOS (los tres, combinables en una misma entrada):
//   1. Nodo de atributos: GEs Infinite con SetByCaller escalado por rango.
//   2. Nodo que desbloquea hechizo: GiveAbility a nivel = rango (usa la Etapa 2).
//   3. Nodo que modifica un hechizo existente: un GE que concede un TAG que la
//      ability consulta al activarse (patrón de dos mitades de State_Pending.md).
UCLASS(ClassGroup = (Panthelia), meta = (BlueprintSpawnableComponent))
class PANTHELIAPROJECT_API UPantheliaSkillTreeComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPantheliaSkillTreeComponent();

	// Delegate público para que el futuro SpellMenuWidgetController se suscriba
	// y refresque el nodo concreto que cambió (sin repintar el árbol entero).
	FOnSkillNodeChanged OnSkillNodeChangedDelegate;

	// Devuelve el rango actual del nodo, o 0 si no está desbloqueado.
	// (Misma convención que GetAbilityLevelFromTag en el ASC: 0 = "no lo tiene".)
	UFUNCTION(BlueprintCallable, Category = "Panthelia|SkillTree")
	int32 GetNodeRank(const FGameplayTag& NodeTag) const;

	// ¿Puede el jugador comprar el siguiente rango de este nodo AHORA MISMO?
	// Comprueba, en orden: que el nodo exista en el Data Asset, que no esté ya al
	// rango máximo, el requisito de nivel de personaje, los puntos de habilidad
	// disponibles y los prerequisitos (todos con rango >= 1).
	// Es const y sin efectos: la UI puede llamarla cada frame para pintar el estado
	// de los botones (comprable / bloqueado) sin miedo a tocar nada.
	UFUNCTION(BlueprintCallable, Category = "Panthelia|SkillTree")
	bool CanUnlockNode(const FGameplayTag& NodeTag) const;

	// Intenta comprar el siguiente rango del nodo: valida con CanUnlockNode, gasta
	// los puntos, sube el rango, aplica el nodo a GAS y broadcastea el delegate.
	// Devuelve true si la compra se realizó. Este es EL punto de entrada de la UI.
	UFUNCTION(BlueprintCallable, Category = "Panthelia|SkillTree")
	bool TryUnlockNode(const FGameplayTag& NodeTag);

	// Re-deriva TODO el estado de GAS desde NodeRanks + el Data Asset.
	// Pensada para el futuro SaveGame: tras cargar NodeRanks desde disco, una sola
	// llamada reconstruye abilities y GEs. Es idempotente: aplicarla dos veces deja
	// el mismo estado (los GEs viejos de cada nodo se remueven antes de reaplicar,
	// y SetAbilityLevel ya ignora niveles repetidos — Etapa 2).
	UFUNCTION(BlueprintCallable, Category = "Panthelia|SkillTree")
	void ReapplyAllNodes();

protected:
	// Definición de todos los nodos del árbol (el "QUÉ existe").
	// ASIGNAR EN EL EDITOR: Class Defaults de BP_PantheliaPlayerState → componente
	// SkillTreeComponent → esta propiedad → DA_SkillTree (cuando se cree).
	// Si está sin asignar, toda la API devuelve false/0 con un warning en el log.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panthelia|SkillTree")
	TObjectPtr<UPantheliaSkillTreeInfo> SkillTreeInfo;

private:
	// === EL ESTADO DEL JUGADOR (lo único que el SaveGame guardará) ===
	// Mapa nodo → rango comprado. La ausencia de un tag equivale a rango 0.
	// VisibleAnywhere para poder inspeccionarlo en el editor durante el debug.
	UPROPERTY(VisibleAnywhere, Category = "Panthelia|SkillTree")
	TMap<FGameplayTag, int32> NodeRanks;

	// === CONTABILIDAD RUNTIME (NUNCA se guarda — se reconstruye) ===
	// Handles de los GEs Infinite activos de cada nodo, para poder removerlos al
	// subir de rango (quitar la instancia del rango viejo antes de aplicar la del
	// nuevo — mismo patrón QUITAR + REAPLICAR de RefreshSecondaryAttributes) y en
	// el futuro respec.
	// NO es UPROPERTY: UHT no soporta TMap con TArray como valor, y no lo necesita —
	// los handles son structs planos sin UObjects que proteger del garbage collector,
	// y su ciclo de vida es puramente runtime.
	TMap<FGameplayTag, TArray<FActiveGameplayEffectHandle>> ActiveNodeEffectHandles;

	// --- Helpers internos ---

	// PlayerState dueño de este componente (el owner del componente ES el PlayerState).
	APantheliaPlayerState* GetPantheliaPlayerState() const;

	// ASC del jugador, obtenido a través del PlayerState dueño.
	UPantheliaAbilitySystemComponent* GetPantheliaASC() const;

	// Traduce un nodo a estado de GAS para el rango dado:
	//   - Ability: la otorga (rango 1) o le sube el nivel (rangos siguientes).
	//   - GEs: remueve las instancias del rango anterior y aplica las nuevas con
	//     las magnitudes SetByCaller evaluadas al rango actual.
	void ApplyNodeToGAS(const FPantheliaSkillNodeInfo& NodeInfo, int32 Rank);

	// Remueve todos los GEs activos de un nodo (por sus handles) y limpia su entrada.
	void RemoveNodeEffects(const FGameplayTag& NodeTag);

	// Deriva el AbilityTag (hijo de la raíz "Abilities") desde el CDO de la clase de
	// ability del nodo. Devuelve tag vacío si la clase es nula o no tiene ability tags.
	// Existe para NO duplicar el tag como campo del Data Asset (anti-desincronización).
	static FGameplayTag GetAbilityTagFromClass(TSubclassOf<class UGameplayAbility> AbilityClass);
};
