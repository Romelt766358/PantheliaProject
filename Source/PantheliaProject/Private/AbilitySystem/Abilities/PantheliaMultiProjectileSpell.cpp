// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/PantheliaMultiProjectileSpell.h"

#include "AbilitySystem/Utilities/PantheliaProjectilePatternLibrary.h"
#include "DrawDebugHelpers.h"
#include "Engine/World.h"
#include "TimerManager.h"

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
	// Solo cancela proyectiles que todavía no fueron spawneados. Los actores ya
	// lanzados son independientes de la ability y continúan su vuelo normalmente.
	ClearProjectileSequenceTimer();
	PendingProjectileRotations.Reset();
	PendingHomingTargetActor.Reset();
	bProjectileSequenceStarted = false;
	bProjectileSequenceFinished = false;
	bCastMontageFinished = false;
	NextProjectileIndex = 0;

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

	const FVector SocketLocation = GetProjectileSocketLocation();
	const FVector TargetLocation = GetFacingTargetLocation();
	const FVector ToTarget = (TargetLocation - SocketLocation).GetSafeNormal();
	if (ToTarget.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("PantheliaMultiProjectileSpell::SpawnProjectiles — dirección inválida en %s."),
			*GetName());
		bProjectileSequenceStarted = true;
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
		bProjectileSequenceStarted = true;
		MarkProjectileSequenceFinished();
		return;
	}

	PendingHomingTargetActor = GetFacingTargetActor();
	ActiveHomingSettings = BuildResolvedHomingSettings();
	ActiveProjectileSpeedOverride = GetResolvedProjectileSpeedOverride();
	ActiveProjectileSpawnInterval = GetResolvedProjectileSpawnInterval();
	NextProjectileIndex = 0;
	bProjectileSequenceStarted = true;
	bProjectileSequenceFinished = false;

	// El primero sale inmediatamente. Esto conserva respuesta instantánea incluso
	// cuando el resto de la descarga tiene cadencia.
	SpawnNextProjectile();

	if (!bProjectileSequenceFinished && ActiveProjectileSpawnInterval > KINDA_SMALL_NUMBER)
	{
		// Los timers están limitados por frame; 0.01 evita un looping timer degenerado.
		const float SafeTimerInterval = FMath::Max(0.01f, ActiveProjectileSpawnInterval);
		World->GetTimerManager().SetTimer(
			ProjectileSpawnTimerHandle,
			this,
			&UPantheliaMultiProjectileSpell::SpawnNextProjectile,
			SafeTimerInterval,
			true);
	}
	else if (!bProjectileSequenceFinished)
	{
		// Intervalo cero: todos los restantes se crean en el mismo frame.
		while (!bProjectileSequenceFinished)
		{
			SpawnNextProjectile();
		}
	}
}

void UPantheliaMultiProjectileSpell::SpawnNextProjectile()
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

void UPantheliaMultiProjectileSpell::NotifyCastMontageFinished()
{
	bCastMontageFinished = true;

	if (!bProjectileSequenceStarted)
	{
		// Un notify ausente no debe dejar la ability activa para siempre. Se registra
		// el problema y se permite terminar limpiamente sin inventar un disparo tardío.
		UE_LOG(LogTemp, Warning,
			TEXT("PantheliaMultiProjectileSpell::NotifyCastMontageFinished — el montage terminó sin iniciar SpawnProjectiles en %s. Revisa AN_MontageEvent y SocketTag."),
			*GetName());
		bProjectileSequenceFinished = true;
	}

	TryCompleteAbility();
}

void UPantheliaMultiProjectileSpell::MarkProjectileSequenceFinished()
{
	ClearProjectileSequenceTimer();
	bProjectileSequenceFinished = true;
	PendingProjectileRotations.Reset();
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

void UPantheliaMultiProjectileSpell::ClearProjectileSequenceTimer()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ProjectileSpawnTimerHandle);
	}
}

void UPantheliaMultiProjectileSpell::ResetRuntimeSequenceState()
{
	ClearProjectileSequenceTimer();
	PendingProjectileRotations.Reset();
	PendingHomingTargetActor.Reset();
	ActiveHomingSettings = FPantheliaProjectileHomingSettings();
	NextProjectileIndex = 0;
	ActiveProjectileSpeedOverride = 0.f;
	ActiveProjectileSpawnInterval = 0.f;
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

	const int32 Count = ProjectileCountOverride > 0
		? FMath::Clamp(ProjectileCountOverride, 1, FMath::Max(1, MaxProjectileCount))
		: GetResolvedProjectileCount();

	const TArray<FVector> Directions =
		UPantheliaProjectilePatternLibrary::MakeEvenlySpacedDirections(
			CenterDirection,
			FVector::UpVector,
			GetResolvedProjectileSpreadDegrees(),
			Count);

	constexpr float MainArrowLength = 180.f;
	constexpr float SpreadArrowLength = 145.f;
	constexpr float DebugDuration = 20.f;
	constexpr float ArrowSize = 10.f;
	constexpr float Thickness = 1.25f;

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
