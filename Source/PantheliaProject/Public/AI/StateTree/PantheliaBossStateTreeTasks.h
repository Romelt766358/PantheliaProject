// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "StateTreeTaskBase.h"
#include "PantheliaBossStateTreeTasks.generated.h"

class AAIController;

USTRUCT()
struct PANTHELIAPROJECT_API FPantheliaBossStateTreeTaskInstanceData
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AActor> Actor = nullptr;

	UPROPERTY(EditAnywhere, Category = Context)
	TObjectPtr<AAIController> AIController = nullptr;

	UPROPERTY(EditAnywhere, Category = Parameter)
	TObjectPtr<AActor> TargetActor = nullptr;
};

USTRUCT(meta = (DisplayName = "Select Boss Action", Category = "Panthelia|Boss"))
struct PANTHELIAPROJECT_API FPantheliaStateTreeSelectBossActionTask : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FPantheliaBossStateTreeTaskInstanceData;

	FPantheliaStateTreeSelectBossActionTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT(meta = (DisplayName = "Execute Selected Boss Action", Category = "Panthelia|Boss"))
struct PANTHELIAPROJECT_API FPantheliaStateTreeExecuteSelectedBossActionTask : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FPantheliaBossStateTreeTaskInstanceData;

	FPantheliaStateTreeExecuteSelectedBossActionTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};

USTRUCT(meta = (DisplayName = "Wait For Boss Action", Category = "Panthelia|Boss"))
struct PANTHELIAPROJECT_API FPantheliaStateTreeWaitForBossActionTask : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FPantheliaBossStateTreeTaskInstanceData;

	FPantheliaStateTreeWaitForBossActionTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const override;
};

USTRUCT(meta = (DisplayName = "Clear Boss Action", Category = "Panthelia|Boss"))
struct PANTHELIAPROJECT_API FPantheliaStateTreeClearBossActionTask : public FStateTreeTaskBase
{
	GENERATED_BODY()

	using FInstanceDataType = FPantheliaBossStateTreeTaskInstanceData;

	FPantheliaStateTreeClearBossActionTask();

	virtual const UStruct* GetInstanceDataType() const override { return FInstanceDataType::StaticStruct(); }
	virtual EStateTreeRunStatus EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const override;
};
