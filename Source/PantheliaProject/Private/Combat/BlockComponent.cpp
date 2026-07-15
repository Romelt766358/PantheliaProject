// Fill out your copyright notice in the Description page of Project Settings.


#include "Combat/BlockComponent.h"

// Sets default values for this component's properties
UBlockComponent::UBlockComponent()
{
	// Este componente legacy no tiene trabajo por frame. Conservamos su API mientras
	// termina la migración, pero evitamos el coste de Tick permanente.
	PrimaryComponentTick.bCanEverTick = false;

	// ...
}


// Called when the game starts
void UBlockComponent::BeginPlay()
{
	Super::BeginPlay();

	// ...
	
}


// Called every frame
void UBlockComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// ...
}

