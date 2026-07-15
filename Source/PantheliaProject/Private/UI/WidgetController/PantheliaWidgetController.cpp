// Fill out your copyright notice in the Description page of Project Settings.


#include "UI/WidgetController/PantheliaWidgetController.h"


void UPantheliaWidgetController::SetWidgetControllerParams(const FWidgetControllerParams& WCParams)
{
	// Una vez bindeados los delegates, esta instancia queda asociada a un único conjunto
	// de dependencias. Cambiar solo los punteros sin retirar antes los delegates antiguos
	// mezclaría callbacks de dos PlayerStates/ASCs. El HUD actual crea y cachea una instancia
	// nueva por conjunto de dependencias; el rebinding real requerirá un teardown explícito.
	if (bCallbacksBound)
	{
		const bool bDependenciesAreUnchanged =
			PlayerController == WCParams.PlayerController &&
			PlayerState == WCParams.PlayerState &&
			AbilitySystemComponent == WCParams.AbilitySystemComponent &&
			AttributeSet == WCParams.AttributeSet;

		ensureMsgf(
			bDependenciesAreUnchanged,
			TEXT("%s intentó cambiar sus dependencias después de BindCallbacksToDependencies. "
				"Crea un controller nuevo o implementa Unbind/Rebind explícito antes de reasignarlas."),
			*GetNameSafe(this));

		if (!bDependenciesAreUnchanged)
		{
			return;
		}
	}

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
