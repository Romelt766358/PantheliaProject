// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemGlobals.h"
#include "PantheliaAbilitySystemGlobals.generated.h"

/**
 * UPantheliaAbilitySystemGlobals
 *
 * Subclase del singleton de configuración global de GAS.
 * Hereda de UAbilitySystemGlobals y sobreescribe AllocGameplayEffectContext()
 * para que GAS use FPantheliaGameplayEffectContext en lugar del base.
 *
 * ¿Por qué hace falta una clase entera para esto?
 * GAS crea el FGameplayEffectContext a través de
 * UAbilitySystemGlobals::AllocGameplayEffectContext(). Si sobreescribimos
 * esa función para que devuelva nuestro tipo custom, TODOS los contexts
 * creados en el proyecto usarán automáticamente FPantheliaGameplayEffectContext.
 * Eso significa que bIsCriticalHit estará disponible en cualquier punto
 * del pipeline de daño, desde el ExecCalc hasta el AttributeSet.
 *
 * Registro: ver Config/DefaultGame.ini:
 *   [/Script/GameplayAbilities.AbilitySystemGlobals]
 *   AbilitySystemGlobalsClassName="/Script/PantheliaProject.PantheliaAbilitySystemGlobals"
 */
UCLASS()
class PANTHELIAPROJECT_API UPantheliaAbilitySystemGlobals : public UAbilitySystemGlobals
{
    GENERATED_BODY()

public:

    // Sobreescribe la función de fábrica del context.
    // Cada vez que GAS llama a MakeEffectContext() internamente,
    // acaba llamando a esta función para crear el objeto de context.
    // Devolvemos nuestro tipo custom para que bIsCriticalHit esté disponible.
    virtual FGameplayEffectContext* AllocGameplayEffectContext() const override;
};
