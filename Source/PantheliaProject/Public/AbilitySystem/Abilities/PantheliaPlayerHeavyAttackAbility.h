// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PantheliaPlayerAttackAbility.h"
#include "PantheliaPlayerHeavyAttackAbility.generated.h"

class UAbilityTask_PlayMontageAndWait;


/**
 * UPantheliaPlayerHeavyAttackAbility
 *
 * Ataque del boton PESADO del jugador, con mecanica TAP-vs-HOLD (modelo Lies of P /
 * Elden Ring). Hereda de UPantheliaPlayerAttackAbility para REUTILIZAR todo el sistema
 * de combo (el ataque especial encadenable usa el mismo motor que el ligero).
 *
 * COMPORTAMIENTO (segun el estandar de Elden Ring, investigado):
 *   - TAP (pulsacion corta, soltar antes del umbral): ataque ESPECIAL, encadenable
 *     hasta 2 veces (tap-tap = 2 especiales en cadena). Reutiliza el combo de la base
 *     con AttackType=Heavy, que lee HeavyAttackMontages del arma.
 *   - HOLD (mantener mas del umbral): ataque PESADO CARGADO, golpe unico mas potente
 *     (sin cadena), usando ChargedHeavyMontage del arma y ChargedHeavyDamageMultiplier.
 *
 * COMO SE DISTINGUE TAP DE HOLD:
 *   Al activarse, NO ataca de inmediato. Arranca un temporizador (HoldThreshold) y
 *   espera el release (notificado por el ASC). El que ocurra primero decide:
 *     - Si suelta ANTES del umbral -> TAP -> ataque especial (combo).
 *     - Si el umbral se cumple primero (sigue presionando) -> HOLD -> pesado cargado.
 *   El especial sale al SOLTAR (necesario para poder distinguir tap de hold). Es el
 *   modelo de Elden Ring: el juego espera para ver si cargas.
 *
 * CADENAS SEPARADAS: el heavy NO encadena desde el light (estandar souls). Son
 * secuencias independientes. El light y el heavy son abilities distintas que se
 * cancelan mutuamente si hace falta (configurable con tags en el futuro).
 *
 * El tag Abilities.Attack (heredado) permite que la futura GA_Dodge cancele tambien
 * este ataque.
 */
UCLASS()
class PANTHELIAPROJECT_API UPantheliaPlayerHeavyAttackAbility : public UPantheliaPlayerAttackAbility
{
	GENERATED_BODY()

public:
	UPantheliaPlayerHeavyAttackAbility();

	// Llamado por el ASC cuando se suelta el boton de ataque pesado. Decide tap-vs-hold:
	// si se suelta antes del umbral -> especial; si ya se cumplio el umbral -> ya salio
	// el cargado. Sustituye a WaitInputRelease (que no detectaba el release porque el
	// sistema de input custom del proyecto no alimenta el estado interno de GAS).
	void NotifyHeavyInputReleased();

protected:
	// Sobrescribe el arranque: en vez de atacar de inmediato (como el ligero), inicia
	// la deteccion tap-vs-hold (WaitInputRelease + temporizador de umbral).
	virtual void StartComboFromActivation() override;

	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	// Umbral en segundos para distinguir tap de hold. Por debajo = tap (especial);
	// por encima = hold (pesado cargado). 0.25s es el estandar perceptible.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Heavy")
	float HoldThreshold = 0.25f;

	// Nombres de las secciones del ChargedHeavyMontage. Configurables por si el montage
	// usa otros nombres. Por defecto: Start (entrada), Loop (pose sostenida), Release (golpe).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Heavy")
	FName ChargeStartSectionName = FName("Start");

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Heavy")
	FName ChargeReleaseSectionName = FName("Release");

	// Intervalo del timer de rotacion durante la carga (~60fps).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Heavy")
	float ChargeRotateInterval = 0.05f;

private:
	// True mientras el boton de ataque pesado sigue presionado. Lo baja el ASC al soltar.
	bool bInputHeld = false;

	// True una vez que ya decidimos (tap o hold), para ignorar callbacks tardios.
	bool bDecisionMade = false;

	// True mientras estamos en la fase de carga sostenida (loop), antes de soltar el golpe.
	bool bIsCharging = false;

	// Task del montage del cargado en curso (para saltar de seccion al soltar).
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> ChargedMontageTask;

	// Timer de la rotacion continua durante la carga.
	FTimerHandle ChargeRotateTimerHandle;

	// Llamado por un timer cuando se cumple HoldThreshold sin soltar -> hold (cargado).
	void OnHoldThresholdReached();

	// Ejecuta el ataque especial (tap): arranca el combo de la base (AttackType=Heavy).
	void ExecuteSpecialAttack();

	// Ejecuta el pesado cargado (hold): reproduce ChargedHeavyMontage con multiplicador.
	// Arranca en la seccion "Start", fluye al "Loop" sostenido (carga), y al soltar
	// salta a "Release" (el golpe). Durante el loop, rota hacia el lock-on/input.
	void ExecuteChargedAttack();

	// Lanza el golpe cargado: salta a la seccion "Release" del montage. Llamado al soltar
	// el boton mientras se carga.
	void ReleaseCharge();

	// Callback del montage del cargado: al terminar, acaba la ability (no encadena).
	UFUNCTION()
	void OnChargedMontageEnded();

	// Tick (por timer) que reorienta hacia el lock-on/input mientras se sostiene la carga.
	void TickChargeRotation();

	// Handle del temporizador del umbral de hold.
	FTimerHandle HoldTimerHandle;
};