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
	AActor* ResolveBossActor(const FStateTreeExecutionContext& Context, AActor* Actor)
	{
		if (Actor)
		{
			return Actor;
		}

		UObject* Owner = Context.GetOwner();
		if (AAIController* OwnerController = Cast<AAIController>(Owner))
		{
			return OwnerController->GetPawn();
		}

		return Cast<AActor>(Owner);
	}

	UPantheliaBossBrainComponent* ResolveBossBrainComponent(const FStateTreeExecutionContext& Context, AActor* Actor)
	{
		if (AActor* BossActor = ResolveBossActor(Context, Actor))
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

	EStateTreeRunStatus RotateActorYawTowardTarget(AActor* Actor, AActor* TargetActor, const float RotationSpeedDegreesPerSecond, const float AcceptableAngleDegrees, const float DeltaTime)
	{
		if (!Actor || !TargetActor)
		{
			return EStateTreeRunStatus::Failed;
		}

		const FVector ToTarget = TargetActor->GetActorLocation() - Actor->GetActorLocation();
		const FVector ToTarget2D = FVector(ToTarget.X, ToTarget.Y, 0.f);
		if (ToTarget2D.IsNearlyZero())
		{
			return EStateTreeRunStatus::Succeeded;
		}

		const float CurrentYaw = Actor->GetActorRotation().Yaw;
		const float TargetYaw = ToTarget2D.Rotation().Yaw;
		const float YawDelta = FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw);
		if (FMath::Abs(YawDelta) <= AcceptableAngleDegrees)
		{
			return EStateTreeRunStatus::Succeeded;
		}

		if (RotationSpeedDegreesPerSecond <= 0.f)
		{
			FRotator TargetRotation = Actor->GetActorRotation();
			TargetRotation.Yaw = TargetYaw;
			Actor->SetActorRotation(TargetRotation);
			return EStateTreeRunStatus::Succeeded;
		}

		const float MaxYawStep = RotationSpeedDegreesPerSecond * DeltaTime;
		if (FMath::Abs(YawDelta) <= MaxYawStep)
		{
			FRotator TargetRotation = Actor->GetActorRotation();
			TargetRotation.Yaw = TargetYaw;
			Actor->SetActorRotation(TargetRotation);
			return EStateTreeRunStatus::Succeeded;
		}

		FRotator NewRotation = Actor->GetActorRotation();
		NewRotation.Yaw = CurrentYaw + FMath::Clamp(YawDelta, -MaxYawStep, MaxYawStep);
		Actor->SetActorRotation(NewRotation);

		return EStateTreeRunStatus::Running;
	}
}

FPantheliaStateTreeFaceTargetTask::FPantheliaStateTreeFaceTargetTask()
{
	bShouldCallTick = true;
}

EStateTreeRunStatus FPantheliaStateTreeFaceTargetTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AActor* BossActor = ResolveBossActor(Context, InstanceData.Actor);
	AActor* TargetActor = ResolveTargetActor(Context, InstanceData.TargetActor);

	if (!BossActor || !TargetActor)
	{
		return EStateTreeRunStatus::Failed;
	}

	const FVector ToTarget = TargetActor->GetActorLocation() - BossActor->GetActorLocation();
	const FVector ToTarget2D = FVector(ToTarget.X, ToTarget.Y, 0.f);
	if (ToTarget2D.IsNearlyZero())
	{
		return EStateTreeRunStatus::Succeeded;
	}

	const float CurrentYaw = BossActor->GetActorRotation().Yaw;
	const float TargetYaw = ToTarget2D.Rotation().Yaw;
	const float YawDelta = FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw);
	return FMath::Abs(YawDelta) <= InstanceData.AcceptableAngleDegrees
		? EStateTreeRunStatus::Succeeded
		: EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FPantheliaStateTreeFaceTargetTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AActor* BossActor = ResolveBossActor(Context, InstanceData.Actor);
	AActor* TargetActor = ResolveTargetActor(Context, InstanceData.TargetActor);

	return RotateActorYawTowardTarget(BossActor, TargetActor, InstanceData.RotationSpeedDegreesPerSecond, InstanceData.AcceptableAngleDegrees, DeltaTime);
}

FPantheliaStateTreeSelectBossActionTask::FPantheliaStateTreeSelectBossActionTask()
{
	bShouldCallTick = false;
	bShouldCopyBoundPropertiesOnTick = false;
}

EStateTreeRunStatus FPantheliaStateTreeSelectBossActionTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	UPantheliaBossBrainComponent* BossBrain = ResolveBossBrainComponent(Context, InstanceData.Actor);
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
	UPantheliaBossBrainComponent* BossBrain = ResolveBossBrainComponent(Context, InstanceData.Actor);
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
	UPantheliaBossBrainComponent* BossBrain = ResolveBossBrainComponent(Context, InstanceData.Actor);

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
	UPantheliaBossBrainComponent* BossBrain = ResolveBossBrainComponent(Context, InstanceData.Actor);

	if (!BossBrain)
	{
		return EStateTreeRunStatus::Failed;
	}

	BossBrain->ClearCurrentAction();
	return EStateTreeRunStatus::Succeeded;
}
