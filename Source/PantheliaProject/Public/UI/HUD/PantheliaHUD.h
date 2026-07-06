// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/HUD.h"
#include "Templates/SubclassOf.h" 
#include "PantheliaHUD.generated.h"

class UPantheliaUserWidget;
class UOverlayWidgetController;
class UAttributeMenuWidgetController;
class UAbilitySystemComponent;
class UAttributeSet;
struct FWidgetControllerParams;

UCLASS()
class PANTHELIAPROJECT_API APantheliaHUD : public AHUD
{
	GENERATED_BODY()

public:
	UPROPERTY()
	UPantheliaUserWidget* OverlayWidget;

	void InitOverlay(APlayerController* PC, APlayerState* PS, UAbilitySystemComponent* ASC, UAttributeSet* AS);

	UOverlayWidgetController* GetOverlayWidgetController(const FWidgetControllerParams& WCParams);

	// Devuelve el AttributeMenuWidgetController. Si no existe, lo construye.
	UAttributeMenuWidgetController* GetAttributeMenuWidgetController(const FWidgetControllerParams& WCParams);

private:
	UPROPERTY(EditAnywhere, Category = "Panthelia|UI")
	TSubclassOf<UPantheliaUserWidget> OverlayWidgetClass;

	UPROPERTY()
	TObjectPtr<UOverlayWidgetController> OverlayWidgetController;

	UPROPERTY(EditAnywhere, Category = "Panthelia|UI")
	TSubclassOf<UOverlayWidgetController> OverlayWidgetControllerClass;

	UPROPERTY()
	TObjectPtr<UAttributeMenuWidgetController> AttributeMenuWidgetController;

	UPROPERTY(EditAnywhere, Category = "Panthelia|UI")
	TSubclassOf<UAttributeMenuWidgetController> AttributeMenuWidgetControllerClass;
};