// Fill out your copyright notice in the Description page of Project Settings.

#include "AI/PantheliaAIController.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "BehaviorTree/BlackboardComponent.h"

APantheliaAIController::APantheliaAIController()
{
	// Creamos el BehaviorTreeComponent para poder ejecutar el árbol de decisiones.
	BehaviorTreeComponent = CreateDefaultSubobject<UBehaviorTreeComponent>(TEXT("BehaviorTreeComponent"));

	// Asignamos el BlackboardComponent a la variable "Blackboard" que ya existe
	// en la clase base AAIController (variable protegida). Así no duplicamos
	// variables innecesariamente y GetBlackboardComponent() funcionará correctamente.
	Blackboard = CreateDefaultSubobject<UBlackboardComponent>(TEXT("BlackboardComponent"));
}
