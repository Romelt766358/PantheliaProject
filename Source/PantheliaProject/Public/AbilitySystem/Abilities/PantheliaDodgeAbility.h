// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PantheliaGameplayAbility.h"
#include "GameplayEffectTypes.h"
#include "ScalableFloat.h"
#include "PantheliaDodgeAbility.generated.h"

class UAnimMontage;
class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitDelay;
class UAbilityTask_WaitGameplayEvent;

/**
 * EPantheliaDodgeDirection
 *
 * Dirección de ocho sectores elegida para una activación concreta del dash.
 * La clase base no decide de dónde sale esa dirección: el jugador la obtiene
 * del input y del lock-on; un boss futuro podrá obtenerla del BossBrain.
 */
UENUM(BlueprintType)
enum class EPantheliaDodgeDirection : uint8
{
	// Las cuatro direcciones originales conservan su orden para no cambiar sus
	// valores serializados si el enum ya fue usado por algún Blueprint.
	Forward       UMETA(DisplayName = "Forward"),
	Backward      UMETA(DisplayName = "Backward"),
	Left          UMETA(DisplayName = "Left"),
	Right         UMETA(DisplayName = "Right"),
	ForwardRight  UMETA(DisplayName = "Forward Right"),
	BackwardRight UMETA(DisplayName = "Backward Right"),
	BackwardLeft  UMETA(DisplayName = "Backward Left"),
	ForwardLeft   UMETA(DisplayName = "Forward Left")
};

/**
 * FPantheliaDodgeMontageData
 *
 * Une un montage de dash con la distancia de root motion con la que fue autorado.
 * La distancia final del dash puede cambiar por nivel, árbol o Corazones sin
 * reautorizar la animación: la ability calcula FinalDistance / AuthoredDistance
 * y entrega esa escala a la task del montage.
 */
USTRUCT(BlueprintType)
struct FPantheliaDodgeMontageData
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	// Distancia en centímetros que recorre el root motion del montage sin escala.
	// Debe medirse una sola vez para cada animación y anotarse aquí.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Dodge", meta = (ClampMin = "1.0"))
	float AuthoredTravelDistance = 300.f;
};

/**
 * FPantheliaDodgeRequest
 *
 * Contrato desacoplado entre quien decide la dirección y la clase base que ejecuta
 * el dash. El futuro dodge de bosses podrá construir el mismo request sin copiar
 * la lógica de coste, montage, root motion ni limpieza de i-frames.
 */
USTRUCT(BlueprintType)
struct FPantheliaDodgeRequest
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Dodge")
	EPantheliaDodgeDirection Direction = EPantheliaDodgeDirection::Backward;

	UPROPERTY(BlueprintReadOnly, Category = "Dodge")
	FVector WorldDirection = FVector::BackwardVector;

	UPROPERTY(BlueprintReadOnly, Category = "Dodge")
	FRotator DesiredRotation = FRotator::ZeroRotator;

	// Sin lock-on y con input se rota hacia la dirección elegida. Con lock-on se deja
	// en false para conservar la orientación/strafe hacia el objetivo.
	UPROPERTY(BlueprintReadOnly, Category = "Dodge")
	bool bApplyRotation = false;

	UPROPERTY(BlueprintReadOnly, Category = "Dodge")
	TObjectPtr<UAnimMontage> Montage = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Dodge")
	float AuthoredTravelDistance = 300.f;
};

/**
 * UPantheliaDodgeAbility
 *
 * Base genérica del dash de Panthelia. No lee input directamente y no contiene
 * lógica específica del jugador, de Corazones ni del árbol de habilidades.
 *
 * Responsabilidades de esta fase:
 *   - Validar el request ANTES de cobrar estamina.
 *   - Detener velocidad/input residual antes del root motion.
 *   - Escalar localmente la traslación root motion de la task del montage.
 *   - Conceder State.Invulnerable.Dodge desde un AnimNotify y guardar su handle.
 *   - Escuchar Event.Dodge.HitAvoided y confirmar un Perfect Dodge solo dentro
 *     de una ventana estrecha, separada de los i-frames generales.
 *   - Emitir Event.Dodge.Perfect y GameplayCue.Dodge.Perfect una sola vez por dash.
 *   - Limpiar tasks, ventana perfecta e i-frames en toda salida normal, cancelada
 *     o interrumpida.
 *
 * Configuración de tags y coste se hace en GA_PlayerDodge (Blueprint hijo):
 *   Ability Tag: Abilities.Dodge
 *   Activation Owned Tag: State.Dodge.Active
 *   Cancel Abilities With Tag: Abilities.Attack
 *   Input: InputTag.Dodge
 *   Cost Gameplay Effect Class: GE_Cost_Dodge
 */
UCLASS(Abstract)
class PANTHELIAPROJECT_API UPantheliaDodgeAbility : public UPantheliaGameplayAbility
{
	GENERATED_BODY()

public:
	UPantheliaDodgeAbility();

	// Lo llama UDodgeIFrameNotify en el frame exacto donde empieza la evasión.
	// El guard impide conceder dos GEs si el notify se duplica accidentalmente.
	UFUNCTION(BlueprintCallable, Category = "Combat|Dodge")
	void StartIFrames();

	// Lo llama UDodgePerfectWindowNotify. La ventana queda siempre contenida dentro
	// de los i-frames, incluso si los dos notifies se procesan en orden inverso.
	UFUNCTION(BlueprintCallable, Category = "Combat|Dodge")
	void StartPerfectDodgeWindow();

	// Punto único de extensión para árbol/Corazones. En esta fase devuelve el valor
	// base del rango de ability con clamp de seguridad.
	UFUNCTION(BlueprintPure, Category = "Combat|Dodge")
	virtual float GetFinalIFrameDuration() const;

	// Ventana estrecha que convierte un golpe evitado en Perfect Dodge. Siempre se
	// limita a la duración restante de los i-frames de esta activación.
	UFUNCTION(BlueprintPure, Category = "Combat|Dodge")
	virtual float GetFinalPerfectDodgeWindowDuration() const;

	// Punto único de extensión para modificadores futuros de distancia.
	UFUNCTION(BlueprintPure, Category = "Combat|Dodge")
	virtual float GetFinalDashDistance() const;

protected:
	virtual bool CanActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayTagContainer* SourceTags = nullptr,
		const FGameplayTagContainer* TargetTags = nullptr,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

	virtual void EndAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		bool bReplicateEndAbility,
		bool bWasCancelled) override;

	// La base proporciona un fallback hacia atrás. La subclase del jugador lo
	// reemplaza con input + lock-on; un boss futuro lo reemplazará con BossBrain.
	virtual bool BuildDodgeRequest(FPantheliaDodgeRequest& OutRequest) const;

	// Duración base de i-frames por rango de GA_Dodge.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Dodge|IFrames")
	FScalableFloat BaseIFrameDuration;

	// Cap de i-frames por rango. Es escalable para permitir variantes distintas sin
	// recompilar, aunque en esta fase normalmente se deja constante en 1.0 s.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Dodge|IFrames")
	FScalableFloat MaxIFrameDuration;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Dodge|IFrames", meta = (ClampMin = "0.01"))
	float MinIFrameDuration = 0.05f;

	// Duración base de la ventana de Perfect Dodge. El valor inicial (0.12 s) es
	// deliberadamente menor que los i-frames generales para premiar el timing preciso.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Dodge|PerfectDodge")
	FScalableFloat BasePerfectDodgeWindowDuration;

	// Cap de balance de la ventana perfecta. El getter también la limita a la
	// duración total/restante de los i-frames, por lo que nunca puede sobresalir.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Dodge|PerfectDodge")
	FScalableFloat MaxPerfectDodgeWindowDuration;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Dodge|PerfectDodge", meta = (ClampMin = "0.001"))
	float MinPerfectDodgeWindowDuration = 0.01f;

	// Distancia final deseada en centímetros. La task escala el root motion del montage
	// usando esta distancia y AuthoredTravelDistance.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Dodge|Movement")
	FScalableFloat BaseDashDistance;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Dodge|Montages")
	FPantheliaDodgeMontageData DodgeForward;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Dodge|Montages")
	FPantheliaDodgeMontageData DodgeBackward;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Dodge|Montages")
	FPantheliaDodgeMontageData DodgeLeft;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Dodge|Montages")
	FPantheliaDodgeMontageData DodgeRight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Dodge|Montages")
	FPantheliaDodgeMontageData DodgeForwardRight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Dodge|Montages")
	FPantheliaDodgeMontageData DodgeBackwardRight;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Dodge|Montages")
	FPantheliaDodgeMontageData DodgeBackwardLeft;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Combat|Dodge|Montages")
	FPantheliaDodgeMontageData DodgeForwardLeft;

private:
	UFUNCTION()
	void OnDodgeMontageCompleted();

	UFUNCTION()
	void OnDodgeMontageInterruptedOrCancelled();

	UFUNCTION()
	void OnHitAvoidedEventReceived(FGameplayEventData Payload);

	UFUNCTION()
	void OnPerfectDodgeWindowFinished();

	void OpenPerfectDodgeWindow();
	void ConfirmPerfectDodge(const FGameplayEventData& RawPayload);
	void ClearIFrames();
	void ClearMontageTask();
	void ClearHitAvoidedEventTask();
	void ClearPerfectDodgeWindowTask();

	// Guard runtime por activación para que un notify duplicado no conceda dos efectos.
	bool bIFramesStarted = false;

	// La petición permite que ambos notifies compartan timestamp sin depender del orden
	// interno en que Unreal los procese. La ventana solo abre tras iniciar i-frames.
	bool bPerfectDodgeWindowRequested = false;
	bool bPerfectDodgeWindowStarted = false;
	bool bPerfectDodgeWindowOpen = false;

	// Un dash puede anular varios impactos, pero solo confirma un Perfect Dodge.
	bool bPerfectDodgeTriggered = false;

	// Tiempo de inicio de los i-frames para recortar la ventana perfecta si su notify
	// se coloca más tarde durante el montage.
	float IFramesStartTimeSeconds = 0.f;

	// Handle exacto del GE que concedió los i-frames de ESTA activación.
	FActiveGameplayEffectHandle IFramesEffectHandle;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_PlayMontageAndWait> CurrentMontageTask = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitGameplayEvent> HitAvoidedEventTask = nullptr;

	UPROPERTY(Transient)
	TObjectPtr<UAbilityTask_WaitDelay> PerfectDodgeWindowTask = nullptr;
};
