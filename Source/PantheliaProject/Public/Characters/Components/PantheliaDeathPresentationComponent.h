#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Characters/Components/PantheliaDeathPresentationTypes.h"
#include "PantheliaDeathPresentationComponent.generated.h"

class APantheliaCharacterBase;
class USkeletalMeshComponent;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(
	FPantheliaDeathPresentationFinishedSignature,
	AActor*,
	DeadActor);

/**
 * Lifecycle reutilizable de presentacion de muerte. No decide la muerte, no toca
 * atributos persistentes y no destruye unilateralmente al Pawn ni al arma.
 */
UCLASS(ClassGroup = (Panthelia), meta = (BlueprintSpawnableComponent))
class PANTHELIAPROJECT_API UPantheliaDeathPresentationComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPantheliaDeathPresentationComponent();

	// Transiciones explicitas usadas por CharacterBase alrededor del shutdown.
	bool RequestDeathPresentation();
	void NotifyGameplayShutdownComplete();

	UFUNCTION(BlueprintCallable, Category = "Panthelia|Death Presentation")
	bool BeginDeathPresentation(const FVector& DeathImpulse);

	UFUNCTION(BlueprintCallable, Category = "Panthelia|Death Presentation")
	void SchedulePresentationFinish();

	UFUNCTION(BlueprintCallable, Category = "Panthelia|Death Presentation")
	void FinishDeathPresentation();

	UFUNCTION(BlueprintCallable, Category = "Panthelia|Death Presentation")
	void ClearPresentationFinishTimer();

	UFUNCTION(BlueprintCallable, Category = "Panthelia|Death Presentation")
	void AbortDeathPresentation();

	UFUNCTION(BlueprintPure, Category = "Panthelia|Death Presentation")
	bool IsPresentationActive() const;

	UFUNCTION(BlueprintPure, Category = "Panthelia|Death Presentation")
	bool HasPresentationFinished() const;

	UFUNCTION(BlueprintPure, Category = "Panthelia|Death Presentation")
	EPantheliaDeathPresentationState GetPresentationState() const { return PresentationState; }

	UFUNCTION(BlueprintCallable, Category = "Panthelia|Death Presentation")
	bool RegisterVisualPart(const FPantheliaDeathVisualPart& VisualPart);

	UFUNCTION(BlueprintCallable, Category = "Panthelia|Death Presentation")
	void ClearRegisteredVisualParts();

	UPROPERTY(BlueprintAssignable, Category = "Panthelia|Death Presentation")
	FPantheliaDeathPresentationFinishedSignature OnPresentationFinished;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panthelia|Death Presentation")
	EPantheliaDeathPoseFollowerTickPolicy PoseFollowerTickPolicy =
		EPantheliaDeathPoseFollowerTickPolicy::PreserveOriginal;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Panthelia|Death Presentation")
	FDeathPresentationFinalizationSettings FinalizationSettings;

protected:
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	bool EnsureAuthoritativeBodyFallback();
	bool RegisterAutomaticVisualParts();
	bool ValidateRegisteredVisualParts();
	bool ConfigureDeathPresentation(const FVector& DeathImpulse);
	void ConfigureCapsulePolicy(APantheliaCharacterBase& CharacterOwner) const;
	void ConfigureAuthoritativeBody(USkeletalMeshComponent& BodyMesh, const FVector& DeathImpulse);
	void ConfigurePoseFollowers();
	void ConfigureGrooms();
	void ConfigureWeaponParts(APantheliaCharacterBase& CharacterOwner, const FVector& DeathImpulse);
	void HandleScheduledPresentationFinish(uint32 CallbackGeneration);

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient,
		Category = "Panthelia|Death Presentation", meta = (AllowPrivateAccess = "true"))
	EPantheliaDeathPresentationState PresentationState = EPantheliaDeathPresentationState::Alive;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Transient,
		Category = "Panthelia|Death Presentation", meta = (AllowPrivateAccess = "true"))
	TArray<FPantheliaDeathVisualPart> RegisteredVisualParts;

	FTimerHandle PresentationTimerHandle;
	uint32 PresentationCallbackGeneration = 0;
	bool bFinishBroadcast = false;
	bool bDeathImpulseApplied = false;
};
