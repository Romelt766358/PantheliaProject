// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/BossCharacter.h"
#include "AbilitySystem/PantheliaAttributeSet.h"
#include "AbilitySystemComponent.h"

ABossCharacter::ABossCharacter()
{
	PrimaryActorTick.bCanEverTick = false;
}

void ABossCharacter::BeginPlay()
{
	Super::BeginPlay();

	const UPantheliaAttributeSet* PantheliaAS = CastChecked<UPantheliaAttributeSet>(AttributeSet);

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		PantheliaAS->GetHealthAttribute()
	).AddLambda([this](const FOnAttributeChangeData& Data)
		{
			OnBossHealthChanged.Broadcast(Data.NewValue);
		});

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		PantheliaAS->GetMaxHealthAttribute()
	).AddLambda([this](const FOnAttributeChangeData& Data)
		{
			OnBossMaxHealthChanged.Broadcast(Data.NewValue);
		});
}

void ABossCharacter::BroadcastInitialValues()
{
	// Broadcastea los valores actuales para que el widget los reciba
	// independientemente de cuándo fue creado.
	const UPantheliaAttributeSet* PantheliaAS = CastChecked<UPantheliaAttributeSet>(AttributeSet);
	OnBossMaxHealthChanged.Broadcast(PantheliaAS->GetMaxHealth());
	OnBossHealthChanged.Broadcast(PantheliaAS->GetHealth());
}

void ABossCharacter::HighlightActor_Implementation()
{
	Super::HighlightActor_Implementation();
}

void ABossCharacter::UnHighlightActor_Implementation()
{
	Super::UnHighlightActor_Implementation();
}