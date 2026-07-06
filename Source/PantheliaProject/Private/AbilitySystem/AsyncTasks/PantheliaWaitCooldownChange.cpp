// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/AsyncTasks/PantheliaWaitCooldownChange.h"
#include "AbilitySystemComponent.h"

UPantheliaWaitCooldownChange* UPantheliaWaitCooldownChange::WaitForCooldownChange(
	UAbilitySystemComponent* AbilitySystemComponent,
	const FGameplayTag& InCooldownTag)
{
	// NewObject (no NewAbilityTask): esto es un Blueprint Async Action, no un Ability Task.
	UPantheliaWaitCooldownChange* WaitCooldownChange = NewObject<UPantheliaWaitCooldownChange>();
	WaitCooldownChange->ASC = AbilitySystemComponent;
	WaitCooldownChange->CooldownTag = InCooldownTag;

	// Salida temprana: si no hay ASC valido o el tag es invalido, no hay nada que escuchar.
	// Terminamos el task de inmediato y devolvemos nullptr (el nodo Blueprint no hara nada).
	if (!IsValid(AbilitySystemComponent) || !InCooldownTag.IsValid())
	{
		WaitCooldownChange->EndTask();
		return nullptr;
	}

	// Suscripcion 1: avisar cuando el CONTEO del tag de cooldown cambia.
	// EGameplayTagEventType::NewOrRemoved dispara el callback tanto al anadirse el tag
	// (conteo 0 -> 1, empieza el cooldown) como al quitarse (conteo 1 -> 0, termina).
	// Usamos esto principalmente para detectar el FIN (cuando NewCount llega a 0).
	AbilitySystemComponent->RegisterGameplayTagEvent(
		InCooldownTag,
		EGameplayTagEventType::NewOrRemoved).AddUObject(
			WaitCooldownChange,
			&UPantheliaWaitCooldownChange::CooldownTagChanged);

	// Suscripcion 2: avisar cuando se aplica cualquier GE basado en duracion.
	// OnActiveGameplayEffectAddedDelegateToSelf se dispara al anadirse un GE con duracion
	// (justo lo que es un cooldown GE). Lo usamos para detectar el INICIO y leer la duracion.
	// NOTA: a diferencia de OnGameplayEffectAppliedDelegateToSelf (que el proyecto ya usa
	// para los asset tags), este se llama tanto en cliente como en servidor. En single-player
	// es indiferente, pero es el delegate correcto para GEs de duracion.
	AbilitySystemComponent->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(
		WaitCooldownChange,
		&UPantheliaWaitCooldownChange::OnActiveEffectAdded);

	return WaitCooldownChange;
}

void UPantheliaWaitCooldownChange::EndTask()
{
	// Si el ASC ya no es valido, no hay delegates que limpiar. Salimos.
	if (!IsValid(ASC)) return;

	// Quitamos nuestra suscripcion al evento del tag (la inversa de RegisterGameplayTagEvent).
	// RemoveAll(this) elimina todos los callbacks que registramos en este objeto.
	ASC->RegisterGameplayTagEvent(
		CooldownTag,
		EGameplayTagEventType::NewOrRemoved).RemoveAll(this);

	// SetReadyToDestroy desregistra el nodo async del game instance (deja de estar "vivo").
	SetReadyToDestroy();
	// MarkAsGarbage lo marca para que el recolector de basura lo elimine.
	MarkAsGarbage();
}

void UPantheliaWaitCooldownChange::CooldownTagChanged(const FGameplayTag InCooldownTag, int32 NewCount)
{
	// NewCount es cuantas instancias del tag tiene el ASC ahora.
	// Si llega a 0, el tag de cooldown se ha quitado -> el cooldown ha TERMINADO.
	if (NewCount == 0)
	{
		// Broadcast 0: no queda tiempo. El widget restaura el icono.
		CooldownEnd.Broadcast(0.f);
	}
	// Si NewCount > 0 el cooldown acaba de empezar, pero la duracion la calculamos en
	// OnActiveEffectAdded (que tiene acceso al spec del GE), no aqui.
}

void UPantheliaWaitCooldownChange::OnActiveEffectAdded(UAbilitySystemComponent* TargetASC,
	const FGameplayEffectSpec& SpecApplied,
	FActiveGameplayEffectHandle ActiveEffectHandle)
{
	// Comprobamos si el GE que se acaba de aplicar es NUESTRO cooldown. Un GE puede llevar
	// el cooldown tag de dos formas: como Asset Tag (tag descriptivo del GE) o como Granted
	// Tag (tag que concede al owner). Los cooldown GE lo conceden como Granted Tag, pero
	// comprobamos ambos por robustez.
	FGameplayTagContainer AssetTags;
	SpecApplied.GetAllAssetTags(AssetTags);

	FGameplayTagContainer GrantedTags;
	SpecApplied.GetAllGrantedTags(GrantedTags);

	// HasTagExact: coincidencia exacta con nuestro CooldownTag (no incluye hijos). Aqui es lo
	// correcto porque cada hechizo escucha su tag hoja concreto.
	if (AssetTags.HasTagExact(CooldownTag) || GrantedTags.HasTagExact(CooldownTag))
	{
		// El GE aplicado ES nuestro cooldown. Calculamos cuanto tiempo le queda.
		// MakeQuery con MatchAnyOwningTags: busca efectos activos cuyo owning tag coincida
		// con cualquiera del contenedor que le pasemos. Le pasamos un contenedor con solo
		// nuestro tag (GetSingleTagContainer crea ese contenedor de un unico tag).
		FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(
			CooldownTag.GetSingleTagContainer());

		// GetActiveEffectsTimeRemaining devuelve un array de floats: el tiempo restante de
		// CADA efecto activo que cumpla la query. Es array porque podria haber varios efectos
		// con el mismo tag a la vez (raro, pero posible).
		TArray<float> TimesRemaining = ASC->GetActiveEffectsTimeRemaining(Query);

		if (TimesRemaining.Num() > 0)
		{
			// Salvaguarda: si hubiera varios efectos con este tag, nos quedamos con el de
			// MAYOR tiempo restante (el cooldown mas largo activo). Algoritmo clasico de
			// "encontrar el maximo en un array".
			float TimeRemaining = TimesRemaining[0];
			for (int32 i = 1; i < TimesRemaining.Num(); ++i)
			{
				if (TimesRemaining[i] > TimeRemaining)
				{
					TimeRemaining = TimesRemaining[i];
				}
			}

			// Avisamos al widget de que el cooldown ha empezado, con su duracion total.
			CooldownStart.Broadcast(TimeRemaining);
		}
	}
}
