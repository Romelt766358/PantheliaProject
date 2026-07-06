// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "NiagaraComponent.h"
#include "GameplayTagContainer.h"
#include "PantheliaDebuffNiagaraComponent.generated.h"

// ============================================================
// UPantheliaDebuffNiagaraComponent (clase 311)
// ============================================================
//
// Subclase de UNiagaraComponent que se activa/desactiva SOLA, reaccionando a si su
// propietario tiene o no un tag de debuff concreto (DebuffTag). No es solo "un
// componente Niagara" — es un componente que además ESCUCHA al ability system del
// personaje al que pertenece.
//
// USO: se añade uno de estos por cada elemento que un personaje pueda sufrir (hoy solo
// Quemadura/Fuego; Electrocución/Saturación/Veneno se añadirían igual el día que existan
// sus VFX — el componente es genérico, solo cambia el Niagara System y el DebuffTag que
// se le asigna en cada instancia). Ver APantheliaCharacterBase, donde se crea
// BurnDebuffComponent en el constructor y se le asigna DebuffTag = Debuff.Burn.
//
// CÓMO SABE CUÁNDO ACTIVARSE: se suscribe al evento de cambio de un gameplay tag
// concreto (DebuffTag) en el ASC de su propietario. Cuando ese tag se concede (el
// personaje empieza a arder), se activa; cuando se remueve (el debuff termina o el
// personaje muere), se desactiva. Ver DebuffTagChanged() en el .cpp.
//
// EL PROBLEMA DE ORDEN QUE RESUELVE (importante entender esto): un componente puede
// ejecutar su propio BeginPlay() ANTES de que el ASC del personaje propietario esté
// listo — Unreal no garantiza en qué orden se inicializan los componentes de un actor
// frente a la lógica de ese actor (InitAbilityActorInfo). Si este componente intentara
// leer el ASC en su BeginPlay() y todavía no existiera, se quedaría sin suscribirse a
// nada, para siempre. La solución: si el ASC ya está listo, nos suscribimos directo; si
// no, nos suscribimos al delegate ICombatInterface::GetOnASCRegisteredDelegate(), que
// el personaje dispara en cuanto su ASC SÍ está listo (ver PantheliaEnemy.cpp y
// MainCharacter.cpp) — así este componente consigue suscribirse tarde pero sin fallar.
// ============================================================
UCLASS()
class PANTHELIAPROJECT_API UPantheliaDebuffNiagaraComponent : public UNiagaraComponent
{
	GENERATED_BODY()

public:
	UPantheliaDebuffNiagaraComponent();

	// Tag que identifica QUÉ debuff activa este componente concreto (ej. Debuff.Burn).
	// Se asigna una vez por instancia, normalmente en el constructor de quien lo crea
	// (ver APantheliaCharacterBase, BurnDebuffComponent).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Panthelia|Debuff")
	FGameplayTag DebuffTag;

protected:
	virtual void BeginPlay() override;

	// Callback del evento de cambio de tag (ver RegisterGameplayTagEvent en el .cpp).
	// NewCount > 0 significa que el tag se concedió (al menos una vez) → activar.
	// NewCount == 0 significa que ya no queda ninguna instancia del tag → desactivar.
	// Firma exigida por RegisterGameplayTagEvent: FGameplayTag por valor (no por
	// referencia) + int32. No es un UFUNCTION porque se bindea con AddUObject, no con
	// AddDynamic (ver la explicación de AddWeakLambda vs AddDynamic en el .cpp).
	void DebuffTagChanged(const FGameplayTag CallbackTag, int32 NewCount);

	// Callback del delegate ICombatInterface::FOnDeath. SÍ necesita ser UFUNCTION()
	// porque FOnDeath es un delegate DYNAMIC (DECLARE_DYNAMIC_MULTICAST_DELEGATE) y se
	// bindea con AddDynamic, que requiere que la función de destino esté reflejada (los
	// delegates dynamic se serializan/reflejan como cualquier UFUNCTION; los no-dynamic,
	// como el de arriba, no lo necesitan).
	UFUNCTION()
	void OnOwnerDeath(AActor* DeadActor);
};
