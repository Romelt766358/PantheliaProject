// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "LockonComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(
	FOnUpdatedTargetSignature,
	ULockonComponent, OnUpdatedTargetDelegate,
	AActor*, NewTargetActorRef
);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PANTHELIAPROJECT_API ULockonComponent : public UActorComponent
{
	GENERATED_BODY()

	ACharacter* OwnerRef;

	APlayerController* Controller;

	class UCharacterMovementComponent* MovementComp;

	class USpringArmComponent* SpringArmComp;

public:
	ULockonComponent();

	AActor* CurrentTargetActor;

	UPROPERTY(BlueprintAssignable)
	FOnUpdatedTargetSignature OnUpdatedTargetDelegate;

	// EndLockon es público para que sistemas externos (como PantheliaEnemy::Die())
	// puedan limpiar el lockon cuando el target muere.
	// Llama a OnDeselect en el target, resetea movimiento, input y spring arm.
	// Solo llamar si CurrentTargetActor es válido — no llama a OnDeselect si el actor
	// ya fue destruido (en ese caso TickComponent maneja la limpieza directamente).
	void EndLockon();

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable)
	void StartLockon(float Radius = 850.0f);

	UFUNCTION(BlueprintCallable)
	void ToggleLockon(float Radius = 850.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite)
	double BreakDistance{ 1200.0f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockon Settings")
	double LockonAngleThreshold{ 0.5f };

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;
};