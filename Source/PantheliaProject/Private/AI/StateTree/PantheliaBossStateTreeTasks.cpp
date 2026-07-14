// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/StateTree/PantheliaBossStateTreeTasks.h"

#include "AIController.h"
#include "AI/PantheliaBossBrainComponent.h"
#include "AI/Data/PantheliaBossProfile.h"
#include "GameFramework/Character.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "PantheliaLogChannels.h"
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

	AAIController* ResolveBossAIController(const FStateTreeExecutionContext& Context, AActor* Actor)
	{
		if (AAIController* OwnerController = Cast<AAIController>(Context.GetOwner()))
		{
			return OwnerController;
		}

		if (APawn* BossPawn = Cast<APawn>(ResolveBossActor(Context, Actor)))
		{
			return Cast<AAIController>(BossPawn->GetController());
		}

		return nullptr;
	}

	float GetDistanceToTarget2D(const AActor* Actor, const AActor* TargetActor)
	{
		if (!Actor || !TargetActor)
		{
			return TNumericLimits<float>::Max();
		}

		return FVector::Dist2D(Actor->GetActorLocation(), TargetActor->GetActorLocation());
	}

	void StopApproachMovement(AAIController* AIController, FPantheliaApproachOrRetryStateTreeTaskInstanceData& InstanceData)
	{
		if (AIController && InstanceData.bMoveRequested)
		{
			AIController->StopMovement();
		}

		InstanceData.bMoveRequested = false;
	}

	EPathFollowingRequestResult::Type RequestApproachMove(
		AAIController* AIController,
		AActor* TargetActor,
		const FPantheliaApproachOrRetryStateTreeTaskInstanceData& InstanceData)
	{
		if (!AIController || !TargetActor)
		{
			return EPathFollowingRequestResult::Failed;
		}

		return AIController->MoveToActor(
			TargetActor,
			InstanceData.AcceptanceRadius,
			InstanceData.bStopOnOverlap,
			InstanceData.bUsePathfinding,
			InstanceData.bCanStrafe,
			nullptr,
			InstanceData.bAllowPartialPath);
	}

	EStateTreeRunStatus SelectBossActionAfterFacing(
		AActor* BossActor,
		AActor* TargetActor,
		UPantheliaBossBrainComponent* BossBrain)
	{
		if (!BossActor || !TargetActor || !BossBrain)
		{
			return EStateTreeRunStatus::Failed;
		}

		FPantheliaBossActionDefinition SelectedAction;
		return BossBrain->SelectAction(TargetActor, SelectedAction)
			? EStateTreeRunStatus::Succeeded
			: EStateTreeRunStatus::Failed;
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
	UPantheliaBossBrainComponent* BossBrain = ResolveBossBrainComponent(Context, InstanceData.Actor);

	if (!BossActor || !TargetActor || !BossBrain || BossBrain->IsOwnerDead())
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
	UPantheliaBossBrainComponent* BossBrain = ResolveBossBrainComponent(Context, InstanceData.Actor);

	if (!BossBrain || BossBrain->IsOwnerDead())
	{
		return EStateTreeRunStatus::Failed;
	}

	return RotateActorYawTowardTarget(BossActor, TargetActor, InstanceData.RotationSpeedDegreesPerSecond, InstanceData.AcceptableAngleDegrees, DeltaTime);
}

FPantheliaStateTreePrepareBossActionTask::FPantheliaStateTreePrepareBossActionTask()
{
	bShouldCallTick = true;
}

EStateTreeRunStatus FPantheliaStateTreePrepareBossActionTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AActor* BossActor = ResolveBossActor(Context, InstanceData.Actor);
	AActor* TargetActor = ResolveTargetActor(Context, InstanceData.TargetActor);
	UPantheliaBossBrainComponent* BossBrain = ResolveBossBrainComponent(Context, InstanceData.Actor);

	if (!BossActor || !TargetActor || !BossBrain || BossBrain->IsOwnerDead())
	{
		return EStateTreeRunStatus::Failed;
	}

	const FVector ToTarget = TargetActor->GetActorLocation() - BossActor->GetActorLocation();
	const FVector ToTarget2D = FVector(ToTarget.X, ToTarget.Y, 0.f);
	if (ToTarget2D.IsNearlyZero())
	{
		return SelectBossActionAfterFacing(BossActor, TargetActor, BossBrain);
	}

	const float CurrentYaw = BossActor->GetActorRotation().Yaw;
	const float TargetYaw = ToTarget2D.Rotation().Yaw;
	const float YawDelta = FMath::FindDeltaAngleDegrees(CurrentYaw, TargetYaw);
	if (FMath::Abs(YawDelta) <= InstanceData.AcceptableAngleDegrees)
	{
		return SelectBossActionAfterFacing(BossActor, TargetActor, BossBrain);
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FPantheliaStateTreePrepareBossActionTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AActor* BossActor = ResolveBossActor(Context, InstanceData.Actor);
	AActor* TargetActor = ResolveTargetActor(Context, InstanceData.TargetActor);
	UPantheliaBossBrainComponent* BossBrain = ResolveBossBrainComponent(Context, InstanceData.Actor);

	if (!BossBrain || BossBrain->IsOwnerDead())
	{
		return EStateTreeRunStatus::Failed;
	}

	const EStateTreeRunStatus FacingStatus = RotateActorYawTowardTarget(
		BossActor,
		TargetActor,
		InstanceData.RotationSpeedDegreesPerSecond,
		InstanceData.AcceptableAngleDegrees,
		DeltaTime);

	if (FacingStatus != EStateTreeRunStatus::Succeeded)
	{
		return FacingStatus;
	}

	return SelectBossActionAfterFacing(BossActor, TargetActor, BossBrain);
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

	if (!BossBrain || !TargetActor || BossBrain->IsOwnerDead())
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

	if (!BossBrain || BossBrain->IsOwnerDead())
	{
		return EStateTreeRunStatus::Failed;
	}

	BossBrain->RefreshActionRuntimeState();

	if (!BossBrain->IsActionTerminal())
	{
		return EStateTreeRunStatus::Running;
	}

	if (BossBrain->HasActionFailed())
	{
		return EStateTreeRunStatus::Failed;
	}

	if (BossBrain->WasActionInterrupted() && BossBrain->IsInterruptionRecoveryActive())
	{
		// La ability ofensiva ya terminó, pero el boss todavía está ejecutando
		// HitReact/Stagger. El StateTree no debe entrar en Recover ni reseleccionar
		// hasta que esa reacción haya terminado.
		return EStateTreeRunStatus::Running;
	}

	// La task de espera cumplió su trabajo tanto para Finished como para Interrupted.
	// La semántica de éxito de la ACCIÓN permanece en BossBrain::DidActionSucceed().
	return EStateTreeRunStatus::Succeeded;
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

FPantheliaStateTreeApproachOrRetryTask::FPantheliaStateTreeApproachOrRetryTask()
{
	bShouldCallTick = true;
}

EStateTreeRunStatus FPantheliaStateTreeApproachOrRetryTask::EnterState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AActor* BossActor = ResolveBossActor(Context, InstanceData.Actor);
	AActor* TargetActor = ResolveTargetActor(Context, InstanceData.TargetActor);
	AAIController* AIController = ResolveBossAIController(Context, InstanceData.Actor);
	UPantheliaBossBrainComponent* BossBrain = ResolveBossBrainComponent(Context, InstanceData.Actor);

	InstanceData.ElapsedTime = 0.f;
	InstanceData.RetryElapsedTime = 0.f;
	InstanceData.bMoveRequested = false;
	InstanceData.bWaitingToRetry = false;
	InstanceData.bTechnicalFailureWait = false;

	if (!BossActor || !TargetActor || !BossBrain || BossBrain->IsOwnerDead())
	{
		if (InstanceData.bLogLocomotion)
		{
			UE_LOG(LogPanthelia, Warning, TEXT("[BOSS LOCOMOTION] Boss=%s Mode=InvalidContext Target=%s Brain=%s"),
				*GetNameSafe(BossActor),
				*GetNameSafe(TargetActor),
				*GetNameSafe(BossBrain));
		}

		return EStateTreeRunStatus::Failed;
	}

	const EPantheliaBossActionFailureReason FailureReason = BossBrain->GetLastFailureReason();
	BossBrain->ClearCurrentAction();

	if (FailureReason != EPantheliaBossActionFailureReason::ActionUnavailable)
	{
		InstanceData.bWaitingToRetry = true;
		InstanceData.bTechnicalFailureWait = true;

		if (InstanceData.bLogLocomotion)
		{
			UE_LOG(LogPanthelia, Warning, TEXT("[BOSS LOCOMOTION] Boss=%s Mode=TechnicalRetry Reason=%s Delay=%.2f"),
				*GetNameSafe(BossActor),
				*StaticEnum<EPantheliaBossActionFailureReason>()->GetNameStringByValue(static_cast<int64>(FailureReason)),
				InstanceData.TechnicalFailureRetryDelay);
		}

		return InstanceData.TechnicalFailureRetryDelay <= 0.f
			? EStateTreeRunStatus::Succeeded
			: EStateTreeRunStatus::Running;
	}

	const float Distance = GetDistanceToTarget2D(BossActor, TargetActor);
	if (Distance <= InstanceData.AcceptanceRadius)
	{
		InstanceData.bWaitingToRetry = true;

		if (InstanceData.bLogLocomotion)
		{
			UE_LOG(LogPanthelia, Log, TEXT("[BOSS LOCOMOTION] Boss=%s Mode=RetryWait Distance=%.1f Acceptance=%.1f Delay=%.2f"),
				*GetNameSafe(BossActor),
				Distance,
				InstanceData.AcceptanceRadius,
				InstanceData.RetryDelay);
		}

		return InstanceData.RetryDelay <= 0.f
			? EStateTreeRunStatus::Succeeded
			: EStateTreeRunStatus::Running;
	}

	if (!AIController)
	{
		InstanceData.bWaitingToRetry = true;
		InstanceData.bTechnicalFailureWait = true;

		if (InstanceData.bLogLocomotion)
		{
			UE_LOG(LogPanthelia, Warning, TEXT("[BOSS LOCOMOTION] Boss=%s Mode=MissingAIController Delay=%.2f"),
				*GetNameSafe(BossActor),
				InstanceData.TechnicalFailureRetryDelay);
		}

		return EStateTreeRunStatus::Running;
	}

	const EPathFollowingRequestResult::Type MoveResult = RequestApproachMove(AIController, TargetActor, InstanceData);
	if (MoveResult == EPathFollowingRequestResult::RequestSuccessful)
	{
		InstanceData.bMoveRequested = true;

		if (InstanceData.bLogLocomotion)
		{
			UE_LOG(LogPanthelia, Log, TEXT("[BOSS LOCOMOTION] Boss=%s Mode=MoveStart Target=%s Distance=%.1f Acceptance=%.1f Pathfinding=%s"),
				*GetNameSafe(BossActor),
				*GetNameSafe(TargetActor),
				Distance,
				InstanceData.AcceptanceRadius,
				InstanceData.bUsePathfinding ? TEXT("true") : TEXT("false"));
		}

		return EStateTreeRunStatus::Running;
	}

	if (MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		InstanceData.bWaitingToRetry = true;
		return InstanceData.RetryDelay <= 0.f
			? EStateTreeRunStatus::Succeeded
			: EStateTreeRunStatus::Running;
	}

	InstanceData.bWaitingToRetry = true;
	InstanceData.bTechnicalFailureWait = true;

	if (InstanceData.bLogLocomotion)
	{
		UE_LOG(LogPanthelia, Warning, TEXT("[BOSS LOCOMOTION] Boss=%s Mode=MoveRequestFailed Target=%s Distance=%.1f Pathfinding=%s Hint=CheckNavMeshBoundsVolume"),
			*GetNameSafe(BossActor),
			*GetNameSafe(TargetActor),
			Distance,
			InstanceData.bUsePathfinding ? TEXT("true") : TEXT("false"));
	}

	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FPantheliaStateTreeApproachOrRetryTask::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	AActor* BossActor = ResolveBossActor(Context, InstanceData.Actor);
	AActor* TargetActor = ResolveTargetActor(Context, InstanceData.TargetActor);
	AAIController* AIController = ResolveBossAIController(Context, InstanceData.Actor);
	UPantheliaBossBrainComponent* BossBrain = ResolveBossBrainComponent(Context, InstanceData.Actor);

	if (!BossActor || !TargetActor || !BossBrain || BossBrain->IsOwnerDead())
	{
		StopApproachMovement(AIController, InstanceData);
		return EStateTreeRunStatus::Failed;
	}

	InstanceData.ElapsedTime += DeltaTime;
	const float Distance = GetDistanceToTarget2D(BossActor, TargetActor);

	if (InstanceData.bWaitingToRetry)
	{
		InstanceData.RetryElapsedTime += DeltaTime;

		if (!InstanceData.bTechnicalFailureWait
			&& Distance > InstanceData.AcceptanceRadius + InstanceData.ResumeMoveDistanceHysteresis)
		{
			InstanceData.bWaitingToRetry = false;
			InstanceData.RetryElapsedTime = 0.f;

			if (!AIController)
			{
				InstanceData.bWaitingToRetry = true;
				InstanceData.bTechnicalFailureWait = true;
				return EStateTreeRunStatus::Running;
			}

			const EPathFollowingRequestResult::Type MoveResult = RequestApproachMove(AIController, TargetActor, InstanceData);
			if (MoveResult == EPathFollowingRequestResult::RequestSuccessful)
			{
				InstanceData.bMoveRequested = true;

				if (InstanceData.bLogLocomotion)
				{
					UE_LOG(LogPanthelia, Log, TEXT("[BOSS LOCOMOTION] Boss=%s Mode=MoveResume Distance=%.1f Acceptance=%.1f"),
						*GetNameSafe(BossActor),
						Distance,
						InstanceData.AcceptanceRadius);
				}

				return EStateTreeRunStatus::Running;
			}

			InstanceData.bWaitingToRetry = true;
			InstanceData.bTechnicalFailureWait = true;
			return EStateTreeRunStatus::Running;
		}

		const float RequiredDelay = InstanceData.bTechnicalFailureWait
			? InstanceData.TechnicalFailureRetryDelay
			: InstanceData.RetryDelay;

		return InstanceData.RetryElapsedTime >= RequiredDelay
			? EStateTreeRunStatus::Succeeded
			: EStateTreeRunStatus::Running;
	}

	if (Distance <= InstanceData.AcceptanceRadius)
	{
		StopApproachMovement(AIController, InstanceData);
		InstanceData.bWaitingToRetry = true;
		InstanceData.RetryElapsedTime = 0.f;

		if (InstanceData.bLogLocomotion)
		{
			UE_LOG(LogPanthelia, Log, TEXT("[BOSS LOCOMOTION] Boss=%s Mode=Reached Distance=%.1f Acceptance=%.1f RetryDelay=%.2f"),
				*GetNameSafe(BossActor),
				Distance,
				InstanceData.AcceptanceRadius,
				InstanceData.RetryDelay);
		}

		return InstanceData.RetryDelay <= 0.f
			? EStateTreeRunStatus::Succeeded
			: EStateTreeRunStatus::Running;
	}

	if (InstanceData.MaxMoveDuration > 0.f && InstanceData.ElapsedTime >= InstanceData.MaxMoveDuration)
	{
		StopApproachMovement(AIController, InstanceData);
		InstanceData.bWaitingToRetry = true;
		InstanceData.bTechnicalFailureWait = true;
		InstanceData.RetryElapsedTime = 0.f;

		if (InstanceData.bLogLocomotion)
		{
			UE_LOG(LogPanthelia, Warning, TEXT("[BOSS LOCOMOTION] Boss=%s Mode=MoveTimeout Distance=%.1f MaxDuration=%.2f"),
				*GetNameSafe(BossActor),
				Distance,
				InstanceData.MaxMoveDuration);
		}

		return EStateTreeRunStatus::Running;
	}

	if (!AIController)
	{
		InstanceData.bWaitingToRetry = true;
		InstanceData.bTechnicalFailureWait = true;
		InstanceData.RetryElapsedTime = 0.f;
		return EStateTreeRunStatus::Running;
	}

	if (AIController->GetMoveStatus() == EPathFollowingStatus::Idle)
	{
		const EPathFollowingRequestResult::Type MoveResult = RequestApproachMove(AIController, TargetActor, InstanceData);
		if (MoveResult == EPathFollowingRequestResult::RequestSuccessful)
		{
			InstanceData.bMoveRequested = true;
		}
		else if (MoveResult == EPathFollowingRequestResult::AlreadyAtGoal)
		{
			StopApproachMovement(AIController, InstanceData);
			InstanceData.bWaitingToRetry = true;
			InstanceData.RetryElapsedTime = 0.f;
		}
		else
		{
			StopApproachMovement(AIController, InstanceData);
			InstanceData.bWaitingToRetry = true;
			InstanceData.bTechnicalFailureWait = true;
			InstanceData.RetryElapsedTime = 0.f;

			if (InstanceData.bLogLocomotion)
			{
				UE_LOG(LogPanthelia, Warning, TEXT("[BOSS LOCOMOTION] Boss=%s Mode=MoveLost Retry=Technical Distance=%.1f"),
					*GetNameSafe(BossActor),
					Distance);
			}
		}
	}

	return EStateTreeRunStatus::Running;
}

void FPantheliaStateTreeApproachOrRetryTask::ExitState(FStateTreeExecutionContext& Context, const FStateTreeTransitionResult& Transition) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	StopApproachMovement(ResolveBossAIController(Context, InstanceData.Actor), InstanceData);
}

void FPantheliaStateTreeApproachOrRetryTask::StateCompleted(
	FStateTreeExecutionContext& Context,
	const EStateTreeRunStatus CompletionStatus,
	const FStateTreeActiveStates& CompletedActiveStates) const
{
	FInstanceDataType& InstanceData = Context.GetInstanceData(*this);
	StopApproachMovement(ResolveBossAIController(Context, InstanceData.Actor), InstanceData);
}

