// Fill out your copyright notice in the Description page of Project Settings.

#include "Actor/PantheliaProjectile.h"
#include "Components/SphereComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/AudioComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "PantheliaProject.h"
// Necesarios para leer DeathImpulseMagnitude (SetByCaller) y escribir el vector final
// en el contexto del spec antes de aplicarlo (clase 313).
#include "PantheliaGameplayTags.h"
#include "AbilitySystem/PantheliaAbilitySystemLibrary.h"
#include "AbilitySystem/PantheliaProjectileTargeting.h"
#include "TimerManager.h"

namespace
{
	const FName PlayerTeamTag(TEXT("Player"));
	const FName EnemyTeamTag(TEXT("Enemy"));

	bool HasCombatTeamTag(const AActor* Actor)
	{
		return IsValid(Actor)
			&& (Actor->ActorHasTag(PlayerTeamTag) || Actor->ActorHasTag(EnemyTeamTag));
	}

	AActor* ResolveCombatTeamActor(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return nullptr;
		}

		if (HasCombatTeamTag(Actor))
		{
			return Actor;
		}

		AActor* InstigatorActor = Actor->GetInstigator();
		if (HasCombatTeamTag(InstigatorActor))
		{
			return InstigatorActor;
		}

		AActor* OwnerActor = Actor->GetOwner();
		if (HasCombatTeamTag(OwnerActor))
		{
			return OwnerActor;
		}

		return nullptr;
	}
}

APantheliaProjectile::APantheliaProjectile()
{
	// El tick permanece desactivado casi toda la vida del actor. Solo se habilita
	// durante la breve ventana de soft homing para actualizar el punto lógico móvil.
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bStartWithTickEnabled = false;
	bReplicates = true;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(Sphere);

	HomingTargetSceneComponent = CreateDefaultSubobject<USceneComponent>(TEXT("HomingTargetSceneComponent"));
	HomingTargetSceneComponent->SetupAttachment(Sphere);
	HomingTargetSceneComponent->SetAbsolute(true, true, true);

	ApplyCanonicalCollisionPolicy();

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->InitialSpeed = 550.f;
	ProjectileMovement->MaxSpeed = 550.f;
	ProjectileMovement->ProjectileGravityScale = 0.f;
}

void APantheliaProjectile::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(Lifespan);

	// Reaplicamos la política de colisión en runtime porque los defaults de un
	// Blueprint hijo pueden conservar respuestas serializadas de una versión anterior.
	// Pawn usa overlap para permitir atravesar aliados e i-frames; la geometría usa
	// blocking hit para no depender de GenerateOverlapEvents en cada pared del mapa.
	ApplyCanonicalCollisionPolicy();

	// El proyectil puede nacer dentro o muy cerca del mesh, arma o cápsula del
	// lanzador. Los overlaps ya filtran al source actor, pero una colisión bloqueante
	// detendría ProjectileMovement antes de llegar a OnSphereOverlap. Ignoramos al
	// instigador y al owner durante el movimiento para que el proyectil pueda abandonar
	// con seguridad el volumen del caster sin disparar OnProjectileStopped.
	AActor* InstigatorActor = GetInstigator();
	if (IsValid(InstigatorActor))
	{
		Sphere->IgnoreActorWhenMoving(InstigatorActor, true);
	}

	AActor* OwnerActor = GetOwner();
	if (IsValid(OwnerActor) && OwnerActor != InstigatorActor)
	{
		Sphere->IgnoreActorWhenMoving(OwnerActor, true);
	}

	Sphere->OnComponentBeginOverlap.AddDynamic(this, &APantheliaProjectile::OnSphereOverlap);

	if (ProjectileMovement)
	{
		ProjectileMovement->OnProjectileStop.AddDynamic(
			this, &APantheliaProjectile::OnProjectileStopped);
	}

	if (bPreparedForDelayedLaunch)
	{
		ApplyPreparedProjectileState();
		return;
	}

	StartLoopingSound();
	ScheduleSoftHomingStart();
}

void APantheliaProjectile::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SoftHomingStartTimerHandle);
		World->GetTimerManager().ClearTimer(SoftHomingStopTimerHandle);
	}

	StopSoftHoming();

	Super::EndPlay(EndPlayReason);
}

void APantheliaProjectile::Tick(const float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bSoftHomingActive)
	{
		UpdateSoftHomingTarget();
	}
}

void APantheliaProjectile::SetProjectileSpeed(const float InSpeed)
{
	if (!ProjectileMovement || !FMath::IsFinite(InSpeed) || InSpeed <= 0.f)
	{
		return;
	}

	ProjectileMovement->InitialSpeed = InSpeed;
	ProjectileMovement->MaxSpeed = InSpeed;
	ProjectileMovement->Velocity = GetActorForwardVector().GetSafeNormal() * InSpeed;
}

void APantheliaProjectile::PrepareForDelayedLaunch()
{
	bPreparedForDelayedLaunch = true;

	// Durante SpawnActorDeferred BeginPlay todavía no ha ocurrido. El flag es suficiente
	// para que BeginPlay aplique el estado preparado antes del primer tick de movimiento.
	if (HasActorBegunPlay())
	{
		ApplyPreparedProjectileState();
	}
}

bool APantheliaProjectile::LaunchPreparedProjectile(
	const FRotator& LaunchRotation,
	AActor* HomingTargetActor,
	const FPantheliaProjectileHomingSettings* HomingSettings,
	const float ProjectileSpeedOverride)
{
	if (!bPreparedForDelayedLaunch || bHit || !Sphere || !ProjectileMovement)
	{
		return false;
	}

	StopSoftHoming();

	SetActorRotation(LaunchRotation, ETeleportType::TeleportPhysics);

	ApplyCanonicalCollisionPolicy();

	if (ProjectileMovement->UpdatedComponent != Sphere)
	{
		ProjectileMovement->SetUpdatedComponent(Sphere);
	}

	const bool bHasSpeedOverride =
		FMath::IsFinite(ProjectileSpeedOverride) && ProjectileSpeedOverride > 0.f;
	const float LaunchSpeed = bHasSpeedOverride
		? ProjectileSpeedOverride
		: FMath::Max(0.f, ProjectileMovement->InitialSpeed);

	if (bHasSpeedOverride)
	{
		ProjectileMovement->InitialSpeed = ProjectileSpeedOverride;
		ProjectileMovement->MaxSpeed = ProjectileSpeedOverride;
	}

	ProjectileMovement->bSimulationEnabled = true;
	if (!ProjectileMovement->IsActive())
	{
		ProjectileMovement->Activate(true);
	}
	ProjectileMovement->Velocity = GetActorForwardVector().GetSafeNormal() * LaunchSpeed;
	ProjectileMovement->UpdateComponentVelocity();

	bPreparedForDelayedLaunch = false;
	SetLifeSpan(Lifespan);
	StartLoopingSound();

	if (HomingSettings && HomingSettings->bEnabled && IsValid(HomingTargetActor))
	{
		ConfigureSoftHoming(HomingTargetActor, *HomingSettings);
	}
	else
	{
		bSoftHomingConfigured = false;
		SoftHomingTargetActor.Reset();
	}

	ScheduleSoftHomingStart();
	return true;
}

void APantheliaProjectile::ApplyCanonicalCollisionPolicy()
{
	if (!Sphere)
	{
		return;
	}

	Sphere->SetCollisionObjectType(ECC_Projectile);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetGenerateOverlapEvents(true);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
}

void APantheliaProjectile::ApplyPreparedProjectileState()
{
	StopSoftHoming();
	SetLifeSpan(0.f);

	// El sonido en loop representa el vuelo, no la materialización. Normalmente el
	// componente todavía no existe porque PrepareForDelayedLaunch ocurre antes de
	// BeginPlay; este bloque también cubre de forma segura una preparación tardía.
	if (IsValid(LoopingSoundComponent))
	{
		LoopingSoundComponent->Stop();
		LoopingSoundComponent = nullptr;
	}

	if (Sphere)
	{
		Sphere->SetGenerateOverlapEvents(false);
		Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->bSimulationEnabled = false;
	}
}

void APantheliaProjectile::StartLoopingSound()
{
	if (!IsValid(LoopingSound) || IsValid(LoopingSoundComponent))
	{
		return;
	}

	LoopingSoundComponent = UGameplayStatics::SpawnSoundAttached(
		LoopingSound,
		GetRootComponent());
}

void APantheliaProjectile::ScheduleSoftHomingStart()
{
	if (!bSoftHomingConfigured || bPreparedForDelayedLaunch)
	{
		return;
	}

	// La rotación actual representa la trayectoria inicial exacta. En un proyectil
	// preparado se actualiza justo antes de lanzar; en uno normal viene del SpawnTransform.
	SoftHomingInitialDirection = GetActorForwardVector().GetSafeNormal();

	if (SoftHomingSettings.StartDelay <= KINDA_SMALL_NUMBER)
	{
		StartSoftHoming();
	}
	else if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SoftHomingStartTimerHandle,
			this,
			&APantheliaProjectile::StartSoftHoming,
			SoftHomingSettings.StartDelay,
			false);
	}
}

void APantheliaProjectile::ConfigureSoftHoming(
	AActor* InTargetActor,
	const FPantheliaProjectileHomingSettings& InSettings)
{
	SoftHomingTargetActor = InTargetActor;
	SoftHomingSettings = InSettings;

	SoftHomingSettings.StartDelay = FMath::Max(0.f, SoftHomingSettings.StartDelay);
	SoftHomingSettings.Duration = FMath::Max(0.f, SoftHomingSettings.Duration);
	SoftHomingSettings.AccelerationMagnitude =
		FMath::Max(0.f, SoftHomingSettings.AccelerationMagnitude);
	SoftHomingSettings.MaxCorrectionAngleDegrees =
		FMath::Clamp(SoftHomingSettings.MaxCorrectionAngleDegrees, 0.f, 180.f);

	bSoftHomingConfigured =
		SoftHomingSettings.bEnabled &&
		IsValid(InTargetActor) &&
		SoftHomingSettings.Duration > KINDA_SMALL_NUMBER &&
		SoftHomingSettings.AccelerationMagnitude > KINDA_SMALL_NUMBER &&
		SoftHomingSettings.MaxCorrectionAngleDegrees > KINDA_SMALL_NUMBER;
}

void APantheliaProjectile::StartSoftHoming()
{
	if (!bSoftHomingConfigured || !ProjectileMovement ||
		!IsValid(SoftHomingTargetActor.Get()) || !HomingTargetSceneComponent)
	{
		StopSoftHoming();
		return;
	}

	const FVector TargetLocation = ResolveSoftHomingTargetLocation();
	if (!IsSoftHomingTargetInsideCorrectionCone(TargetLocation))
	{
		StopSoftHoming();
		return;
	}

	HomingTargetSceneComponent->SetWorldLocation(TargetLocation);
	ProjectileMovement->HomingTargetComponent = HomingTargetSceneComponent;
	ProjectileMovement->HomingAccelerationMagnitude =
		SoftHomingSettings.AccelerationMagnitude;
	ProjectileMovement->bIsHomingProjectile = true;
	bSoftHomingActive = true;
	SetActorTickEnabled(true);

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			SoftHomingStopTimerHandle,
			this,
			&APantheliaProjectile::StopSoftHoming,
			SoftHomingSettings.Duration,
			false);
	}
}

void APantheliaProjectile::StopSoftHoming()
{
	bSoftHomingActive = false;
	SetActorTickEnabled(false);

	if (ProjectileMovement)
	{
		ProjectileMovement->bIsHomingProjectile = false;
		ProjectileMovement->HomingTargetComponent.Reset();
	}

	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(SoftHomingStartTimerHandle);
		World->GetTimerManager().ClearTimer(SoftHomingStopTimerHandle);
	}
}

void APantheliaProjectile::UpdateSoftHomingTarget()
{
	if (!IsValid(SoftHomingTargetActor.Get()) || !HomingTargetSceneComponent)
	{
		StopSoftHoming();
		return;
	}

	const FVector TargetLocation = ResolveSoftHomingTargetLocation();
	if (!IsSoftHomingTargetInsideCorrectionCone(TargetLocation))
	{
		// El objetivo salió del cono original. El proyectil mantiene la velocidad
		// actual y continúa sin intentar darse la vuelta para perseguirlo.
		StopSoftHoming();
		return;
	}

	HomingTargetSceneComponent->SetWorldLocation(TargetLocation);
}

FVector APantheliaProjectile::ResolveSoftHomingTargetLocation() const
{
	AActor* TargetActor = SoftHomingTargetActor.Get();
	FVector TargetLocation = FVector::ZeroVector;

	if (PantheliaProjectileTargeting::TryResolveTargetPoint(
		this,
		GetInstigator(),
		TargetActor,
		SoftHomingSettings.TargetPointMode,
		SoftHomingSettings.GroundTraceUpDistance,
		SoftHomingSettings.GroundTraceDownDistance,
		SoftHomingSettings.GroundTraceChannel,
		SoftHomingSettings.GroundSurfaceOffset,
		TargetLocation))
	{
		return TargetLocation;
	}

	return GetActorLocation() + GetActorForwardVector() * 1000.f;
}

bool APantheliaProjectile::IsSoftHomingTargetInsideCorrectionCone(
	const FVector& TargetLocation) const
{
	if (SoftHomingSettings.MaxCorrectionAngleDegrees >= 180.f - KINDA_SMALL_NUMBER)
	{
		return true;
	}

	const FVector DesiredDirection =
		(TargetLocation - GetActorLocation()).GetSafeNormal();
	if (DesiredDirection.IsNearlyZero() || SoftHomingInitialDirection.IsNearlyZero())
	{
		return false;
	}

	const float Dot = FMath::Clamp(
		FVector::DotProduct(SoftHomingInitialDirection, DesiredDirection),
		-1.f,
		1.f);
	const float AngleDegrees = FMath::RadiansToDegrees(FMath::Acos(Dot));

	return AngleDegrees <= SoftHomingSettings.MaxCorrectionAngleDegrees;
}

void APantheliaProjectile::OnSphereOverlap(
	UPrimitiveComponent* OverlappedComponent,
	AActor* OtherActor,
	UPrimitiveComponent* OtherComp,
	int32 OtherBodyIndex,
	bool bFromSweep,
	const FHitResult& SweepResult)
{
	// FIX 1: Ignorar si el overlap no aporta actor. El spec directo se valida en
	// HandleActorImpact para que una subclase de área pueda usar sus propios payloads.
	if (!IsValid(OtherActor) || bHit || ShouldIgnoreImpactActor(OtherActor))
	{
		return;
	}

	FHitResult ImpactResult = SweepResult;
	if (!bFromSweep)
	{
		const FVector ImpactLocation = GetActorLocation();
		const FVector ImpactNormal = -GetActorForwardVector().GetSafeNormal();
		ImpactResult = FHitResult(
			OtherActor,
			OtherComp,
			ImpactLocation,
			ImpactNormal.IsNearlyZero() ? FVector::UpVector : ImpactNormal);
	}

	HandleActorImpact(OtherActor, ImpactResult);
}

void APantheliaProjectile::OnProjectileStopped(const FHitResult& ImpactResult)
{
	// Materializar una formación detiene el movement component de forma intencional.
	// Ese estado nunca debe interpretarse como un impacto contra geometría.
	if (bPreparedForDelayedLaunch || bHit)
	{
		return;
	}

	HandleWorldImpact(ImpactResult);
}

void APantheliaProjectile::HandleActorImpact(
	AActor* OtherActor,
	const FHitResult& ImpactResult)
{
	// La clase base conserva el contrato del Firebolt lineal: un spec directo por
	// proyectil, atraviesa i-frames y se consume ante hit aceptado/block/parry.
	if (!DamageEffectSpecHandle.IsValid())
	{
		return;
	}

	FPantheliaProjectileDamageApplicationResult ApplicationResult =
		ApplyDamageSpecToTarget(
			OtherActor,
			DamageEffectSpecHandle,
			GetActorForwardVector());

	if (!ApplicationResult.bShouldConsumeProjectile)
	{
		IgnoredActors.Add(TWeakObjectPtr<AActor>(OtherActor));
		return;
	}

	FVector ImpactLocation = GetActorLocation();
	if (!ImpactResult.ImpactPoint.IsNearlyZero())
	{
		ImpactLocation = ImpactResult.ImpactPoint;
	}

	ConsumeProjectile(
		ApplicationResult.bShouldPlayImpactFeedback,
		ImpactLocation);
}

void APantheliaProjectile::HandleWorldImpact(const FHitResult& ImpactResult)
{
	FVector ImpactLocation = GetActorLocation();
	if (ImpactResult.bBlockingHit)
	{
		ImpactLocation = ImpactResult.ImpactPoint;
	}

	ConsumeProjectile(/*bPlayImpactFeedback=*/true, ImpactLocation);
}

bool APantheliaProjectile::ShouldIgnoreImpactActor(AActor* OtherActor)
{
	if (!IsValid(OtherActor) || OtherActor == this)
	{
		return true;
	}

	if (IgnoredActors.Contains(TWeakObjectPtr<AActor>(OtherActor)))
	{
		return true;
	}

	// FIX 2: Ignorar si el actor golpeado es el propio lanzador del proyectil.
	// El EffectCauser en el context es normalmente el AvatarActor (el personaje), no
	// el proyectil. Las familias que limpian el spec directo pueden congelar la fuente
	// explícitamente; Instigator y Owner permanecen como fallbacks defensivos.
	AActor* SourceActor = ResolveImpactSourceActor();

	if (OtherActor == SourceActor || OtherActor == GetInstigator()
		|| OtherActor == GetOwner())
	{
		return true;
	}

	// FIX COMBAT-07: los aliados no reciben GE, no generan feedback ofensivo y no
	// consumen el proyectil. Solo aplicamos la comparación de equipos cuando ambos
	// actores declaran un equipo de combate; la geometría y props sin tags siguen
	// consumiendo el proyectil normalmente.
	AActor* SourceTeamActor = ResolveCombatTeamActor(SourceActor);
	AActor* TargetTeamActor = ResolveCombatTeamActor(OtherActor);
	const bool bIsFriendly = SourceTeamActor && TargetTeamActor
		&& !UPantheliaAbilitySystemLibrary::IsNotFriend(
			SourceTeamActor,
			TargetTeamActor);

	if (bIsFriendly)
	{
		IgnoredActors.Add(TWeakObjectPtr<AActor>(OtherActor));
	}

	return bIsFriendly;
}

void APantheliaProjectile::SetResolvedImpactSourceActor(AActor* InSourceActor)
{
	ResolvedImpactSourceActor = InSourceActor;
}

AActor* APantheliaProjectile::ResolveImpactSourceActor() const
{
	if (ResolvedImpactSourceActor.IsValid())
	{
		return ResolvedImpactSourceActor.Get();
	}

	if (DamageEffectSpecHandle.IsValid())
	{
		const FGameplayEffectContextHandle EffectContext =
			DamageEffectSpecHandle.Data->GetContext();
		if (IsValid(EffectContext.GetEffectCauser()))
		{
			return EffectContext.GetEffectCauser();
		}
	}

	if (IsValid(GetInstigator()))
	{
		return GetInstigator();
	}

	return IsValid(GetOwner()) ? GetOwner() : nullptr;
}

FPantheliaProjectileDamageApplicationResult
APantheliaProjectile::ApplyDamageSpecToTarget(
	AActor* TargetActor,
	FGameplayEffectSpecHandle& SpecHandle,
	const FVector& ImpactDirection)
{
	FPantheliaProjectileDamageApplicationResult Result;

	if (!SpecHandle.IsValid() || !IsValid(TargetActor) || !HasAuthority())
	{
		return Result;
	}

	const FVector SafeImpactDirection = ImpactDirection.IsNearlyZero()
		? GetActorForwardVector().GetSafeNormal()
		: ImpactDirection.GetSafeNormal();

	const FPantheliaGameplayTags& GameplayTags = FPantheliaGameplayTags::Get();

	// Aplicamos el daño solo en el servidor/singleplayer.
	// El atributo Health está replicado — el cliente verá el cambio automáticamente.

	// --- IMPULSO DE MUERTE (clase 313) ---
	// Aquí, y solo aquí, conocemos la DIRECCIÓN del impacto. La ability que creó este
	// spec solo conocía la MAGNITUD. En el proyectil lineal usamos su forward actual;
	// en una explosión, la subclase entrega la dirección radial concreta por target.
	// El vector debe escribirse antes de ApplyGameplayEffectSpecToSelf o
	// HandleIncomingDamage llegaría demasiado tarde y encontraría cero.
	const float DeathImpulseMagnitude = SpecHandle.Data->GetSetByCallerMagnitude(
		GameplayTags.CombatTricks_DeathImpulseMagnitude,
		false,
		0.f);

	{
		// GetContext() devuelve el handle por valor, pero comparte el mismo objeto
		// subyacente. Escribir en esta copia modifica el context real del spec.
		FGameplayEffectContextHandle ContextHandle = SpecHandle.Data->GetContext();
		UPantheliaAbilitySystemLibrary::SetDeathImpulse(
			ContextHandle,
			DeathImpulseMagnitude > 0.f
				? SafeImpactDirection * DeathImpulseMagnitude
				: FVector::ZeroVector);
	}

	// --- KNOCKBACK (clase 315) + LAUNCH (Nivel 3) — patrón "escribir siempre" ---
	//
	// FIX DE CONTAMINACIÓN DE CONTEXTO: cada aplicación escribe SIEMPRE el vector
	// real de este impacto o ZeroVector/false. Esto es obligatorio para explosiones,
	// porque un template se clona y aplica a múltiples objetivos. Knockback conserva
	// su dado independiente: primero comprobamos magnitud y después tiramos chance.
	FVector KnockbackForce = FVector::ZeroVector;
	bool bKnockbackIsHeavyThisHit = false;

	const float KnockbackForceMagnitude = SpecHandle.Data->GetSetByCallerMagnitude(
		GameplayTags.CombatTricks_KnockbackForceMagnitude,
		false,
		0.f);

	if (KnockbackForceMagnitude > 0.f)
	{
		const float KnockbackChance = SpecHandle.Data->GetSetByCallerMagnitude(
			GameplayTags.CombatTricks_KnockbackChance,
			false,
			0.f);

		if (FMath::RandRange(1, 100) <= KnockbackChance)
		{
			// Pitch override de 45°: un knockback horizontal parece un deslizamiento.
			const FVector KnockbackDirection =
				UPantheliaAbilitySystemLibrary::GetDirectionWithPitchOverride(
					SafeImpactDirection,
					45.f);
			KnockbackForce = KnockbackDirection * KnockbackForceMagnitude;
			bKnockbackIsHeavyThisHit =
				SpecHandle.Data->GetSetByCallerMagnitude(
					GameplayTags.CombatTricks_KnockbackIsHeavy,
					false,
					0.f) > 0.5f;
		}
	}

	// --- LAUNCH / NIVEL 3 (post-315) ---
	// Sistema completamente independiente de Knockback: tags, chance, vector de
	// contexto y pitch configurable propios.
	FVector LaunchForce = FVector::ZeroVector;
	const float LaunchForceMagnitude = SpecHandle.Data->GetSetByCallerMagnitude(
		GameplayTags.CombatTricks_LaunchForceMagnitude,
		false,
		0.f);

	if (LaunchForceMagnitude > 0.f)
	{
		const float LaunchChance = SpecHandle.Data->GetSetByCallerMagnitude(
			GameplayTags.CombatTricks_LaunchChance,
			false,
			0.f);

		if (FMath::RandRange(1, 100) <= LaunchChance)
		{
			const float LaunchPitchOverride = SpecHandle.Data->GetSetByCallerMagnitude(
				GameplayTags.CombatTricks_LaunchPitchOverride,
				false,
				65.f);
			const FVector LaunchDirection =
				UPantheliaAbilitySystemLibrary::GetDirectionWithPitchOverride(
					SafeImpactDirection,
					LaunchPitchOverride);
			LaunchForce = LaunchDirection * LaunchForceMagnitude;
		}
	}

	// ESCRIBIR SIEMPRE (el corazón del fix): los tres resultados van al contexto
	// con el valor real de ESTE impacto — éxito o cero/false.
	FGameplayEffectContextHandle ContextHandle = SpecHandle.Data->GetContext();
	UPantheliaAbilitySystemLibrary::SetKnockbackForce(
		ContextHandle,
		KnockbackForce);
	UPantheliaAbilitySystemLibrary::SetLaunchForce(
		ContextHandle,
		LaunchForce);
	UPantheliaAbilitySystemLibrary::SetKnockbackIsHeavy(
		ContextHandle,
		bKnockbackIsHeavyThisHit);
	// El spec puede reutilizar su context durante toda la vida del proyectil.
	// Reiniciamos explícitamente el resultado antes de aplicar para no depender
	// de ningún valor previo escrito por otra ruta.
	UPantheliaAbilitySystemLibrary::SetHitOutcome(
		ContextHandle,
		EPantheliaHitOutcome::Unresolved);

	UAbilitySystemComponent* TargetASC =
		UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!TargetASC)
	{
		return Result;
	}

	TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	// ApplyGameplayEffectSpecToSelf ejecuta el ExecCalc de forma síncrona.
	// Solo Accepted reproduce el feedback ofensivo normal.
	Result.HitOutcome =
		UPantheliaAbilitySystemLibrary::GetHitOutcome(ContextHandle);
	Result.bDamageAccepted =
		Result.HitOutcome == EPantheliaHitOutcome::Accepted;
	Result.bShouldPlayImpactFeedback = Result.bDamageAccepted;
	// Los i-frames representan que el proyectil no conectó físicamente y el
	// Firebolt lineal debe atravesar. Una explosión no se deshace, pero cuenta ese
	// target como no aceptado y sigue resolviendo al resto.
	Result.bShouldConsumeProjectile =
		Result.HitOutcome != EPantheliaHitOutcome::NegatedInvulnerability;

	return Result;
}

FGameplayEffectSpecHandle
APantheliaProjectile::DuplicateDamageSpecWithIndependentContext(
	const FGameplayEffectSpecHandle& TemplateSpec) const
{
	if (!TemplateSpec.IsValid())
	{
		return FGameplayEffectSpecHandle();
	}

	const FGameplayEffectContextHandle IndependentContext =
		TemplateSpec.Data->GetContext().Duplicate();
	FGameplayEffectSpec* DuplicatedSpec = new FGameplayEffectSpec(
		*TemplateSpec.Data.Get(),
		IndependentContext);

	return FGameplayEffectSpecHandle(DuplicatedSpec);
}

bool APantheliaProjectile::BeginImpactResolution()
{
	if (bHit)
	{
		return false;
	}

	bHit = true;

	if (IsValid(LoopingSoundComponent))
	{
		LoopingSoundComponent->Stop();
	}

	if (Sphere)
	{
		Sphere->SetGenerateOverlapEvents(false);
		Sphere->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	}

	if (ProjectileMovement)
	{
		ProjectileMovement->StopMovementImmediately();
		ProjectileMovement->bSimulationEnabled = false;
		ProjectileMovement->Deactivate();
	}

	return true;
}

void APantheliaProjectile::FinishImpactResolution(
	const bool bPlayImpactFeedback,
	const FVector& ImpactLocation)
{
	if (bPlayImpactFeedback)
	{
		if (IsValid(ImpactSound))
		{
			UGameplayStatics::PlaySoundAtLocation(
				this,
				ImpactSound,
				ImpactLocation,
				FRotator::ZeroRotator);
		}

		if (IsValid(ImpactEffect))
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				this,
				ImpactEffect,
				ImpactLocation);
		}
	}

	Destroy();
}

void APantheliaProjectile::ConsumeProjectile(
	const bool bPlayImpactFeedback,
	const FVector& ImpactLocation)
{
	if (!BeginImpactResolution())
	{
		return;
	}

	FinishImpactResolution(bPlayImpactFeedback, ImpactLocation);
}
