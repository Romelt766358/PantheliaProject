// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/WidgetController/AttributeMenuWidgetController.h"
#include "AbilitySystem/PantheliaAttributeSet.h"
#include "AbilitySystem/PantheliaAbilitySystemComponent.h"
#include "Player/PantheliaPlayerState.h"

void UAttributeMenuWidgetController::BroadcastInitialValues()
{
	const UPantheliaAttributeSet* AS = CastChecked<UPantheliaAttributeSet>(AttributeSet);

	// En lugar de llamar BroadcastAttributeInfo manualmente para cada atributo,
	// iteramos el mapa TagsToAttributes del AttributeSet.
	// Esto significa que cuando se añada un atributo nuevo al juego,
	// solo hay que añadirlo al mapa en PantheliaAttributeSet.cpp —
	// este widget controller no necesita cambios.
	for (auto& Pair : AS->TagsToAttributes)
	{
		BroadcastAttributeInfo(Pair.Key, Pair.Value());
	}

	// Broadcast inicial de los puntos del jugador, para que el menú muestre
	// el valor correcto en cuanto se abre (sin esperar a que cambien).
	// Casteamos el PlayerState genérico de la clase base al de Panthelia.
	const APantheliaPlayerState* PS = CastChecked<APantheliaPlayerState>(PlayerState);
	AttributePointsChangedDelegate.Broadcast(PS->GetAttributePoints());
	SkillPointsChangedDelegate.Broadcast(PS->GetSkillPoints());
}

void UAttributeMenuWidgetController::BindCallbacksToDependencies()
{
	// Los delegates son aditivos. Esta guarda evita que una segunda llamada accidental
	// duplique todos los broadcasts del menú sobre la misma instancia de controller.
	if (bCallbacksBound)
	{
		return;
	}
	bCallbacksBound = true;

	const UPantheliaAttributeSet* AS = CastChecked<UPantheliaAttributeSet>(AttributeSet);

	// Iteramos el mismo mapa que en BroadcastInitialValues.
	// Para cada atributo, nos suscribimos al delegate que el ASC broadcastea
	// cuando ese atributo cambia. Capturamos solo el tag y el FGameplayAttribute por valor.
	// AddWeakLambda comprueba el lifetime de este UObject antes de ejecutar el callback;
	// el ASC persistente no puede llamar a un controller ya destruido tras recrear la UI.
	for (const auto& Pair : AS->TagsToAttributes)
	{
		const FGameplayTag AttributeTag = Pair.Key;
		const FGameplayAttribute Attribute = Pair.Value();

		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(Attribute).AddWeakLambda(
			this,
			[this, AttributeTag, Attribute](const FOnAttributeChangeData& /*Data*/)
			{
				BroadcastAttributeInfo(AttributeTag, Attribute);
			});
	}

	// --- Puntos de atributo y habilidad ---
	//
	// El PlayerState broadcastea FOnPlayerStatChanged (C++-only) cuando cambian
	// estos contadores. Nos bindeamos con AddUObject y reenviamos el valor mediante
	// los delegates dinámicos que escuchan los widgets Blueprint.
	APantheliaPlayerState* PS = CastChecked<APantheliaPlayerState>(PlayerState);

	PS->OnAttributePointsChangedDelegate.AddUObject(
		this,
		&UAttributeMenuWidgetController::OnAttributePointsChanged);

	PS->OnSkillPointsChangedDelegate.AddUObject(
		this,
		&UAttributeMenuWidgetController::OnSkillPointsChanged);
}

void UAttributeMenuWidgetController::BroadcastAttributeInfo(
	const FGameplayTag& AttributeTag,
	const FGameplayAttribute& Attribute) const
{
	check(AttributeInfo);

	// Buscamos el struct en el data asset por tag.
	// bLogNotFound=true para detectar entradas que falten en el data asset.
	FPantheliaAttributeInfo Info = AttributeInfo->FindAttributeInfoForTag(AttributeTag, true);

	// Rellenamos el valor actual desde el ASC en runtime.
	Info.AttributeValue = AbilitySystemComponent->GetNumericAttribute(Attribute);

	// Broadcasteamos el struct completo al delegate.
	AttributeInfoDelegate.Broadcast(Info);
}

void UAttributeMenuWidgetController::OnAttributePointsChanged(int32 NewValue)
{
	AttributePointsChangedDelegate.Broadcast(NewValue);
}

void UAttributeMenuWidgetController::OnSkillPointsChanged(int32 NewValue)
{
	SkillPointsChangedDelegate.Broadcast(NewValue);
}

void UAttributeMenuWidgetController::UpgradeAttribute(const FGameplayTag& AttributeTag)
{
	// El único trabajo de este controller es delegar al ASC. La lógica de
	// comprobar saldo, enviar el Gameplay Event y descontar el punto vive en
	// UPantheliaAbilitySystemComponent::UpgradeAttribute (sin RPC de servidor:
	// Panthelia es single-player).
	UPantheliaAbilitySystemComponent* PantheliaASC = CastChecked<UPantheliaAbilitySystemComponent>(AbilitySystemComponent);
	PantheliaASC->UpgradeAttribute(AttributeTag);
}