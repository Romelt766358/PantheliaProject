// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/LockonComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/KismetMathLibrary.h"
#include "GameFramework/SpringArmComponent.h"
#include "Interfaces/Enemy.h"

ULockonComponent::ULockonComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
}

void ULockonComponent::BeginPlay()
{
	Super::BeginPlay();

	OwnerRef = GetOwner<ACharacter>();
	Controller = GetWorld()->GetFirstPlayerController();
	MovementComp = OwnerRef->GetCharacterMovement();
	SpringArmComp = OwnerRef->FindComponentByClass<USpringArmComponent>();
}

void ULockonComponent::StartLockon(float Radius)
{
	FHitResult OutResult;
	FVector CurrentLocation{ OwnerRef->GetActorLocation() };
	FCollisionShape Sphere{ FCollisionShape::MakeSphere(Radius) };
	FCollisionQueryParams IgnoreParams{
		FName{ TEXT("Ignore Collision Params")},
		false,
		OwnerRef
	};

	bool bHasFoundTarget{ GetWorld()->SweepSingleByChannel(
		OutResult,
		CurrentLocation,
		CurrentLocation,
		FQuat::Identity,
		ECollisionChannel::ECC_GameTraceChannel1,
		Sphere,
		IgnoreParams
	) };

	if (!bHasFoundTarget) { return; }

	FVector CameraLocation;
	FRotator CameraRotation;
	Controller->GetPlayerViewPoint(CameraLocation, CameraRotation);
	FVector CameraForward = CameraRotation.Vector();

	FVector EnemyLocation = OutResult.GetActor()->GetActorLocation();
	FVector DirectionToEnemy = (EnemyLocation - CameraLocation).GetSafeNormal();

	double DotProduct = FVector::DotProduct(CameraForward, DirectionToEnemy);

	if (DotProduct < LockonAngleThreshold)
	{
		return;
	}

	if (!OutResult.GetActor()->Implements<UEnemy>()) { return; }

	CurrentTargetActor = OutResult.GetActor();

	Controller->SetIgnoreLookInput(true);
	MovementComp->bOrientRotationToMovement = false;
	MovementComp->bUseControllerDesiredRotation = true;

	SpringArmComp->TargetOffset = FVector{ 0.0f, 0.0f, 100.0f };

	IEnemy::Execute_OnSelect(CurrentTargetActor);

	OnUpdatedTargetDelegate.Broadcast(CurrentTargetActor);
}

void ULockonComponent::EndLockon()
{
	IEnemy::Execute_OnDeselect(CurrentTargetActor);

	CurrentTargetActor = nullptr;

	MovementComp->bOrientRotationToMovement = true;
	MovementComp->bUseControllerDesiredRotation = false;
	SpringArmComp->TargetOffset = FVector::ZeroVector;

	Controller->ResetIgnoreLookInput();

	OnUpdatedTargetDelegate.Broadcast(CurrentTargetActor);
}

void ULockonComponent::ToggleLockon(float Radius)
{
	if (IsValid(CurrentTargetActor))
	{
		EndLockon();
	}
	else
	{
		StartLockon(Radius);
	}
}

void ULockonComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Si el puntero es null no hay target activo — nada que hacer.
	if (CurrentTargetActor == nullptr) { return; }

	if (!IsValid(CurrentTargetActor))
	{
		// El actor fue destruido externamente (SetLifeSpan expiró, etc.)
		// IsValid falla pero el puntero no es null — el actor fue destruido
		// sin pasar por PantheliaEnemy::Die() (caso edge) o el lifespan expiró.
		// NO llamamos a Execute_OnDeselect porque el actor ya no existe — crash.
		// Reseteamos el estado de lockon directamente.
		CurrentTargetActor = nullptr;
		MovementComp->bOrientRotationToMovement = true;
		MovementComp->bUseControllerDesiredRotation = false;
		SpringArmComp->TargetOffset = FVector::ZeroVector;
		Controller->ResetIgnoreLookInput();
		OnUpdatedTargetDelegate.Broadcast(nullptr);
		return;
	}

	FVector CurrentLocation{ OwnerRef->GetActorLocation() };
	FVector TargetLocation{ CurrentTargetActor->GetActorLocation() };

	double TargetDistance{
		FVector::Distance(CurrentLocation, TargetLocation)
	};

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