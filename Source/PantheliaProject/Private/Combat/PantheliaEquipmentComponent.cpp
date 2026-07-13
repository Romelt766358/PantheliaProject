// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/PantheliaEquipmentComponent.h"
#include "Combat/PantheliaWeapon.h"
#include "Combat/PantheliaWeaponDefinition.h"
#include "Combat/WeaponTraceComponent.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"

UPantheliaEquipmentComponent::UPantheliaEquipmentComponent()
{
	// El equipo no necesita Tick: solo reacciona a equip/unequip.
	PrimaryComponentTick.bCanEverTick = false;
}

void UPantheliaEquipmentComponent::BeginPlay()
{
	Super::BeginPlay();

	// Si hay un arma por defecto configurada, la equipamos automáticamente.
	// Permite dar un arma inicial al jugador sin lógica extra en el Blueprint.
	if (DefaultWeaponClass && DefaultWeaponDefinition)
	{
		EquipWeapon(DefaultWeaponClass, DefaultWeaponDefinition);
	}
}

void UPantheliaEquipmentComponent::EquipWeapon(
	TSubclassOf<APantheliaWeapon> WeaponClass, UPantheliaWeaponDefinition* Definition)
{
	if (!WeaponClass || !Definition)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipWeapon: WeaponClass o Definition es null."));
		return;
	}

	UWorld* World = GetWorld();
	if (!World) return;

	// El owner debe ser un Character (necesitamos su mesh para attachear el arma).
	ACharacter* OwnerCharacter = Cast<ACharacter>(GetOwner());
	if (!OwnerCharacter)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipWeapon: el owner del EquipmentComponent no es un ACharacter."));
		return;
	}

	// Si ya hay un arma equipada, la quitamos primero.
	if (EquippedWeapon)
	{
		UnequipWeapon();
	}

	// Spawnear el Actor del arma. Owner = el personaje (para autoría/instigador).
	FActorSpawnParameters SpawnParams;
	SpawnParams.Owner = OwnerCharacter;
	SpawnParams.Instigator = OwnerCharacter;
	SpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

	APantheliaWeapon* NewWeapon = World->SpawnActor<APantheliaWeapon>(WeaponClass, SpawnParams);
	if (!NewWeapon)
	{
		UE_LOG(LogTemp, Warning, TEXT("EquipWeapon: no se pudo spawnear el arma."));
		return;
	}

	// Asignar el WeaponDefinition e inicializar (esto activa el mesh correcto).
	NewWeapon->WeaponDefinition = Definition;
	NewWeapon->InitializeFromDefinition();

	// Attachear el arma al socket de la mano del personaje.
	const FAttachmentTransformRules AttachRules(EAttachmentRule::SnapToTarget, true);
	NewWeapon->AttachToComponent(OwnerCharacter->GetMesh(), AttachRules, HandSocketName);

	// Cachear y notificar.
	EquippedWeapon = NewWeapon;
	OnWeaponEquipped.Broadcast(EquippedWeapon);
}

void UPantheliaEquipmentComponent::UnequipWeapon()
{
	if (!EquippedWeapon) return;

	// El arma del jugador es la fuente externa del WeaponTraceComponent. Limpiamos
	// TODA la referencia de trace antes de destruir el Actor del arma para que un
	// notify tardío no pueda reutilizar el spec, sonido o mesh del ataque anterior.
	if (AActor* OwnerActor = GetOwner())
	{
		if (UWeaponTraceComponent* TraceComponent =
			OwnerActor->FindComponentByClass<UWeaponTraceComponent>())
		{
			TraceComponent->ClearExternalWeaponTraceSource();
		}
	}

	EquippedWeapon->Destroy();
	EquippedWeapon = nullptr;

	OnWeaponEquipped.Broadcast(nullptr);
}

UPantheliaWeaponDefinition* UPantheliaEquipmentComponent::GetEquippedWeaponDefinition() const
{
	if (EquippedWeapon)
	{
		return EquippedWeapon->WeaponDefinition;
	}
	return nullptr;
}
