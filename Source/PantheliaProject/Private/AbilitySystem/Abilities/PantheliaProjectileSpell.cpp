// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/PantheliaProjectileSpell.h"
#include "Actor/PantheliaProjectile.h"
#include "Interfaces/CombatInterface.h"
#include "Combat/LockonComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "PantheliaGameplayTags.h"
#include "AbilitySystem/PantheliaAttributeSet.h"

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

void UPantheliaProjectileSpell::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);
}

FVector UPantheliaProjectileSpell::GetFacingTargetLocation() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor) return FVector::ZeroVector;

	ULockonComponent* LockonComp = AvatarActor->FindComponentByClass<ULockonComponent>();
	if (LockonComp && IsValid(LockonComp->CurrentTargetActor))
	{
		return LockonComp->CurrentTargetActor->GetActorLocation();
	}

	return AvatarActor->GetActorLocation() + AvatarActor->GetActorForwardVector() * 2000.f;
}

void UPantheliaProjectileSpell::SpawnProjectile()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor || !AvatarActor->HasAuthority()) return;

	// Si no se asignó ProjectileClass en el Blueprint del ability, no hay nada que spawnear.
	// Sin este guard, SpawnActorDeferred devuelve null y FinishSpawning crashea.
	if (!ProjectileClass)
	{
		UE_LOG(LogTemp, Warning, TEXT("PantheliaProjectileSpell::SpawnProjectile — ProjectileClass no asignado en %s"), *GetName());
		return;
	}

	const FPantheliaGameplayTags& GameplayTags = FPantheliaGameplayTags::Get();

	// Usar el SocketTag configurado en el BP del ability.
	// Si no se configuró (tag inválido), fallback al socket de arma
	// para mantener compatibilidad con el ability del jugador.
	const FGameplayTag ResolvedSocketTag = SocketTag.IsValid()
		? SocketTag
		: GameplayTags.Montage_Attack_Weapon;

	// BlueprintNativeEvent: llamar siempre via Execute_ en C++.
	const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(
		AvatarActor, ResolvedSocketTag);
	const FVector TargetLocation = GetFacingTargetLocation();
	const FVector Direction = (TargetLocation - SocketLocation).GetSafeNormal();

	FRotator ProjectileRotation = Direction.Rotation();
	ProjectileRotation.Pitch = 0.f;

	FTransform SpawnTransform;
	SpawnTransform.SetLocation(SocketLocation);
	SpawnTransform.SetRotation(ProjectileRotation.Quaternion());

	APantheliaProjectile* Projectile = GetWorld()->SpawnActorDeferred<APantheliaProjectile>(
		ProjectileClass,
		SpawnTransform,
		GetOwningActorFromActorInfo(),
		Cast<APawn>(AvatarActor),
		ESpawnActorCollisionHandlingMethod::AlwaysSpawn
	);

	// SpawnActorDeferred puede devolver null si la clase no es válida o hay un
	// conflicto de colisión irrecuperable. Siempre verificar antes de usar.
	if (!Projectile)
	{
		UE_LOG(LogTemp, Warning, TEXT("PantheliaProjectileSpell::SpawnProjectile — SpawnActorDeferred devolvió null para %s"), *ProjectileClass->GetName());
		return;
	}

	if (DamageEffectClass)
	{
		if (UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(AvatarActor))
		{
			FGameplayEffectContextHandle EffectContextHandle = SourceASC->MakeEffectContext();
			EffectContextHandle.AddSourceObject(AvatarActor);

			FGameplayEffectSpecHandle DamageSpecHandle = SourceASC->MakeOutgoingSpec(
				DamageEffectClass, GetAbilityLevel(), EffectContextHandle);

			// Aplicar todo el escalado de daño (base + atributos + postura) al spec.
			// Lógica centralizada en la clase base UPantheliaDamageGameplayAbility,
			// compartida con CauseDamage (melee) para garantizar consistencia.
			ApplyDamageScalingToSpec(DamageSpecHandle, SourceASC);

			Projectile->DamageEffectSpecHandle = DamageSpecHandle;
		}
	}

	Projectile->FinishSpawning(SpawnTransform);
}