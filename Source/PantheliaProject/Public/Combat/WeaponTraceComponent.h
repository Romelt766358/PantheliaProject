// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "GameplayEffectTypes.h"
#include "WeaponTraceComponent.generated.h"

class USoundBase;

/**
 * UWeaponTraceComponent
 *
 * Sistema de hitbox por sweep continuo a lo largo de la hoja del arma, estilo
 * soulslike (Elden Ring, Lies of P). Reemplaza la detección de hits melee
 * puntual del curso (overlap esférico de radio fijo en un solo socket).
 *
 * FUNCIONAMIENTO (ver State_Combat.md §9):
 * 1. La ability de ataque (GA_MeleeAttack) construye un FGameplayEffectSpecHandle
 * con todo el escalado de daño ya aplicado y se lo entrega a este componente
 * vía SetDamageSpec() — mismo patrón que usan los proyectiles.
 * 2. Un UWeaponTraceNotifyState en el montage abre la ventana de daño:
 * NotifyBegin -> ActivateTrace(), NotifyEnd -> DeactivateTrace().
 * 3. Mientras la ventana está activa, cada tick hace un SweepMultiByChannel
 * con forma de cápsula entre los sockets WeaponBaseSocketName y
 * WeaponTipSocketName del mesh del arma.
 * 4. Por cada actor golpeado: si no está en IgnoredActors de este swing y
 * IsNotFriend devuelve true, se le aplica el spec de daño guardado y se
 * añade a la lista de ignorados (un hit por swing, multi-objetivo permitido).
 *
 * DISEÑO: componente reutilizable. Pensado primero para enemigos melee, pero
 * cualquier actor (incluido el jugador a futuro) puede usarlo. El daño se aplica
 * vía GAS (el spec ya viene de UPantheliaDamageGameplayAbility), NO vía TakeDamage.
 *
 * Es el reemplazo GAS-nativo del UTraceComponent legacy (sistema pre-GAS del jugador).
 */
UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class PANTHELIAPROJECT_API UWeaponTraceComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UWeaponTraceComponent();

	virtual void TickComponent(float DeltaTime, ELevelTick TickType,
		FActorComponentTickFunction* ThisTickFunction) override;

	// Guarda el spec de daño que se aplicará a los actores golpeados durante el swing.
	// Lo llama la ability de ataque (GA_MeleeAttack) al activarse, ANTES de que el
	// notify abra la ventana de daño. El spec ya trae todo el escalado aplicado.
	UFUNCTION(BlueprintCallable, Category = "WeaponTrace")
	void SetDamageSpec(const FGameplayEffectSpecHandle& InDamageSpecHandle);

	// Registra el MontageTag del ataque en curso (Montage.Attack.Weapon, etc.).
	// La ability de ataque lo llama junto a SetDamageSpec, antes de la ventana de daño.
	// PerformTrace lo incluye en los params del GameplayCue.Melee.Impact (AggregatedSourceTags)
	// para que el GC sepa qué sonido de impacto reproducir (ImpactSound en FTaggedMontage).
	UFUNCTION(BlueprintCallable, Category = "WeaponTrace")
	void SetActiveMontageTag(const FGameplayTag& InMontageTag);

	// Registra el sonido de impacto del ataque en curso. La ability de ataque lo llama junto
	// a SetDamageSpec, antes de la ventana de daño, leyendolo del montage del arma equipada.
	// PerformTrace lo reproduce con PlaySoundAtLocation en el punto de impacto, solo si hay hit.
	// Si es null (no se asigno sonido a ese montage), no se reproduce nada (silencioso).
	// Reproducir aqui directamente (no por Cue) es suficiente porque el juego es single-player.
	UFUNCTION(BlueprintCallable, Category = "WeaponTrace")
	void SetActiveImpactSound(USoundBase* InImpactSound);

	// Define si este swing puede activar auto lock-on al conectar.
	// No es una opción global: lo setea la ability antes de cada ataque.
	// La opción global vive en ULockonComponent para que el futuro menú pueda activarla/desactivarla.
	UFUNCTION(BlueprintCallable, Category = "WeaponTrace")
	void SetAutoLockOnFromBasicAttackHitAllowed(bool bInAllowed);

	// Abre la ventana de daño: a partir de aquí el Tick hace sweeps cada frame.
	// Lo llama UWeaponTraceNotifyState::NotifyBegin.
	UFUNCTION(BlueprintCallable, Category = "WeaponTrace")
	void ActivateTrace();

	// Cierra la ventana de daño y limpia la lista de ignorados para el próximo swing.
	// Lo llama UWeaponTraceNotifyState::NotifyEnd.
	UFUNCTION(BlueprintCallable, Category = "WeaponTrace")
	void DeactivateTrace();

	// Asigna externamente el mesh del arma y, opcionalmente, los nombres de socket.
	// Pensado para el JUGADOR: su arma es un Actor separado (APantheliaWeapon), no un
	// componente del personaje, así que ResolveWeaponMesh() no la encontraría. La
	// ability de ataque del jugador llama esto pasando el GetActiveMeshComponent() del
	// arma equipada y los nombres de socket del WeaponDefinition.
	// Si BaseSocket/TipSocket se dejan en NAME_None, conserva los nombres ya configurados.
	// Los enemigos NO usan esto — siguen resolviendo su mesh vía ResolveWeaponMesh().
	UFUNCTION(BlueprintCallable, Category = "WeaponTrace")
	void SetWeaponMeshComponent(UPrimitiveComponent* InWeaponMesh,
		FName InBaseSocketName = NAME_None, FName InTipSocketName = NAME_None);

	// El mesh del arma del que se leen los sockets WeaponBase/WeaponTip.
	// Si es null, el componente intenta resolverlo en BeginPlay desde el dueño
	// (busca un componente con el tag "Weapon" o el primer StaticMeshComponent).
	// Configurable por Blueprint para armas que viven en componentes distintos.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WeaponTrace")
	TObjectPtr<UPrimitiveComponent> WeaponMeshComponent;

protected:
	virtual void BeginPlay() override;

	// Radio de la cápsula de sweep (grosor del hitbox de la hoja). Más grande = más
	// permisivo. Para una espada normal 10-20 suele bastar.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WeaponTrace")
	float TraceRadius = 15.f;

	// Socket en la empuñadura/base de la hoja (inicio del sweep).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WeaponTrace")
	FName WeaponBaseSocketName = FName("WeaponBase");

	// Socket en la punta de la hoja (fin del sweep).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WeaponTrace")
	FName WeaponTipSocketName = FName("WeaponTip");

	// Canal de colisión usado por el sweep. Por defecto el canal "Fighter"
	// (ECC_GameTraceChannel1), al que el jugador responde con Overlap.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WeaponTrace")
	TEnumAsByte<ECollisionChannel> TraceChannel = ECollisionChannel::ECC_GameTraceChannel1;

	// Dibuja la cápsula de sweep durante la ventana de daño para depurar.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "WeaponTrace")
	bool bDebugMode = false;

private:
	// True mientras la ventana de daño está abierta (entre NotifyBegin y NotifyEnd).
	bool bIsTracing = false;

	// Spec de daño a aplicar a los actores golpeados. Lo provee la ability.
	FGameplayEffectSpecHandle DamageSpecHandle;

	// MontageTag del ataque en curso (ej. Montage.Attack.Weapon). Lo provee la ability
	// junto al DamageSpec. PerformTrace lo pasa en AggregatedSourceTags del Cue para que
	// el GC_MeleeImpact pueda buscar el ImpactSound correcto en el array de montages
	// del atacante (EffectCauser).
	FGameplayTag ActiveMontageTag;

	// Sonido de impacto del ataque en curso. Lo provee la ability (lo lee del montage del
	// arma equipada). PerformTrace lo reproduce con PlaySoundAtLocation en el punto de hit.
	// Null = sin sonido para ese ataque (no se reproduce nada).
	UPROPERTY()
	TObjectPtr<USoundBase> ActiveImpactSound = nullptr;

	// True solo para swings que deben activar auto lock-on al conectar.
	// La ability del jugador lo pone en true para ataque básico y false para otros ataques.
	bool bAutoLockOnFromBasicAttackHitAllowed = false;

	// Actores ya golpeados en el swing actual. Evita multi-hit del mismo swing.
	// Se limpia en DeactivateTrace().
	UPROPERTY()
	TArray<TObjectPtr<AActor>> IgnoredActors;

	// Hace un sweep de cápsula entre los sockets y aplica daño a los impactados.
	// Se llama cada tick mientras bIsTracing es true.
	void PerformTrace();

	// Resuelve el mesh del arma si WeaponMeshComponent no fue asignado en el editor.
	void ResolveWeaponMesh();
};
