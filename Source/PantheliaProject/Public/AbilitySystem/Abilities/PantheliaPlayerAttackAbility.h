// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PantheliaDamageGameplayAbility.h"
#include "PantheliaPlayerAttackAbility.generated.h"

class UPantheliaWeaponDefinition;
class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
struct FPantheliaWeaponAttackModifiers;

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

	// Coste dinámico de ataque obtenido del arma equipada.
	//
	// CheckCost valida el coste antes del commit y permite que GAS comunique
	// correctamente un fallo por stamina insuficiente.
	virtual bool CheckCost(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	// Construye el spec del Cost Gameplay Effect e inyecta Cost.Stamina mediante
	// SetByCaller. El valor se aplica negativo porque el modificador del GE usa Add.
	virtual void ApplyCost(const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

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

	// Contexto consumido del ASC al comenzar esta activación. Solo DodgeFollowup altera
	// el montage de apertura; después del primer golpe, el combo vuelve a su cadena normal.
	EPantheliaAttackEntryContext CurrentAttackEntryContext =
		EPantheliaAttackEntryContext::Normal;

	// True únicamente mientras el montage especial post-dodge es el golpe actual.
	// Al encadenar, se limpia antes de saltar al índice 1 de la cadena normal.
	bool bUsingDodgeFollowupOpeningMontage = false;

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

	// Permite a la ability pesada saltarse tap-vs-hold cuando la entrada proviene del
	// dodge. El contexto ya fue consumido del ASC y permanece estable durante la activación.
	bool IsDodgeFollowupEntry() const
	{
		return CurrentAttackEntryContext == EPantheliaAttackEntryContext::DodgeFollowup;
	}

	// Valida que la activación tenga al menos un montage ejecutable ANTES de cobrar
	// stamina. Las hijas pueden ampliar la validación si ofrecen rutas alternativas
	// (por ejemplo, el heavy cargado además de su cadena normal).
	virtual bool HasValidActivationMontage() const;

	// Reproduce el montage del golpe actual (ComboIndex) y engancha sus callbacks.
	// Si no hay montage valido, termina la ability. Protected para que las hijas
	// puedan reproducir golpes del combo (el especial encadenable reutiliza esto).
	void PlayCurrentComboMontage();

	// Prepara el dano del golpe en el WeaponTraceComponent del jugador (mesh + spec).
	// Protected para que las hijas configuren el trace de sus propios montages.
	// AttackModifiers separa daño a HP, postura y buildup por categoría de ataque.
	void SetupWeaponTraceForCurrentAttack(
		const FPantheliaWeaponAttackModifiers& AttackModifiers);

	// Construye el spec de dano desde el arma equipada (dano + scaling + poise).
	// Cada canal usa su multiplicador independiente del perfil ofensivo recibido.
	// Protected para que el pesado cargado reutilice exactamente el mismo pipeline.
	FGameplayEffectSpecHandle MakeWeaponDamageSpec(
		const FPantheliaWeaponAttackModifiers& AttackModifiers);

	// Copia los datos de daño del arma equipada a las propiedades heredadas de la
	// ability para que el pipeline compartido ApplyDamageScalingToSpec los use.
	//
	// Este helper existe para mantener un único punto de copia. Antes, los campos
	// se copiaban manualmente dentro de MakeWeaponDamageSpec y cualquier propiedad
	// nueva del arma podía quedar desconectada silenciosamente.
	//
	// El nombre está limitado a DamageData a propósito: knockback, launch, impulso
	// de muerte, Heridas Graves y otros parámetros de la clase base no pertenecen
	// automáticamente al arma y no deben copiarse aquí.
	void ApplyWeaponDamageDataToAbility(const UPantheliaWeaponDefinition* WeaponDef);

	// Intenta obtener el coste del golpe desde el WeaponDefinition actual.
	// Devuelve false cuando no existe un arma válida o su Definition no está
	// disponible. No confundir "sin arma" con un coste legítimo de cero.
	bool TryGetCurrentAttackStaminaCost(float& OutCost) const;

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
