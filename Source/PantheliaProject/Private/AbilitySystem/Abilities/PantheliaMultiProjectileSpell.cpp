// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/PantheliaMultiProjectileSpell.h"

#include "AbilitySystem/Utilities/PantheliaProjectilePatternLibrary.h"
#include "Combat/LockonComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"

#if WITH_EDITOR
#include "AbilitySystem/Abilities/PantheliaSpellValidation.h"
#include "Misc/DataValidation.h"
#endif

#if WITH_EDITOR
EDataValidationResult UPantheliaMultiProjectileSpell::IsDataValid(
	FDataValidationContext& Context) const
{
	if (GetClass()->HasAnyClassFlags(CLASS_Abstract))
	{
		return EDataValidationResult::NotValidated;
	}

	const EDataValidationResult SuperResult = Super::IsDataValid(Context);
	const EDataValidationResult MultiResult =
		PantheliaSpellValidation::ValidateMultiProjectileSpell(
			*this,
			Context);

	return SuperResult == EDataValidationResult::Invalid
		|| MultiResult == EDataValidationResult::Invalid
		? EDataValidationResult::Invalid
		: EDataValidationResult::Valid;
}
#endif

UPantheliaMultiProjectileSpell::UPantheliaMultiProjectileSpell()
{
	// La secuencia guarda índices y timers entre frames. InstancedPerActor garantiza
	// estado independiente por caster sin crear una instancia nueva por cada proyectil.
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UPantheliaMultiProjectileSpell::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	ResetRuntimeSequenceState();
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

void UPantheliaMultiProjectileSpell::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const bool bReplicateEndAbility,
	const bool bWasCancelled)
{
	// Solo destruye proyectiles materializados que todavía no fueron lanzados. Los
	// actores ya lanzados son independientes de la ability y continúan su vuelo.
	ResetRuntimeSequenceState();

	Super::EndAbility(
		Handle,
		ActorInfo,
		ActivationInfo,
		bReplicateEndAbility,
		bWasCancelled);
}

int32 UPantheliaMultiProjectileSpell::GetResolvedProjectileCount() const
{
	const float RawCount = ProjectileCountByAbilityLevel.GetValueAtLevel(GetAbilityLevel());
	const int32 SafeMaximum = FMath::Max(1, MaxProjectileCount);

	if (!FMath::IsFinite(RawCount))
	{
		return 1;
	}

	return FMath::Clamp(FMath::RoundToInt(RawCount), 1, SafeMaximum);
}

float UPantheliaMultiProjectileSpell::GetResolvedProjectileSpreadDegrees() const
{
	const float RawSpread = ProjectileSpreadDegrees.GetValueAtLevel(GetAbilityLevel());
	return FMath::IsFinite(RawSpread)
		? FMath::Clamp(FMath::Abs(RawSpread), 0.f, 360.f)
		: 0.f;
}

float UPantheliaMultiProjectileSpell::GetResolvedProjectileSpawnInterval() const
{
	const float RawInterval = ProjectileSpawnInterval.GetValueAtLevel(GetAbilityLevel());
	return FMath::IsFinite(RawInterval)
		? FMath::Max(0.f, RawInterval)
		: 0.f;
}

float UPantheliaMultiProjectileSpell::GetResolvedProjectileSpeedOverride() const
{
	const float RawSpeed = ProjectileSpeedOverride.GetValueAtLevel(GetAbilityLevel());
	return FMath::IsFinite(RawSpeed)
		? FMath::Max(0.f, RawSpeed)
		: 0.f;
}

float UPantheliaMultiProjectileSpell::GetResolvedLaunchPitchDegrees() const
{
	const float RawPitch = LaunchPitchDegrees.GetValueAtLevel(GetAbilityLevel());
	return FMath::IsFinite(RawPitch)
		? FMath::Clamp(RawPitch, -89.f, 89.f)
		: 0.f;
}

float UPantheliaMultiProjectileSpell::GetResolvedFormationForwardOffset() const
{
	const float RawValue = FormationForwardOffset.GetValueAtLevel(GetAbilityLevel());
	return FMath::IsFinite(RawValue) ? RawValue : 0.f;
}

float UPantheliaMultiProjectileSpell::GetResolvedFormationHeightOffset() const
{
	const float RawValue = FormationHeightOffset.GetValueAtLevel(GetAbilityLevel());
	return FMath::IsFinite(RawValue) ? RawValue : 0.f;
}

float UPantheliaMultiProjectileSpell::GetResolvedFormationProjectileSpacing() const
{
	const float RawValue = FormationProjectileSpacing.GetValueAtLevel(GetAbilityLevel());
	return FMath::IsFinite(RawValue) ? FMath::Max(0.f, RawValue) : 0.f;
}

float UPantheliaMultiProjectileSpell::GetResolvedFormationHoldDuration() const
{
	const float RawValue = FormationHoldDuration.GetValueAtLevel(GetAbilityLevel());
	return FMath::IsFinite(RawValue) ? FMath::Max(0.f, RawValue) : 0.f;
}

float UPantheliaMultiProjectileSpell::GetResolvedProjectileWaveDelay() const
{
	const float RawValue = ProjectileWaveDelay.GetValueAtLevel(GetAbilityLevel());
	return FMath::IsFinite(RawValue) ? FMath::Max(0.f, RawValue) : 0.f;
}

float UPantheliaMultiProjectileSpell::GetResolvedFallbackTargetSearchRadius() const
{
	const float RawValue = FallbackTargetSearchRadius.GetValueAtLevel(GetAbilityLevel());
	return FMath::IsFinite(RawValue) ? FMath::Max(1.f, RawValue) : 1.f;
}

FPantheliaProjectileHomingSettings
UPantheliaMultiProjectileSpell::BuildResolvedHomingSettings() const
{
	FPantheliaProjectileHomingSettings Settings;
	Settings.bEnabled = bEnableSoftHoming;

	const float RawStartDelay = HomingStartDelay.GetValueAtLevel(GetAbilityLevel());
	const float RawDuration = HomingDuration.GetValueAtLevel(GetAbilityLevel());
	const float RawAcceleration =
		HomingAccelerationMagnitude.GetValueAtLevel(GetAbilityLevel());
	const float RawMaxAngle =
		MaxHomingCorrectionAngleDegrees.GetValueAtLevel(GetAbilityLevel());

	Settings.StartDelay = FMath::IsFinite(RawStartDelay)
		? FMath::Max(0.f, RawStartDelay)
		: 0.f;
	Settings.Duration = FMath::IsFinite(RawDuration)
		? FMath::Max(0.f, RawDuration)
		: 0.f;
	Settings.AccelerationMagnitude = FMath::IsFinite(RawAcceleration)
		? FMath::Max(0.f, RawAcceleration)
		: 0.f;
	Settings.MaxCorrectionAngleDegrees = FMath::IsFinite(RawMaxAngle)
		? FMath::Clamp(RawMaxAngle, 0.f, 180.f)
		: 0.f;

	return Settings;
}

void UPantheliaMultiProjectileSpell::SpawnProjectiles()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!IsValid(AvatarActor) || !IsValid(World) || !AvatarActor->HasAuthority())
	{
		return;
	}

	if (bProjectileSequenceStarted && !bProjectileSequenceFinished)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("PantheliaMultiProjectileSpell::SpawnProjectiles — el notify intentó iniciar dos veces %s."),
			*GetName());
		return;
	}

	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("PantheliaMultiProjectileSpell::SpawnProjectiles — ProjectileClass no asignado en %s."),
			*GetName());
		bProjectileSequenceStarted = true;
		MarkProjectileSequenceFinished();
		return;
	}

	ActiveHomingSettings = BuildResolvedHomingSettings();
	ActiveProjectileSpeedOverride = GetResolvedProjectileSpeedOverride();
	ActiveProjectileSpawnInterval = GetResolvedProjectileSpawnInterval();
	NextProjectileIndex = 0;
	bProjectileSequenceStarted = true;
	bProjectileSequenceFinished = false;

	switch (SpawnPattern)
	{
	case EPantheliaMultiProjectileSpawnPattern::FixedWorldLineWaves:
		StartFixedWorldLineSequence();
		break;

	case EPantheliaMultiProjectileSpawnPattern::SocketFan:
	default:
		StartSocketFanSequence();
		break;
	}
}

void UPantheliaMultiProjectileSpell::StartSocketFanSequence()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		MarkProjectileSequenceFinished();
		return;
	}

	const FVector SocketLocation = GetProjectileSocketLocation();
	const FVector TargetLocation = GetFacingTargetLocation();
	const FVector ToTarget = (TargetLocation - SocketLocation).GetSafeNormal();
	if (ToTarget.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("PantheliaMultiProjectileSpell::StartSocketFanSequence — dirección inválida en %s."),
			*GetName());
		MarkProjectileSequenceFinished();
		return;
	}

	FRotator CenterRotation = ToTarget.Rotation();
	CenterRotation.Pitch = GetResolvedLaunchPitchDegrees();
	CenterRotation.Roll = 0.f;

	const TArray<FVector> Directions =
		UPantheliaProjectilePatternLibrary::MakeEvenlySpacedDirections(
			CenterRotation.Vector(),
			FVector::UpVector,
			GetResolvedProjectileSpreadDegrees(),
			GetResolvedProjectileCount());

	PendingProjectileRotations.Reset(Directions.Num());
	for (const FVector& Direction : Directions)
	{
		PendingProjectileRotations.Add(Direction.Rotation());
	}

	if (PendingProjectileRotations.Num() == 0)
	{
		MarkProjectileSequenceFinished();
		return;
	}

	PendingHomingTargetActor = GetFacingTargetActor();

	// El primero sale inmediatamente. Esto conserva respuesta instantánea incluso
	// cuando el resto de la descarga tiene cadencia.
	SpawnNextSocketFanProjectile();

	if (!bProjectileSequenceFinished && ActiveProjectileSpawnInterval > KINDA_SMALL_NUMBER)
	{
		// Los timers están limitados por frame; 0.01 evita un looping timer degenerado.
		const float SafeTimerInterval = FMath::Max(0.01f, ActiveProjectileSpawnInterval);
		World->GetTimerManager().SetTimer(
			ProjectileSpawnTimerHandle,
			this,
			&UPantheliaMultiProjectileSpell::SpawnNextSocketFanProjectile,
			SafeTimerInterval,
			true);
	}
	else if (!bProjectileSequenceFinished)
	{
		// Intervalo cero: todos los restantes se crean en el mismo frame.
		while (!bProjectileSequenceFinished)
		{
			SpawnNextSocketFanProjectile();
		}
	}
}

void UPantheliaMultiProjectileSpell::SpawnNextSocketFanProjectile()
{
	if (!PendingProjectileRotations.IsValidIndex(NextProjectileIndex))
	{
		MarkProjectileSequenceFinished();
		return;
	}

	AActor* HomingTargetActor = PendingHomingTargetActor.Get();
	const FPantheliaProjectileHomingSettings* HomingSettings =
		ActiveHomingSettings.bEnabled && IsValid(HomingTargetActor)
			? &ActiveHomingSettings
			: nullptr;

	SpawnProjectileWithRotation(
		PendingProjectileRotations[NextProjectileIndex],
		HomingTargetActor,
		HomingSettings,
		ActiveProjectileSpeedOverride);

	++NextProjectileIndex;
	if (NextProjectileIndex >= PendingProjectileRotations.Num())
	{
		MarkProjectileSequenceFinished();
	}
}

void UPantheliaMultiProjectileSpell::StartFixedWorldLineSequence()
{
	RemainingProjectileCount = GetResolvedProjectileCount();
	CurrentWaveIndex = 0;
	PendingHomingTargetActor = ResolveFormationTargetActor();
	RefreshCapturedTargetLocation();

	if (!BuildFrozenFormationBasis())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("PantheliaMultiProjectileSpell::StartFixedWorldLineSequence — no se pudo construir la formación en %s."),
			*GetName());
		MarkProjectileSequenceFinished();
		return;
	}

	MaterializeNextFixedWorldLineWave();
}

AActor* UPantheliaMultiProjectileSpell::ResolveFormationTargetActor() const
{
	if (AActor* LockedTarget = GetFacingTargetActor())
	{
		// GetFacingTargetActor ya excluye actores inválidos y muertos.
		return LockedTarget;
	}

	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return nullptr;
	}

	if (ULockonComponent* LockonComp =
		AvatarActor->FindComponentByClass<ULockonComponent>())
	{
		return LockonComp->FindNearestTarget(GetResolvedFallbackTargetSearchRadius());
	}

	return nullptr;
}

bool UPantheliaMultiProjectileSpell::RefreshCapturedTargetLocation()
{
	FVector ResolvedLocation = FVector::ZeroVector;
	if (TryResolveTargetLocation(PendingHomingTargetActor.Get(), ResolvedLocation))
	{
		LastKnownTargetLocation = ResolvedLocation;
		bHasLastKnownTargetLocation = true;
		return true;
	}

	return false;
}

bool UPantheliaMultiProjectileSpell::BuildFrozenFormationBasis()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return false;
	}

	const FVector SocketLocation = GetProjectileSocketLocation();
	FrozenFormationSourceLocation = SocketLocation;
	FVector HorizontalForward = bHasLastKnownTargetLocation
		? LastKnownTargetLocation - SocketLocation
		: AvatarActor->GetActorForwardVector();
	HorizontalForward.Z = 0.f;

	if (!HorizontalForward.Normalize())
	{
		HorizontalForward = AvatarActor->GetActorForwardVector();
		HorizontalForward.Z = 0.f;
		if (!HorizontalForward.Normalize())
		{
			return false;
		}
	}

	FVector HorizontalRight = FVector::CrossProduct(FVector::UpVector, HorizontalForward);
	if (!HorizontalRight.Normalize())
	{
		return false;
	}

	FrozenFormationForward = HorizontalForward;
	FrozenFormationRight = HorizontalRight;
	FrozenFormationCenter =
		SocketLocation
		+ FrozenFormationForward * GetResolvedFormationForwardOffset()
		+ FVector::UpVector * GetResolvedFormationHeightOffset();

	if (!bHasLastKnownTargetLocation)
	{
		LastKnownTargetLocation =
			FrozenFormationCenter + FrozenFormationForward * 2000.f;
		bHasLastKnownTargetLocation = true;
	}

	return true;
}

TArray<FVector> UPantheliaMultiProjectileSpell::BuildCenteredLineLocations(
	const int32 ProjectileCount) const
{
	TArray<FVector> Locations;
	if (ProjectileCount <= 0)
	{
		return Locations;
	}

	Locations.Reserve(ProjectileCount);
	const float Spacing = GetResolvedFormationProjectileSpacing();
	const float CenterIndex = static_cast<float>(ProjectileCount - 1) * 0.5f;

	for (int32 Index = 0; Index < ProjectileCount; ++Index)
	{
		const float SignedOffset = (static_cast<float>(Index) - CenterIndex) * Spacing;
		Locations.Add(FrozenFormationCenter + FrozenFormationRight * SignedOffset);
	}

	return Locations;
}

void UPantheliaMultiProjectileSpell::MaterializeNextFixedWorldLineWave()
{
	UWorld* World = GetWorld();
	if (!IsValid(World) || RemainingProjectileCount <= 0)
	{
		MarkProjectileSequenceFinished();
		return;
	}

	DestroyUnlaunchedPreparedProjectiles();
	NextProjectileIndex = 0;

	const int32 ConfiguredWaveSize = CurrentWaveIndex == 0
		? InitialWaveProjectileCount
		: AdditionalWaveProjectileCount;
	const int32 WaveProjectileCount = FMath::Min(
		RemainingProjectileCount,
		FMath::Max(1, ConfiguredWaveSize));
	RemainingProjectileCount -= WaveProjectileCount;

	// Este refresh solo mejora la rotación inicial cosmética de la formación.
	// La dirección real se recalcula al lanzar cada proyectil y la última ubicación
	// válida ya fue inicializada por BuildFrozenFormationBasis.
	RefreshCapturedTargetLocation();
	const TArray<FVector> RequestedLocations =
		BuildCenteredLineLocations(WaveProjectileCount);
	TArray<FVector> MaterializedLocations;
	MaterializedLocations.Reserve(RequestedLocations.Num());
	PreparedWaveProjectiles.Reserve(RequestedLocations.Num());

	for (const FVector& SpawnLocation : RequestedLocations)
	{
		FVector LaunchDirection =
			(LastKnownTargetLocation - SpawnLocation).GetSafeNormal();
		if (LaunchDirection.IsNearlyZero())
		{
			LaunchDirection = FrozenFormationForward;
		}

		FRotator InitialRotation = LaunchDirection.Rotation();
		InitialRotation.Roll = 0.f;

		FTransform SpawnTransform;
		SpawnTransform.SetLocation(SpawnLocation);
		SpawnTransform.SetRotation(InitialRotation.Quaternion());

		APantheliaProjectile* Projectile = SpawnProjectileAtTransform(
			SpawnTransform,
			nullptr,
			nullptr,
			0.f,
			true);

		if (IsValid(Projectile))
		{
			PreparedWaveProjectiles.Add(Projectile);
			MaterializedLocations.Add(SpawnLocation);
		}
	}

	if (PreparedWaveProjectiles.Num() == 0)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("PantheliaMultiProjectileSpell::MaterializeNextFixedWorldLineWave — no se pudo materializar ningún proyectil en %s."),
			*GetName());
		MarkProjectileSequenceFinished();
		return;
	}

	K2_OnProjectileWaveMaterialized(
		CurrentWaveIndex,
		MaterializedLocations,
		FrozenFormationSourceLocation,
		MaterializedLocations[0],
		MaterializedLocations.Last());

	const float HoldDuration = GetResolvedFormationHoldDuration();
	if (HoldDuration > KINDA_SMALL_NUMBER)
	{
		World->GetTimerManager().SetTimer(
			ProjectileSpawnTimerHandle,
			this,
			&UPantheliaMultiProjectileSpell::LaunchNextFixedWorldLineProjectile,
			HoldDuration,
			false);
	}
	else
	{
		LaunchNextFixedWorldLineProjectile();
	}
}

void UPantheliaMultiProjectileSpell::LaunchNextFixedWorldLineProjectile()
{
	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		MarkProjectileSequenceFinished();
		return;
	}

	while (NextProjectileIndex < PreparedWaveProjectiles.Num())
	{
		if (!PreparedWaveProjectiles[NextProjectileIndex].IsValid())
		{
			++NextProjectileIndex;
			continue;
		}

		APantheliaProjectile* Projectile =
			PreparedWaveProjectiles[NextProjectileIndex].Get();
		const bool bTargetStillUsable = RefreshCapturedTargetLocation();

		FVector LaunchDirection =
			(LastKnownTargetLocation - Projectile->GetActorLocation()).GetSafeNormal();
		if (LaunchDirection.IsNearlyZero())
		{
			LaunchDirection = FrozenFormationForward;
		}

		FRotator LaunchRotation = LaunchDirection.Rotation();
		LaunchRotation.Roll = 0.f;

		AActor* HomingTargetActor = bTargetStillUsable
			? PendingHomingTargetActor.Get()
			: nullptr;
		const FPantheliaProjectileHomingSettings* HomingSettings =
			ActiveHomingSettings.bEnabled && IsValid(HomingTargetActor)
				? &ActiveHomingSettings
				: nullptr;

		const bool bLaunched = Projectile->LaunchPreparedProjectile(
			LaunchRotation,
			HomingTargetActor,
			HomingSettings,
			ActiveProjectileSpeedOverride);

		if (!bLaunched && IsValid(Projectile))
		{
			Projectile->Destroy();
		}

		PreparedWaveProjectiles[NextProjectileIndex].Reset();
		++NextProjectileIndex;

		// Con intervalo positivo se lanza un proyectil por callback. Con intervalo
		// cero, el while vacía la oleada completa en el mismo frame sin recursión.
		if (ActiveProjectileSpawnInterval > KINDA_SMALL_NUMBER
			&& NextProjectileIndex < PreparedWaveProjectiles.Num())
		{
			World->GetTimerManager().SetTimer(
				ProjectileSpawnTimerHandle,
				this,
				&UPantheliaMultiProjectileSpell::LaunchNextFixedWorldLineProjectile,
				FMath::Max(0.01f, ActiveProjectileSpawnInterval),
				false);
			return;
		}
	}

	HandleFixedWorldLineWaveFinished();
}

void UPantheliaMultiProjectileSpell::HandleFixedWorldLineWaveFinished()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ProjectileSpawnTimerHandle);
	}

	PreparedWaveProjectiles.Reset();
	NextProjectileIndex = 0;

	if (RemainingProjectileCount <= 0)
	{
		MarkProjectileSequenceFinished();
		return;
	}

	++CurrentWaveIndex;
	const float WaveDelay = GetResolvedProjectileWaveDelay();
	if (WaveDelay > KINDA_SMALL_NUMBER)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().SetTimer(
				ProjectileWaveTimerHandle,
				this,
				&UPantheliaMultiProjectileSpell::MaterializeNextFixedWorldLineWave,
				WaveDelay,
				false);
		}
		else
		{
			MarkProjectileSequenceFinished();
		}
	}
	else
	{
		MaterializeNextFixedWorldLineWave();
	}
}

void UPantheliaMultiProjectileSpell::DestroyUnlaunchedPreparedProjectiles()
{
	for (const TWeakObjectPtr<APantheliaProjectile>& ProjectilePtr : PreparedWaveProjectiles)
	{
		APantheliaProjectile* Projectile = ProjectilePtr.Get();
		if (IsValid(Projectile) && Projectile->IsPreparedForDelayedLaunch())
		{
			Projectile->Destroy();
		}
	}

	PreparedWaveProjectiles.Reset();
}

void UPantheliaMultiProjectileSpell::NotifyCastMontageFinished()
{
	bCastMontageFinished = true;

	if (!bProjectileSequenceStarted)
	{
		// Un notify ausente no debe dejar la ability activa para siempre. Se registra
		// el problema y se permite terminar limpiamente sin inventar un disparo tardío.
		UE_LOG(LogTemp, Warning,
			TEXT("PantheliaMultiProjectileSpell::NotifyCastMontageFinished — el montage terminó sin iniciar SpawnProjectiles en %s. Revisa AN_MontageEvent y ProjectileSpawnEventTag."),
			*GetName());
		bProjectileSequenceFinished = true;
	}

	TryCompleteAbility();
}

void UPantheliaMultiProjectileSpell::MarkProjectileSequenceFinished()
{
	ClearProjectileSequenceTimers();
	bProjectileSequenceFinished = true;
	PendingProjectileRotations.Reset();
	DestroyUnlaunchedPreparedProjectiles();
	PendingHomingTargetActor.Reset();
	TryCompleteAbility();
}

void UPantheliaMultiProjectileSpell::TryCompleteAbility()
{
	if (!bCastMontageFinished || !bProjectileSequenceFinished || !CurrentActorInfo)
	{
		return;
	}

	EndAbility(
		CurrentSpecHandle,
		CurrentActorInfo,
		CurrentActivationInfo,
		true,
		false);
}

void UPantheliaMultiProjectileSpell::ClearProjectileSequenceTimers()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ProjectileSpawnTimerHandle);
		World->GetTimerManager().ClearTimer(ProjectileWaveTimerHandle);
	}
}

void UPantheliaMultiProjectileSpell::ResetRuntimeSequenceState()
{
	ClearProjectileSequenceTimers();
	DestroyUnlaunchedPreparedProjectiles();
	PendingProjectileRotations.Reset();
	PendingHomingTargetActor.Reset();
	ActiveHomingSettings = FPantheliaProjectileHomingSettings();
	FrozenFormationSourceLocation = FVector::ZeroVector;
	FrozenFormationCenter = FVector::ZeroVector;
	FrozenFormationForward = FVector::ForwardVector;
	FrozenFormationRight = FVector::RightVector;
	LastKnownTargetLocation = FVector::ZeroVector;
	NextProjectileIndex = 0;
	RemainingProjectileCount = 0;
	CurrentWaveIndex = 0;
	ActiveProjectileSpeedOverride = 0.f;
	ActiveProjectileSpawnInterval = 0.f;
	bHasLastKnownTargetLocation = false;
	bProjectileSequenceStarted = false;
	bProjectileSequenceFinished = false;
	bCastMontageFinished = false;
}

void UPantheliaMultiProjectileSpell::DrawProjectileSpreadDebug(
	const int32 ProjectileCountOverride) const
{
#if ENABLE_DRAW_DEBUG
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!IsValid(AvatarActor) || !IsValid(World))
	{
		return;
	}

	const int32 Count = ProjectileCountOverride > 0
		? FMath::Clamp(ProjectileCountOverride, 1, FMath::Max(1, MaxProjectileCount))
		: GetResolvedProjectileCount();

	constexpr float DebugDuration = 20.f;
	constexpr float Thickness = 1.25f;

	if (SpawnPattern == EPantheliaMultiProjectileSpawnPattern::FixedWorldLineWaves)
	{
		const FVector SocketLocation = GetProjectileSocketLocation();
		FVector TargetLocation = GetFacingTargetLocation();
		FVector HorizontalForward = TargetLocation - SocketLocation;
		HorizontalForward.Z = 0.f;
		if (!HorizontalForward.Normalize())
		{
			HorizontalForward = AvatarActor->GetActorForwardVector().GetSafeNormal2D();
		}

		const FVector Right = FVector::CrossProduct(
			FVector::UpVector,
			HorizontalForward).GetSafeNormal();
		const FVector Center =
			SocketLocation
			+ HorizontalForward * GetResolvedFormationForwardOffset()
			+ FVector::UpVector * GetResolvedFormationHeightOffset();
		const int32 PreviewWaveCount = FMath::Min(
			Count,
			FMath::Max(1, InitialWaveProjectileCount));
		const float Spacing = GetResolvedFormationProjectileSpacing();
		const float CenterIndex = static_cast<float>(PreviewWaveCount - 1) * 0.5f;

		for (int32 Index = 0; Index < PreviewWaveCount; ++Index)
		{
			const FVector Location = Center + Right *
				((static_cast<float>(Index) - CenterIndex) * Spacing);
			DrawDebugSphere(
				World,
				Location,
				12.f,
				12,
				FColor::Orange,
				false,
				DebugDuration,
				0,
				Thickness);
			DrawDebugDirectionalArrow(
				World,
				Location,
				Location + (TargetLocation - Location).GetSafeNormal() * 145.f,
				10.f,
				FColor::Red,
				false,
				DebugDuration,
				0,
				Thickness);
		}
		return;
	}

	const FVector SocketLocation = GetProjectileSocketLocation();
	const FVector TargetLocation = GetFacingTargetLocation();
	const FVector ToTarget = TargetLocation - SocketLocation;
	if (ToTarget.IsNearlyZero())
	{
		return;
	}

	FRotator CenterRotation = ToTarget.Rotation();
	CenterRotation.Pitch = GetResolvedLaunchPitchDegrees();
	CenterRotation.Roll = 0.f;
	const FVector CenterDirection = CenterRotation.Vector().GetSafeNormal();

	const TArray<FVector> Directions =
		UPantheliaProjectilePatternLibrary::MakeEvenlySpacedDirections(
			CenterDirection,
			FVector::UpVector,
			GetResolvedProjectileSpreadDegrees(),
			Count);

	constexpr float MainArrowLength = 180.f;
	constexpr float SpreadArrowLength = 145.f;
	constexpr float ArrowSize = 10.f;

	DrawDebugDirectionalArrow(
		World,
		SocketLocation,
		SocketLocation + CenterDirection * MainArrowLength,
		ArrowSize,
		FColor::White,
		false,
		DebugDuration,
		0,
		Thickness);

	const FVector SpreadStart = SocketLocation + FVector::UpVector * 8.f;
	for (const FVector& Direction : Directions)
	{
		DrawDebugDirectionalArrow(
			World,
			SpreadStart,
			SpreadStart + Direction * SpreadArrowLength,
			ArrowSize,
			FColor::Red,
			false,
			DebugDuration,
			0,
			Thickness);
	}
#else
	(void)ProjectileCountOverride;
#endif
}
