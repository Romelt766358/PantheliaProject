// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif
#include "PantheliaAbilityInfo.generated.h"

// Struct que agrupa la información de una habilidad/hechizo para la UI.
// El WidgetController lo broadcastea cuando necesita mostrar o actualizar
// el slot de un hechizo (icono, fondo, input asignado, cooldown tag).
//
// Convención de nombrado (IMPORTANTE):
//   El struct se llama FPantheliaAbilityInfo y la clase UPantheliaAbilityInfoAsset.
//   El sufijo "Asset" en la clase es intencional: UHT prohíbe que un UCLASS y un
//   USTRUCT compartan el mismo nombre base (sin prefijo U/F). Sin el sufijo,
//   ambos tendrían la base "PantheliaAbilityInfo" y el compilador lanzaría error.
//   Este es el mismo patrón que usa FPantheliaAttributeInfo + UPantheliaAttributeInfoAsset.
USTRUCT(BlueprintType)
struct FPantheliaAbilityInfo
{
	GENERATED_BODY()

	// Tag que identifica la habilidad. Se usa para hacer el lookup en el DA
	// y para que cada slot del HUD sepa si este broadcast le corresponde.
	// Se asigna en el editor del Data Asset — es la clave primaria de la entrada.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	FGameplayTag AbilityTag = FGameplayTag();

	// Tag del input al que está asignada esta habilidad en este momento
	// (p. ej. InputTag.Spell.1, InputTag.Spell.Ultimate).
	// NO se asigna en el editor del Data Asset: es información runtime que cambia
	// si el jugador cicla los hechizos o cambia el equipamiento. Se fija desde código
	// al inicializar o reasignar la habilidad.
	UPROPERTY(BlueprintReadOnly, Category = "Ability")
	FGameplayTag InputTag = FGameplayTag();

	// Tag de cooldown único de esta habilidad (p. ej. Cooldown.Spell.Fire.Firebolt).
	// SÍ se asigna en el editor del Data Asset porque es estático: una habilidad
	// siempre usa el mismo cooldown tag, a diferencia del InputTag que puede cambiar.
	// El slot del HUD lo pasa al nodo async WaitForCooldownChange para saber
	// cuándo oscurecer el icono y cuándo restaurarlo.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability")
	FGameplayTag CooldownTag = FGameplayTag();

	// Icono de la habilidad. Se muestra en el slot del HUD y (futuro) en el árbol.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TObjectPtr<const UTexture2D> Icon = nullptr;

	// Material de fondo del slot de habilidad en el HUD.
	// Permite diferenciar visualmente hechizos de fuego, agua, etc. por su fondo.
	// Útil incluso sin los spell globes del curso: en Panthelia irá en el slot
	// de hechizo equipado (esquina del HUD, estilo Elden Ring).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "UI")
	TObjectPtr<const UMaterialInterface> BackgroundMaterial = nullptr;
};

// Data Asset con la información de todas las habilidades/hechizos del juego.
// Permite buscar un FPantheliaAbilityInfo por GameplayTag.
//
// Flujo de uso:
//   1. En el editor, crea DA_AbilityInfo desde este asset.
//   2. Añade una entrada por cada hechizo, asignando AbilityTag, CooldownTag,
//      Icon y BackgroundMaterial.
//   3. Asigna DA_AbilityInfo a la propiedad AbilityInfo en BP_OverlayWidgetController.
//   4. Cuando el WidgetController necesite mostrar un hechizo, llama a FindAbilityInfoForTag.
UCLASS()
class PANTHELIAPROJECT_API UPantheliaAbilityInfoAsset : public UDataAsset
{
	GENERATED_BODY()

public:

	// Array con la información de todas las habilidades del juego.
	// Se rellena desde el Blueprint del Data Asset en el editor.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Ability Information")
	TArray<FPantheliaAbilityInfo> AbilityInformation;

	// Busca en el array la entrada cuyo AbilityTag coincida con el tag dado.
	// bLogNotFound: si es true y no encuentra el tag, loguea un error.
	// Devuelve un struct vacío (con tags inválidos y punteros nullptr) si no lo encuentra.
	FPantheliaAbilityInfo FindAbilityInfoForTag(
		const FGameplayTag& AbilityTag,
		bool bLogNotFound = false) const;

#if WITH_EDITOR
	// Valida las invariantes del asset mediante Data Validation del editor.
	virtual EDataValidationResult IsDataValid(FDataValidationContext& Context) const override;
#endif
};
