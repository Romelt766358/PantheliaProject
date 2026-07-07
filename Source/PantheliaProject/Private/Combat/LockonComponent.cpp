// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/LockonComponent.h"

#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Interfaces/Enemy.h"
#include "Kismet/KismetMathLibrary.h"

ULockonComponent::ULockonComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULockonComponent::BeginPlay()
{
	Super::BeginPlay();

	RefreshCachedReferences();
}

void ULockonComponent::RefreshCachedReferences()
{
	if (!OwnerRef)
	{
		OwnerRef = Cast<ACharacter>(GetOwner());
	}

	if (!OwnerRef)
	{
		return;
	}

	AController* OwnerController = OwnerRef->GetController();
	if (!Controller || Controller.Get() != OwnerController)
	{
		Controller = Cast<APlayerController>(OwnerController);
	}

	if (!Controller && GetWorld())
	{
		Controller = GetWorld()->GetFirstPlayerController();
	}

	if (!MovementComp)
	{
		MovementComp = OwnerRef->GetCharacterMovement();
	}

	if (!SpringArmComp)
	{
		SpringArmComp = OwnerRef->FindComponentByClass<USpringArmComponent>();
	}
}

void ULockonComponent::StartLockon(float Radius)
{
	RefreshCachedReferences();

	AActor* BestTarget = FindBestInitialTarget(Radius);
	if (!BestTarget)
	{
		return;
	}

	SetCurrentTarget(BestTarget);
}

void ULockonComponent::ToggleLockon(float Radius)
{
	const float CurrentTimeSeconds = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0f;
	if ((CurrentTimeSeconds - LastToggleTimeSeconds) < ToggleDebounceSeconds)
	{
		UE_LOG(LogPanthelia, Warning, TEXT("[LOCKON] Toggle ignored by debounce. Delta=%.3f"), CurrentTimeSeconds - LastToggleTimeSeconds);
		return;
	}

	LastToggleTimeSeconds = CurrentTimeSeconds;

	if (IsValid(CurrentTargetActor))
	{
		EndLockon();
	}
	else
	{
		StartLockon(Radius);
	}
}

void ULockonComponent::SwitchTarget(float Direction)
{
	RefreshCachedReferences();

	if (!IsValid(CurrentTargetActor))
	{
		return;
	}

	if (FMath::IsNearlyZero(Direction))
	{
		return;
	}

	AActor* NewTarget = FindBestDirectionalTarget(Direction);
	if (!NewTarget)
	{
		return;
	}

	SetCurrentTarget(NewTarget);
}

void ULockonComponent::EndLockon()
{
	SetCurrentTarget(nullptr);
}

void ULockonComponent::HandleCurrentTargetLost(AActor* LostTarget)
{
	RefreshCachedReferences();

	if (CurrentTargetActor.Get() != LostTarget)
	{
		return;
	}

	AActor* NewTarget = FindBestAutoRetargetTarget(LostTarget);
	if (NewTarget)
	{
		SetCurrentTarget(NewTarget);
		return;
	}

	EndLockon();
}

void ULockonComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	RefreshCachedReferences();

	// Si el puntero es null no hay target activo — nada que hacer.
	if (CurrentTargetActor == nullptr)
	{
		return;
	}

	if (!IsValid(CurrentTargetActor))
	{
		// El actor fue destruido externamente (SetLifeSpan expiró, etc.)
		// IsValid falla pero el puntero no es null — el actor fue destruido
		// sin pasar por PantheliaEnemy::Die() (caso edge) o el lifespan expiró.
		// NO llamamos a Execute_OnDeselect porque el actor ya no existe — crash.
		// Intentamos retarget automático; si no hay candidato, limpiamos el estado.
		AActor* NewTarget = FindBestAutoRetargetTarget(nullptr);

		CurrentTargetActor = nullptr;

		if (NewTarget)
		{
			SetCurrentTarget(NewTarget, false);
		}
		else
		{
			ClearLockonState();
			OnUpdatedTargetDelegate.Broadcast(nullptr);
		}

		return;
	}

	if (!OwnerRef || !Controller)
	{
		return;
	}

	FVector CurrentLocation{ OwnerRef->GetActorLocation() };
	FVector TargetLocation{ CurrentTargetActor->GetActorLocation() };

	double TargetDistance{ FVector::Distance(CurrentLocation, TargetLocation) };

	if (TargetDistance >= BreakDistance)
	{
		EndLockon();
		return;
	}

	TargetLocation.Z -= 125.0f;

	FRotator NewRotation{ UKismetMathLibrary::FindLookAtRotation(
		CurrentLocation, TargetLocation
	) };

	Controller->SetControlRotation(NewRotation);
}

TArray<AActor*> ULockonComponent::FindLockonCandidates(float Radius, AActor* ActorToIgnore)
{
	RefreshCachedReferences();

	TArray<AActor*> Candidates;

	if (!GetWorld() || !OwnerRef)
	{
		return Candidates;
	}

	const FVector CurrentLocation{ OwnerRef->GetActorLocation() };
	const FCollisionShape Sphere{ FCollisionShape::MakeSphere(Radius) };

	FCollisionQueryParams IgnoreParams{ FName{ TEXT("Ignore Collision Params") }, false, OwnerRef };
	if (ActorToIgnore)
	{
		IgnoreParams.AddIgnoredActor(ActorToIgnore);
	}

	TArray<FHitResult> OutResults;
	const bool bHasFoundTargets = GetWorld()->SweepMultiByChannel(
		OutResults,
		CurrentLocation,
		CurrentLocation,
		FQuat::Identity,
		ECollisionChannel::ECC_GameTraceChannel1,
		Sphere,
		IgnoreParams
	);

	if (!bHasFoundTargets)
	{
		return Candidates;
	}

	for (const FHitResult& OutResult : OutResults)
	{
		AActor* HitActor = OutResult.GetActor();
		if (!IsValidLockonCandidate(HitActor))
		{
			continue;
		}

		if (HitActor == ActorToIgnore)
		{
			continue;
		}

		Candidates.AddUnique(HitActor);
	}

	return Candidates;
}

AActor* ULockonComponent::FindBestInitialTarget(float Radius)
{
	const TArray<AActor*> Candidates = FindLockonCandidates(Radius);

	AActor* BestTarget = nullptr;
	float BestScore = -FLT_MAX;

	if (!Controller || !OwnerRef)
	{
		return nullptr;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	Controller->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FVector CameraForward = CameraRotation.Vector();

	for (AActor* Candidate : Candidates)
	{
		if (!PassesCameraAngleCheck(Candidate, LockonAngleThreshold))
		{
			continue;
		}

		const FVector DirectionToCandidate = (Candidate->GetActorLocation() - CameraLocation).GetSafeNormal();
		const float ForwardDot = FVector::DotProduct(CameraForward, DirectionToCandidate);

		const float Distance = FVector::Distance(OwnerRef->GetActorLocation(), Candidate->GetActorLocation());
		const float DistanceScore = 1.0f - FMath::Clamp(Distance / Radius, 0.0f, 1.0f);

		// Primer lock-on: priorizamos que esté centrado en cámara, y luego cercanía.
		const float Score = (ForwardDot * 0.75f) + (DistanceScore * 0.25f);

		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}

AActor* ULockonComponent::FindBestAutoRetargetTarget(AActor* LostTarget)
{
	const TArray<AActor*> Candidates = FindLockonCandidates(AutoRetargetRadius, LostTarget);

	AActor* BestTarget = nullptr;
	float BestScore = -FLT_MAX;

	if (!Controller || !OwnerRef)
	{
		return nullptr;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	Controller->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FVector CameraForward = CameraRotation.Vector();

	for (AActor* Candidate : Candidates)
	{
		const FVector DirectionToCandidate = (Candidate->GetActorLocation() - CameraLocation).GetSafeNormal();
		const float ForwardDot = FVector::DotProduct(CameraForward, DirectionToCandidate);

		// No saltar automáticamente a enemigos claramente detrás de la cámara.
		if (ForwardDot < 0.0f)
		{
			continue;
		}

		const float Distance = FVector::Distance(OwnerRef->GetActorLocation(), Candidate->GetActorLocation());
		const float DistanceScore = 1.0f - FMath::Clamp(Distance / AutoRetargetRadius, 0.0f, 1.0f);

		// Auto-retarget: priorizamos cercanía, pero evitamos snaps raros detrás o muy fuera de cámara.
		const float Score = (DistanceScore * 0.65f) + (ForwardDot * 0.35f);

		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}

AActor* ULockonComponent::FindBestDirectionalTarget(float Direction)
{
	const TArray<AActor*> Candidates = FindLockonCandidates(SwitchTargetRadius, CurrentTargetActor.Get());

	AActor* BestTarget = nullptr;
	float BestScore = -FLT_MAX;

	if (!Controller || !OwnerRef)
	{
		return nullptr;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	Controller->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FVector CameraForward = CameraRotation.Vector();
	const FVector CameraRight = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Y);

	for (AActor* Candidate : Candidates)
	{
		const FVector DirectionToCandidate = (Candidate->GetActorLocation() - CameraLocation).GetSafeNormal();

		const float ForwardDot = FVector::DotProduct(CameraForward, DirectionToCandidate);
		if (ForwardDot < SwitchForwardThreshold)
		{
			continue;
		}

		const float SideDot = FVector::DotProduct(CameraRight, DirectionToCandidate);
		const float SignedSide = Direction > 0.0f ? SideDot : -SideDot;

		if (SignedSide < SwitchSideThreshold)
		{
			continue;
		}

		const float Distance = FVector::Distance(OwnerRef->GetActorLocation(), Candidate->GetActorLocation());
		const float DistanceScore = 1.0f - FMath::Clamp(Distance / SwitchTargetRadius, 0.0f, 1.0f);

		// Cambio manual: priorizamos el lado solicitado, luego que siga delante,
		// y por último la cercanía para evitar saltos absurdos a targets lejanos.
		const float Score = (SignedSide * 0.65f) + (ForwardDot * 0.25f) + (DistanceScore * 0.10f);

		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}

bool ULockonComponent::IsValidLockonCandidate(AActor* Candidate) const
{
	if (!IsValid(Candidate))
	{
		return false;
	}

	if (Candidate == OwnerRef)
	{
		return false;
	}

	if (!Candidate->Implements<UEnemy>())
	{
		return false;
	}

	return true;
}

bool ULockonComponent::PassesCameraAngleCheck(AActor* Candidate, float MinDot)
{
	RefreshCachedReferences();

	if (!Controller || !Candidate)
	{
		return false;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	Controller->GetPlayerViewPoint(CameraLocation, CameraRotation);

	const FVector CameraForward = CameraRotation.Vector();
	const FVector DirectionToCandidate = (Candidate->GetActorLocation() - CameraLocation).GetSafeNormal();

	const float DotProduct = FVector::DotProduct(CameraForward, DirectionToCandidate);
	return DotProduct >= MinDot;
}

void ULockonComponent::SetCurrentTarget(AActor* NewTarget, bool bCallDeselectOnOldTarget)
{
	if (CurrentTargetActor == NewTarget)
	{
		return;
	}

	if (bCallDeselectOnOldTarget && IsValid(CurrentTargetActor))
	{
		IEnemy::Execute_OnDeselect(CurrentTargetActor.Get());
	}

	CurrentTargetActor = NewTarget;

	if (IsValid(CurrentTargetActor))
	{
		ApplyLockonState();
		IEnemy::Execute_OnSelect(CurrentTargetActor.Get());
		OnUpdatedTargetDelegate.Broadcast(CurrentTargetActor.Get());
	}
	else
	{
		ClearLockonState();
		OnUpdatedTargetDelegate.Broadcast(nullptr);
	}
}

void ULockonComponent::ApplyLockonState()
{
	if (bLockonStateApplied)
	{
		return;
	}

	if (Controller)
	{
		Controller->SetIgnoreLookInput(true);
	}

	if (MovementComp)
	{
		MovementComp->bOrientRotationToMovement = false;
		MovementComp->bUseControllerDesiredRotation = true;
	}

	if (SpringArmComp)
	{
		SpringArmComp->TargetOffset = FVector{ 0.0f, 0.0f, 100.0f };
	}

	bLockonStateApplied = true;
}

void ULockonComponent::ClearLockonState()
{
	if (!bLockonStateApplied)
	{
		return;
	}

	if (MovementComp)
	{
		MovementComp->bOrientRotationToMovement = true;
		MovementComp->bUseControllerDesiredRotation = false;
	}

	if (SpringArmComp)
	{
		SpringArmComp->TargetOffset = FVector::ZeroVector;
	}

	if (Controller)
	{
		Controller->ResetIgnoreLookInput();
	}

	bLockonStateApplied = false;
}
