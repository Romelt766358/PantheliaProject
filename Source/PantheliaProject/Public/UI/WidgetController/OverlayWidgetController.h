// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/WidgetController/PantheliaWidgetController.h"
#include "GameplayEffectTypes.h"
#include "GameplayTagContainer.h"
#include "Engine/DataTable.h"
// El include completo es necesario (no basta un forward declaration) porque
// DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam necesita el tamaño completo del
// struct FPantheliaAbilityInfo para generar el código del delegate.
#include "AbilitySystem/Data/PantheliaAbilityInfo.h"
#include "OverlayWidgetController.generated.h"

class UPantheliaUserWidget;
// Forward declaration suficiente aquí: solo lo usamos como puntero en la firma
// de OnInitializeStartupAbilities. El .cpp incluirá el header completo.
class UPantheliaAbilitySystemComponent;

USTRUCT(BlueprintType)
struct FUIWidgetRow : public FTableRowBase
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FGameplayTag MessageTag = FGameplayTag();

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FText Message = FText();

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TSubclassOf<UPantheliaUserWidget> MessageWidget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	TObjectPtr<UTexture2D> Image = nullptr;
};

// Un único delegate para todos los atributos que broadcasta un float.
// Antes teníamos uno por atributo, pero son funcionalmente idénticos.
// También se reutiliza para el porcentaje de la barra de XP (que es un float entre 0 y 1).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnAttributeChangedSignature, float, NewValue);

// Delegate para valores enteros del jugador: nivel actual y XP ganada por kill.
// Separado de FOnAttributeChangedSignature para no perder precisión convirtiendo int→float.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnPlayerStatIntSignature, int32, NewValue);

// Delegate para mensajes de UI al recoger pickups
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FMessageWidgetRowSignature, FUIWidgetRow, Row);

// Delegate para broadcastear la información de una habilidad a los widgets.
// Los slots de hechizo del HUD escuchan este delegate y se actualizan solos
// cuando reciben un FPantheliaAbilityInfo cuyo InputTag coincida con el suyo.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FAbilityInfoSignature, FPantheliaAbilityInfo, AbilityInfo);

UCLASS(BlueprintType, Blueprintable)
class PANTHELIAPROJECT_API UOverlayWidgetController : public UPantheliaWidgetController
{
	GENERATED_BODY()

public:
	virtual void BroadcastInitialValues() override;
	virtual void BindCallbacksToDependencies() override;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature OnHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature OnMaxHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature OnStaminaChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature OnMaxStaminaChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature OnManaChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature OnMaxManaChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature OnPoiseChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Attributes")
	FOnAttributeChangedSignature OnMaxPoiseChanged;

	UPROPERTY(BlueprintAssignable, Category = "GAS|Messages")
	FMessageWidgetRowSignature MessageWidgetRowDelegate;

	// Delegate para enviar información de habilidades a los slots del HUD.
	// Se broadcastea desde OnInitializeStartupAbilities cuando se conocen
	// las abilities concedidas y sus datos del DA_AbilityInfo.
	UPROPERTY(BlueprintAssignable, Category = "GAS|Abilities")
	FAbilityInfoSignature AbilityInfoDelegate;

	// Delegate para el porcentaje de la barra de XP (0.0 a 1.0).
	// Se broadcastea cada vez que cambia la XP del jugador o en BroadcastInitialValues.
	// Reutiliza FOnAttributeChangedSignature porque la firma es idéntica (un float).
	UPROPERTY(BlueprintAssignable, Category = "GAS|XP")
	FOnAttributeChangedSignature OnXPPercentChanged;

	// Broadcastea el nivel actual del jugador (1, 2, 3...).
	// Se dispara en BroadcastInitialValues (valor inicial) y cada vez que el jugador sube de nivel.
	// El widget de XP lo usa para actualizar el texto "Nv. X".
	UPROPERTY(BlueprintAssignable, Category = "GAS|XP")
	FOnPlayerStatIntSignature OnLevelChanged;

	// Broadcastea la XP ganada en UNA sola muerte (el delta, no el total acumulado).
	// Por ejemplo: si el jugador tenía 300 XP y mata un enemigo que da 150, broadcastea 150.
	// El widget lo usa para mostrar el texto flotante "+150" sobre la barra de XP.
	// Solo se broadcastea cuando la XP aumenta (no en BroadcastInitialValues).
	UPROPERTY(BlueprintAssignable, Category = "GAS|XP")
	FOnPlayerStatIntSignature OnXPGained;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Widget Data")
	TObjectPtr<UDataTable> MessageWidgetDataTable;

	// Data Asset con la información de todas las habilidades (icono, fondo, tags).
	// Debe asignarse en BP_OverlayWidgetController → Class Defaults.
	// Empieza vacío hasta que se implementen los hechizos del sistema de corazones.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Widget Data")
	TObjectPtr<UPantheliaAbilityInfoAsset> AbilityInfo;

	// Callback que se llama cuando el ASC ha terminado de conceder todas sus startup abilities.
	// Puede llegar de dos formas (ver BindCallbacksToDependencies):
	//   a) Directamente si las abilities ya fueron dadas antes de que llegaramos a bindear.
	//   b) Via AbilitiesGivenDelegate si llegamos primero y el ASC aún no las había dado.
	// Su responsabilidad: iterar las abilities del ASC, buscarlas en DA_AbilityInfo y
	// broadcastear FPantheliaAbilityInfo a los widgets. (Implementación completa: clase 241.)
	void OnInitializeStartupAbilities(UPantheliaAbilitySystemComponent* PantheliaASC);

	// Caché del valor de XP en el último broadcast. Usado para calcular el delta (XP ganada
	// en una sola muerte) que se broadcastea como OnXPGained para el texto flotante "+N".
	// Se inicializa en BroadcastInitialValues con la XP actual del jugador.
	int32 CachedXP = 0;

	// Callback vinculado a APantheliaPlayerState::OnXPChangedDelegate.
	// Recibe la nueva XP total acumulada del jugador, calcula el porcentaje
	// de la barra de XP para el nivel actual, y lo broadcastea a la UI.
	// El cálculo usa costes INCREMENTALES del DA_LevelUpInfo (ver implementación .cpp).
	void OnXPChanged(int32 NewXP);

	// Callback vinculado a APantheliaPlayerState::OnLevelChangedDelegate.
	// Recibe el nuevo nivel y lo broadcastea a la UI vía OnLevelChanged.
	void OnLevelChangedCallback(int32 NewLevel);

	// Callbacks UObject explícitos para los delegates de atributos. AddUObject conoce el
	// lifetime de este controller y deja de invocarlo cuando se destruye; a diferencia de
	// AddLambda([this]), no conserva un puntero crudo potencialmente colgante.
	void OnHealthAttributeChanged(const FOnAttributeChangeData& Data);
	void OnMaxHealthAttributeChanged(const FOnAttributeChangeData& Data);
	void OnStaminaAttributeChanged(const FOnAttributeChangeData& Data);
	void OnMaxStaminaAttributeChanged(const FOnAttributeChangeData& Data);
	void OnManaAttributeChanged(const FOnAttributeChangeData& Data);
	void OnMaxManaAttributeChanged(const FOnAttributeChangeData& Data);
	void OnPoiseAttributeChanged(const FOnAttributeChangeData& Data);
	void OnMaxPoiseAttributeChanged(const FOnAttributeChangeData& Data);

	// Reenvía los Asset Tags de efectos hacia la tabla de mensajes del HUD sin usar una
	// lambda que capture this. El filtrado y el lookup permanecen idénticos.
	void OnEffectAssetTagsReceived(const FGameplayTagContainer& AssetTags);

	template<typename T>
	T* GetDataTableRowByTag(UDataTable* DataTable, const FGameplayTag& Tag)
	{
		return DataTable->FindRow<T>(Tag.GetTagName(), TEXT(""));
	}
};