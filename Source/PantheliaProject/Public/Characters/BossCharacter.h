// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/PantheliaEnemy.h"
#include "BossCharacter.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBossAttributeChangedSignature, float, NewValue);

UCLASS()
class PANTHELIAPROJECT_API ABossCharacter : public APantheliaEnemy
{
	GENERATED_BODY()

public:
	ABossCharacter();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Boss")
	FText BossName;

	UPROPERTY(BlueprintAssignable, Category = "Boss")
	FBossAttributeChangedSignature OnBossHealthChanged;

	UPROPERTY(BlueprintAssignable, Category = "Boss")
	FBossAttributeChangedSignature OnBossMaxHealthChanged;

	// Broadcastea los valores actuales de Health y MaxHealth.
	// Llamar desde el Level Blueprint después de crear el widget,
	// para que el widget reciba los valores iniciales correctos.
	UFUNCTION(BlueprintCallable, Category = "Boss")
	void BroadcastInitialValues();

protected:
	virtual void BeginPlay() override;

	virtual void HighlightActor_Implementation() override;
	virtual void UnHighlightActor_Implementation() override;
};