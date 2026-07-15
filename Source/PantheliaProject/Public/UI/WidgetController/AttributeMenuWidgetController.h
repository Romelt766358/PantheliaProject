// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AttributeSet.h"
#include "UI/WidgetController/PantheliaWidgetController.h"
#include "AbilitySystem/Data/PantheliaAttributeInfo.h"
#include "AttributeMenuWidgetController.generated.h"

// Delegate que broadcastea un FPantheliaAttributeInfo a los widgets.
// Cualquier widget puede bindearse a este delegate para recibir info
// de un atributo (tag, nombre, descripción, valor).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAttributeInfoSignature, const FPantheliaAttributeInfo&, Info);

// Delegate dinámico para avisar a los widgets del menú de que un contador de
// puntos (atributo o habilidad) ha cambiado. Es DINÁMICO y BlueprintAssignable
// porque los widgets del menú se bindean desde Blueprint (a diferencia del
// FOnPlayerStatChanged del PlayerState, que es C++-only y se bindea aquí).
// Pasa el nuevo valor como int32 para que el widget lo muestre directamente.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerPointsChangedSignature, int32, NewValue);

UCLASS(BlueprintType, Blueprintable)
class PANTHELIAPROJECT_API UAttributeMenuWidgetController : public UPantheliaWidgetController
{
	GENERATED_BODY()

public:
	virtual void BroadcastInitialValues() override;
	virtual void BindCallbacksToDependencies() override;

	// Delegate al que los widgets del menú se bindean para recibir
	// la información de cada atributo cuando cambia o al inicializar.
	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FAttributeInfoSignature AttributeInfoDelegate;

	// Delegate para los puntos de atributo disponibles del jugador.
	// El widget del menú se bindea aquí para mostrar cuántos puntos quedan.
	// Se re-broadcastea cada vez que el PlayerState cambia AttributePoints
	// (subida de nivel, gasto de puntos, respec).
	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnPlayerPointsChangedSignature AttributePointsChangedDelegate;

	// Delegate para los puntos de habilidad disponibles del jugador.
	// Aunque el menú actual solo muestra puntos de atributo, exponemos este
	// por separado para que el futuro árbol de habilidades pueda bindearse
	// sin tocar este controller (escalabilidad).
	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnPlayerPointsChangedSignature SkillPointsChangedDelegate;

	// Función puente Blueprint-callable que las filas de atributos primarios llaman
	// al pulsar su botón "+". Solo delega al ASC — este controller no decide nada
	// sobre GAS, solo hace de intermediario entre la UI y el ASC (mismo rol que
	// cumple en todos los demás métodos de esta clase).
	UFUNCTION(BlueprintCallable, Category = "GAS|Attributes")
	void UpgradeAttribute(const FGameplayTag& AttributeTag);

protected:

	// Data Asset que contiene la información de todos los atributos.
	// Se asigna desde el Blueprint BP_AttributeMenuWidgetController.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "GAS|Attributes")
	TObjectPtr<UPantheliaAttributeInfoAsset> AttributeInfo;

private:

	// Función auxiliar para broadcastear la info de un atributo dado su tag y valor.
	void BroadcastAttributeInfo(const FGameplayTag& AttributeTag, const FGameplayAttribute& Attribute) const;

	// Reenvíos UObject explícitos para los delegates C++ del PlayerState. AddUObject
	// respeta el lifetime del controller y evita lambdas con this crudo.
	void OnAttributePointsChanged(int32 NewValue);
	void OnSkillPointsChanged(int32 NewValue);
};