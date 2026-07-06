// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/PantheliaCharacterBase.h"
#include "Interfaces/Enemy.h"
// PantheliaCharacterClassInfo ya se incluye transitivamente a través de CombatInterface.h
// (que incluye PantheliaCharacterBase.h → CombatInterface.h → PantheliaCharacterClassInfo.h).
// Lo dejamos comentado como referencia; ya no es necesario incluirlo directamente aquí.
// #include "AbilitySystem/Data/PantheliaCharacterClassInfo.h"
#include "PantheliaEnemy.generated.h"

// Forward declarations: evitamos incluir headers pesados en el .h.
// Solo necesitamos los punteros aquí; los includes van en el .cpp.
class UBehaviorTree;
class APantheliaAIController;

UCLASS()
class PANTHELIAPROJECT_API APantheliaEnemy : public APantheliaCharacterBase, public IEnemy
{
	GENERATED_BODY()

public:
	APantheliaEnemy();

	/** IEnemy Interface */
	virtual void HighlightActor_Implementation() override;
	virtual void UnHighlightActor_Implementation() override;

	// CombatTarget: el actor que este enemigo está atacando ahora mismo.
	// Seteado desde BTT_Attack antes de activar la ability, usado en GA_MeleeAttack
	// para orientar el Motion Warping hacia él (UpdateFacingTarget).
	UPROPERTY(BlueprintReadWrite, Category = "Panthelia|Combat")
	TObjectPtr<AActor> CombatTarget;

	virtual void SetCombatTarget_Implementation(AActor* InCombatTarget) override;
	virtual AActor* GetCombatTarget_Implementation() const override;

	/** ICombatInterface */
	virtual int32 GetPlayerLevel_Implementation() const override;
	virtual void Die(const FVector& DeathImpulse) override;

	// Devuelve el elemento defensivo de este enemigo.
	// Afecta la tabla de afinidades en ExecCalc_Damage.
	virtual EPantheliaElement GetDefensiveElement() const override { return DefensiveElement; }

	// --- Getters de XP (usados por UPantheliaAttributeSet::SendXPEvent) ---
	// Necesarios para que el AttributeSet lea BaseXPReward y EnemyID sin acceder
	// a miembros protected directamente desde una clase no relacionada.
	FORCEINLINE int32 GetBaseXPReward() const { return BaseXPReward; }
	FORCEINLINE FName GetEnemyID() const { return EnemyID; }

	UFUNCTION()
	void HitReactTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	UFUNCTION()
	void StaggerTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	UPROPERTY(BlueprintReadOnly, Category = "Panthelia|Combat")
	bool bHitReacting = false;

	UPROPERTY(BlueprintReadOnly, Category = "Panthelia|Combat")
	bool bStaggered = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panthelia|Combat")
	float BaseWalkSpeed = 250.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panthelia|Combat")
	float Lifespan = 5.f;

protected:
	virtual void BeginPlay() override;
	virtual void InitAbilityActorInfo() override;
	virtual void InitializeDefaultAttributes() const override;

	// PossessedBy se llama cuando un controller toma control de este personaje.
	// Para los enemigos, es el momento en que el AIController los posee,
	// lo que lo convierte en el lugar ideal para arrancar el Behavior Tree.
	// (Para el jugador, PossessedBy se usa para inicializar el ASC —
	//  ver AMainCharacter. Aquí lo usamos para inicializar la IA.)
	virtual void PossessedBy(AController* NewController) override;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panthelia|Combat")
	int32 Level = 1;

	// CharacterClass se hereda de APantheliaCharacterBase (Details → Panthelia|Combat).
	// Configura aquí qué arquetipo usa este enemigo (Warrior, Ranger, Elementalist).
	// Usado por DA_CharacterClassInfo para asignar atributos y abilities de clase.

	// Elemento defensivo del enemigo.
	// Determina qué tipos de daño elemental reciben modificador de tabla de afinidades.
	// None = neutro (sin modificador). Configurable por Blueprint en Details → Panthelia|Combat.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panthelia|Combat")
	EPantheliaElement DefensiveElement = EPantheliaElement::None;

	// ============================================================
	// SISTEMA DE RECOMPENSA DE XP
	// ============================================================

	// XP base que este enemigo otorga al ser eliminado.
	// Valor fijo definido a mano en el Blueprint de cada enemigo.
	// El sistema de rendimientos decrecientes aplica un multiplicador sobre este valor
	// según cuántas veces el jugador haya matado a ESTA instancia (por EnemyID).
	//
	// Bosses y minibosses: poner aquí la cantidad deseada; como no respawnean,
	// el sistema de rendimientos decrecientes no aplica sobre ellos.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panthelia|XP")
	int32 BaseXPReward = 0;

	// Identificador único de esta instancia de enemigo en el mundo.
	// Formato sugerido: "NombreEnemigo_Zona_Descripcion_Numero"
	// Ejemplo: "Goblin_Montana_Entrada_01"
	//
	// El PlayerState usa este ID para llevar un conteo de cuántas veces el jugador
	// ha matado a ESTE enemigo específico (entre respawns, al descansar en hoguera).
	// Eso permite aplicar el multiplicador de rendimientos decrecientes correcto.
	//
	// IMPORTANTE: cada instancia colocada en el nivel DEBE tener un ID único.
	// Si se deja vacío (NAME_None), este enemigo siempre dará el 100% de XP
	// sin tracking — útil para bosses y minibosses que no respawnean.
	//
	// GANCHO DE AUTOLEVEL: el sistema de escalado de zonas leerá EnemyID junto a
	// ZoneLevelOverride para mantener la progresión coherente al elegir el orden de zonas.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panthelia|XP")
	FName EnemyID = NAME_None;

	// GANCHO DE AUTOLEVEL (NO IMPLEMENTADO — futuro sistema de escalado de zonas).
	// Cuando el sistema de zonas exista, sobreescribirá este valor en runtime para
	// ajustar el nivel efectivo del enemigo al orden en que el jugador visita las zonas.
	// Por ahora siempre es 0 (sin efecto). No modificar manualmente en el editor.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Panthelia|XP|Autolevel")
	int32 ZoneLevelOverride = 0;

	// Si true, este enemigo puede usar la rama de ataque a distancia del Behavior Tree.
	//
	// Reemplaza la lógica anterior de "CharacterClass != Warrior" para determinar
	// si el enemigo es atacante a distancia. Ahora es configurable por Blueprint,
	// permitiendo enemigos híbridos (melee + ranged) independientemente de su clase.
	//
	// Uso:
	//   false (default) → solo ataca en melee. El BT ignora la rama ranged.
	//   true            → puede atacar a distancia. El BT ejecuta la rama ranged
	//                     cuando las condiciones se cumplen (ej: distancia al jugador).
	//                     Si también tiene GA_MeleeAttack en CombatAbilities, el BT
	//                     alternará entre melee y ranged según la distancia.
	//
	// IMPORTANTE: También hay que añadir GA_RangedAttack (o similar) a CombatAbilities
	// para que el ASC tenga la ability disponible para activar.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panthelia|AI")
	bool bCanRangedAttack = false;

	// Abilities de combate específicas de este enemigo, concedidas en BeginPlay.
	// Usar para ataques a distancia, hechizos especiales, o cualquier ability
	// que deba ser exclusiva de ciertos enemigos y no compartida por toda la clase.
	//
	// Complementa las abilities de clase en DA_CharacterClassInfo:
	//   DA_CharacterClassInfo.StartupAbilities → abilities de clase (ej: GA_MeleeAttack para Warrior)
	//   CombatAbilities                        → abilities extra por enemigo (ej: GA_RangedAttack)
	//
	// Se conceden al nivel del personaje (Level) para que escalen con él.
	// Configurar en el Blueprint del enemigo (Details → Panthelia|Abilities).
	UPROPERTY(EditAnywhere, Category = "Panthelia|Abilities")
	TArray<TSubclassOf<UGameplayAbility>> CombatAbilities;

	// Asset del Behavior Tree que controla las decisiones de este enemigo.
	// Asignable desde Blueprint: cada tipo de enemigo puede usar un árbol diferente.
	// (Ej: BP_EnemyWarrior usa BT_Warrior, BP_EnemyMage usa BT_Mage)
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panthelia|AI")
	TObjectPtr<UBehaviorTree> BehaviorTree;

	// Referencia cacheada al AIController que posee a este enemigo.
	// Se establece en PossessedBy() y se usa para controlar el Behavior Tree.
	// No se expone a Blueprint por ahora — se gestiona enteramente desde C++.
	UPROPERTY()
	TObjectPtr<APantheliaAIController> PantheliaAIController;
};