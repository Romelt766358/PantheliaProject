// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/PantheliaUserWidget.h"

void UPantheliaUserWidget::SetWidgetController(UObject* InWidgetController)
{
	WidgetController = InWidgetController;
	WidgetControllerSet();
}