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

	// Intenta fijar lock-on al enemigo golpeado por un ataque básico del jugador.
	// Solo hace algo si NO hay lock-on activo; si CurrentTargetActor ya existe,
	// no cambia de objetivo. Esto evita que golpear al enemigo B cambie el lock-on
	// cuando el jugador ya estaba fijado al enemigo A.
	UFUNCTION(BlueprintCallable, Category = "Lockon")
	bool TryAutoLockOnFromBasicAttackHit(AActor* HitActor);

	// Gancho para el futuro menú de opciones: permite activar/desactivar el auto lock-on
	// al conectar un ataque básico sin tocar WeaponTrace ni las abilities.
	UFUNCTION(BlueprintCallable, Category = "Lockon|Options")
	void SetAutoLockOnFromBasicAttackHitEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Lockon|Options")
	bool IsAutoLockOnFromBasicAttackHitEnabled() const;

	// Gancho para el futuro menú de opciones: activa/desactiva el soft-lock de ataques
	// melee. Soft-lock NO fija CurrentTargetActor; solo ayuda a orientar el ataque hacia
	// un enemigo cercano/frontal cuando el jugador no tiene lock-on duro activo.
	UFUNCTION(BlueprintCallable, Category = "Lockon|Options")
	void SetSoftLockOnMeleeAttacksEnabled(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Lockon|Options")
	bool IsSoftLockOnMeleeAttacksEnabled() const;

	// Devuelve el mejor objetivo para soft-lock melee sin activar lock-on duro.
	// Null si el lock-on duro ya está activo, si la opción está desactivada, o si
	// ningún enemigo cercano/frontal pasa los filtros de visibilidad/targeteabilidad.
	AActor* FindBestSoftLockTarget(float RadiusOverride = -1.0f);

	// Punto centralizado desde el que cámara, proyectiles y soft-lock pueden apuntar
	// al enemigo. Hoy delega en IEnemy::GetLockonLocation() y cae a GetActorLocation.
	FVector GetLockonLocation(AActor* TargetActor) const;

protected:
	virtual void BeginPlay() override;

	UFUNCTION(BlueprintCallable, Category = "Lockon")
	void StartLockon(float Radius = 850.0f);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockon Settings")
	double BreakDistance{ 1200.0f };

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockon Settings")
	double LockonAngleThreshold{ 0.5f };

	// Si está activo, el lock-on solo puede adquirir/cambiar a objetivos con línea
	// de visión clara desde la cámara. No rompe aún el lock-on si una pared se interpone
	// después de haber seleccionado el target; eso queda para la siguiente iteración
	// con temporizador de oclusión.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockon Settings")
	bool bRequireLineOfSightToAcquireLockon = true;

	// Canal usado para comprobar paredes/obstáculos entre la cámara y el target.
	// Visibility es el default esperado para objetos de mundo que bloquean visión.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockon Settings")
	TEnumAsByte<ECollisionChannel> LineOfSightTraceChannel = ECC_Visibility;

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

	// Opción de gameplay: si está activa, un ataque básico que golpea a un enemigo
	// fija lock-on automáticamente SOLO cuando no hay lock-on activo.
	// Vive aquí como gancho del futuro menú de opciones. El menú solo tendrá que llamar
	// SetAutoLockOnFromBasicAttackHitEnabled() o escribir esta propiedad desde Blueprint.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockon Settings|Options")
	bool bAutoLockOnFromBasicAttackHit = true;

	// Opción de gameplay: ayuda a orientar ataques melee hacia enemigos cercanos/frontrales
	// cuando NO hay lock-on duro activo. No selecciona target ni muestra widget; solo
	// devuelve un objetivo de asistencia para que la ability rote al personaje.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockon Settings|Options")
	bool bSoftLockOnMeleeAttacks = true;

	// Radio de búsqueda para la asistencia de ataque sin lock-on. Debe ser menor que el
	// lock-on duro para que no atraiga golpes hacia enemigos lejanos.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockon Settings|Soft Lock")
	float SoftLockRadius = 650.0f;

	// Dot mínimo contra el forward del personaje. 0.25 permite targets ligeramente
	// laterales; subirlo hace el soft-lock más estricto.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Lockon Settings|Soft Lock")
	float SoftLockForwardThreshold = 0.25f;

public:
	virtual void TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

private:
	bool bLockonStateApplied = false;

	// Protege contra doble llamada al mismo input en el mismo frame.
	// Esto cubre el caso típico en UE donde queda una llamada vieja en Blueprint
	// y otra nueva en C++ sobre el mismo IA_Lockon.
	float LastToggleTimeSeconds = -1000.0f;

	UPROPERTY(EditAnywhere, Category = "Lockon Settings")
	float ToggleDebounceSeconds = 0.12f;

	// Refresca referencias que pueden no existir en BeginPlay todavía. En especial,
	// el Controller puede llegar después por PossessedBy, y si queda null el lock-on
	// no puede leer la cámara para elegir target.
	void RefreshCachedReferences();

	TArray<AActor*> FindLockonCandidates(float Radius, AActor* ActorToIgnore = nullptr);
	AActor* FindBestInitialTarget(float Radius);
	AActor* FindBestAutoRetargetTarget(AActor* LostTarget);
	AActor* FindBestDirectionalTarget(float Direction);

	bool IsValidLockonCandidate(AActor* Candidate) const;
	bool HasLineOfSightToCandidate(AActor* Candidate);
	bool IsSelectableSearchCandidate(AActor* Candidate);
	bool PassesCameraAngleCheck(AActor* Candidate, float MinDot);

	void SetCurrentTarget(AActor* NewTarget, bool bCallDeselectOnOldTarget = true);
	void ApplyLockonState();
	void ClearLockonState();
};
