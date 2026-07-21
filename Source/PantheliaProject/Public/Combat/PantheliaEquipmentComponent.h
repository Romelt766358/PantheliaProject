// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Characters/Components/PantheliaDeathPresentationTypes.h"
#include "PantheliaEquipmentComponent.generated.h"

class ACharacter;
class APantheliaWeapon;
class UPantheliaWeaponDefinition;
class UMeshComponent;
class USkeletalMeshComponent;
class UPrimitiveComponent;

// Delegate para avisar cuándo cambia el arma equipada (UI, animaciones, etc.).
// Pasa el arma nueva (puede ser null si se desequipó).
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnWeaponEquipped, APantheliaWeapon*, NewWeapon);

/**
 * UPantheliaEquipmentComponent
 *
 * Gestiona el equipo del personaje (modelo soulslike). En esta fase maneja UN slot
 * de arma a la vez, pero está estructurado para crecer a múltiples slots (arma
 * secundaria, escudo, armadura, anillos) sin reescribir la API existente.
 *
 * Responsabilidad: saber qué arma tiene equipada el personaje y gestionar el
 * equipar/desequipar (spawn del Actor APantheliaWeapon, attach a la mano, etc.).
 *
 * La ability de ataque del jugador (UPantheliaPlayerAttackAbility) consulta este
 * componente vía GetEquippedWeapon() para obtener el arma y su WeaponDefinition,
 * de donde saca el moveset, el daño y el coste de stamina.
 *
 * Va en AMainCharacter (el jugador). Los enemigos NO lo usan — ellos tienen arma
 * fija vía FinalWeaponMesh, no intercambiable.
 */
UCLASS(ClassGroup=(Combat), meta=(BlueprintSpawnableComponent))
class PANTHELIAPROJECT_API UPantheliaEquipmentComponent : public UActorComponent
{
	GENERATED_BODY()

public:
	UPantheliaEquipmentComponent();

	// Wrapper compatible con Blueprints existentes. Intenta equipar el arma de forma
	// transaccional y conserva el arma anterior si la nueva configuración falla.
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void EquipWeapon(TSubclassOf<APantheliaWeapon> WeaponClass, UPantheliaWeaponDefinition* Definition);

	// Equipa un arma de forma transaccional:
	//   1. Valida owner, socket de mano, clase y WeaponDefinition.
	//   2. Spawnea e inicializa el arma nueva en una variable temporal.
	//   3. Valida mesh activo y sockets de Weapon Trace.
	//   4. Valida el attachment.
	//   5. Solo entonces publica el arma nueva y destruye la anterior.
	// Devuelve false sin alterar el arma válida anterior cuando falla cualquier paso.
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	bool TryEquipWeapon(TSubclassOf<APantheliaWeapon> WeaponClass, UPantheliaWeaponDefinition* Definition);

	// Desequipa y destruye el arma actual (si hay). Dispara OnWeaponEquipped con null.
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void UnequipWeapon();

	// Devuelve el Actor del arma equipada actualmente. Null si no hay arma válida.
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	APantheliaWeapon* GetEquippedWeapon() const;

	// Atajo: devuelve el WeaponDefinition del arma equipada. Null si no hay arma.
	// La ability lo usa para leer moveset/daño/stamina sin pasar por el Actor.
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	UPantheliaWeaponDefinition* GetEquippedWeaponDefinition() const;

	// Cierra primero el Weapon Trace, desprende una sola vez el Actor logico del arma
	// y devuelve referencias debiles para presentacion. Equipment conserva ownership.
	FPantheliaEquippedWeaponDeathHandoff PrepareEquippedWeaponForDeath();

	// Enumera partes visuales sin asumir un unico StaticMeshComponent. El mesh activo
	// actual se devuelve primero; armas duales futuras pueden aportar varios meshes.
	void GetEquippedWeaponVisualParts(TArray<UPrimitiveComponent*>& OutVisualParts) const;

	// Se dispara cada vez que cambia el arma equipada (equip o unequip).
	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FOnWeaponEquipped OnWeaponEquipped;

	// Nombre del socket en el mesh del personaje donde se attachea el arma.
	// Debe existir en el esqueleto del personaje (normalmente la mano derecha).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	FName HandSocketName = FName("WeaponHandSocket");

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

	// Si se asigna, el componente equipa esta arma automáticamente al empezar.
	// Útil para dar al jugador un arma inicial sin lógica extra.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Defaults")
	TSubclassOf<APantheliaWeapon> DefaultWeaponClass;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Equipment|Defaults")
	TObjectPtr<UPantheliaWeaponDefinition> DefaultWeaponDefinition;

private:
	// El arma equipada actualmente (un solo slot en esta fase). Null si no hay.
	UPROPERTY()
	TObjectPtr<APantheliaWeapon> EquippedWeapon;

	bool bPreparedForDeath = false;
	FPantheliaEquippedWeaponDeathHandoff CachedDeathHandoff;

	bool ResolveOwnerAndHandSocket(ACharacter*& OutOwnerCharacter, USkeletalMeshComponent*& OutOwnerMesh) const;
	bool ValidateCandidateWeapon(APantheliaWeapon* CandidateWeapon,
		UPantheliaWeaponDefinition* Definition, UMeshComponent*& OutWeaponMesh) const;
	void ClearOwnerWeaponTraceSource() const;
	void DestroyEquippedWeapon(bool bBroadcastChange);
};
