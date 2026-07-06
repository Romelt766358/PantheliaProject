// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "PantheliaGameModeBase.generated.h"

// Forward declaration: el tipo completo solo se necesita en el .cpp
class UPantheliaCharacterClassInfo;

UCLASS()
class PANTHELIAPROJECT_API APantheliaGameModeBase : public AGameModeBase
{
	GENERATED_BODY()

public:

	// Data Asset que contiene la información de todos los arquetipos de personaje.
	// DEBE asignarse en el Blueprint BP_PantheliaGameModeBase.
	// Los enemigos lo consultan al inicializarse para obtener sus atributos por clase y nivel.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Character Class Defaults")
	TObjectPtr<UPantheliaCharacterClassInfo> CharacterClassInfo;
};