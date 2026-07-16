// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystem/Abilities/PantheliaDamageGameplayAbility.h"
#include "PantheliaProjectileSpell.generated.h"

class APantheliaProjectile;
class UAnimMontage;

/**
 * UPantheliaProjectileSpell
 *
 * Ability base para hechizos que spawnan proyectiles.
 * Hereda de UPantheliaDamageGameplayAbility para tener acceso a DamageTypes y PoiseDamage.
 *
 * SpawnProjectile() crea el proyectil, le asigna el GE de daño con SetByCaller
 * para cada entrada de DamageTypes (y Damage.Poise si > 0), y lo lanza.
 *
 * Flujo de uso en el Event Graph del Blueprint hijo:
 *   1. ActivateAbility → CommitAbility (una sola vez)
 *   2. Si CommitAbility devuelve true: UpdateFacingTarget → PlayMontageAndWait
 *   3. WaitGameplayEvent (SocketTag) → SpawnProjectile()
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

	virtual void ActivateAbility(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo,
		const FGameplayEventData* TriggerEventData) override;

protected:
	// Montage de lanzamiento de hechizo para este ability.
	// Se reproduce al activar la ability. El notify AN_MontageEvent en el montage
	// (con tag igual a SocketTag) dispara SpawnProjectile() en el momento correcto.
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
	// Este tag debe coincidir con el tag del notify AN_MontageEvent en CastMontage
	// para que WaitGameplayEvent lo reciba correctamente.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Projectile")
	FGameplayTag SocketTag;

	// Clase del proyectil a spawnear. Asignar en el Blueprint del ability.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Projectile")
	TSubclassOf<APantheliaProjectile> ProjectileClass;

	// Spawna el proyectil y le asigna el spec de daño completo.
	// Llamar desde Blueprint cuando WaitGameplayEvent recibe el evento del notify.
	UFUNCTION(BlueprintCallable, Category = "Projectile")
	void SpawnProjectile();

	// Devuelve el SocketTag configurado en el Blueprint o, si está vacío, el socket
	// estándar de arma. Se mantiene protegido para que abilities especializadas
	// (como Firebolt) reutilicen exactamente la misma resolución que SpawnProjectile().
	FGameplayTag GetResolvedSocketTag() const;

	// Devuelve el punto lógico hacia el que debe salir el proyectil. Con lock-on usa
	// ULockonComponent::GetLockonLocation(), de modo que cámara, proyectiles y futuras
	// trayectorias homing apunten al mismo punto del enemigo. Sin lock-on usa un punto
	// frontal estable a 2000 cm, conservando el comportamiento actual del proyecto.
	FVector GetFacingTargetLocation() const;
};
