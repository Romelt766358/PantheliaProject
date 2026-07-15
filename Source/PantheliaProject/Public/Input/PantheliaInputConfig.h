// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "PantheliaInputConfig.generated.h"

class UInputAction;

// Struct que vincula una InputAction con un GameplayTag.
// El GameplayTag identifica semánticamente el input (ej: "InputTag.LightAttack")
// sin importar qué tecla física esté asignada en el IMC.
// Esto nos permite cambiar keybindings en runtime sin tocar las abilities.
USTRUCT(BlueprintType)
struct FPantheliaInputAction
{
	GENERATED_BODY()

	// La InputAction de Enhanced Input. Se asigna desde el Data Asset en el editor.
	UPROPERTY(EditDefaultsOnly)
	TObjectPtr<const UInputAction> InputAction = nullptr;

	// El tag semántico que identifica este input.
	// Todas las abilities buscan este tag para saber si deben activarse.
	UPROPERTY(EditDefaultsOnly)
	FGameplayTag InputTag = FGameplayTag();
};

/**
 * UPantheliaInputConfig
 *
 * Data Asset que vincula InputActions de Enhanced Input con GameplayTags.
 * Actúa como tabla de búsqueda: dado un InputTag, devuelve su InputAction.
 *
 * Uso: crear un Blueprint de este asset (DA_InputConfig) en el editor,
 * rellenar AbilityInputActions con cada par InputAction + Tag,
 * y asignar ese asset al PlayerController o al Character.
 */
UCLASS()
class PANTHELIAPROJECT_API UPantheliaInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	// Busca y devuelve el InputAction asociado al InputTag dado.
	// Retorna nullptr si no encuentra ninguno.
	// Si bLogNotFound es true y no lo encuentra, loguea un error en consola.
	const UInputAction* FindAbilityInputActionForTag(const FGameplayTag& InputTag, bool bLogNotFound = false) const;

	// Lista de pares InputAction + InputTag que definen todos los inputs de habilidad.
	// Se rellena desde el Blueprint del Data Asset (DA_InputConfig).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FPantheliaInputAction> AbilityInputActions;

#if WITH_EDITOR
	// Valida las invariantes del asset mediante Data Validation del editor.
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
