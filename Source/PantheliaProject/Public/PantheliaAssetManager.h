#pragma once

#include "CoreMinimal.h"
#include "Engine/AssetManager.h"
#include "PantheliaAssetManager.generated.h"

UCLASS()
class PANTHELIAPROJECT_API UPantheliaAssetManager : public UAssetManager
{
	GENERATED_BODY()

public:

	// Devuelve la única instancia del AssetManager casteada a nuestro tipo.
	static UPantheliaAssetManager& Get();

protected:

	// Se llama muy temprano al inicio del juego, antes de que nada se cargue.
	// Es el lugar ideal para inicializar los native gameplay tags.
	virtual void StartInitialLoading() override;
};