#include "PantheliaAssetManager.h"
#include "PantheliaGameplayTags.h"

UPantheliaAssetManager& UPantheliaAssetManager::Get()
{
	// GEngine siempre debería ser válido. Si no lo es, tenemos problemas mayores.
	check(GEngine);

	UPantheliaAssetManager* AssetManager = Cast<UPantheliaAssetManager>(GEngine->AssetManager);
	return *AssetManager;
}

void UPantheliaAssetManager::StartInitialLoading()
{
	Super::StartInitialLoading();

	// Inicializamos los native gameplay tags lo antes posible,
	// antes de que cualquier clase intente usarlos.
	FPantheliaGameplayTags::InitializeNativeGameplayTags();
}