// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/LockonComponent.h"

#include "PantheliaLogChannels.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/SpringArmComponent.h"
#include "Interfaces/CombatInterface.h"
#include "Interfaces/Enemy.h"
#include "Kismet/KismetMathLibrary.h"

ULockonComponent::ULockonComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	PrimaryComponentTick.bStartWithTickEnabled = false;
}

void ULockonComponent::BeginPlay()
{
	Super::BeginPlay();

	RefreshCachedReferences();
	SetComponentTickEnabled(false);
}

void ULockonComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	// EndPlay puede ocurrir sin que el target ejecute OnDeselect. Solo restauramos
	// el estado que pertenece a este componente y evitamos tocar actores destruidos.
	CurrentTargetActor = nullptr;
	ClearLockonState();
	SetComponentTickEnabled(false);

	Super::EndPlay(EndPlayReason);
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

// =========================
// API pública / flujo principal
// =========================

void ULockonComponent::StartLockon(float RadiusOverride)
{
	RefreshCachedReferences();

	const float SearchRadius = RadiusOverride > 0.0f
		? RadiusOverride
		: InitialLockonRadius;

	if (!FMath::IsFinite(SearchRadius) || SearchRadius <= 0.0f)
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[LOCKON] Radio inicial inválido: %.3f en %s."),
			SearchRadius,
			*GetNameSafe(this));
		return;
	}

	AActor* BestTarget = FindBestInitialTarget(SearchRadius);
	if (!BestTarget)
	{
		return;
	}

	SetCurrentTarget(BestTarget);
}

void ULockonComponent::ToggleLockon(float RadiusOverride)
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
		StartLockon(RadiusOverride);
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

bool ULockonComponent::TryAutoLockOnFromBasicAttackHit(AActor* HitActor)
{
	if (!bAutoLockOnFromBasicAttackHit)
	{
		return false;
	}

	// Si ya hay lock-on activo, no tocamos nada. Esta es la regla clave:
	// golpear a B mientras el jugador está fijado a A NO debe cambiar el target.
	if (IsValid(CurrentTargetActor))
	{
		return false;
	}

	RefreshCachedReferences();

	if (!IsSelectableSearchCandidate(HitActor))
	{
		return false;
	}

	SetCurrentTarget(HitActor);
	return true;
}

void ULockonComponent::SetAutoLockOnFromBasicAttackHitEnabled(bool bEnabled)
{
	bAutoLockOnFromBasicAttackHit = bEnabled;
}

bool ULockonComponent::IsAutoLockOnFromBasicAttackHitEnabled() const
{
	return bAutoLockOnFromBasicAttackHit;
}

void ULockonComponent::SetSoftLockOnMeleeAttacksEnabled(bool bEnabled)
{
	bSoftLockOnMeleeAttacks = bEnabled;
}

bool ULockonComponent::IsSoftLockOnMeleeAttacksEnabled() const
{
	return bSoftLockOnMeleeAttacks;
}

// =========================
// Soft-lock melee
// =========================

AActor* ULockonComponent::FindBestSoftLockTarget(float RadiusOverride)
{
	if (!bSoftLockOnMeleeAttacks)
	{
		return nullptr;
	}

	// Soft-lock solo asiste ataques SIN lock-on duro activo. Si ya existe
	// CurrentTargetActor, la rotación del ataque debe respetar ese target.
	if (IsValid(CurrentTargetActor))
	{
		return nullptr;
	}

	RefreshCachedReferences();

	if (!OwnerRef)
	{
		return nullptr;
	}

	const float SearchRadius = RadiusOverride > 0.0f ? RadiusOverride : SoftLockRadius;
	const TArray<AActor*> Candidates = FindLockonCandidates(SearchRadius);

	AActor* BestTarget = nullptr;
	float BestScore = -FLT_MAX;

	const FVector OwnerLocation = OwnerRef->GetActorLocation();
	FVector OwnerForward = OwnerRef->GetActorForwardVector();
	OwnerForward.Z = 0.0f;
	OwnerForward.Normalize();

	for (AActor* Candidate : Candidates)
	{
		FVector ToCandidate = GetLockonLocation(Candidate) - OwnerLocation;
		ToCandidate.Z = 0.0f;

		if (ToCandidate.IsNearlyZero())
		{
			continue;
		}

		const float Distance = ToCandidate.Size();
		const FVector DirectionToCandidate = ToCandidate / Distance;
		const float ForwardDot = FVector::DotProduct(OwnerForward, DirectionToCandidate);

		if (ForwardDot < SoftLockForwardThreshold)
		{
			continue;
		}

		const float DistanceScore = 1.0f - FMath::Clamp(Distance / SearchRadius, 0.0f, 1.0f);

		// Soft-lock melee: priorizamos que esté frente al personaje, con cercanía
		// como segundo factor. No queremos atraer un básico a un enemigo lateral lejano.
		const float Score = (ForwardDot * 0.70f) + (DistanceScore * 0.30f);

		if (Score > BestScore)
		{
			BestScore = Score;
			BestTarget = Candidate;
		}
	}

	return BestTarget;
}

AActor* ULockonComponent::FindNearestTarget(const float RadiusOverride)
{
	RefreshCachedReferences();

	if (!OwnerRef)
	{
		return nullptr;
	}

	const float SearchRadius =
		RadiusOverride > 0.0f ? RadiusOverride : InitialLockonRadius;
	const TArray<AActor*> Candidates = FindLockonCandidates(SearchRadius);

	AActor* NearestTarget = nullptr;
	float NearestDistanceSquared = TNumericLimits<float>::Max();
	const FVector OwnerLocation = OwnerRef->GetActorLocation();

	for (AActor* Candidate : Candidates)
	{
		const float DistanceSquared = FVector::DistSquared(
			OwnerLocation,
			GetLockonLocation(Candidate));

		if (DistanceSquared < NearestDistanceSquared)
		{
			NearestDistanceSquared = DistanceSquared;
			NearestTarget = Candidate;
		}
	}

	return NearestTarget;
}

// =========================
// Tick / cámara
// =========================

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
			SetComponentTickEnabled(false);
			OnUpdatedTargetDelegate.Broadcast(nullptr);
		}

		return;
	}

	if (!OwnerRef || !Controller)
	{
		return;
	}

	FVector CurrentLocation{ OwnerRef->GetActorLocation() };
	FVector TargetLocation{ GetLockonLocation(CurrentTargetActor.Get()) };

	double TargetDistance{ FVector::Distance(CurrentLocation, TargetLocation) };

	if (TargetDistance >= BreakDistance)
	{
		EndLockon();
		return;
	}

	if (ShouldBreakLockonFromBlockedLineOfSight(DeltaTime))
	{
		EndLockon();
		return;
	}

	TargetLocation.Z += LockonCameraTargetVerticalOffset;

	FRotator DesiredRotation{ UKismetMathLibrary::FindLookAtRotation(
		CurrentLocation, TargetLocation
	) };

	if (bSmoothLockonCameraRotation && LockonCameraRotationInterpSpeed > 0.0f)
	{
		const FRotator CurrentRotation = Controller->GetControlRotation();
		DesiredRotation = FMath::RInterpTo(CurrentRotation, DesiredRotation, DeltaTime, LockonCameraRotationInterpSpeed);
	}

	Controller->SetControlRotation(DesiredRotation);
}

// =========================
// Búsqueda y scoring de targets
// =========================

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
		if (HitActor == ActorToIgnore)
		{
			continue;
		}

		if (!IsSelectableSearchCandidate(HitActor))
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

	if (!OwnerRef)
	{
		return nullptr;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	if (!GetCameraViewPoint(CameraLocation, CameraRotation))
	{
		return nullptr;
	}

	const FVector CameraForward = CameraRotation.Vector();

	for (AActor* Candidate : Candidates)
	{
		if (!PassesCameraAngleCheck(Candidate, LockonAngleThreshold))
		{
			continue;
		}

		const FVector DirectionToCandidate = (GetLockonLocation(Candidate) - CameraLocation).GetSafeNormal();
		const float ForwardDot = FVector::DotProduct(CameraForward, DirectionToCandidate);

		const float Distance = FVector::Distance(OwnerRef->GetActorLocation(), GetLockonLocation(Candidate));
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

	if (!OwnerRef)
	{
		return nullptr;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	if (!GetCameraViewPoint(CameraLocation, CameraRotation))
	{
		return nullptr;
	}

	const FVector CameraForward = CameraRotation.Vector();

	for (AActor* Candidate : Candidates)
	{
		const FVector DirectionToCandidate = (GetLockonLocation(Candidate) - CameraLocation).GetSafeNormal();
		const float ForwardDot = FVector::DotProduct(CameraForward, DirectionToCandidate);

		// No saltar automáticamente a enemigos claramente detrás de la cámara.
		if (ForwardDot < 0.0f)
		{
			continue;
		}

		const float Distance = FVector::Distance(OwnerRef->GetActorLocation(), GetLockonLocation(Candidate));
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

	if (!OwnerRef)
	{
		return nullptr;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	if (!GetCameraViewPoint(CameraLocation, CameraRotation))
	{
		return nullptr;
	}

	const FVector CameraForward = CameraRotation.Vector();
	const FVector CameraRight = FRotationMatrix(CameraRotation).GetUnitAxis(EAxis::Y);

	for (AActor* Candidate : Candidates)
	{
		const FVector DirectionToCandidate = (GetLockonLocation(Candidate) - CameraLocation).GetSafeNormal();

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

		const float Distance = FVector::Distance(OwnerRef->GetActorLocation(), GetLockonLocation(Candidate));
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

// =========================
// Validación / línea de visión
// =========================

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

	// Un enemigo muerto puede seguir existiendo varios segundos por Lifespan/dissolve.
	// No debe poder entrar en la lista de candidatos durante ese intervalo.
	if (Candidate->Implements<UCombatInterface>() && ICombatInterface::Execute_IsDead(Candidate))
	{
		return false;
	}

	// Gancho de targeteabilidad: permite que un enemigo vivo sea temporalmente
	// no seleccionable (cinemática, spawn protegido, invisible, etc.) sin acoplar
	// LockonComponent a una clase concreta.
	if (!IEnemy::Execute_IsLockonTargetable(Candidate))
	{
		return false;
	}

	return true;
}

bool ULockonComponent::HasRequiredLineOfSightToCandidate(AActor* Candidate)
{
	if (!bRequireLineOfSightToAcquireLockon)
	{
		return true;
	}

	return HasClearLineOfSightToTarget(Candidate);
}

bool ULockonComponent::HasClearLineOfSightToTarget(AActor* TargetActor)
{
	RefreshCachedReferences();

	if (!TargetActor || !GetWorld())
	{
		return false;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	if (!GetCameraViewPoint(CameraLocation, CameraRotation))
	{
		return false;
	}

	const FVector TargetLocation = GetLockonLocation(TargetActor);

	FCollisionQueryParams QueryParams{ FName{ TEXT("Lockon Line Of Sight") }, false, OwnerRef };
	QueryParams.AddIgnoredActor(OwnerRef);

	FHitResult HitResult;
	const bool bHit = GetWorld()->LineTraceSingleByChannel(
		HitResult,
		CameraLocation,
		TargetLocation,
		LineOfSightTraceChannel,
		QueryParams
	);

	if (!bHit)
	{
		// Si el enemigo no bloquea Visibility, no hay obstáculo entre cámara y target.
		return true;
	}

	AActor* HitActor = HitResult.GetActor();
	if (HitActor == TargetActor)
	{
		return true;
	}

	// Algunos componentes/actores auxiliares pueden estar owned por el enemigo.
	// En ese caso también consideramos que la línea de visión llegó al target.
	if (HitActor && HitActor->GetOwner() == TargetActor)
	{
		return true;
	}

	return false;
}

bool ULockonComponent::IsSelectableSearchCandidate(AActor* Candidate)
{
	return IsValidLockonCandidate(Candidate) && HasRequiredLineOfSightToCandidate(Candidate);
}

FVector ULockonComponent::GetLockonLocation(AActor* TargetActor) const
{
	if (!IsValid(TargetActor))
	{
		return FVector::ZeroVector;
	}

	if (TargetActor->Implements<UEnemy>())
	{
		return IEnemy::Execute_GetLockonLocation(TargetActor);
	}

	return TargetActor->GetActorLocation();
}

bool ULockonComponent::PassesCameraAngleCheck(AActor* Candidate, float MinDot)
{
	if (!Candidate)
	{
		return false;
	}

	FVector CameraLocation;
	FRotator CameraRotation;
	if (!GetCameraViewPoint(CameraLocation, CameraRotation))
	{
		return false;
	}

	const FVector CameraForward = CameraRotation.Vector();
	const FVector DirectionToCandidate = (GetLockonLocation(Candidate) - CameraLocation).GetSafeNormal();

	const float DotProduct = FVector::DotProduct(CameraForward, DirectionToCandidate);
	return DotProduct >= MinDot;
}

bool ULockonComponent::GetCameraViewPoint(FVector& OutLocation, FRotator& OutRotation)
{
	RefreshCachedReferences();

	if (!Controller)
	{
		return false;
	}

	Controller->GetPlayerViewPoint(OutLocation, OutRotation);
	return true;
}

bool ULockonComponent::ShouldBreakLockonFromBlockedLineOfSight(float DeltaTime)
{
	if (!bBreakLockonWhenLineOfSightBlocked)
	{
		CurrentLineOfSightBlockedTime = 0.0f;
		return false;
	}

	if (!IsValid(CurrentTargetActor))
	{
		CurrentLineOfSightBlockedTime = 0.0f;
		return false;
	}

	if (HasClearLineOfSightToTarget(CurrentTargetActor.Get()))
	{
		CurrentLineOfSightBlockedTime = 0.0f;
		return false;
	}

	CurrentLineOfSightBlockedTime += DeltaTime;
	return CurrentLineOfSightBlockedTime >= LineOfSightBreakDelay;
}

// =========================
// Estado aplicado al jugador / target
// =========================

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
	CurrentLineOfSightBlockedTime = 0.0f;

	if (IsValid(CurrentTargetActor))
	{
		SetComponentTickEnabled(true);
		ApplyLockonState();
		IEnemy::Execute_OnSelect(CurrentTargetActor.Get());
		OnUpdatedTargetDelegate.Broadcast(CurrentTargetActor.Get());
	}
	else
	{
		ClearLockonState();
		SetComponentTickEnabled(false);
		OnUpdatedTargetDelegate.Broadcast(nullptr);
	}
}

void ULockonComponent::ApplyLockonState()
{
	if (bLockonStateApplied)
	{
		return;
	}

	RefreshCachedReferences();

	AppliedController = Controller;
	AppliedMovementComp = MovementComp;
	AppliedSpringArmComp = SpringArmComp;

	if (APlayerController* AppliedControllerPtr = AppliedController.Get())
	{
		// SetIgnoreLookInput usa solicitudes emparejadas. Guardamos que esta instancia
		// añadió una para retirar exactamente una después, sin resetear las de menús,
		// cinematics, stun u otros sistemas.
		AppliedControllerPtr->SetIgnoreLookInput(true);
		bOwnsIgnoreLookInputRequest = true;
	}

	if (UCharacterMovementComponent* AppliedMovementPtr = AppliedMovementComp.Get())
	{
		bSavedOrientRotationToMovement = AppliedMovementPtr->bOrientRotationToMovement;
		bSavedUseControllerDesiredRotation = AppliedMovementPtr->bUseControllerDesiredRotation;
		bHasMovementSnapshot = true;

		AppliedMovementPtr->bOrientRotationToMovement = false;
		AppliedMovementPtr->bUseControllerDesiredRotation = true;
	}

	if (USpringArmComponent* AppliedSpringArmPtr = AppliedSpringArmComp.Get())
	{
		SavedSpringArmTargetOffset = AppliedSpringArmPtr->TargetOffset;
		bHasSpringArmSnapshot = true;

		AppliedSpringArmPtr->TargetOffset = FVector{ 0.0f, 0.0f, 100.0f };
	}

	bLockonStateApplied = true;
}

void ULockonComponent::ClearLockonState()
{
	if (!bLockonStateApplied)
	{
		return;
	}

	if (bHasMovementSnapshot)
	{
		if (UCharacterMovementComponent* AppliedMovementPtr = AppliedMovementComp.Get())
		{
			AppliedMovementPtr->bOrientRotationToMovement = bSavedOrientRotationToMovement;
			AppliedMovementPtr->bUseControllerDesiredRotation = bSavedUseControllerDesiredRotation;
		}
	}

	if (bHasSpringArmSnapshot)
	{
		if (USpringArmComponent* AppliedSpringArmPtr = AppliedSpringArmComp.Get())
		{
			AppliedSpringArmPtr->TargetOffset = SavedSpringArmTargetOffset;
		}
	}

	if (bOwnsIgnoreLookInputRequest)
	{
		if (APlayerController* AppliedControllerPtr = AppliedController.Get())
		{
			AppliedControllerPtr->SetIgnoreLookInput(false);
		}
	}

	AppliedController.Reset();
	AppliedMovementComp.Reset();
	AppliedSpringArmComp.Reset();
	bOwnsIgnoreLookInputRequest = false;
	bHasMovementSnapshot = false;
	bHasSpringArmSnapshot = false;
	bLockonStateApplied = false;
}
