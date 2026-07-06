#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "PantheliaAttributeInfo.generated.h"

// Struct que agrupa toda la información de un atributo que se envía a los widgets.
// Cada atributo tiene su propio tag, nombre visible, descripción y valor actual.
// El widget controller lo broadcastea cuando un atributo cambia.
USTRUCT(BlueprintType)
struct FPantheliaAttributeInfo
{
	GENERATED_BODY()

	// Tag que identifica a qué atributo pertenece este struct.
	// Se usa para hacer el lookup en el data asset y para que
	// cada fila del menú sepa si este broadcast le corresponde.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FGameplayTag AttributeTag = FGameplayTag();

	// Nombre visible del atributo en la UI (ej. "Resilience", "Armor").
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText AttributeName = FText();

	// Descripción del atributo para tooltips u otras UI.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	FText AttributeDescription = FText();

	// Valor actual del atributo. NO se expone al editor del data asset
	// porque se rellena en runtime cuando el atributo cambia.
	// Solo readable desde Blueprint para que los widgets puedan leerlo.
	UPROPERTY(BlueprintReadOnly)
	float AttributeValue = 0.f;
};

// Data Asset que contiene la información de todos los atributos del juego.
// Permite buscar un FPantheliaAttributeInfo por GameplayTag.
UCLASS()
class PANTHELIAPROJECT_API UPantheliaAttributeInfoAsset : public UDataAsset
{
	GENERATED_BODY()

public:

	// Busca en el array el struct cuyo AttributeTag coincida con el tag dado.
	// Si bLogNotFound es true y no lo encuentra, loguea un error.
	FPantheliaAttributeInfo FindAttributeInfoForTag(
		const FGameplayTag& AttributeTag,
		bool bLogNotFound = false) const;

	// Array con la información de todos los atributos del juego.
	// Se rellena desde el Blueprint del data asset en el editor.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
	TArray<FPantheliaAttributeInfo> AttributeInformation;
};