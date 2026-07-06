// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PlayerActionsComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(FOnSprintSignature, UPlayerActionsComponent, OnSprintDelegate, float, Cost);

class ACharacter;
class UCharacterMovementComponent;
class IMainPlayer;

UCLASS( ClassGroup=(Custom), meta=(BlueprintSpawnableComponent) )
class PANTHELIAPROJECT_API UPlayerActionsComponent : public UActorComponent
{
	GENERATED_BODY()

	UPROPERTY()
	ACharacter* CharacterReference;

	UPROPERTY()
	TScriptInterface<IMainPlayer> PlayerInterface;

	UPROPERTY()
	UCharacterMovementComponent* MovementComponent;


	UPROPERTY(EditAnywhere, Category = "Movement")
	float SprintSpeed = 800.0f;

	UPROPERTY(EditAnywhere, Category = "Movement")
	float WalkSpeed = 400.0f;

public:	
	// Sets default values for this component's properties
	UPlayerActionsComponent();

protected:
	// Called when the game starts
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	UFUNCTION(BlueprintCallable, Category = "Movement")
	void Sprint();

	UFUNCTION(BlueprintCallable, Category = "Movement")
	void Walk();
};
