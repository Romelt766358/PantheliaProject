// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/PantheliaCharacterBase.h"
#include "GameFramework/Character.h"
#include "Interfaces/MainPlayer.h"
#include "Interfaces/PantheliaPlayerInterface.h"
#include "MainCharacter.generated.h"

// Forward declarations de componentes.
// CombatComponent y TraceComponent (melee legacy NO-GAS) fueron retirados en la
// migración del ataque básico a GAS (Parte 5). El combate del jugador ahora usa
// UPantheliaPlayerAttackAbility + UWeaponTraceComponent + UPantheliaEquipmentComponent.
class ULockonComponent;
class UPlayerActionsComponent;
class UPantheliaEquipmentComponent;
class UWeaponTraceComponent;
// Cámara y efectos — añadidos en clase 262 para orientar el Niagara de level up
// hacia la cámara desde C++. La cámara se migra de Blueprint a C++ para poder
// leer su posición en LevelUp_Implementation sin necesidad de multicast (sin multijugador).
class UCameraComponent;
class USpringArmComponent;
class UNiagaraComponent;

// ICombatInterface se elimina de esta lista porque ahora viene de APantheliaCharacterBase.
// UHT no permite declarar la misma interfaz dos veces en la cadena de herencia.
// GetPlayerLevel() sigue siendo override válido porque ICombatInterface sigue siendo base,
// solo que ahora llega a través de APantheliaCharacterBase.
// IFighter se retiró en la Parte 5: era la interfaz del daño melee legacy (GetDamage),
// reemplazada por el daño GAS del arma (UPantheliaWeaponDefinition + spec del WeaponTrace).
UCLASS()
class PANTHELIAPROJECT_API AMainCharacter : public APantheliaCharacterBase, public IMainPlayer, public IPantheliaPlayerInterface
{
	GENERATED_BODY()

public:
	AMainCharacter();

	// Para el Servidor
	virtual void PossessedBy(AController* NewController) override;

	// Para el Cliente
	virtual void OnRep_PlayerState() override;

	// CombatComponent y TraceComponent (melee legacy NO-GAS) retirados en la Parte 5.

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat", meta = (AllowPrivateAccess = "true"))
	ULockonComponent* LockonComponent;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Actions", meta = (AllowPrivateAccess = "true"))
	UPlayerActionsComponent* PlayerActionsComponent;

	// Sistema de equipo nuevo (GAS-ready). Gestiona el arma equipada del jugador.
	// Reemplazará a EquippedWeapon (legacy ABaseWeapon) cuando se retire el sistema
	// antiguo en la Parte 5 de la migración. La ability UPantheliaPlayerAttackAbility
	// consulta este componente para obtener el arma y su WeaponDefinition.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Equipment")
	UPantheliaEquipmentComponent* EquipmentComponent;

	// Sistema de trace de arma nuevo (GAS-ready, sweep por sockets de la hoja).
	// Reemplaza al UTraceComponent legacy. La ability de ataque del jugador le pasa
	// el mesh del arma equipada (SetWeaponMeshComponent) y el spec de daño
	// (SetDamageSpec); el WeaponTraceNotifyState del montage abre/cierra la ventana.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Combat")
	UWeaponTraceComponent* WeaponTraceComponent;

	// EquippedWeapon (legacy ABaseWeapon) retirado en la Parte 5. El arma del jugador
	// ahora la gestiona EquipmentComponent (spawnea un APantheliaWeapon data-driven).

	// Boom (brazo) de la cámara en tercera persona. Migrado de Blueprint a C++ en
	// clase 262 para que LevelUp_Implementation pueda leer la posición de la cámara
	// sin depender de un Multicast RPC (Panthelia no tiene multijugador).
	// Configura TargetArmLength, lags y rotación en Class Defaults del Blueprint.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	USpringArmComponent* CameraBoom;

	// Componente de cámara en tercera persona. Se adjunta al socket final del CameraBoom.
	// bUsePawnControlRotation = false (el boom ya gestiona la rotación).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	UCameraComponent* PlayerCameraComponent;

	// Componente Niagara para el efecto de partículas de subida de nivel.
	// bAutoActivate = false: se activa manualmente en LevelUp_Implementation.
	// Asigna el Niagara System en Class Defaults del Blueprint
	// (NS_LevelUp: /Game/TestAssets/Effects/LevelUp/NS_LevelUp).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Effects")
	UNiagaraComponent* LevelUpNiagaraComponent;

	/** ICombatInterface */
	virtual int32 GetPlayerLevel_Implementation() const override;
	/** End ICombatInterface */

	/** IPantheliaPlayerInterface */
	virtual void AddToXP_Implementation(int32 InXP) override;
	virtual void LevelUp_Implementation() override;
	virtual int32 GetXP_Implementation() const override;
	virtual int32 FindLevelForXP_Implementation(int32 InXP) const override;
	virtual int32 GetAttributePointsReward_Implementation(int32 Level) const override;
	virtual int32 GetSkillPointsReward_Implementation(int32 Level) const override;
	virtual int32 GetAttributePoints_Implementation() const override;
	virtual int32 GetSkillPoints_Implementation() const override;
	virtual void AddToPlayerLevel_Implementation(int32 InLevel) override;
	virtual void AddToAttributePoints_Implementation(int32 InAttributePoints) override;
	virtual void AddToSkillPoints_Implementation(int32 InSkillPoints) override;
	/** End IPantheliaPlayerInterface */

	// Resetea el combo del jugador buscando la ability de ataque activa en el ASC.
	// Llamado desde ABP_Player en el AnimNotify_ResetAttack (fin de ventana de combo).
	// Evita buscar el nodo "Get Active Abilities by Class" en Blueprint, que no es
	// fácil de localizar en UE5. La ability es Instanced Per Actor, por lo que existe
	// aunque no esté activa en este momento — el ComboIndex se resetea entre golpes.
	UFUNCTION(BlueprintCallable, Category = "Combat")
	void ResetPlayerCombo();

protected:
	virtual void BeginPlay() override;

	UPROPERTY(BlueprintReadOnly, Category = "Animations")
	class UPlayerAnimInstance* PlayerAnim;

	// WeaponClass (legacy) retirado: el arma inicial se configura en EquipmentComponent.

	// Abilities pasivas que se otorgan Y activan automáticamente al inicio.
	// No tienen input — corren en el servidor durante toda la sesión.
	// Actualmente: GA_ListenForXPEvents (escucha muertes y acredita XP al jugador).
	// En el futuro: buffs de corazón elemental, listeners de eventos de zona, etc.
	// Configurar en BP_ThirdPersonCharacter → Class Defaults → Startup Passive Abilities.
	UPROPERTY(EditDefaultsOnly, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupPassiveAbilities;

public:
	virtual void Tick(float DeltaTime) override;
	virtual void SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent) override;

private:
	virtual void InitAbilityActorInfo() override;

	// Handler enganchado a PS->OnLevelChangedDelegate (ver InitAbilityActorInfo en el .cpp).
	// Se limita a llamar RefreshSecondaryAttributes() — ver la explicación extendida de esa
	// función en PantheliaCharacterBase.h para entender por qué es necesaria: MaxHealth/
	// MaxMana/MaxStamina dependen del nivel del personaje dentro de su cálculo en C++ (MMC),
	// pero GAS no vigila el nivel como un atributo, así que no se recalculan solos al subir
	// de nivel — hay que forzarlo explícitamente aquí.
	void OnPlayerLevelChanged(int32 NewLevel);

	// === PROTECCIÓN CONTRA DOBLE INICIALIZACIÓN (ver explicación en el .cpp) ===
	//
	// Unreal llama a InitAbilityActorInfo() desde DOS sitios distintos:
	// PossessedBy() y OnRep_PlayerState(). Esta bandera evita ejecutar dos veces el
	// cableado de ESTE Pawn (asignar punteros, conectar HUD, suscribir delegates).
	//
	// DIVISIÓN DE RESPONSABILIDADES (Etapa 4) — hay DOS guardas que responden preguntas
	// distintas, y conviene no confundirlas:
	//   - ESTA bandera (por-Pawn): "¿este CUERPO ya hizo su cableado?" Vive y muere con
	//     el Pawn. Protege contra la doble llamada PossessedBy + OnRep_PlayerState
	//     sobre el mismo Pawn.
	//   - bAttributesInitialized (en el ASC, persistente): "¿los DATOS de GAS ya se
	//     inicializaron alguna vez en esta partida?" Sobrevive al respawn junto al ASC
	//     del PlayerState. Protege contra reaplicar los GEs de atributos por defecto
	//     (y perder puntos gastados) cuando un Pawn NUEVO se inicialice tras la muerte.
	// Gracias a la segunda guarda, que un Pawn nuevo re-ejecute InitAbilityActorInfo
	// completo en el respawn es CORRECTO y necesario (hay que fijar el nuevo Avatar);
	// las partes que no deben repetirse se protegen solas en el ASC/CharacterBase.
	//
	// bAbilityActorInfoInitialized empieza en false y se pone a true la primera vez
	// que InitAbilityActorInfo() completa su trabajo. Las llamadas siguientes se ignoran.
	//
	// NOTA HONESTA: se confirmó con pruebas que esta doble llamada NO era la causa del bug
	// de "atributos secundarios saltan una sola vez al gastar el primer punto" — esa causa
	// real es la que describe RefreshSecondaryAttributes() en PantheliaCharacterBase.h.
	bool bAbilityActorInfoInitialized = false;
};