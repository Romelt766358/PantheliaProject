// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/StateTree/PantheliaBossStateTreeTasks.h"

#include "AIController.h"
#include "AI/PantheliaBossBrainComponent.h"
#include "AI/Data/PantheliaBossProfile.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "StateTreeExecutionContext.h"

namespace
{
	AActor* ResolveBossActor(const FStateTreeExecutionContext& Context, AActor* Actor, AAIController* AIController)
	{
		if (Actor)
		{
			return Actor;
		}

		if (AIController)
		{
			return AIController->GetPawn();
		}

		UObject* Owner = Context.GetOwner();
		if (AAIController* OwnerController = Cast<AAIController>(Owner))
		{
			return OwnerController->GetPawn();
		}

		return Cast<AActor>(Owner);
	}

	UPantheliaBossBrainComponent* ResolveBossBrainComponent(const FStateTreeExecutionContext& Context, AActor* Actor, AAIController* AIController)
	{
		if (AActor* BossActor = ResolveBossActor(Context, Actor, AIController))
		{
			return BossActor->FindComponentByClass<UPantheliaBossBrainComponent>();
		}

		return nullptr;
	}

	AActor* ResolveTargetActor(const FStateTreeExecutionContext& Context, AActor* TargetActor)
	{
		if (TargetActor)
		{
			return TargetActor;
		}

		// Temporary fallback: final integration should provide TargetActor from StateTree/Blackboard/Context binding.
		return UGameplayStatics::GetPlayerCharacter(Context.GetWorld(), 0);
	}
}

FPantheliaStateTreeSelectBossActionTask::FPantheliaStateTreeSelectBossActionTask()
{
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = false;
}

EStateTreeRunStatus FPantheliaStateTreeSelectBossActionTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UPantheliaBossBrainComponent* BossBrain = ResolveBossBrainComponent(Context, InstanceData.Actor, InstanceData.AIController);
	AActor* TargetActor = ResolveTargetActor(Context, InstanceData.TargetActor);

	if (!BossBrain || !TargetActor)
	{
		return EStateTreeRunStatus::Failed;
	}

	FPantheliaBossActionDefinition SelectedAction;
	return BossBrain->SelectAction(TargetActor, SelectedAction)
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Failed;
}

FPantheliaStateTreeExecuteSelectedBossActionTask::FPantheliaStateTreeExecuteSelectedBossActionTask()
{
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = false;
}

EStateTreeRunStatus FPantheliaStateTreeExecuteSelectedBossActionTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UPantheliaBossBrainComponent* BossBrain = ResolveBossBrainComponent(Context, InstanceData.Actor, InstanceData.AIController);
	AActor* TargetActor = ResolveTargetActor(Context, InstanceData.TargetActor);

	if (!BossBrain || !TargetActor || !BossBrain->HasSelectedAction())
	{
		return EStateTreeRunStatus::Failed;
	}

	const FGameplayTag CurrentActionTag = BossBrain->GetCurrentActionTag();
	if (!CurrentActionTag.IsValid())
	{
		return EStateTreeRunStatus::Failed;
	}

	return BossBrain->TryExecuteAction(TargetActor, CurrentActionTag)
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Failed;
}

FPantheliaStateTreeWaitForBossActionTask::FPantheliaStateTreeWaitForBossActionTask()
{
	bShouldCallTick = true;
}

EStateTreeRunStatus FPantheliaStateTreeWaitForBossActionTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UPantheliaBossBrainComponent* BossBrain = ResolveBossBrainComponent(Context, InstanceData.Actor, InstanceData.AIController);

	if (!BossBrain)
	{
		return EStateTreeRunStatus::Failed;
	}

	BossBrain->RefreshActionRuntimeState();

	if (BossBrain->HasActionFinished())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	if (BossBrain->HasActionFailed())
	{
		return EStateTreeRunStatus::Failed;
	}

	return EStateTreeRunStatus::Running;
}

FPantheliaStateTreeClearBossActionTask::FPantheliaStateTreeClearBossActionTask()
{
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = false;
}

EStateTreeRunStatus FPantheliaStateTreeClearBossActionTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UPantheliaBossBrainComponent* BossBrain = ResolveBossBrainComponent(Context, InstanceData.Actor, InstanceData.AIController);

	if (!BossBrain)
	{
		return EStateTreeRunStatus::Failed;
	}

	BossBrain->ClearCurrentAction();
	return EStateTreeRunStatus::Succeeded;
}
