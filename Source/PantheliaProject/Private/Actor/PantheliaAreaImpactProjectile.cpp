// Fill out your copyright notice in the Description page of Project Settings.

#include "Actor/PantheliaAreaImpactProjectile.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Engine/OverlapResult.h"
#include "Engine/World.h"

#if !UE_BUILD_SHIPPING
#include "Components/SphereComponent.h"
#include "DrawDebugHelpers.h"
#include "Engine/Engine.h"
#include "HAL/IConsoleManager.h"
#endif
#include "Interfaces/CombatInterface.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "PantheliaLogChannels.h"
#include "WorldCollision.h"

namespace
{
#if !UE_BUILD_SHIPPING
	TAutoConsoleVariable<int32> CVarPantheliaDebugAreaImpact(
		TEXT("panthelia.Debug.AreaImpact"),
		0,
		TEXT("0: desactivado. 1: dibuja punto objetivo, esfera de colision al impactar, radio de explosion y targets afectados."),
		ECVF_Cheat);

	TAutoConsoleVariable<float> CVarPantheliaDebugAreaImpactDuration(
		TEXT("panthelia.Debug.AreaImpactDuration"),
		3.f,
		TEXT("Duracion en segundos de las formas de debug de proyectiles de area."),
		ECVF_Cheat);

	bool IsAreaImpactDebugEnabled()
	{
		return CVarPantheliaDebugAreaImpact.GetValueOnGameThread() > 0;
	}

	float GetAreaImpactDebugDuration()
	{
		return FMath::Max(0.05f,
			CVarPantheliaDebugAreaImpactDuration.GetValueOnGameThread());
	}
#endif
	AActor* ResolveAreaImpactSourceFromSpec(
		const FGameplayEffectSpecHandle& SpecHandle)
	{
		if (!SpecHandle.IsValid())
		{
			return nullptr;
		}

		const FGameplayEffectContextHandle EffectContext =
			SpecHandle.Data->GetContext();
		return IsValid(EffectContext.GetEffectCauser())
			? EffectContext.GetEffectCauser()
			: nullptr;
	}

	bool DidAreaPayloadAffectTarget(
		const FPantheliaProjectileDamageApplicationResult& ApplicationResult)
	{
		return ApplicationResult.HitOutcome == EPantheliaHitOutcome::Accepted
			|| ApplicationResult.HitOutcome == EPantheliaHitOutcome::MitigatedBlock;
	}
}

bool APantheliaAreaImpactProjectile::ConfigureAreaImpact(
	const FPantheliaAreaImpactRuntimeConfig& InRuntimeConfig,
	const FGameplayEffectSpecHandle& InExplosionDamageSpecTemplate,
	const FGameplayEffectSpecHandle& InDirectImpactDamageSpecTemplate)
{
	if (HasActorBegunPlay())
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("PantheliaAreaImpactProjectile::ConfigureAreaImpact debe ejecutarse antes de FinishSpawning en %s."),
			*GetName());
		return false;
	}

	FPantheliaAreaImpactRuntimeConfig SanitizedRuntimeConfig = InRuntimeConfig;
	SanitizedRuntimeConfig.ExplosionRadius =
		FMath::Max(0.f, SanitizedRuntimeConfig.ExplosionRadius);
	SanitizedRuntimeConfig.MaxAffectedTargets = FMath::Clamp(
		SanitizedRuntimeConfig.MaxAffectedTargets,
		1,
		128);

	const bool bNeedsExplosionSpec =
		SanitizedRuntimeConfig.DamagePolicy
			!= EPantheliaAreaImpactDamagePolicy::DirectOnly;
	const bool bNeedsDirectSpec =
		SanitizedRuntimeConfig.DamagePolicy
			!= EPantheliaAreaImpactDamagePolicy::ExplosionOnly;

	if (bNeedsExplosionSpec && !InExplosionDamageSpecTemplate.IsValid())
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("PantheliaAreaImpactProjectile::ConfigureAreaImpact — falta ExplosionDamageSpecTemplate en %s."),
			*GetName());
		return false;
	}

	if (bNeedsExplosionSpec
		&& SanitizedRuntimeConfig.ExplosionRadius <= KINDA_SMALL_NUMBER)
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("PantheliaAreaImpactProjectile::ConfigureAreaImpact — ExplosionRadius debe ser mayor que cero para la política %d en %s."),
			static_cast<int32>(SanitizedRuntimeConfig.DamagePolicy),
			*GetName());
		return false;
	}

	if (bNeedsDirectSpec && !InDirectImpactDamageSpecTemplate.IsValid())
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("PantheliaAreaImpactProjectile::ConfigureAreaImpact — falta DirectImpactDamageSpecTemplate en %s."),
			*GetName());
		return false;
	}

	// Resolver una sola fuente canónica antes de limpiar el spec directo de la clase
	// base. Preferimos el payload de explosión y usamos el directo solo como fallback.
	AActor* ResolvedSourceActor = bNeedsExplosionSpec
		? ResolveAreaImpactSourceFromSpec(InExplosionDamageSpecTemplate)
		: nullptr;
	if (!IsValid(ResolvedSourceActor) && bNeedsDirectSpec)
	{
		ResolvedSourceActor = ResolveAreaImpactSourceFromSpec(
			InDirectImpactDamageSpecTemplate);
	}
	if (!IsValid(ResolvedSourceActor))
	{
		ResolvedSourceActor = GetInstigator();
	}
	if (!IsValid(ResolvedSourceActor))
	{
		ResolvedSourceActor = GetOwner();
	}
	if (!IsValid(ResolvedSourceActor))
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("PantheliaAreaImpactProjectile::ConfigureAreaImpact — no pudo resolverse la fuente canónica en %s."),
			*GetName());
		return false;
	}

	// Asignar estado runtime solo después de validar todas las precondiciones. Así un
	// fallo no deja el actor deferred parcialmente configurado.
	RuntimeConfig = SanitizedRuntimeConfig;
	ExplosionDamageSpecTemplate = InExplosionDamageSpecTemplate;
	DirectImpactDamageSpecTemplate = InDirectImpactDamageSpecTemplate;
	SetResolvedImpactSourceActor(ResolvedSourceActor);

	// La clase base no debe aplicar DamageEffectSpecHandle en el overlap: toda la
	// resolución pertenece a los dos payloads explícitos del área.
	DamageEffectSpecHandle = FGameplayEffectSpecHandle();
	bAreaImpactConfigured = true;
	return true;
}

void APantheliaAreaImpactProjectile::HandleActorImpact(
	AActor* OtherActor,
	const FHitResult& ImpactResult)
{
	FVector ImpactLocation = GetActorLocation();
	if (!ImpactResult.ImpactPoint.IsNearlyZero())
	{
		ImpactLocation = ImpactResult.ImpactPoint;
	}

	TryDetonate(OtherActor, ImpactLocation, /*bWasWorldImpact=*/false);
}

void APantheliaAreaImpactProjectile::HandleWorldImpact(
	const FHitResult& ImpactResult)
{
	FVector ImpactLocation = GetActorLocation();
	if (ImpactResult.bBlockingHit)
	{
		ImpactLocation = ImpactResult.ImpactPoint;
	}

	TryDetonate(nullptr, ImpactLocation, /*bWasWorldImpact=*/true);
}

void APantheliaAreaImpactProjectile::TryDetonate(
	AActor* DirectHitActor,
	const FVector& ImpactLocation,
	const bool bWasWorldImpact)
{
	if (!bAreaImpactConfigured)
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("PantheliaAreaImpactProjectile::TryDetonate — %s recibió un impacto sin configuración runtime; se consumirá sin daño."),
			*GetName());

		if (BeginImpactResolution())
		{
			FinishImpactResolution(/*bPlayImpactFeedback=*/false, ImpactLocation);
		}
		return;
	}

	if (!BeginImpactResolution())
	{
		return;
	}

#if !UE_BUILD_SHIPPING
	const bool bDrawAreaImpactDebug = IsAreaImpactDebugEnabled();
	const float AreaImpactDebugDuration = bDrawAreaImpactDebug
		? GetAreaImpactDebugDuration()
		: 0.f;

	if (bDrawAreaImpactDebug)
	{
		UWorld* DebugWorld = GetWorld();
		if (IsValid(DebugWorld))
		{
			// Amarillo: punto real donde se resolvio la detonacion.
			DrawDebugPoint(
				DebugWorld,
				ImpactLocation,
				18.f,
				FColor::Yellow,
				/*bPersistentLines=*/false,
				AreaImpactDebugDuration);

			// Naranja: volumen exacto de la consulta radial de la explosion.
			DrawDebugSphere(
				DebugWorld,
				ImpactLocation,
				RuntimeConfig.ExplosionRadius,
				32,
				FColor::Orange,
				/*bPersistentLines=*/false,
				AreaImpactDebugDuration,
				/*DepthPriority=*/0,
				/*Thickness=*/2.f);

			// Cian: esfera de colision efectiva del proyectil en el instante del impacto.
			if (const USphereComponent* CollisionSphere =
				Cast<USphereComponent>(GetRootComponent()))
			{
				DrawDebugSphere(
					DebugWorld,
					GetActorLocation(),
					CollisionSphere->GetScaledSphereRadius(),
					20,
					FColor::Cyan,
					/*bPersistentLines=*/false,
					AreaImpactDebugDuration,
					/*DepthPriority=*/0,
					/*Thickness=*/1.5f);
			}
		}
	}
#endif

	FPantheliaAreaImpactResult Result;
	Result.ImpactLocation = ImpactLocation;
	Result.DirectHitActor = DirectHitActor;
	Result.bHadDirectActorImpact = IsValid(DirectHitActor);
	Result.bWasWorldImpact = bWasWorldImpact;
	Result.Element = RuntimeConfig.Element;
	Result.SourceAbilityTag = RuntimeConfig.SourceAbilityTag;

	const bool bApplyDirect =
		RuntimeConfig.DamagePolicy == EPantheliaAreaImpactDamagePolicy::DirectOnly
		|| RuntimeConfig.DamagePolicy
			== EPantheliaAreaImpactDamagePolicy::DirectAndExplosion;
	const bool bApplyExplosion =
		RuntimeConfig.DamagePolicy == EPantheliaAreaImpactDamagePolicy::ExplosionOnly
		|| RuntimeConfig.DamagePolicy
			== EPantheliaAreaImpactDamagePolicy::DirectAndExplosion;

	if (bApplyDirect && IsValidAreaTarget(DirectHitActor))
	{
		const FVector DirectDirection =
			(DirectHitActor->GetActorLocation() - ImpactLocation).GetSafeNormal();
		const FPantheliaProjectileDamageApplicationResult DirectApplicationResult =
			ApplyTemplateSpecToTarget(
				DirectImpactDamageSpecTemplate,
				DirectHitActor,
				DirectDirection.IsNearlyZero()
					? GetActorForwardVector()
					: DirectDirection);
		Result.bDirectImpactAffected =
			DidAreaPayloadAffectTarget(DirectApplicationResult);
	}

	if (bApplyExplosion && RuntimeConfig.ExplosionRadius > KINDA_SMALL_NUMBER)
	{
		TArray<AActor*> ExplosionTargets;
		CollectExplosionTargets(
			ImpactLocation,
			DirectHitActor,
			ExplosionTargets);

		Result.SelectedTargetCount = ExplosionTargets.Num();
		for (AActor* TargetActor : ExplosionTargets)
		{
			FVector RadialDirection =
				(TargetActor->GetActorLocation() - ImpactLocation).GetSafeNormal();
			if (RadialDirection.IsNearlyZero())
			{
				RadialDirection = GetActorForwardVector().GetSafeNormal();
			}

			const FPantheliaProjectileDamageApplicationResult ApplicationResult =
				ApplyTemplateSpecToTarget(
					ExplosionDamageSpecTemplate,
					TargetActor,
					RadialDirection);
			const bool bTargetAffected =
				DidAreaPayloadAffectTarget(ApplicationResult);
			if (bTargetAffected)
			{
				++Result.AffectedTargetCount;
			}

#if !UE_BUILD_SHIPPING
			if (bDrawAreaImpactDebug)
			{
				if (UWorld* DebugWorld = GetWorld())
				{
					const FColor TargetColor = bTargetAffected
						? FColor::Green
						: FColor::Red;
					const FVector TargetLocation = TargetActor->GetActorLocation();

					DrawDebugLine(
						DebugWorld,
						ImpactLocation,
						TargetLocation,
						TargetColor,
						/*bPersistentLines=*/false,
						AreaImpactDebugDuration,
						/*DepthPriority=*/0,
						/*Thickness=*/2.f);

					DrawDebugSphere(
						DebugWorld,
						TargetLocation,
						24.f,
						12,
						TargetColor,
						/*bPersistentLines=*/false,
						AreaImpactDebugDuration,
						/*DepthPriority=*/0,
						/*Thickness=*/1.5f);
				}
			}
#endif
		}
	}

	if (IsValid(ExplosionSound))
	{
		UGameplayStatics::PlaySoundAtLocation(
			this,
			ExplosionSound,
			ImpactLocation);
	}

	if (IsValid(ExplosionEffect))
	{
		UNiagaraFunctionLibrary::SpawnSystemAtLocation(
			this,
			ExplosionEffect,
			ImpactLocation);
	}

#if !UE_BUILD_SHIPPING
	if (bDrawAreaImpactDebug)
	{
		UE_LOG(LogPanthelia, Display,
			TEXT("[AREA IMPACT DEBUG] Projectile='%s' Impact=%s Radius=%.1f Selected=%d Affected=%d DirectActor='%s' WorldImpact=%s"),
			*GetName(),
			*ImpactLocation.ToCompactString(),
			RuntimeConfig.ExplosionRadius,
			Result.SelectedTargetCount,
			Result.AffectedTargetCount,
			IsValid(DirectHitActor) ? *DirectHitActor->GetName() : TEXT("None"),
			bWasWorldImpact ? TEXT("true") : TEXT("false"));

		if (GEngine)
		{
			GEngine->AddOnScreenDebugMessage(
				INDEX_NONE,
				AreaImpactDebugDuration,
				FColor::Orange,
				FString::Printf(
					TEXT("AreaImpact R=%.0f | Selected=%d | Affected=%d"),
					RuntimeConfig.ExplosionRadius,
					Result.SelectedTargetCount,
					Result.AffectedTargetCount));
		}
	}
#endif

	K2_OnAreaImpactResolved(Result);
	FinishImpactResolution(/*bPlayImpactFeedback=*/false, ImpactLocation);
}

void APantheliaAreaImpactProjectile::CollectExplosionTargets(
	const FVector& ImpactLocation,
	AActor* DirectHitActor,
	TArray<AActor*>& OutTargets)
{
	OutTargets.Reset();

	UWorld* World = GetWorld();
	if (!IsValid(World))
	{
		return;
	}

	TSet<TWeakObjectPtr<AActor>> UniqueTargets;
	// El actor golpeado directamente se considera visible porque el proyectil ya
	// alcanzó físicamente su superficie. Solo ocupa un slot radial cuando la política
	// realmente le aplicará el payload de explosión.
	const bool bIncludeDirectTargetInExplosion =
		RuntimeConfig.DamagePolicy == EPantheliaAreaImpactDamagePolicy::ExplosionOnly
		|| (RuntimeConfig.DamagePolicy
				== EPantheliaAreaImpactDamagePolicy::DirectAndExplosion
			&& RuntimeConfig.bDirectTargetReceivesExplosion);
	if (bIncludeDirectTargetInExplosion && IsValidAreaTarget(DirectHitActor))
	{
		UniqueTargets.Add(TWeakObjectPtr<AActor>(DirectHitActor));
	}

	FCollisionObjectQueryParams ObjectQueryParams;
	ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(PantheliaAreaImpactOverlap),
		/*bTraceComplex=*/false);
	QueryParams.AddIgnoredActor(this);
	if (AActor* SourceActor = ResolveImpactSourceActor())
	{
		QueryParams.AddIgnoredActor(SourceActor);
	}
	if (IsValid(GetInstigator()))
	{
		QueryParams.AddIgnoredActor(GetInstigator());
	}
	if (IsValid(GetOwner()))
	{
		QueryParams.AddIgnoredActor(GetOwner());
	}

	TArray<FOverlapResult> Overlaps;
	World->OverlapMultiByObjectType(
		Overlaps,
		ImpactLocation,
		FQuat::Identity,
		ObjectQueryParams,
		FCollisionShape::MakeSphere(RuntimeConfig.ExplosionRadius),
		QueryParams);

	for (const FOverlapResult& Overlap : Overlaps)
	{
		AActor* Candidate = Overlap.GetActor();
		if (IsValidAreaTarget(Candidate)
			&& HasExplosionLineOfSight(ImpactLocation, Candidate, DirectHitActor))
		{
			UniqueTargets.Add(TWeakObjectPtr<AActor>(Candidate));
		}
	}

	for (const TWeakObjectPtr<AActor>& Target : UniqueTargets)
	{
		if (Target.IsValid())
		{
			OutTargets.Add(Target.Get());
		}
	}

	OutTargets.Sort([ImpactLocation](const AActor& Left, const AActor& Right)
	{
		const float LeftDistance = FVector::DistSquared(
			ImpactLocation,
			Left.GetActorLocation());
		const float RightDistance = FVector::DistSquared(
			ImpactLocation,
			Right.GetActorLocation());

		if (!FMath::IsNearlyEqual(LeftDistance, RightDistance))
		{
			return LeftDistance < RightDistance;
		}

		return Left.GetPathName() < Right.GetPathName();
	});

	if (OutTargets.Num() > RuntimeConfig.MaxAffectedTargets)
	{
		OutTargets.SetNum(RuntimeConfig.MaxAffectedTargets, EAllowShrinking::No);
	}
}

bool APantheliaAreaImpactProjectile::IsValidAreaTarget(AActor* Candidate)
{
	if (ShouldIgnoreImpactActor(Candidate))
	{
		return false;
	}

	if (Candidate->GetClass()->ImplementsInterface(UCombatInterface::StaticClass())
		&& ICombatInterface::Execute_IsDead(Candidate))
	{
		return false;
	}

	return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Candidate) != nullptr;
}

bool APantheliaAreaImpactProjectile::HasExplosionLineOfSight(
	const FVector& ImpactLocation,
	AActor* Candidate,
	AActor* DirectHitActor) const
{
	if (!RuntimeConfig.bExplosionRequiresLineOfSight)
	{
		return true;
	}

	UWorld* World = GetWorld();
	if (!IsValid(World) || !IsValid(Candidate))
	{
		return false;
	}

	FVector TargetPoint = Candidate->GetActorLocation();
	FVector BoundsOrigin = FVector::ZeroVector;
	FVector BoundsExtent = FVector::ZeroVector;
	Candidate->GetActorBounds(
		/*bOnlyCollidingComponents=*/true,
		BoundsOrigin,
		BoundsExtent,
		/*bIncludeFromChildActors=*/true);
	if (!BoundsExtent.IsNearlyZero())
	{
		TargetPoint = BoundsOrigin;
	}

	const FVector TraceDirection = (TargetPoint - ImpactLocation).GetSafeNormal();
	if (TraceDirection.IsNearlyZero())
	{
		return true;
	}

	// El trace nace en el punto real de detonación. No se desplaza hacia el candidato:
	// hacerlo podría atravesar geometría muy delgada y permitir daño al otro lado.
	const FVector TraceStart = ImpactLocation;

	FCollisionQueryParams QueryParams(
		SCENE_QUERY_STAT(PantheliaAreaImpactLineOfSight),
		/*bTraceComplex=*/false);
	QueryParams.AddIgnoredActor(this);
	if (AActor* SourceActor = ResolveImpactSourceActor())
	{
		QueryParams.AddIgnoredActor(SourceActor);
	}
	if (IsValid(GetInstigator()))
	{
		QueryParams.AddIgnoredActor(GetInstigator());
	}
	if (IsValid(GetOwner()))
	{
		QueryParams.AddIgnoredActor(GetOwner());
	}
	// El punto de detonación de un actor impactado está sobre su superficie. Sin
	// ignorarlo, su propia cápsula/mesh podría bloquear la mitad posterior del radio.
	if (IsValid(DirectHitActor) && DirectHitActor != Candidate)
	{
		QueryParams.AddIgnoredActor(DirectHitActor);
	}

	FHitResult LineOfSightHit;
	const bool bBlocked = World->LineTraceSingleByChannel(
		LineOfSightHit,
		TraceStart,
		TargetPoint,
		RuntimeConfig.ExplosionLineOfSightTraceChannel,
		QueryParams);

	AActor* BlockingActor = LineOfSightHit.GetActor();
	return !bBlocked
		|| BlockingActor == Candidate
		|| (IsValid(BlockingActor) && BlockingActor->GetOwner() == Candidate);
}

FPantheliaProjectileDamageApplicationResult
APantheliaAreaImpactProjectile::ApplyTemplateSpecToTarget(
	const FGameplayEffectSpecHandle& TemplateSpec,
	AActor* TargetActor,
	const FVector& ImpactDirection)
{
	FGameplayEffectSpecHandle TargetSpec =
		DuplicateDamageSpecWithIndependentContext(TemplateSpec);
	if (!TargetSpec.IsValid())
	{
		return FPantheliaProjectileDamageApplicationResult();
	}

	return ApplyDamageSpecToTarget(TargetActor, TargetSpec, ImpactDirection);
}
