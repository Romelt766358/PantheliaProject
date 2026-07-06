// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "PantheliaAIController.generated.h"

// Forward declaration: evita incluir el header completo aquí.
// Solo necesitamos el puntero; el .cpp incluirá el header real.
class UBehaviorTreeComponent;

/**
 * APantheliaAIController
 *
 * Controlador de IA para todos los enemigos de Panthelia.
 * Su responsabilidad principal es arrancar el Behavior Tree del enemigo.
 *
 * ¿Por qué un AIController propio en lugar del genérico de Unreal?
 * Necesitamos un UBehaviorTreeComponent explícito para poder ejecutar
 * el árbol de decisiones. El UBlackboardComponent (la "pizarra" de datos)
 * ya lo hereda AAIController como variable protegida llamada "Blackboard",
 * así que no hace falta crear uno propio.
 *
 * Directorio sugerido: Source/PantheliaProject/AI/
 */
UCLASS()
class PANTHELIAPROJECT_API APantheliaAIController : public AAIController
{
	GENERATED_BODY()

public:
	APantheliaAIController();

protected:
	// Componente que ejecuta el Behavior Tree del enemigo cada tick.
	// Lo creamos explícitamente para tener una referencia directa a él.
	// AAIController no tiene uno por defecto — el Blackboard sí, este no.
	UPROPERTY()
	TObjectPtr<UBehaviorTreeComponent> BehaviorTreeComponent;
};
