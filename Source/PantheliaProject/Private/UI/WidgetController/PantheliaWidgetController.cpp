// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/WidgetController/PantheliaWidgetController.h"


void UPantheliaWidgetController::SetWidgetControllerParams(const FWidgetControllerParams& WCParams)
{
	PlayerController = WCParams.PlayerController;
	PlayerState = WCParams.PlayerState;
	AbilitySystemComponent = WCParams.AbilitySystemComponent;
	AttributeSet = WCParams.AttributeSet;
}

void UPantheliaWidgetController::BroadcastInitialValues()
{
}

void UPantheliaWidgetController::BindCallbacksToDependencies()
{
}
