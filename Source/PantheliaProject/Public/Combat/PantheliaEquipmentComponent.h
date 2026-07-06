// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "PantheliaEquipmentComponent.generated.h"

class APantheliaWeapon;
class UPantheliaWeaponDefinition;

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

	// Equipa un arma a partir de su WeaponDefinition:
	//   1. Si ya hay un arma equipada, la desequipa (destruye su Actor).
	//   2. Spawnea un APantheliaWeapon, le asigna el WeaponDefinition y lo inicializa.
	//   3. Lo attachea al socket de la mano del personaje (HandSocketName).
	//   4. Lo cachea como arma equipada y dispara OnWeaponEquipped.
	// WeaponClass: la clase de Actor a spawnear (normalmente un BP hijo de APantheliaWeapon).
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void EquipWeapon(TSubclassOf<APantheliaWeapon> WeaponClass, UPantheliaWeaponDefinition* Definition);

	// Desequipa y destruye el arma actual (si hay). Dispara OnWeaponEquipped con null.
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	void UnequipWeapon();

	// Devuelve el Actor del arma equipada actualmente. Null si no hay arma.
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	APantheliaWeapon* GetEquippedWeapon() const { return EquippedWeapon; }

	// Atajo: devuelve el WeaponDefinition del arma equipada. Null si no hay arma.
	// La ability lo usa para leer moveset/daño/stamina sin pasar por el Actor.
	UFUNCTION(BlueprintCallable, Category = "Equipment")
	UPantheliaWeaponDefinition* GetEquippedWeaponDefinition() const;

	// Se dispara cada vez que cambia el arma equipada (equip o unequip).
	UPROPERTY(BlueprintAssignable, Category = "Equipment")
	FOnWeaponEquipped OnWeaponEquipped;

	// Nombre del socket en el mesh del personaje donde se attachea el arma.
	// Debe existir en el esqueleto del personaje (normalmente la mano derecha).
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Equipment")
	FName HandSocketName = FName("WeaponHandSocket");

protected:
	virtual void BeginPlay() override;

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
};
