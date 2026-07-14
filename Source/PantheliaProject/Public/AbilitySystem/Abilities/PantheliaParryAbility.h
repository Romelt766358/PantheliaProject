// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PantheliaGameplayAbility.h"
#include "PantheliaParryAbility.generated.h"

class UAnimMontage;

/**
 * EParryType
 * Distingue las dos defensas del jugador (modelo Lies of P):
 *  - Physical: brazo derecho / arma. Un golpe con componente fisico puede ser parry perfecto.
 *  - Magic:    brazo izquierdo / elemento. Un golpe con componente magico puede ser parry perfecto.
 * Una vez reconocido el parry perfecto, se anula el paquete ofensivo COMPLETO del golpe,
 * aunque fuera mixto. La logica de ambas defensas es identica; solo cambian los
 * GameplayTags de estado que conceden
 * y los valores del arma que consultan. Por eso una sola clase C++ parametrizada, con
 * dos Blueprints derivados (GA_Parry_Physical / GA_Parry_Magic) que setean este enum.
 */
UENUM(BlueprintType)
enum class EParryType : uint8
{
	Physical UMETA(DisplayName = "Physical (arma / brazo derecho)"),
	Magic    UMETA(DisplayName = "Magic (elemento / brazo izquierdo)")
};

/**
 * EPantheliaParryRuntimeState
 * Estado lógico de la defensa. Está separado de los Gameplay Tags porque State.Block.X
 * se concede desde el primer frame para el blend visual y no identifica por sí solo si
 * la ability sigue en ventana perfecta o ya entró en guardia sostenida.
 */
UENUM(BlueprintType)
enum class EPantheliaParryRuntimeState : uint8
{
	Inactive UMETA(DisplayName = "Inactive"),
	PerfectWindow UMETA(DisplayName = "Perfect Window"),
	SustainedGuard UMETA(DisplayName = "Sustained Guard"),
	Ending UMETA(DisplayName = "Ending")
};

/**
 * UPantheliaParryAbility
 *
 * Defensa del jugador con el sistema "perfect guard" de Lies of P. Terminologia del
 * proyecto: PARRY = bloqueo perfecto (input dentro de la ventana); BLOQUEO = bloqueo
 * imperfecto (fuera de la ventana, mientras se mantiene la guardia).
 *
 * FLUJO ACTUAL:
 *   1. Al activar: paga el coste de entrada, reproduce la guardia y abre PerfectWindow.
 *   2. Soltar durante PerfectWindow NO la cancela; solo evita la guardia sostenida.
 *   3. Si la ventana expira y el jugador sigue manteniendo el boton: entra en
 *      SustainedGuard (bloqueo imperfecto sostenido).
 *   4. Soltar durante SustainedGuard termina la ability y limpia los tags.
 *
 * ExecCalc_Damage lee los tags defensivos para decidir anulación/mitigación. El enum
 * EPantheliaParryRuntimeState es la fuente de verdad del flujo; State.Block.X también
 * existe durante PerfectWindow para sostener el blend visual y no identifica la fase.
 *
 * SISTEMA DE COSTES (dos momentos):
 *   1. Al entrar en la ventana de parry (ActivateAbility → CommitAbility):
 *      usa el Cost Gameplay Effect Class estandar de GAS (asignado en el Blueprint).
 *      Representa el esfuerzo inicial de intentar un parry.
 *   2. Al transicionar a bloqueo sostenido (OnParryWindowExpired con boton presionado):
 *      aplica BlockTransitionCostEffectClass (tambien asignado en el Blueprint).
 *      Representa el coste puntual de pasar a guardia sostenida. Si no puede pagarse,
 *      la guardia se rompe inmediatamente y reutiliza el pipeline de Stagger.
 *   3. Cada impacto defendido paga un coste calculado desde la WeaponDefinition:
 *      Normal = coste base, Heavy = coste base x multiplicador, parry perfecto = coste
 *      fijo relativo al coste base. Llegar exactamente a 0 mantiene la guardia; el
 *      siguiente impacto no pagable provoca State.GuardBroken + Stagger.
 *
 * Los Cost Gameplay Effects gobiernan entrada/transición. La WeaponDefinition gobierna
 * el coste por impacto, para que arma, perks y balance futuro compartan una sola fuente.
 */
UCLASS()
class PANTHELIAPROJECT_API UPantheliaParryAbility : public UPantheliaGameplayAbility
{
	GENERATED_BODY()

public:
	UPantheliaParryAbility();

	// Llamado por el ASC cuando el jugador suelta el boton. Durante PerfectWindow solo
	// registra el release; durante SustainedGuard termina la guardia.
	// (El sistema de input custom del proyecto no alimenta el InputReleased interno de GAS
	// de forma fiable, asi que lo notificamos explicitamente, igual que el heavy attack.)
	void NotifyBlockInputReleased();

	// Llamado por el ASC cuando el pipeline de daño detecta parry o bloqueo. Mantiene el
	// GuardLoopMontage intacto, dispara el Gameplay Cue defensivo y aplica retroceso físico
	// únicamente al bloqueo imperfecto. El parry perfecto no desplaza al jugador.
	// Una guardia rota termina la ability y deja la reacción al pipeline de Stagger.
	void NotifyParryImpact(bool bWasPerfectParry, bool bGuardBroken);

protected:
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	// Tipo de defensa: lo setea cada Blueprint derivado (Physical o Magic).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Parry")
	EParryType ParryType = EParryType::Physical;

	// Duracion de la ventana de parry (bloqueo perfecto), en segundos. Lies of P usa
	// 0.15s; nosotros 0.2s por defecto. Gancho: el arbol/corazon podra modificarla.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Parry", meta = (ClampMin = "0.0"))
	float ParryWindow = 0.2f;

	// Montage de entrada a guardia. Puede quedar sin asignar si la presentación se
	// resuelve completamente mediante GuardLoopMontage/AnimBlueprint.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Parry")
	TObjectPtr<UAnimMontage> GuardMontage;

	// Montage de la GUARDIA SOSTENIDA (pose mantenida en loop). Va en el slot 'GuardSlot',
	// que en el AnimBlueprint se mezcla solo en el tren superior (Layered Bone Blend con
	// spine_01) controlado por bIsGuarding. Asi, mientras se mantiene el bloqueo, el torso
	// hace la pose de guardia y las piernas siguen la locomocion (caminar de lock-on).
	// Se reproduce al pasar a bloqueo sostenido y se detiene al terminar la ability.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Parry")
	TObjectPtr<UAnimMontage> GuardLoopMontage;

	// Campos legacy reservados para una posible reacción aditiva futura. NotifyParryImpact
	// no salta actualmente a estas secciones, para no romper el GuardLoopMontage.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Parry")
	FName ParryHitSectionName = FName("ParryHit");

	// Sección legacy equivalente para bloqueo imperfecto. Se conserva por compatibilidad
	// con los Class Defaults existentes, pero no gobierna el flujo runtime actual.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Parry")
	FName BlockHitSectionName = FName("BlockHit");

	// Retroceso fisico al recibir un golpe en BLOQUEO imperfecto. El parry perfecto
	// no desplaza al jugador porque anula por completo la consecuencia ofensiva del golpe.
	// En cm/s.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Parry")
	float BlockKnockbackSpeed = 350.f;

	// GE que se aplica UNA SOLA VEZ al transicionar de ventana-de-parry a bloqueo sostenido.
	// Se activa en OnParryWindowExpired cuando bInputHeld es true (el jugador sigue
	// presionando el boton tras expirar la ventana). Representa el coste de mantener la
	// guardia firme cuando no se consiguio un parry perfecto.
	//
	// El primer coste (al entrar en la ventana) usa el Cost Gameplay Effect Class estandar
	// de GAS (clase base UGameplayAbility). Este es el SEGUNDO coste, separado porque ocurre
	// en un momento distinto del flujo: no al activar la ability, sino ~0.2s despues.
	//
	// ASIGNAR en: GA_Parry_Physical y GA_Parry_Magic → Class Defaults → "Combat|Parry".
	// Si no se asigna, no se aplica ningun segundo coste (comportamiento seguro por defecto).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Parry")
	TSubclassOf<UGameplayEffect> BlockTransitionCostEffectClass;

private:
	// Concede los tags de la ventana de parry (State.Parry.X) y arranca el timer.
	void EnterParryWindow();

	// Llamado por timer al expirar la ventana: si el boton sigue presionado, pasa a
	// bloqueo imperfecto sostenido (State.Block.X). Si no, termina.
	void OnParryWindowExpired();

	// Quita todos los tags de estado de parry/bloqueo que esta ability haya concedido.
	void ClearParryBlockTags();

	// Reproduce el montage de guardia sostenida (GuardLoopMontage) en el slot 'GuardSlot'.
	// Se llama al pasar a bloqueo sostenido. El montage hace loop por si mismo; lo detiene
	// EndAbility. Usa la AnimInstance directamente (Montage_Play) en vez de un AbilityTask
	// porque no necesitamos esperar su fin: la guardia la termina el input/la ventana.
	void PlayGuardLoopMontage();

	// Intenta pagar el GE de transición a guardia sostenida. Evalúa el modificador
	// de Stamina del spec para permitir que el coste deje exactamente 0. Si no puede
	// pagarse, vacía la stamina restante y dispara guardia rota/Stagger.
	bool TryPayBlockTransitionCost();

	// Aplica un pequeno retroceso fisico al personaje al recibir un golpe en bloqueo
	// imperfecto. El parry perfecto no llama este helper.
	void ApplyGuardKnockback();

	// Dispara el Gameplay Cue correspondiente al tipo de parry/bloqueo.
	// Selecciona el tag segun EParryType (Physical/Magic) y bWasPerfectParry.
	// Si el asset del Cue no existe en el editor, la llamada es silenciosa.
	void FireParryCue(bool bWasPerfectParry);

	// Devuelve el tag de ventana de parry segun ParryType (State.Parry.Physical/Magic).
	FGameplayTag GetParryStateTag() const;

	// Devuelve el tag de bloqueo sostenido segun ParryType (State.Block.Physical/Magic).
	FGameplayTag GetBlockStateTag() const;

	// True mientras el jugador mantiene presionado el boton de bloqueo.
	bool bInputHeld = false;

	// Fuente de verdad del flujo defensivo. No inferir la fase desde State.Block.X:
	// ese tag existe también durante PerfectWindow para mantener la pose upper-body.
	EPantheliaParryRuntimeState RuntimeState = EPantheliaParryRuntimeState::Inactive;

	FTimerHandle ParryWindowTimerHandle;
};
