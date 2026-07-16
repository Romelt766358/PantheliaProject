// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/PantheliaProjectileSpell.h"

#include "Actor/PantheliaProjectile.h"
#include "Combat/LockonComponent.h"
#include "Interfaces/CombatInterface.h"
#include "PantheliaGameplayTags.h"

UPantheliaProjectileSpell::UPantheliaProjectileSpell()
{
	// Los hechizos del jugador pueden activar el pipeline común desde sus Class Defaults.
	// Los hechizos de enemigos conservan bUsePantheliaResourceCost=false y no pagan recursos.
	// El coste principal es Mana y la entrada adicional de Stamina se cobra dentro del
	// mismo CommitAbility. Sus valores concretos siguen siendo datos de cada hechizo.
	ResourceCostType = EPantheliaResourceCostType::Mana;

	FPantheliaAdditionalResourceCost StaminaCost;
	StaminaCost.ResourceType = EPantheliaResourceCostType::Stamina;
	AdditionalResourceCosts.Add(StaminaCost);
}

void UPantheliaProjectileSpell::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

FGameplayTag UPantheliaProjectileSpell::GetResolvedSocketTag() const
{
	const FPantheliaGameplayTags& GameplayTags = FPantheliaGameplayTags::Get();

	return SocketTag.IsValid()
		? SocketTag
		: GameplayTags.Montage_Attack_Weapon;
}

AActor* UPantheliaProjectileSpell::GetFacingTargetActor() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return nullptr;
	}

	if (ULockonComponent* LockonComp =
		AvatarActor->FindComponentByClass<ULockonComponent>())
	{
		return IsValid(LockonComp->CurrentTargetActor)
			? LockonComp->CurrentTargetActor.Get()
			: nullptr;
	}

	return nullptr;
}

FVector UPantheliaProjectileSpell::GetFacingTargetLocation() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return FVector::ZeroVector;
	}

	if (AActor* TargetActor = GetFacingTargetActor())
	{
		if (ULockonComponent* LockonComp =
			AvatarActor->FindComponentByClass<ULockonComponent>())
		{
			// El punto lógico puede estar en torso, cabeza u otra zona configurada.
			// No usar GetActorLocation(): normalmente apunta a los pies/origen.
			return LockonComp->GetLockonLocation(TargetActor);
		}
	}

	return AvatarActor->GetActorLocation() +
		AvatarActor->GetActorForwardVector() * 2000.f;
}

FVector UPantheliaProjectileSpell::GetProjectileSocketLocation() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!IsValid(AvatarActor))
	{
		return FVector::ZeroVector;
	}

	if (!AvatarActor->GetClass()->ImplementsInterface(UCombatInterface::StaticClass()))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("PantheliaProjectileSpell::GetProjectileSocketLocation — %s no implementa CombatInterface."),
			*GetNameSafe(AvatarActor));
		return AvatarActor->GetActorLocation();
	}

	// BlueprintNativeEvent: llamar siempre via Execute_ en C++.
	return ICombatInterface::Execute_GetCombatSocketLocation(
		AvatarActor,
		GetResolvedSocketTag());
}

void UPantheliaProjectileSpell::SpawnProjectile()
{
	const FVector SocketLocation = GetProjectileSocketLocation();
	const FVector TargetLocation = GetFacingTargetLocation();
	const FVector Direction = (TargetLocation - SocketLocation).GetSafeNormal();

	if (Direction.IsNearlyZero())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("PantheliaProjectileSpell::SpawnProjectile — dirección inválida en %s."),
			*GetName());
		return;
	}

	FRotator ProjectileRotation = Direction.Rotation();
	ProjectileRotation.Pitch = 0.f;
	ProjectileRotation.Roll = 0.f;

	SpawnProjectileWithRotation(ProjectileRotation);
}

APantheliaProjectile* UPantheliaProjectileSpell::SpawnProjectileWithRotation(
	const FRotator& ProjectileRotation,
	AActor* HomingTargetActor,
	const FPantheliaProjectileHomingSettings* HomingSettings,
	const float ProjectileSpeedOverride)
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	UWorld* World = GetWorld();
	if (!IsValid(AvatarActor) || !IsValid(World) || !AvatarActor->HasAuthority())
	{
		return nullptr;
	}

	// Si no se asignó ProjectileClass en el Blueprint del ability, no hay nada que spawnear.
	// Sin este guard, SpawnActorDeferred devuelve null y FinishSpawning crashea.
	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("PantheliaProjectileSpell::SpawnProjectileWithRotation — ProjectileClass no asignado en %s"),
			*GetName());
		return nullptr;
	}

	FTransform SpawnTransform;
	SpawnTransform.SetLocation(GetProjectileSocketLocation());
	SpawnTransform.SetRotation(ProjectileRotation.Quaternion());

	APantheliaProjectile* Projectile = World->SpawnActorDeferred<APantheliaProjectile>(
		ProjectileClass,
		SpawnTransform,
		GetOwningActorFromActorInfo(),
		Cast<APawn>(AvatarActor),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn);

	// SpawnActorDeferred puede devolver null si la clase no es válida o hay un
	// conflicto de colisión irrecuperable. Siempre verificar antes de usar.
	if (!Projectile)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("PantheliaProjectileSpell::SpawnProjectileWithRotation — SpawnActorDeferred devolvió null para %s"),
			*ProjectileClass->GetName());
		return nullptr;
	}

	if (DamageEffectClass)
	{
		// Un spec nuevo por proyectil. APantheliaProjectile escribe dirección de muerte,
		// knockback y launch en su EffectContext al impactar; compartir el mismo spec entre
		// hermanos permitiría que dos impactos simultáneos contaminaran ese contexto.
		Projectile->DamageEffectSpecHandle = MakeDamageSpec();
	}

	if (HomingSettings && HomingSettings->bEnabled && IsValid(HomingTargetActor))
	{
		Projectile->ConfigureSoftHoming(HomingTargetActor, *HomingSettings);
	}

	Projectile->FinishSpawning(SpawnTransform);
	Projectile->SetProjectileSpeed(ProjectileSpeedOverride);

	return Projectile;
}
