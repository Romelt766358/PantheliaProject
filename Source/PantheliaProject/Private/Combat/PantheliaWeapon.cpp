// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/PantheliaWeapon.h"
#include "Combat/PantheliaWeaponDefinition.h"
#include "Components/SceneComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"

APantheliaWeapon::APantheliaWeapon()
{
	// El arma no necesita Tick: es pasiva, solo se mueve attacheada a la mano.
	PrimaryActorTick.bCanEverTick = false;

	Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent = Root;

	// Creamos AMBOS componentes de mesh. Cada arma (Blueprint hijo) asigna su mesh
	// en UNO de ellos según sea skeletal o static; el otro queda vacío.
	// Ambos ignoran colisión física para que el arma no empuje al personaje;
	// la detección de golpes la hace el WeaponTraceComponent vía sweep, no la colisión.
	SkeletalMeshComponent = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SkeletalMeshComponent"));
	SkeletalMeshComponent->SetupAttachment(Root);
	SkeletalMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	SkeletalMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	StaticMeshComponent = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("StaticMeshComponent"));
	StaticMeshComponent->SetupAttachment(Root);
	StaticMeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
	StaticMeshComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void APantheliaWeapon::BeginPlay()
{
	Super::BeginPlay();

	// Si el arma ya trae un WeaponDefinition asignado por Blueprint, la inicializamos
	// automáticamente. Si se asigna por código tras spawnear, llamar InitializeFromDefinition
	// manualmente después de setear WeaponDefinition.
	if (WeaponDefinition)
	{
		InitializeFromDefinition();
	}
}

void APantheliaWeapon::OnConstruction(const FTransform& Transform)
{
	Super::OnConstruction(Transform);

	// El mesh ahora se asigna directamente en el componente del Blueprint del arma,
	// por lo que se previsualiza en el viewport sin necesidad de inicializarlo aquí.
	// Este override se conserva por si en el futuro se necesita lógica de construcción
	// dependiente del WeaponDefinition (ej. tintes/VFX por elemento en el editor).
}

void APantheliaWeapon::InitializeFromDefinition()
{
	// Sin datos no hay nada que configurar.
	if (!WeaponDefinition)
	{
		UE_LOG(LogTemp, Warning, TEXT("APantheliaWeapon::InitializeFromDefinition — WeaponDefinition es null en %s"), *GetName());
		return;
	}

	// NOTA: el mesh ya NO se asigna aquí. Cada arma es un Blueprint con su mesh
	// asignado en el componente (ajustado visualmente en el viewport del BP).
	// Este método queda disponible para futura lógica que dependa de los DATOS del
	// arma al equiparse (ej. aplicar VFX por elemento, configurar trail, etc.).
	// El mesh activo se resuelve bajo demanda en GetActiveMeshComponent().
}

UMeshComponent* APantheliaWeapon::GetActiveMeshComponent() const
{
	// Detecta cuál componente tiene un mesh asignado en el Blueprint del arma.
	// Prioriza el skeletal si ambos tuvieran (caso raro). El WeaponTraceComponent
	// usa el resultado para leer los sockets de trace de la hoja.
	if (SkeletalMeshComponent && SkeletalMeshComponent->GetSkeletalMeshAsset())
	{
		return SkeletalMeshComponent;
	}
	if (StaticMeshComponent && StaticMeshComponent->GetStaticMesh())
	{
		return StaticMeshComponent;
	}
	return nullptr;
}