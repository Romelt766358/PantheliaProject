// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PantheliaGameplayAbility.h"
#include "PantheliaParryAbility.generated.h"

class UAnimMontage;

/**
 * EParryType
 * Distingue las dos defensas del jugador (modelo Lies of P):
 *  - Physical: brazo derecho / arma. Parry perfecto anula dano fisico y rompe postura.
 *  - Magic:    brazo izquierdo / elemento. Parry perfecto anula dano magico y efectos.
 * La logica de ambas es identica; solo cambian los GameplayTags de estado que conceden
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
 * UPantheliaParryAbility
 *
 * Defensa del jugador con el sistema "perfect guard" de Lies of P. Terminologia del
 * proyecto: PARRY = bloqueo perfecto (input dentro de la ventana); BLOQUEO = bloqueo
 * imperfecto (fuera de la ventana, mientras se mantiene la guardia).
 *
 * FLUJO (Fase 1 — solo estado y animacion, sin mitigacion de dano todavia):
 *   1. Al activar: reproduce el montage de guardia y concede el tag State.Parry.X
 *      durante ParryWindow segundos (0.2 por defecto, ajustable).
 *   2. Si la ventana expira y el jugador SIGUE mateniendo el boton: cambia a
 *      State.Block.X (bloqueo imperfecto sostenido).
 *   3. Al soltar el boton: termina la ability (quita los tags de estado).
 *
 * El ExecCalc del dano entrante (Fase 2) leera estos tags de estado del defensor para
 * decidir anular/mitigar el dano. La reaccion (dano de postura al enemigo, efectos
 * elementales) vendra en la Fase 3.
 *
 * SISTEMA DE COSTES (dos momentos):
 *   1. Al entrar en la ventana de parry (ActivateAbility → CommitAbility):
 *      usa el Cost Gameplay Effect Class estandar de GAS (asignado en el Blueprint).
 *      Representa el esfuerzo inicial de intentar un parry.
 *   2. Al transicionar a bloqueo sostenido (OnParryWindowExpired con boton presionado):
 *      aplica BlockTransitionCostEffectClass (tambien asignado en el Blueprint).
 *      Representa el coste de mantener la guardia firme cuando no pudiste parry.
 *      Es un coste PUNTUAL (no drenaje continuo). El drenaje continuo por bloqueo
 *      se dejara para cuando el sistema de stamina este mas maduro.
 *
 * El coste de estamina sale del arma equipada (ParryStaminaCost / BlockStaminaCost).
 */
UCLASS()
class PANTHELIAPROJECT_API UPantheliaParryAbility : public UPantheliaGameplayAbility
{
	GENERATED_BODY()

public:
	UPantheliaParryAbility();

	// Llamado por el ASC cuando el jugador suelta el boton de bloqueo. Termina la guardia.
	// (El sistema de input custom del proyecto no alimenta el InputReleased interno de GAS
	// de forma fiable, asi que lo notificamos explicitamente, igual que el heavy attack.)
	void NotifyBlockInputReleased();

	// Llamado por el ASC cuando el sistema de dano detecta que esta ability paro un golpe.
	// Reproduce el retroceso correspondiente saltando a la seccion del montage de guardia:
	//   - bWasPerfectParry = true  -> seccion ParryHitSectionName (retroceso de parry)
	//   - bWasPerfectParry = false -> seccion BlockHitSectionName (retroceso de bloqueo)
	// Si el GuardMontage o la seccion no existen, solo loguea (no rompe el flujo).
	void NotifyParryImpact(bool bWasPerfectParry);

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

	// Montage de guardia (entrar en pose de bloqueo). Opcional en Fase 1.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Parry")
	TObjectPtr<UAnimMontage> GuardMontage;

	// Montage de la GUARDIA SOSTENIDA (pose mantenida en loop). Va en el slot 'GuardSlot',
	// que en el AnimBlueprint se mezcla solo en el tren superior (Layered Bone Blend con
	// spine_01) controlado por bIsGuarding. Asi, mientras se mantiene el bloqueo, el torso
	// hace la pose de guardia y las piernas siguen la locomocion (caminar de lock-on).
	// Se reproduce al pasar a bloqueo sostenido y se detiene al terminar la ability.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Parry")
	TObjectPtr<UAnimMontage> GuardLoopMontage;

	// Nombre de la seccion del GuardMontage a la que se salta cuando el defensor logra un
	// PARRY PERFECTO y recibe el impacto (retroceso de parry). Configurable por si el
	// montage usa otro nombre. Si la seccion no existe, NotifyParryImpact solo loguea.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Parry")
	FName ParryHitSectionName = FName("ParryHit");

	// Nombre de la seccion del GuardMontage a la que se salta cuando el defensor BLOQUEA
	// (imperfecto) y recibe el impacto (retroceso de bloqueo).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Parry")
	FName BlockHitSectionName = FName("BlockHit");

	// Retroceso fisico (knockback) al recibir un golpe en PARRY perfecto. Es el empujon
	// hacia atras que da feel de impacto (estilo Lies of P / Sekiro). Mas fuerte que el de
	// bloqueo porque un parry perfecto repele con mas energia. En cm/s (velocidad de launch).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Parry")
	float ParryKnockbackSpeed = 600.f;

	// Retroceso fisico al recibir un golpe en BLOQUEO imperfecto. Mas suave que el parry:
	// el bloqueo absorbe, no repele. En cm/s.
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

	// Aplica un pequeno retroceso fisico (knockback) al personaje hacia atras, para dar
	// feel de impacto al recibir un golpe en guardia/parry (estilo Lies of P / Sekiro).
	// La direccion es opuesta a hacia donde mira el personaje. La fuerza la decide
	// bWasPerfectParry (ParryKnockbackSpeed vs BlockKnockbackSpeed).
	void ApplyGuardKnockback(bool bWasPerfectParry);

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

	FTimerHandle ParryWindowTimerHandle;
};