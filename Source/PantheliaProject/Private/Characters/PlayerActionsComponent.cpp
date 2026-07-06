// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/PlayerActionsComponent.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Interfaces/MainPlayer.h"

// Sets default values for this component's properties
UPlayerActionsComponent::UPlayerActionsComponent()
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// ...
}


// Called when the game starts
void UPlayerActionsComponent::BeginPlay()
{
	Super::BeginPlay();

	CharacterReference = Cast<ACharacter>(GetOwner());
	if (CharacterReference)
	{
		MovementComponent = CharacterReference->GetCharacterMovement();

		if (CharacterReference->Implements<UMainPlayer>())
		{
			PlayerInterface.SetObject(CharacterReference);
			PlayerInterface.SetInterface(Cast<IMainPlayer>(CharacterReference));
		}
	}
	
}


// Called every frame
void UPlayerActionsComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

void UPlayerActionsComponent::Sprint()
{
	if (PlayerInterface && MovementComponent)
	{

		// BUG 1: Si no nos movemos (Velocidad casi cero), no gastamos estamina
		if (MovementComponent->Velocity.Equals(FVector::ZeroVector, 1.0f))
		{
			return;
		}

		// Si pasamos los filtros, aplicamos velocidad y avisamos al mundo
		MovementComponent->MaxWalkSpeed = SprintSpeed;
	}
}

void UPlayerActionsComponent::Walk()
{
	if (MovementComponent)
	{
		MovementComponent->MaxWalkSpeed = WalkSpeed;
	}
}