// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "PantheliaWidgetController.generated.h"

// Forward declarations para no sobrecargar el header
class UAbilitySystemComponent;
class UAttributeSet;

USTRUCT(BlueprintType)
struct FWidgetControllerParams
{
	GENERATED_BODY()

	FWidgetControllerParams() {}
	FWidgetControllerParams(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS)
		: PlayerController(PC), PlayerState(PS), AbilitySystemComponent(ASC), AttributeSet(AS) {
	}

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<APlayerController> PlayerController = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<APlayerState> PlayerState = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	TObjectPtr<UAttributeSet> AttributeSet = nullptr;
};

UCLASS()
class PANTHELIAPROJECT_API UPantheliaWidgetController : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable)
	void SetWidgetControllerParams(const FWidgetControllerParams& WCParams);

	// Función virtual que los hijos van a sobreescribir.
	// BlueprintCallable para poder llamarla desde el Event WidgetControllerSet
	// en los widgets Blueprint.
	UFUNCTION(BlueprintCallable)
	virtual void BroadcastInitialValues();

	virtual void BindCallbacksToDependencies();

protected:
	// Los 4 pilares de datos que cualquier Widget podría necesitar
	UPROPERTY(BlueprintReadOnly, Category = "WidgetController")
	TObjectPtr<APlayerController> PlayerController;

	UPROPERTY(BlueprintReadOnly, Category = "WidgetController")
	TObjectPtr<APlayerState> PlayerState;

	UPROPERTY(BlueprintReadOnly, Category = "WidgetController")
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY(BlueprintReadOnly, Category = "WidgetController")
	TObjectPtr<UAttributeSet> AttributeSet;

	// BindCallbacksToDependencies puede ser llamado accidentalmente más de una vez desde
	// Blueprint/HUD. Los binds nativos son aditivos, así que sin esta guarda cada cambio
	// de atributo se rebroadcastearía varias veces. Cada instancia de controller se enlaza
	// una sola vez con dependencias inmutables; SetWidgetControllerParams rechaza cambiar
	// PlayerState/ASC después del bind. No debe existir un simple "reset" de esta bandera:
	// un rebinding futuro exige retirar primero todos los delegates de las dependencias viejas.
	bool bCallbacksBound = false;
};