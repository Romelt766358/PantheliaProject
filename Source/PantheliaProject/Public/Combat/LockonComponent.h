// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"

class AActor;
class ACharacter;
class APlayerController;

#include "LockonComponent.generated.h"

DECLARE_DYNAMIC_MULTICAST_SPARSE_DELEGATE_OneParam(
	FOnUpdatedTargetSignature,
	ULockonComponent,
	OnUpdatedTargetDelegate,
	AActor*,
	NewTargetActorRef
);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PANTHELIAPROJECT_API ULockonComponent : public UActorComponent
{
	GENERATED_BODY()

	UPROPERTY()
	TObjectPtr<ACharacter> OwnerRef;

	UPROPERTY()
	TObjectPtr<APlayerController> Controller;

	UPROPERTY()
	TObjectPtr<class UCharacterMovementComponent> MovementComp;

	UPROPERTY()
	TObjectPtr<class USpringArmComponent> SpringArmComp;

public:
	ULockonComponent();

	// Target actual del lock-on. Otros sistemas lo leen directamente, por ejemplo
	// los proyectiles para apuntar al enemigo fijado.
	UPROPERTY(BlueprintReadOnly, Category = "Lockon")
	TObjectPtr<AActor> CurrentTargetActor;

	UPROPERTY(BlueprintAssignable)
	FOnUpdatedTargetSignature OnUpdatedTargetDelegate;

	// Activa o desactiva el lock-on. Debe ser público para que el PlayerController
	// pueda llamarlo desde IA_ToggleLockon.
	UFUNCTION(BlueprintCallable, Category = "Lockon")
	void ToggleLockon(float Radius = 850.0f);

	// Cambia de target usando una dirección horizontal.
	// Direction > 0 = derecha, Direction < 0 = izquierda.
	// Pensado para la rueda del ratón: Wheel Up = +1, Wheel Down = -1.
	UFUNCTION(BlueprintCallable, Category = "Lockon")
	void SwitchTarget(float Direction);

	// EndLockon es público para que sistemas externos (como PantheliaEnemy::Die())
	// puedan limpiar el lockon cuando el target muere.
	// Llama a OnDeselect en el target, resetea movimiento, input y spring arm.
	// Solo llamar si CurrentTargetActor es válido — no llama a OnDeselect si el actor
	// ya fue destruido (en ese caso TickComponent maneja la limpieza directamente).
	UFUNCTION(BlueprintCallable, Category = "Lockon")
	void EndLockon();

	// Se llama cuando el target actual muere, pero antes de que el actor sea destruido.
	// Intenta saltar automáticamente a otro enemigo cercano. Si no encuentra ninguno,
	// termina el lock-on.
	void HandleCurrentTargetLost(AActor* LostTarget);

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Lockon")
	void StartLockon(float Radius = 850.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockon Settings")
	double BreakDistance{ 1200.0f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockon Settings")
	double LockonAngleThreshold{ 0.5f };

	// Radio usado cuando el target muere y buscamos otro enemigo cercano.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockon Settings")
	float AutoRetargetRadius{ 950.0f };

	// Radio usado al cambiar manualmente con la rueda.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockon Settings")
	float SwitchTargetRadius{ 1100.0f };

	// Evita cambiar manualmente a enemigos claramente detrás de la cámara.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockon Settings")
	float SwitchForwardThreshold{ 0.05f };

	// Cuánto debe estar el candidato hacia el lado solicitado para considerarlo.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockon Settings")
	float SwitchSideThreshold{ 0.1f };

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool bLockonStateApplied = false;

	TArray<AActor*> FindLockonCandidates(float Radius, AActor* ActorToIgnore = nullptr) const;
	AActor* FindBestInitialTarget(float Radius) const;
	AActor* FindBestAutoRetargetTarget(AActor* LostTarget) const;
	AActor* FindBestDirectionalTarget(float Direction) const;

	bool IsValidLockonCandidate(AActor* Candidate) const;
	bool PassesCameraAngleCheck(AActor* Candidate, float MinDot) const;

	void SetCurrentTarget(AActor* NewTarget, bool bCallDeselectOnOldTarget = true);
	void ApplyLockonState();
	void ClearLockonState();
};
