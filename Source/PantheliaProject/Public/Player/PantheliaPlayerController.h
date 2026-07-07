// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "InputActionValue.h"
#include "Templates/SubclassOf.h"
#include "Interfaces/Enemy.h"
#include "GameplayTagContainer.h"
#include "PantheliaPlayerController.generated.h"

class UInputMappingContext;
class UInputAction;
class UPantheliaUserWidget;
class UPantheliaInputConfig;
class UPantheliaAbilitySystemComponent;

UCLASS()
class PANTHELIAPROJECT_API APantheliaPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	APantheliaPlayerController();

protected:
	virtual void BeginPlay() override;
	virtual void SetupInputComponent() override;
	virtual void PlayerTick(float DeltaTime) override;

private:
	// --- INPUT ---

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputMappingContext> PantheliaContext;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	UPROPERTY(EditAnywhere, Category = "Input")
	TObjectPtr<UInputAction> OpenAttributeMenuAction;

	// Input normal del PlayerController, no GAS.
	// IA_ToggleLockon: Bool, recomendado Tab o Middle Mouse Button.
	UPROPERTY(EditAnywhere, Category = "Input|Lockon")
	TObjectPtr<UInputAction> ToggleLockonAction;

	// Input normal del PlayerController, no GAS.
	// IA_SwitchTarget: Axis1D/Float. Wheel Up = +1, Wheel Down = -1.
	UPROPERTY(EditAnywhere, Category = "Input|Lockon")
	TObjectPtr<UInputAction> SwitchLockonTargetAction;

	// Data Asset que vincula cada InputAction con su GameplayTag.
	// Asignar desde el Blueprint del PlayerController (DA_InputConfig).
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UPantheliaInputConfig> InputConfig;

	void Move(const FInputActionValue& InputActionValue);
	void Look(const FInputActionValue& InputActionValue);
	void ToggleAttributeMenu();
	void ToggleLockonInput();
	void SwitchLockonTargetInput(const FInputActionValue& InputActionValue);

	// --- CALLBACKS DE HABILIDADES ---

	// Estas tres funciones se bindean a TODOS los inputs del InputConfig.
	// Cada una recibe el InputTag del input que la disparó.

	// Llamada UNA VEZ cuando se presiona el input (ETriggerEvent::Started).
	// Por ahora vacía: usamos Held para activar. Aquí irá lógica futura si es necesario.
	void AbilityInputTagPressed(FGameplayTag InputTag);

	// Llamada UNA VEZ cuando se suelta el input (ETriggerEvent::Completed).
	void AbilityInputTagReleased(FGameplayTag InputTag);

	// Llamada CADA FRAME mientras el input esté presionado (ETriggerEvent::Triggered).
	// Es la que activa las abilities (si no están activas) y mantiene el estado de input.
	void AbilityInputTagHeld(FGameplayTag InputTag);

	// --- ASC CACHEADO ---

	// Guardamos el ASC para no castear en cada frame (AbilityInputTagHeld se llama cada frame).
	// Se inicializa lazy en GetASC() la primera vez que se necesita.
	UPROPERTY()
	TObjectPtr<UPantheliaAbilitySystemComponent> PantheliaASC;

	// Devuelve el ASC del Pawn controlado, casteado a UPantheliaAbilitySystemComponent.
	// Castea solo la primera vez. Puede retornar nullptr si se llama muy temprano.
	UPantheliaAbilitySystemComponent* GetASC();

	// --- INTERACTION TRACE ---

	void InteractionTrace();

	UPROPERTY()
	TScriptInterface<IEnemy> LastActor;

	UPROPERTY()
	TScriptInterface<IEnemy> ThisActor;

	// --- ATTRIBUTE MENU ---

	// La clase del widget del menú de atributos. Se asigna desde el Blueprint del Controller.
	UPROPERTY(EditAnywhere, Category = "UI|Attribute Menu")
	TSubclassOf<UPantheliaUserWidget> AttributeMenuClass;

	// Referencia al widget creado. Null cuando el menú está cerrado.
	UPROPERTY()
	TObjectPtr<UPantheliaUserWidget> AttributeMenuWidget;

	// Controla si el menú está abierto o no.
	bool bAttributeMenuOpen = false;

	void OpenAttributeMenu();
	void CloseAttributeMenu();
};
