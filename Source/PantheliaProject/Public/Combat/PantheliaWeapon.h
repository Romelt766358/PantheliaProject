// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "PantheliaWeapon.generated.h"

class UPantheliaWeaponDefinition;
class USkeletalMeshComponent;
class UStaticMeshComponent;
class UMeshComponent;

/**
 * APantheliaWeapon
 *
 * Presencia física de un arma del jugador en el mundo (modelo soulslike).
 * Es el Actor que se attachea a la mano del personaje. Apunta a un
 * UPantheliaWeaponDefinition (DataAsset) que contiene TODOS sus datos:
 * moveset, daño, scaling, stamina y nombres de socket de trace.
 *
 * Separación de responsabilidades:
 *   - UPantheliaWeaponDefinition = datos del arma (qué es).
 *   - APantheliaWeapon (este actor) = presencia física (el objeto en el mundo).
 *   - GA_PlayerLightAttack/HeavyAttack = comportamiento (cómo ataca).
 *
 * SOPORTA AMBOS TIPOS DE MESH: tiene un SkeletalMeshComponent y un
 * StaticMeshComponent. En InitializeFromDefinition activa el que corresponda
 * según los datos del WeaponDefinition y oculta/desactiva el otro. Esto permite
 * armas de alta calidad (skeletal, con efectos/animación) y armas simples (static)
 * con una sola clase. El WeaponTraceComponent lee los sockets del componente
 * activo vía la API común de USceneComponent::GetSocketLocation.
 *
 * Reemplaza el ABaseWeapon legacy (pre-GAS, solo static, con SceneComponents de
 * trace sueltos). El legacy se retira en la Parte 5 de la migración.
 */
UCLASS()
class PANTHELIAPROJECT_API APantheliaWeapon : public AActor
{
	GENERATED_BODY()

public:
	APantheliaWeapon();

	// Se ejecuta en el editor (y en runtime) cada vez que cambia una propiedad o se
	// coloca el actor. Llama a InitializeFromDefinition para que el mesh del arma se
	// PREVISUALICE en el viewport del Blueprint en cuanto se asigna el WeaponDefinition.
	// Esto permite ajustar la posición del mesh visualmente con el gizmo en el editor.
	virtual void OnConstruction(const FTransform& Transform) override;

	// El DataAsset que define este arma. Asignable por Blueprint (en el BP del arma)
	// o por código al spawnear/equipar. Es la fuente de todos los datos del arma.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Weapon")
	TObjectPtr<UPantheliaWeaponDefinition> WeaponDefinition;

	// Configura el arma a partir de su WeaponDefinition: asigna el mesh correcto
	// (skeletal o static), activa ese componente y oculta el otro.
	// Llamar tras spawnear el arma y haber asignado WeaponDefinition.
	// Si WeaponDefinition es null, no hace nada (el arma queda sin mesh visible).
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	void InitializeFromDefinition();

	// Devuelve el componente de mesh actualmente activo (skeletal o static) como
	// UMeshComponent (clase base común). El WeaponTraceComponent lo usa para leer
	// los sockets de trace. Devuelve null si el arma aún no se ha inicializado.
	UFUNCTION(BlueprintCallable, Category = "Weapon")
	UMeshComponent* GetActiveMeshComponent() const;

protected:
	virtual void BeginPlay() override;

	// Raíz del actor. Los dos meshes cuelgan de aquí.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USceneComponent> Root;

	// Componente para armas skeletal (alta calidad, efectos, animación).
	// Si el arma usa skeletal, asigna su mesh aquí en el Blueprint.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<USkeletalMeshComponent> SkeletalMeshComponent;

	// Componente para armas static (rígidas, simples, más baratas).
	// Si el arma usa static, asigna su mesh aquí en el Blueprint.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Components")
	TObjectPtr<UStaticMeshComponent> StaticMeshComponent;
};