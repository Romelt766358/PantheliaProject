// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "PantheliaBossStateTreeTasks.generated.h"

USTRUCT()
struct PANTHELIAPROJECT_API FPantheliaBossStateTreeTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<AActor> TargetActor = nullptr;
};

USTRUCT()
struct PANTHELIAPROJECT_API FPantheliaFaceTargetStateTreeTaskInstanceData : public FPantheliaBossStateTreeTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RotationSpeedDegreesPerSecond = 540.f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AcceptableAngleDegrees = 5.f;
};

USTRUCT()
struct PANTHELIAPROJECT_API FPantheliaApproachOrRetryStateTreeTaskInstanceData : public FPantheliaBossStateTreeTaskInstanceData
{
	GENERATED_BODY()

	// Distancia a la que termina la persecución y se vuelve a intentar preparar una acción.
	// Para el WarriorBoss debe quedar dentro del rango de sus ataques melee normales.
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float AcceptanceRadius = 300.f;

	// Histeresis para que el boss no alterne Move/Wait cada frame cuando el jugador
	// se mueve alrededor del límite de AcceptanceRadius.
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float ResumeMoveDistanceHysteresis = 75.f;

	// Espera corta cuando ya está en rango pero ninguna acción está disponible
	// por cooldown, tags, línea de visión o memoria.
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float RetryDelay = 0.20f;

	// Fallos técnicos no deben producir un bucle por frame. Se reintentan más lento
	// y quedan visibles en el log para poder corregir el asset o configuración.
	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float TechnicalFailureRetryDelay = 1.0f;

	UPROPERTY(EditAnywhere, Category = Parameter, meta = (ClampMin = "0.0", UIMin = "0.0"))
	float MaxMoveDuration = 8.0f;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bUsePathfinding = true;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bAllowPartialPath = true;

	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bStopOnOverlap = false;

	// Mantener false mientras el boss no tenga locomoción lateral/strafe dedicada.
	UPROPERTY(EditAnywhere, Category = Parameter)
	bool bCanStrafe = false;

	UPROPERTY(EditAnywhere, Category = Debug)
	bool bLogLocomotion = true;

	UPROPERTY(Transient)
	float ElapsedTime = 0.f;

	UPROPERTY(Transient)
	float RetryElapsedTime = 0.f;

	UPROPERTY(Transient)
	bool bMoveRequested = false;

	UPROPERTY(Transient)
	bool bWaitingToRetry = false;

	UPROPERTY(Transient)
	bool bTechnicalFailureWait = false;
};

USTRUCT(meta = (DisplayName = "Face Target", Category = "Panthelia|Boss"))
struct PANTHELIAPROJECT_API FPantheliaStateTreeFaceTargetTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FPantheliaFaceTargetStateTreeTaskInstanceData;

	FPantheliaStateTreeFaceTargetTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

// Esta task secuencia correctamente Face Target -> Select Boss Action dentro de
// una sola task. Las tasks de un mismo estado de StateTree empiezan en paralelo;
// por eso no se debe asumir que dos tasks hermanas se ejecutan una después de otra.
USTRUCT(meta = (DisplayName = "Prepare Boss Action", Category = "Panthelia|Boss"))
struct PANTHELIAPROJECT_API FPantheliaStateTreePrepareBossActionTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FPantheliaFaceTargetStateTreeTaskInstanceData;

	FPantheliaStateTreePrepareBossActionTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

USTRUCT(meta = (DisplayName = "Select Boss Action", Category = "Panthelia|Boss"))
struct PANTHELIAPROJECT_API FPantheliaStateTreeSelectBossActionTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FPantheliaBossStateTreeTaskInstanceData;

	FPantheliaStateTreeSelectBossActionTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT(meta = (DisplayName = "Execute Selected Boss Action", Category = "Panthelia|Boss"))
struct PANTHELIAPROJECT_API FPantheliaStateTreeExecuteSelectedBossActionTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FPantheliaBossStateTreeTaskInstanceData;

	FPantheliaStateTreeExecuteSelectedBossActionTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT(meta = (DisplayName = "Wait For Boss Action", Category = "Panthelia|Boss"))
struct PANTHELIAPROJECT_API FPantheliaStateTreeWaitForBossActionTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FPantheliaBossStateTreeTaskInstanceData;

	FPantheliaStateTreeWaitForBossActionTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

USTRUCT(meta = (DisplayName = "Clear Boss Action", Category = "Panthelia|Boss"))
struct PANTHELIAPROJECT_API FPantheliaStateTreeClearBossActionTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FPantheliaBossStateTreeTaskInstanceData;

	FPantheliaStateTreeClearBossActionTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT(meta = (DisplayName = "Approach Or Retry", Category = "Panthelia|Boss"))
struct PANTHELIAPROJECT_API FPantheliaStateTreeApproachOrRetryTask : public FStateTreeTaskCommonBase
{
	GENERATED_BODY()

	using FInstanceDataType = FPantheliaApproachOrRetryStateTreeTaskInstanceData;

	FPantheliaStateTreeApproachOrRetryTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
	virtual void ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
	virtual void StateCompleted(FStateTreeExecutionContext& Context, const EStateTreeRunStatus CompletionStatus, const FStateTreeActiveStates& CompletedActiveStates) const override;
};
