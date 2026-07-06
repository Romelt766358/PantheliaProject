// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "EnhancedInputComponent.h"
#include "Input/PantheliaInputConfig.h"
#include "PantheliaInputComponent.generated.h"

/**
 * UPantheliaInputComponent
 *
 * Componente de input custom que extiende UEnhancedInputComponent.
 *
 * Su única responsabilidad es BindAbilityActions: dado un InputConfig (Data Asset)
 * y tres funciones callback, bindea automáticamente cada InputAction del config
 * a esas tres funciones con su GameplayTag correspondiente como parámetro.
 *
 * Para que Unreal use este componente en lugar del default, hay que configurarlo en:
 * Project Settings > Engine > Input > Default Input Component Class → PantheliaInputComponent
 *
 * El template permite que los callbacks tengan cualquier firma, siempre que
 * acepten un FGameplayTag como último parámetro.
 */
UCLASS()
class PANTHELIAPROJECT_API UPantheliaInputComponent : public UEnhancedInputComponent
{
	GENERATED_BODY()

public:

	/**
	 * Bindea todos los InputActions del InputConfig a tres funciones callback.
	 *
	 * @param InputConfig   El Data Asset con los pares InputAction + InputTag.
	 * @param Object        El objeto dueño de los callbacks (normalmente el PlayerController).
	 * @param PressedFunc   Callback disparado UNA vez al presionar el input (ETriggerEvent::Started).
	 * @param ReleasedFunc  Callback disparado UNA vez al soltar el input (ETriggerEvent::Completed).
	 * @param HeldFunc      Callback disparado CADA FRAME mientras el input esté presionado (ETriggerEvent::Triggered).
	 *
	 * Cada callback recibe el FGameplayTag del input que lo disparó, permitiendo
	 * identificar qué tecla fue presionada sin necesidad de funciones separadas por input.
	 */
	template<class UserClass, typename PressedFuncType, typename ReleasedFuncType, typename HeldFuncType>
	void BindAbilityActions(const UPantheliaInputConfig* InputConfig, UserClass* Object,
		PressedFuncType PressedFunc, ReleasedFuncType ReleasedFunc, HeldFuncType HeldFunc)
	{
		// Si el InputConfig es null, hay un problema de configuración en el editor.
		// CastChecked en el llamador debería prevenir esto, pero por si acaso.
		check(InputConfig);

		// Iteramos cada par InputAction + InputTag registrado en el Data Asset.
		for (const FPantheliaInputAction& Action : InputConfig->AbilityInputActions)
		{
			// Saltamos entradas inválidas (InputAction nulo o tag vacío).
			if (!Action.InputAction || !Action.InputTag.IsValid()) continue;

			// --- PRESSED (Started) ---
			// Se dispara una sola vez en el primer frame que se detecta el input.
			// Útil para: activar abilities, iniciar cargas, registrar el inicio de un bloqueo.
			if (PressedFunc)
			{
				BindAction(Action.InputAction, ETriggerEvent::Started, Object, PressedFunc, Action.InputTag);
			}

			// --- RELEASED (Completed) ---
			// Se dispara una sola vez cuando se deja de presionar el input.
			// Útil para: soltar cargas, terminar bloqueos, cancelar canales.
			if (ReleasedFunc)
			{
				BindAction(Action.InputAction, ETriggerEvent::Completed, Object, ReleasedFunc, Action.InputTag);
			}

			// --- HELD (Triggered) ---
			// Se dispara CADA FRAME mientras el input esté presionado.
			// Útil para: abilities que requieren mantener presionado, cargas continuas.
			// CUIDADO: genera muchas llamadas. Usarlo solo cuando se necesite polling continuo.
			if (HeldFunc)
			{
				BindAction(Action.InputAction, ETriggerEvent::Triggered, Object, HeldFunc, Action.InputTag);
			}
		}
	}
};