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
	const UPantheliaAttributeSet* AS = CastChecked<UPantheliaAttributeSet>(AttributeSet);

	// Iteramos el mismo mapa que en BroadcastInitialValues.
	// Para cada atributo, nos suscribimos al delegate que el ASC broadcastea
	// cuando ese atributo cambia. Cuando cambia, ejecutamos una lambda que
	// broadcastea el nuevo valor al widget.
	// Capturamos Pair por valor (no por referencia) porque cuando la lambda
	// se ejecute, el Pair local del for loop habrá salido de scope.
	// Capturamos AS por valor también para acceder al AttributeSet al ejecutar.
	for (auto& Pair : AS->TagsToAttributes)
	{
		AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
			Pair.Value()).AddLambda([this, Pair, AS](const FOnAttributeChangeData& Data)
				{
					BroadcastAttributeInfo(Pair.Key, Pair.Value());
				});
	}

	// --- Puntos de atributo y habilidad ---
	//
	// El PlayerState broadcastea FOnPlayerStatChanged (C++-only) cuando cambian
	// estos contadores. Nos bindeamos aquí en C++ y, dentro de cada lambda,
	// reenviamos el valor a través de nuestro delegate DINÁMICO (BlueprintAssignable),
	// que es el que el widget del menú puede escuchar desde Blueprint.
	//
	// Este patrón (C++ delegate del PlayerState → lambda → delegate dinámico del
	// controller) es el mismo que el curso usa: el PlayerState no debe conocer
	// tipos de UI, y los widgets no acceden directamente al PlayerState.
	APantheliaPlayerState* PS = CastChecked<APantheliaPlayerState>(PlayerState);

	PS->OnAttributePointsChangedDelegate.AddLambda(
		[this](int32 NewValue)
		{
			AttributePointsChangedDelegate.Broadcast(NewValue);
		});

	PS->OnSkillPointsChangedDelegate.AddLambda(
		[this](int32 NewValue)
		{
			SkillPointsChangedDelegate.Broadcast(NewValue);
		});
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

void UAttributeMenuWidgetController::UpgradeAttribute(const FGameplayTag& AttributeTag)
{
	// El único trabajo de este controller es delegar al ASC. La lógica de
	// comprobar saldo, enviar el Gameplay Event y descontar el punto vive en
	// UPantheliaAbilitySystemComponent::UpgradeAttribute (sin RPC de servidor:
	// Panthelia es single-player).
	UPantheliaAbilitySystemComponent* PantheliaASC = CastChecked<UPantheliaAbilitySystemComponent>(AbilitySystemComponent);
	PantheliaASC->UpgradeAttribute(AttributeTag);
}