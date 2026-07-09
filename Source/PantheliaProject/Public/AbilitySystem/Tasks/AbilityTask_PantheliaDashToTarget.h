// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/Tasks/AbilityTask.h"
#include "AbilityTask_PantheliaDashToTarget.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE(FPantheliaDashTaskDelegate);

/**
 * AbilityTask_PantheliaDashToTarget
 *
 * Dash terrestre controlado por la Gameplay Ability.
 *
 * No usa LaunchCharacter porque, en bosses controlados por IA/StateTree, el CharacterMovement
 * o el path following pueden consumir/anular la velocidad casi inmediatamente. Este task mueve
 * directamente la cápsula del Character con sweep durante una ventana corta de tiempo.
 */
UCLASS()
class PANTHELIAPROJECT_API UAbilityTask_PantheliaDashToTarget : public UAbilityTask
{
	GENERATED_BODY()

public:
	UAbilityTask_PantheliaDashToTarget(const FObjectInitializer& ObjectInitializer);

	UPROPERTY(BlueprintAssignable)
	FPantheliaDashTaskDelegate OnFinished;

	UPROPERTY(BlueprintAssignable)
	FPantheliaDashTaskDelegate OnBlocked;

	UPROPERTY(BlueprintAssignable)
	FPantheliaDashTaskDelegate OnFailed;

	// Mueve el avatar de la ability hacia TargetActor, frenando en StopDistance.
	// MaxTravelDistance limita cuánto puede avanzar en un solo dash. Si es <= 0, no limita.
	UFUNCTION(BlueprintCallable, Category = "Panthelia|Ability|Tasks", meta = (DisplayName = "Dash Avatar Toward Actor", HidePin = "OwningAbility", DefaultToSelf = "OwningAbility", BlueprintInternalUseOnly = "true"))
	static UAbilityTask_PantheliaDashToTarget* DashAvatarTowardActor(
		UGameplayAbility* OwningAbility,
		AActor* TargetActor,
		float StopDistance = 140.f,
		float Duration = 0.18f,
		float MaxTravelDistance = 450.f,
		bool bSweep = true,
		bool bStopMovementBeforeDash = true);

	virtual void Activate() override;
	virtual void TickTask(float DeltaTime) override;

protected:
	virtual void OnDestroy(bool bInOwnerFinished) override;

private:
	UPROPERTY()
	TObjectPtr<AActor> TargetActor;

	UPROPERTY()
	TObjectPtr<ACharacter> AvatarCharacter;

	FVector StartLocation = FVector::ZeroVector;
	FVector EndLocation = FVector::ZeroVector;
	float ElapsedTime = 0.f;
	float DashDuration = 0.18f;
	float StopDistance = 140.f;
	float MaxTravelDistance = 450.f;
	bool bSweep = true;
	bool bStopMovementBeforeDash = true;
	bool bDidFinish = false;

	bool InitializeDash();
	void StopAvatarMovement() const;
	void BroadcastAndFinish(FPantheliaDashTaskDelegate& DelegateToBroadcast);
};
