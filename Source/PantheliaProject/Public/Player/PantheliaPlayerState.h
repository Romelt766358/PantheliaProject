// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "PantheliaPlayerState.generated.h"

class UAbilitySystemComponent;
class UAttributeSet;
class UPantheliaLevelUpInfo;
class UPantheliaSkillTreeComponent;
class UPantheliaCostAttributeSet;

// Delegate genérico para avisar a la UI de que un contador de progreso ha
// cambiado (XP, nivel, puntos). NO es dinámico porque el binding se hace en
// C++ desde el OverlayWidgetController, igual que ya se hace con las barras
// de vida/maná/stamina. Pasa el nuevo valor como parámetro.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnPlayerStatChanged, int32 /*NewValue*/);

UCLASS()
class PANTHELIAPROJECT_API APantheliaPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	APantheliaPlayerState();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UAttributeSet* GetAttributeSet() const { return AttributeSet; }
	UPantheliaCostAttributeSet* GetCostAttributeSet() const { return CostAttributeSet.Get(); }

	// El nivel del jugador. No es un atributo de GAS porque no tiene las
	// interrelaciones complejas de los atributos, sino que es un contador
	// de progreso. Los MMCs accederán a él a través de una interfaz.
	FORCEINLINE int32 GetPlayerLevel() const { return Level; }

	// Otros contadores de progreso, bajo el mismo criterio que Level:
	// son progreso puro (no se mitigan ni se capturan en ExecCalc), así que
	// no son atributos de GAS, sino enteros planos del PlayerState.
	FORCEINLINE int32 GetXP() const { return XP; }
	FORCEINLINE int32 GetAttributePoints() const { return AttributePoints; }
	FORCEINLINE int32 GetSkillPoints() const { return SkillPoints; }

	// Getter para la tabla de progresión de niveles.
	// Necesario para que sistemas externos (ej. OverlayWidgetController) puedan
	// calcular el porcentaje de la barra de XP sin acceso directo al miembro protected.
	FORCEINLINE const UPantheliaLevelUpInfo* GetLevelUpInfo() const { return LevelUpInfo; }

	// Getter del componente del árbol de habilidades (Etapa 5).
	// El futuro SpellMenuWidgetController lo usará para consultar rangos, suscribirse
	// a OnSkillNodeChangedDelegate y pedir compras con TryUnlockNode.
	FORCEINLINE UPantheliaSkillTreeComponent* GetSkillTreeComponent() const { return SkillTreeComponent; }

	// --- Delegates para la UI ---
	// Se bindean desde el OverlayWidgetController en la Fase 2 (barra de XP, etc.).
	// Por ahora se declaran y se broadcastean; aún no hay nadie escuchando.
	FOnPlayerStatChanged OnXPChangedDelegate;
	FOnPlayerStatChanged OnLevelChangedDelegate;
	FOnPlayerStatChanged OnAttributePointsChangedDelegate;
	FOnPlayerStatChanged OnSkillPointsChangedDelegate;

	// --- Modificadores aditivos ---

	// Entrada principal del sistema de niveles: suma XP y, si se cruza algún
	// umbral, sube de nivel automáticamente (puede subir varios de golpe).
	// Todo lo que otorgue XP (matar enemigos, etc.) terminará llamando aquí.
	// BlueprintCallable para poder probarlo con una tecla de debug en la Fase 1.
	UFUNCTION(BlueprintCallable, Category = "Panthelia|Level")
	void AddToXP(int32 InXP);

	// Suman puntos a las reservas (p. ej. reembolsos o premios directos).
	void AddToLevel(int32 InLevel);
	void AddToAttributePoints(int32 InPoints);
	void AddToSkillPoints(int32 InPoints);

	// --- Setters directos (asignan y broadcastean el nuevo valor) ---
	void SetXP(int32 InXP);
	void SetLevel(int32 InLevel);
	void SetAttributePoints(int32 InPoints);
	void SetSkillPoints(int32 InPoints);

	// ============================================================
	// SISTEMA DE RENDIMIENTOS DECRECIENTES DE XP
	// ============================================================
	//
	// El jugador obtiene menos XP cada vez que mata al mismo enemigo (por EnemyID).
	// Esto aplica solo a enemigos con respawn (EnemyID no vacío).
	// Los bosses y minibosses no respawnean — dejan EnemyID vacío y siempre dan 100%.
	//
	// La tabla de multiplicadores es (ver PantheliaAbilitySystemLibrary):
	//   1ª muerte → 100%
	//   2ª muerte → 60%
	//   3ª muerte → 35%
	//   4ª muerte → 20%
	//   5ª+ muerte → 10% (piso permanente)
	//
	// El conteo se incrementa DESPUÉS de calcular y otorgar la XP de esa muerte.
	// "Mismo enemigo" = misma instancia que respawnea, identificada por EnemyID único.
	// Instancias distintas del mismo tipo de enemigo en distintos puntos del mapa
	// tienen EnemyIDs distintos y se cuentan de forma completamente independiente.

	// Devuelve cuántas veces el jugador ha matado al enemigo con ese ID.
	// Devuelve 0 si nunca lo ha matado (primera vez = kill count 0 = 100% XP).
	int32 GetEnemyKillCount(FName EnemyID) const;

	// Registra una nueva muerte de este enemigo (incrementa su contador).
	// Llamar DESPUÉS de haber calculado y otorgado la XP de esta muerte.
	void RecordEnemyKill(FName EnemyID);

protected:
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UAttributeSet> AttributeSet;

	// AttributeSet separado para multiplicadores/planos de costes. Vive junto al ASC
	// en el PlayerState para persistir entre respawns igual que los atributos base.
	UPROPERTY()
	TObjectPtr<UPantheliaCostAttributeSet> CostAttributeSet;

	// Tabla de progresión de niveles (umbrales de XP + premios por nivel).
	// DEBE asignarse en el Blueprint BP_PantheliaPlayerState (Class Defaults).
	// Si está sin asignar, AddToXP avisará por log y no podrá subir de nivel.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panthelia|Level")
	TObjectPtr<UPantheliaLevelUpInfo> LevelUpInfo;

	// Componente del árbol de habilidades (Etapa 5). Vive aquí, junto al ASC, por la
	// misma razón de persistencia: el PlayerState sobrevive a la muerte/respawn del
	// Pawn, así que los nodos comprados sobreviven también. Se crea en el constructor
	// (CreateDefaultSubobject) — aparece automáticamente como componente en
	// BP_PantheliaPlayerState, donde se le asignará su Data Asset SkillTreeInfo.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Panthelia|SkillTree")
	TObjectPtr<UPantheliaSkillTreeComponent> SkillTreeComponent;

private:
	// Nivel del jugador. Empieza en 1.
	UPROPERTY(VisibleAnywhere, Category = "Panthelia|Level")
	int32 Level = 1;

	// XP total acumulada del jugador. Empieza en 0.
	// Es acumulada: no se "gasta" al subir de nivel (modelo Dragon Age).
	UPROPERTY(VisibleAnywhere, Category = "Panthelia|Level")
	int32 XP = 0;

	// Puntos disponibles para gastar en atributos primarios (5 por nivel por defecto).
	UPROPERTY(VisibleAnywhere, Category = "Panthelia|Level")
	int32 AttributePoints = 0;

	// Puntos disponibles para gastar en el árbol de habilidades (1 por nivel por defecto).
	UPROPERTY(VisibleAnywhere, Category = "Panthelia|Level")
	int32 SkillPoints = 0;

	// Bucle reutilizable de subida de nivel. Mientras la XP acumulada supere el
	// umbral del siguiente nivel, sube uno a uno otorgando los premios de cada
	// nivel cruzado. Es la ÚNICA fuente de verdad de la subida de nivel:
	//   - AddToXP lo llama tras sumar XP (subida normal).
	//   - El objeto de reseteo (Fase 4) lo reutilizará: pondrá Level=1, vaciará
	//     las reservas y volverá a llamar aquí para re-otorgar todos los puntos.
	void UpdateLevelFromXP();

	// Mapa de EnemyID → número de veces que el jugador ha matado ese enemigo.
	// Solo contiene entradas para enemigos que ya han sido eliminados al menos una vez.
	// La ausencia de un ID equivale a kill count = 0 (primera muerte, 100% XP).
	//
	// NOTA DE PERSISTENCIA: este mapa debe guardarse/cargarse con el sistema de save.
	// Cuando ese sistema exista, RecordEnemyKill y GetEnemyKillCount deberán
	// conectarse a él. Por ahora los datos viven solo en memoria (se pierden al cerrar).
	TMap<FName, int32> EnemyKillCounts;
};