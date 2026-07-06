// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "GameplayTagContainer.h"
#include "ActiveGameplayEffectHandle.h"
#include "PantheliaWaitCooldownChange.generated.h"

class UAbilitySystemComponent;
struct FGameplayEffectSpec;

// Delegate que broadcast el tiempo restante de un cooldown. Se convierte en un pin de
// ejecucion del nodo async en Blueprint. Es DYNAMIC porque los pins de un nodo async
// Blueprint deben serlo (Blueprint solo entiende delegates dinamicos).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FCooldownChangeSignature, float, TimeRemaining);

/**
 * UPantheliaWaitCooldownChange
 *
 * Nodo asincrono de Blueprint (NO un Ability Task) que escucha el cooldown de un hechizo
 * concreto y avisa cuando EMPIEZA (con su duracion) y cuando TERMINA.
 *
 * Diferencia clave Ability Task vs Blueprint Async Action:
 *   - Un Ability Task (UAbilityTask) solo vive DENTRO de una GameplayAbility.
 *   - Un Blueprint Async Action (UBlueprintAsyncActionBase, lo que usamos aqui) se puede
 *     usar en CUALQUIER Event Graph, en este caso el del widget del slot de hechizo del HUD.
 *
 * Por que async y no polling:
 *   En vez de preguntar cada frame "¿cuanto cooldown queda?", este nodo se suscribe a dos
 *   eventos del ASC y solo reacciona cuando algo cambia. El widget recibe la duracion al
 *   empezar el cooldown y, a partir de ahi, hace su propia cuenta atras local (mucho mas
 *   barato que consultar el ASC cada frame). Cuando el cooldown acaba, recibe el aviso de fin.
 *
 * Uso en Blueprint (futuro slot de hechizo del HUD soulslike):
 *   Event Widget Controller Set -> WaitForCooldownChange(ASC, CooldownTag)
 *     - pin CooldownStart(TimeRemaining): oscurecer el icono y arrancar la cuenta atras.
 *     - pin CooldownEnd(TimeRemaining=0): restaurar el icono.
 */
UCLASS(BlueprintType, meta = (ExposedAsyncProxy = "AsyncTask"))
class PANTHELIAPROJECT_API UPantheliaWaitCooldownChange : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()

public:
	// Pin de ejecucion que se dispara cuando el cooldown EMPIEZA.
	// Broadcast el tiempo total del cooldown para que el widget arranque su cuenta atras.
	UPROPERTY(BlueprintAssignable)
	FCooldownChangeSignature CooldownStart;

	// Pin de ejecucion que se dispara cuando el cooldown TERMINA.
	// Broadcast 0 (no queda tiempo).
	UPROPERTY(BlueprintAssignable)
	FCooldownChangeSignature CooldownEnd;

	// Factory que crea el nodo. Es la funcion que aparece como nodo en Blueprint.
	// meta=(BlueprintInternalUseOnly="true") oculta esta funcion del menu normal: solo
	// el sistema de nodos async la usa internamente para construir el proxy.
	// DefaultToSelf/HidePin no aplican aqui porque el ASC se pasa explicitamente.
	UFUNCTION(BlueprintCallable, meta = (BlueprintInternalUseOnly = "true"))
	static UPantheliaWaitCooldownChange* WaitForCooldownChange(
		UAbilitySystemComponent* AbilitySystemComponent,
		const FGameplayTag& InCooldownTag);

	// Limpia las suscripciones a delegates y marca el nodo para destruccion.
	// Se llama manualmente desde Blueprint (p. ej. en Event Destruct del widget) para
	// evitar que el nodo siga vivo y escuchando cuando el widget desaparece.
	UFUNCTION(BlueprintCallable)
	void EndTask();

protected:
	// El ASC que estamos escuchando. UPROPERTY para que el GC no lo recolecte mientras
	// el nodo este vivo.
	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> ASC;

	// El tag de cooldown concreto que este nodo escucha (p. ej. Cooldown.Spell.Fire.Firebolt).
	FGameplayTag CooldownTag;

	// Callback: el ASC lo llama cuando el CONTEO de CooldownTag cambia (se anade o se quita).
	// Firma impuesta por el delegate de RegisterGameplayTagEvent: el FGameplayTag va por
	// valor (no por referencia const), asi lo exige GAS.
	void CooldownTagChanged(const FGameplayTag InCooldownTag, int32 NewCount);

	// Callback: el ASC lo llama cuando se aplica cualquier GE basado en duracion. Lo usamos
	// para detectar el INICIO del cooldown y leer su duracion. Firma impuesta por el delegate
	// OnActiveGameplayEffectAddedDelegateToSelf.
	void OnActiveEffectAdded(UAbilitySystemComponent* TargetASC,
		const FGameplayEffectSpec& SpecApplied,
		FActiveGameplayEffectHandle ActiveEffectHandle);
};
