// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "PantheliaUserWidget.generated.h"

UCLASS()
class PANTHELIAPROJECT_API UPantheliaUserWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// Función para asignar el controlador desde el HUD o C++
	UFUNCTION(BlueprintCallable)
	void SetWidgetController(UObject* InWidgetController);

	// El controlador (lo guardamos como UObject para que sea genérico)
	UPROPERTY(BlueprintReadOnly)
	TObjectPtr<UObject> WidgetController;

protected:
	// Evento que implementaremos en Blueprints para inicializar la barra de vida, etc.
	UFUNCTION(BlueprintImplementableEvent)
	void WidgetControllerSet();
};