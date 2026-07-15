// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/HUD/PantheliaHUD.h"

#include "AbilitySystemComponent.h"
#include "GameFramework/PlayerState.h"
#include "PantheliaLogChannels.h"
#include "UI/WidgetController/AttributeMenuWidgetController.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "UI/WidgetController/PantheliaWidgetController.h"
#include "UI/Widgets/PantheliaUserWidget.h"

namespace
{
	bool AreWidgetControllerParamsValid(const FWidgetControllerParams& Params)
	{
		return IsValid(Params.PlayerController) &&
			IsValid(Params.PlayerState) &&
			IsValid(Params.AbilitySystemComponent) &&
			IsValid(Params.AttributeSet);
	}
}

UOverlayWidgetController* APantheliaHUD::GetOverlayWidgetController(
	const FWidgetControllerParams& WCParams)
{
	if (!AreWidgetControllerParamsValid(WCParams))
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[HUD] No se puede crear OverlayWidgetController en %s: parámetros incompletos."),
			*GetNameSafe(this));
		return nullptr;
	}

	if (!IsValid(OverlayWidgetController))
	{
		if (!OverlayWidgetControllerClass)
		{
			UE_LOG(LogPanthelia, Error,
				TEXT("[HUD] OverlayWidgetControllerClass no está configurada en %s."),
				*GetNameSafe(this));
			return nullptr;
		}

		OverlayWidgetController = NewObject<UOverlayWidgetController>(
			this, OverlayWidgetControllerClass);
		if (!IsValid(OverlayWidgetController))
		{
			UE_LOG(LogPanthelia, Error,
				TEXT("[HUD] NewObject falló para OverlayWidgetControllerClass=%s."),
				*GetNameSafe(OverlayWidgetControllerClass.Get()));
			return nullptr;
		}

		OverlayWidgetController->SetWidgetControllerParams(WCParams);
		OverlayWidgetController->BindCallbacksToDependencies();
	}

	return OverlayWidgetController;
}

UAttributeMenuWidgetController* APantheliaHUD::GetAttributeMenuWidgetController(
	const FWidgetControllerParams& WCParams)
{
	if (!AreWidgetControllerParamsValid(WCParams))
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[HUD] No se puede crear AttributeMenuWidgetController en %s: parámetros incompletos."),
			*GetNameSafe(this));
		return nullptr;
	}

	if (!IsValid(AttributeMenuWidgetController))
	{
		if (!AttributeMenuWidgetControllerClass)
		{
			UE_LOG(LogPanthelia, Error,
				TEXT("[HUD] AttributeMenuWidgetControllerClass no está configurada en %s."),
				*GetNameSafe(this));
			return nullptr;
		}

		AttributeMenuWidgetController = NewObject<UAttributeMenuWidgetController>(
			this, AttributeMenuWidgetControllerClass);
		if (!IsValid(AttributeMenuWidgetController))
		{
			UE_LOG(LogPanthelia, Error,
				TEXT("[HUD] NewObject falló para AttributeMenuWidgetControllerClass=%s."),
				*GetNameSafe(AttributeMenuWidgetControllerClass.Get()));
			return nullptr;
		}

		AttributeMenuWidgetController->SetWidgetControllerParams(WCParams);
		AttributeMenuWidgetController->BindCallbacksToDependencies();
	}

	return AttributeMenuWidgetController;
}

void APantheliaHUD::InitOverlay(
	APlayerController* PC,
	APlayerState* PS,
	UAbilitySystemComponent* ASC,
	UAttributeSet* AS)
{
	if (!IsValid(PC) || !IsValid(PS) || !IsValid(ASC) || !IsValid(AS))
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[HUD] InitOverlay rechazado en %s: PC=%s PS=%s ASC=%s AS=%s."),
			*GetNameSafe(this),
			*GetNameSafe(PC),
			*GetNameSafe(PS),
			*GetNameSafe(ASC),
			*GetNameSafe(AS));
		return;
	}

	if (!OverlayWidgetClass || !OverlayWidgetControllerClass)
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[HUD] InitOverlay rechazado en %s: OverlayWidgetClass=%s ControllerClass=%s. Revisa BP_PantheliaHUD."),
			*GetNameSafe(this),
			*GetNameSafe(OverlayWidgetClass.Get()),
			*GetNameSafe(OverlayWidgetControllerClass.Get()));
		return;
	}

	UPantheliaUserWidget* NewOverlayWidget = CreateWidget<UPantheliaUserWidget>(
		PC, OverlayWidgetClass);
	if (!IsValid(NewOverlayWidget))
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[HUD] CreateWidget falló o la clase %s no deriva de UPantheliaUserWidget."),
			*GetNameSafe(OverlayWidgetClass.Get()));
		return;
	}

	const FWidgetControllerParams WidgetControllerParams(PC, PS, ASC, AS);
	UOverlayWidgetController* WidgetController =
		GetOverlayWidgetController(WidgetControllerParams);
	if (!IsValid(WidgetController))
	{
		NewOverlayWidget->RemoveFromParent();
		return;
	}

	// InitOverlay puede reejecutarse al reconstruir el Pawn. Evitamos overlays
	// duplicados, pero conservamos el controller cacheado con las mismas dependencias.
	if (IsValid(OverlayWidget) && OverlayWidget != NewOverlayWidget)
	{
		OverlayWidget->RemoveFromParent();
	}

	OverlayWidget = NewOverlayWidget;
	OverlayWidget->AddToViewport();
	OverlayWidget->SetWidgetController(WidgetController);
	WidgetController->BroadcastInitialValues();
}
