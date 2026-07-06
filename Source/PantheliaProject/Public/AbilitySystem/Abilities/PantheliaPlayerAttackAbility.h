// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PantheliaDamageGameplayAbility.h"
#include "PantheliaPlayerAttackAbility.generated.h"

class UPantheliaWeaponDefinition;
class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;

/**
 * EPlayerAttackType
 *
 * Distingue qué cadena de combo usa esta ability: ligera o pesada.
 * Cada arma (UPantheliaWeaponDefinition) tiene un array de montages por tipo.
 */
UENUM(BlueprintType)
enum class EPlayerAttackType : uint8
{
	Light UMETA(DisplayName = "Light Attack"),
	Heavy UMETA(DisplayName = "Heavy Attack"),
};

/**
 * UPantheliaPlayerAttackAbility
 *
 * Ability de ataque cuerpo a cuerpo del JUGADOR (modelo soulslike, arma reemplazable).
 * Reemplaza al sistema legacy NO-GAS (UCombatComponent + UTraceComponent + IFighter).
 *
 * TODO viene del ARMA EQUIPADA: moveset (montages), daño, scaling y coste de stamina
 * salen del UPantheliaWeaponDefinition. La ability solo orquesta.
 *
 * COMBO CON BUFFER DE INPUT (modelo Dark Souls clásico, investigado y confirmado):
 *   - Una sola activacion maneja TODO el combo (la ability vive hasta que se rompe).
 *   - Reproduce el montage del golpe actual y queda viva.
 *   - El montage lleva un UComboWindowNotifyState que marca la ventana de encadenado.
 *   - Durante la ventana, si el jugador pulsa el ataque, se guarda UN input en buffer
 *     (maximo 1, sin importar cuantas veces pulse - regla de diseno del proyecto).
 *   - Al CERRAR la ventana: si el buffer esta activo -> encadena el siguiente golpe
 *     (reproduce el siguiente montage sin re-activar la ability); si no -> resetea el
 *     combo y termina la ability.
 *   - Si el montage termina sin ventana o sin encadenar -> termina la ability.
 *
 * Esto da la fluidez de "pulsa durante el swing y encadena", con una ventana acotada
 * (no encadena 5 segundos despues). El buffer corto (1 input) evita el problema del
 * buffer pegajoso de Elden Ring.
 *
 * CANCELACION POR DASH (futuro): la ability tendra el tag Abilities.Attack; cuando se
 * implemente la GA_Dodge, esta declarara "Cancel Abilities with Tag: Abilities.Attack"
 * y al esquivar cancelara el ataque y limpiara el buffer automaticamente.
 *
 * STAMINA (Dark Souls): coste via CostGameplayEffect. Sin stamina, CanActivateAbility
 * falla y no ataca. El valor sale del arma (Light/HeavyAttackStaminaCost).
 *
 * Los Blueprints GA_PlayerLightAttack/HeavyAttack heredan de esta y solo configuran
 * AttackType, DamageEffectClass y el GE de costo. La logica vive en C++.
 */
UCLASS()
class PANTHELIAPROJECT_API UPantheliaPlayerAttackAbility : public UPantheliaDamageGameplayAbility
{
	GENERATED_BODY()

public:
	// Bloquea la reactivacion mientras la ability ya esta activa. El input es Held
	// (cada frame), asi que sin esto el AbilityInputTagHeld reactivaba la ability en
	// cuanto habia un instante libre, reseteando el combo a 0 (bug del "solo ataque 1").
	// Con el combo viviendo en una sola activacion, las pulsaciones extra se manejan
	// por el buffer (TryBufferComboInput), NO por reactivacion.
	virtual bool CanActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	// Punto de entrada de la ability: arranca el combo en el golpe actual.
	virtual void ActivateAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	// Limpieza al terminar (resetea estado de combo y buffer).
	virtual void EndAbility(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility, bool bWasCancelled) override;

	// Llamado por el ASC cuando el jugador pulsa el ataque (una vez por pulsacion real,
	// via AbilityInputTagPressed edge-triggered, NO cada frame). Si la ventana de combo
	// esta abierta, marca el buffer (maximo 1). Sustituye al override de InputPressed,
	// que con input Held se disparaba cada frame y desincronizaba el estado.
	void TryBufferComboInput();

	// Llamados por UComboWindowNotifyState al abrir/cerrar la ventana de combo.
	// OpenComboWindow: empieza a aceptar input buffered.
	// CloseComboWindow: decide encadenar (si hay buffer) o terminar (si no).
	void OpenComboWindow();
	void CloseComboWindow();

	// Resetea el combo a 0 externamente. Llamado desde MainCharacter::ResetPlayerCombo()
	// que a su vez lo invoca el ABP en AnimNotify_ResetAttack. Aunque el nuevo sistema
	// maneja el reset internamente via CloseComboWindow, este método existe como
	// fallback de seguridad para casos donde el notify de ventana no se dispara
	// (p.ej. si se interrumpe el montage antes de que abra la ventana).
	void ResetCombo();

protected:
	// Que cadena de combo usa esta ability (ligera o pesada).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Combo")
	EPlayerAttackType AttackType = EPlayerAttackType::Light;

	// Indice actual dentro de la cadena de combo (0 = primer golpe).
	int32 ComboIndex = 0;

	// True si la ventana de combo esta abierta ahora mismo.
	bool bComboWindowOpen = false;

	// Buffer de input: true si el jugador pulso ataque durante la ventana.
	// Maximo 1 (un bool no acumula): pulsar varias veces sigue siendo un encadenado.
	bool bComboInputBuffered = false;

	// La task de montage en curso, para poder reproducir el siguiente golpe.
	UPROPERTY()
	TObjectPtr<UAbilityTask_PlayMontageAndWait> CurrentMontageTask;

	// Arranca el combo en el golpe actual. La base lo llama directamente en
	// ActivateAbility (tras CommitAbility). Las hijas (p.ej. el ataque pesado, que
	// debe decidir tap-vs-hold antes de atacar) pueden NO llamar al Super y diferir
	// el inicio hasta resolver su logica de input. Es virtual para ese fin.
	virtual void StartComboFromActivation();

	// Reproduce el montage del golpe actual (ComboIndex) y engancha sus callbacks.
	// Si no hay montage valido, termina la ability. Protected para que las hijas
	// puedan reproducir golpes del combo (el especial encadenable reutiliza esto).
	void PlayCurrentComboMontage();

	// Prepara el dano del golpe en el WeaponTraceComponent del jugador (mesh + spec).
	// Protected para que las hijas configuren el trace de sus propios montages.
	// DamageMultiplier escala el dano (1.0 = sin cambio; el pesado cargado usa >1).
	void SetupWeaponTraceForCurrentAttack(float DamageMultiplier = 1.0f);

	// Construye el spec de dano desde el arma equipada (dano + scaling + poise).
	// DamageMultiplier escala el dano final (1.0 = sin cambio; el pesado cargado usa >1).
	// Protected y con multiplicador para que el pesado cargado reutilice el pipeline.
	FGameplayEffectSpecHandle MakeWeaponDamageSpec(float DamageMultiplier = 1.0f);

	// Helper: WeaponDefinition del arma equipada del avatar. Null si no hay.
	// Protected para que las hijas lean los montages/datos del arma.
	UPantheliaWeaponDefinition* GetEquippedWeaponDefinition() const;

	// --- Reorientacion de ataque (modelo Lies of P) ---
	// Calcula hacia donde deberia mirar el ataque: con lock-on, hacia el objetivo; sin
	// lock-on, hacia la direccion del input de movimiento; si no hay input, la actual.
	FRotator GetDesiredAttackRotation() const;

	// Arranca una reorientacion suave (interpolada por timer) hacia la direccion deseada.
	// Se llama al inicio de cada golpe del combo. Protected para que el cargado pesado
	// reutilice la misma rotacion durante su fase de carga.
	void StartReorientToDesiredDirection();

	// Detiene la reorientacion en curso (limpia el timer). Seguro de llamar siempre.
	void StopReorient();

	// Velocidad de interpolacion del giro de reorientacion. Mas alto = mas rapido/seco.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Reorient")
	float ReorientInterpSpeed = 13.0f;

	// Duracion maxima (segundos) de la fase de reorientacion antes de cortar el timer.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Reorient")
	float ReorientMaxDuration = 0.18f;

private:
	// Tick interno de la interpolacion de reorientacion (llamado por timer).
	void TickReorient();

	// Estado de la reorientacion en curso.
	FRotator ReorientTargetRotation = FRotator::ZeroRotator;
	float ReorientElapsed = 0.f;
	FTimerHandle ReorientTimerHandle;

	// Intervalo del timer de reorientacion (~60fps).
	static constexpr float ReorientTickInterval = 0.016f;

	// Encadena al siguiente golpe: avanza el indice y reproduce el siguiente montage.
	void AdvanceAndPlayNext();

	// Devuelve el montage del combo actual segun arma y AttackType. Null si invalido.
	UAnimMontage* GetCurrentComboMontage() const;

	// Callbacks de la task de montage.
	UFUNCTION()
	void OnMontageCompleted();
	UFUNCTION()
	void OnMontageInterruptedOrCancelled();
};