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

	// El widget propietario conserva este proxy en WaitCooldownTask mientras lo necesita.
	// No lo registramos con la GameInstance: ese registro global impediria que el GC alcanzara
	// BeginDestroy si el Blueprint olvidara EndTask durante una reconstruccion o cambio de mapa.

	// Suscripcion 1: avisar cuando el CONTEO del tag de cooldown cambia.
	// EGameplayTagEventType::NewOrRemoved dispara el callback tanto al anadirse el tag
	// (conteo 0 -> 1, empieza el cooldown) como al quitarse (conteo 1 -> 0, termina).
	// Usamos esto principalmente para detectar el FIN (cuando NewCount llega a 0).
	WaitCooldownChange->CooldownTagChangedDelegateHandle =
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
	WaitCooldownChange->ActiveEffectAddedDelegateHandle =
		AbilitySystemComponent->OnActiveGameplayEffectAddedDelegateToSelf.AddUObject(
			WaitCooldownChange,
			&UPantheliaWaitCooldownChange::OnActiveEffectAdded);

	return WaitCooldownChange;
}

void UPantheliaWaitCooldownChange::EndTask()
{
	// EndTask puede llamarse desde Event Destruct y volver a alcanzarse durante la
	// destruccion del proxy. La segunda llamada debe ser un no-op seguro.
	if (bTaskEnded)
	{
		return;
	}

	bTaskEnded = true;
	UnbindFromAbilitySystemComponent();

	// Soltamos la referencia fuerte al ASC despues de retirar los delegates.
	ASC = nullptr;

	// SetReadyToDestroy marca el nodo async como finalizado y permite que el GC lo recolecte
	// de forma normal cuando el widget suelte su referencia. No usamos MarkAsGarbage: forzar
	// basura durante un callback puede invalidar el proxy antes de terminar la pila actual.
	SetReadyToDestroy();
}

void UPantheliaWaitCooldownChange::BeginDestroy()
{
	// Red de seguridad: si un Blueprint olvida llamar EndTask, nunca dejamos callbacks
	// registrados hacia un objeto que esta entrando en destruccion. Marcamos primero el
	// estado terminal para que cualquier callback rezagado se convierta en un no-op.
	bTaskEnded = true;
	UnbindFromAbilitySystemComponent();
	ASC = nullptr;

	Super::BeginDestroy();
}

void UPantheliaWaitCooldownChange::UnbindFromAbilitySystemComponent()
{
	if (!IsValid(ASC))
	{
		CooldownTagChangedDelegateHandle.Reset();
		ActiveEffectAddedDelegateHandle.Reset();
		return;
	}

	if (CooldownTagChangedDelegateHandle.IsValid() && CooldownTag.IsValid())
	{
		ASC->RegisterGameplayTagEvent(
			CooldownTag,
			EGameplayTagEventType::NewOrRemoved).Remove(
				CooldownTagChangedDelegateHandle);
		CooldownTagChangedDelegateHandle.Reset();
	}

	if (ActiveEffectAddedDelegateHandle.IsValid())
	{
		ASC->OnActiveGameplayEffectAddedDelegateToSelf.Remove(
			ActiveEffectAddedDelegateHandle);
		ActiveEffectAddedDelegateHandle.Reset();
	}
}

void UPantheliaWaitCooldownChange::CooldownTagChanged(const FGameplayTag InCooldownTag, int32 NewCount)
{
	if (bTaskEnded || !IsValid(ASC) || !InCooldownTag.MatchesTagExact(CooldownTag))
	{
		return;
	}

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
	FActiveGameplayEffectHandle /*ActiveEffectHandle*/)
{
	if (bTaskEnded || !IsValid(ASC) || TargetASC != ASC)
	{
		return;
	}

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
