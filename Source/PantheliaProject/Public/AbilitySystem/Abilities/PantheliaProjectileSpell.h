// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PantheliaDamageGameplayAbility.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

#include "PantheliaProjectileSpell.generated.h"

class APantheliaProjectile;
class UAnimMontage;
struct FPantheliaProjectileHomingSettings;

/**
 * UPantheliaProjectileSpell
 *
 * Ability base para hechizos que spawnan proyectiles.
 * Hereda de UPantheliaDamageGameplayAbility para tener acceso a DamageTypes y PoiseDamage.
 *
 * SpawnProjectile() mantiene el flujo de un único proyectil usado por Firebolt y por
 * enemigos ranged. Las subclases especializadas pueden reutilizar
 * SpawnProjectileWithRotation() para crear patrones sin duplicar el pipeline de daño.
 *
 * Flujo de uso en el Event Graph del Blueprint hijo de proyectil único:
 *   1. ActivateAbility → CommitAbility (una sola vez)
 *   2. Si CommitAbility devuelve true: UpdateFacingTarget → PlayMontageAndWait
 *   3. WaitGameplayEvent (ProjectileSpawnEventTag) → SpawnProjectile()
 *   4. OnCompleted / OnInterrupted / OnCancelled → EndAbility
 *
 * COSTES DEL JUGADOR:
 *   - Coste principal: Mana.
 *   - AdditionalResourceCosts[0]: Stamina.
 * Ambos se validan antes de cobrar y se aplican dentro del mismo CommitAbility. Los
 * hechizos de enemigos mantienen bUsePantheliaResourceCost=false.
 *
 * El socket de spawn se configura por Blueprint via SocketTag:
 *   Montage.Attack.Weapon    → punta del arma (jugador)
 *   Montage.Attack.RightHand → mano derecha (enemigos mágicos)
 *   Montage.Attack.LeftHand  → mano izquierda
 *
 * IMPORTANTE: NO añadir CastMontage al array AttackMontages del enemigo.
 * GA_MeleeAttack elige montages de ese array al azar — mezclar montages de
 * casting causaría que el ataque melee reproduzca animaciones incorrectas.
 */
UCLASS()
class PANTHELIAPROJECT_API UPantheliaProjectileSpell : public UPantheliaDamageGameplayAbility
{
	GENERATED_BODY()

public:
	UPantheliaProjectileSpell();

#if WITH_EDITOR
	virtual EDataValidationResult IsDataValid(
		FDataValidationContext& Context) const override;

	UAnimMontage* GetCastMontageForEditor() const
	{
		return CastMontage;
	}

	FGameplayTag GetSocketTagForEditor() const
	{
		return SocketTag;
	}

	FGameplayTag GetProjectileSpawnEventTagForEditor() const
	{
		return ProjectileSpawnEventTag;
	}

	TSubclassOf<APantheliaProjectile>
	GetProjectileClassForEditor() const
	{
		return ProjectileClass;
	}
#endif

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	// Montage de lanzamiento de hechizo para este ability.
	// Se reproduce al activar la ability. El notify AN_MontageEvent en el montage
	// (con tag igual a ProjectileSpawnEventTag) dispara SpawnProjectile() en el momento correcto.
	//
	// Asignar en el Blueprint del ability (GA_RangedAttack, GA_Firebolt, etc.).
	// NO añadir este montage al array AttackMontages del enemigo — ese array es
	// exclusivo de GA_MeleeAttack que lo usa con selección aleatoria.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	TObjectPtr<UAnimMontage> CastMontage;

	// Socket desde donde se spawna el proyectil.
	// Debe coincidir con un tag que GetCombatSocketLocation sepa resolver:
	//   Montage.Attack.Weapon    → punta del arma (jugador con arma equipada)
	//   Montage.Attack.RightHand → mano derecha del mesh (enemigos mágicos)
	//   Montage.Attack.LeftHand  → mano izquierda del mesh
	// Por defecto: tag inválido → fallback automático a Weapon en SpawnProjectile().
	//
	// Este tag solo representa el socket físico usado por GetCombatSocketLocation.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	FGameplayTag SocketTag;

	// Gameplay Event que espera WaitGameplayEvent para disparar SpawnProjectile().
	// Debe coincidir con el tag del notify AN_MontageEvent en CastMontage.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	FGameplayTag ProjectileSpawnEventTag;

	// Clase del proyectil a spawnear. Asignar en el Blueprint del ability.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile")
	TSubclassOf<APantheliaProjectile> ProjectileClass;

	// Spawna un único proyectil y le asigna el spec de daño completo.
	// Llamar desde Blueprint cuando WaitGameplayEvent recibe el evento del notify.
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void SpawnProjectile();

	// Devuelve el SocketTag configurado en el Blueprint o, si está vacío, el socket
	// estándar de arma. Las abilities especializadas reutilizan la misma resolución.
	FGameplayTag GetResolvedSocketTag() const;

	// Devuelve el actor de lock-on actual. Null cuando el caster no tiene lock-on.
	// Se deja virtual para que futuras variantes enemigas resuelvan su CombatTarget
	// sin duplicar el resto del sistema de proyectiles.
	virtual AActor* GetFacingTargetActor() const;

	// Devuelve el punto lógico hacia el que debe salir el proyectil. Con lock-on usa
	// ULockonComponent::GetLockonLocation(), de modo que cámara, proyectiles y homing
	// apunten al mismo punto. Sin lock-on usa un punto frontal estable a 2000 cm.
	virtual FVector GetFacingTargetLocation() const;

	// Obtiene el socket real de este disparo mediante ICombatInterface.
	FVector GetProjectileSocketLocation() const;

	// Ruta única de spawn para proyectiles simples y múltiples. Cada llamada crea un
	// spec de daño independiente; esto evita compartir un EffectContext mutable entre
	// proyectiles hermanos que pueden impactar al mismo tiempo.
	APantheliaProjectile* SpawnProjectileWithRotation(
		const FRotator& ProjectileRotation,
		AActor* HomingTargetActor = nullptr,
		const FPantheliaProjectileHomingSettings* HomingSettings = nullptr,
		float ProjectileSpeedOverride = 0.f);
};
