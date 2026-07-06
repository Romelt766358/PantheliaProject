// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/PantheliaAbilitySystemGlobals.h"
#include "AbilitySystem/PantheliaAbilityTypes.h"

FGameplayEffectContext* UPantheliaAbilitySystemGlobals::AllocGameplayEffectContext() const
{
    // Devolvemos una nueva instancia de nuestro context custom.
    // Aunque el return type es FGameplayEffectContext*, devolver
    // FPantheliaGameplayEffectContext* es válido (herencia pública).
    // A partir de aquí, TODOS los contexts del proyecto tendrán
    // bIsCriticalHit disponible — ExecCalc puede escribirlo,
    // PostGameplayEffectExecute puede leerlo.
    return new FPantheliaGameplayEffectContext();
}
