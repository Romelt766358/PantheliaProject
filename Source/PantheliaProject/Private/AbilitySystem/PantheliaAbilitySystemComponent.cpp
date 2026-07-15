// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/PantheliaAbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/Abilities/PantheliaGameplayAbility.h"
#include "AbilitySystem/Abilities/PantheliaPlayerAttackAbility.h"
#include "AbilitySystem/Abilities/PantheliaPlayerHeavyAttackAbility.h"
#include "AbilitySystem/Abilities/PantheliaPlayerDodgeAbility.h"
#include "AbilitySystem/Abilities/PantheliaParryAbility.h"
#include "AbilitySystem/PantheliaAttributeSet.h"
#include "Player/PantheliaPlayerState.h"
#include "Interfaces/PantheliaPlayerInterface.h"
#include "AbilitySystemBlueprintLibrary.h"
// Singleton de tags nativos: usado por GetAbilityTagFromSpec / GetInputTagFromSpec
// para las queries jerárquicas contra las raíces "Abilities" e "InputTag" sin strings.
#include "PantheliaGameplayTags.h"
#include "PantheliaLogChannels.h"

void UPantheliaAbilitySystemComponent::AbilityActorInfoSet()
{
	// El ASC del jugador persiste entre Pawns. Cualquier contexto de entrada que hubiera
	// quedado pendiente pertenece al avatar anterior y debe descartarse al reinicializar
	// ActorInfo, incluso si el delegate de efectos ya estaba enlazado.
	ResetPendingAttackEntryContext();

	// Guarda contra el doble bind (ver bEffectAppliedDelegateBound en el .h): en un
	// futuro respawn, el Pawn nuevo vuelve a llamar aquí sobre este MISMO ASC persistente,
	// y el bind anterior sigue vivo (su objeto es el propio ASC, no el Pawn muerto).
	if (bEffectAppliedDelegateBound) return;

	// Bindeamos nuestro callback al delegate del ASC.
	// OnGameplayEffectAppliedDelegateToSelf se dispara con cualquier GE
	// (Instant, Duration e Infinite) aplicado a este componente.
	// No es dynamic, así que usamos AddUObject en lugar de AddDynamic.
	OnGameplayEffectAppliedDelegateToSelf.AddUObject(this, &UPantheliaAbilitySystemComponent::EffectApplied);

	bEffectAppliedDelegateBound = true;
}

void UPantheliaAbilitySystemComponent::EffectApplied(UAbilitySystemComponent* AbilitySystemComponent,
	const FGameplayEffectSpec& EffectSpec,
	FActiveGameplayEffectHandle ActiveEffectHandle)
{
	// Obtenemos todos los Asset Tags del efecto aplicado y los broadcasteamos.
	// El widget controller recibirá este tag container y decidirá qué mostrar en la UI.
	FGameplayTagContainer TagContainer;
	EffectSpec.GetAllAssetTags(TagContainer);

	EffectAssetTags.Broadcast(TagContainer);
}

void UPantheliaAbilitySystemComponent::AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities)
{
	// Guarda contra la re-concesión (Etapa 4): en un futuro respawn del jugador, el Pawn
	// nuevo vuelve a llamar aquí sobre este MISMO ASC persistente, que ya tiene las
	// abilities. Sin la guarda, GiveAbility las otorgaría por duplicado (dos specs de
	// GA_Firebolt, dos de ataque ligero...) y los inputs activarían las dos copias.
	// Los listeners tardíos de la UI ya se resuelven con el propio flag + delegate
	// (ver bStartupAbilitiesGiven en el .h), así que aquí basta con retornar.
	if (bStartupAbilitiesGiven) return;

	for (const TSubclassOf<UGameplayAbility>& AbilityClass : StartupAbilities)
	{
		// Creamos la spec: define qué ability es y en qué nivel empieza.
		// Nivel 1 = rango inicial, POR DISEÑO (decisión cerrada): el nivel de una ability
		// es independiente del nivel del personaje y solo lo sube el árbol de habilidades
		// vía SetAbilityLevel (ver ese método). No es un placeholder.
		// NO puede ser const: GetDynamicSpecSourceTags() devuelve referencia no-const.
		FGameplayAbilitySpec AbilitySpec(AbilityClass, 1);

		// Si la ability hereda de UPantheliaGameplayAbility (lo cual siempre debería),
		// tomamos su StartupInputTag y lo agregamos via GetDynamicSpecSourceTags() (API nueva).
		// GetDynamicSpecSourceTags() (antes DynamicAbilityTags) esta disenado para ser editado en runtime, lo que nos permite
		// remapear abilities a distintos inputs desde el árbol de habilidades.
		if (const UPantheliaGameplayAbility* PantheliaAbility = Cast<UPantheliaGameplayAbility>(AbilitySpec.Ability))
		{
			// GetDynamicSpecSourceTags() reemplaza a DynamicAbilityTags (deprecado en UE5.3+).
			// Devuelve una referencia no-const, por lo que se puede escribir directamente.
			AbilitySpec.GetDynamicSpecSourceTags().AddTag(PantheliaAbility->StartupInputTag);
		}

		// GiveAbility otorga la ability al ASC sin activarla.
		// En un soulslike las abilities se activan por input del jugador.
			//GiveAbilityAndActivateOnce(AbilitySpec);
		GiveAbility(AbilitySpec);
	}

	// Todas las abilities han sido otorgadas.
	// Seteamos el flag ANTES de broadcastar: si algún listener llama bStartupAbilitiesGiven
	// de forma síncrona dentro del broadcast, ya lo verá como true.
	bStartupAbilitiesGiven = true;
	AbilitiesGivenDelegate.Broadcast(this);
}


void UPantheliaAbilitySystemComponent::AddCharacterPassiveAbilities(const TArray<TSubclassOf<UGameplayAbility>>& PassiveAbilities)
{
	// Guarda contra la re-concesión (Etapa 4), simétrica a la de AddCharacterAbilities.
	// Aquí es aún más importante: las pasivas se otorgan Y ACTIVAN, así que duplicarlas
	// dejaría dos instancias activas escuchando (p. ej. dos GA_ListenForXPEvents
	// procesando cada Gameplay Event dos veces).
	if (bPassiveAbilitiesGiven) return;

	// Las abilities pasivas se otorgan Y activan de inmediato (GiveAbilityAndActivateOnce).
	// No tienen InputTag — no se activan por input del jugador, sino que corren siempre.
	// Ejemplo: GA_ListenForXPEvents escucha GameplayEvents durante toda la partida y aplica
	// el GE de XP cada vez que el AttributeSet detecta una muerte fatal.
	for (const TSubclassOf<UGameplayAbility>& AbilityClass : PassiveAbilities)
	{
		if (!AbilityClass) continue;

		FGameplayAbilitySpec AbilitySpec(AbilityClass, 1);
		// GiveAbilityAndActivateOnce: otorga la ability Y la activa en el mismo frame.
		// Si la ability es NonInstanced o InstancedPerActor (como GA_ListenForXPEvents),
		// quedará activa indefinidamente hasta que alguien la cancele explícitamente.
		GiveAbilityAndActivateOnce(AbilitySpec);
	}

	// Marcamos la guarda al final, con todas las pasivas ya otorgadas y activas.
	bPassiveAbilitiesGiven = true;
}

void UPantheliaAbilitySystemComponent::ForEachAbility(const FForEachAbility& Delegate)
{
	// Bloqueamos la lista de abilities mientras iteramos. Si durante el for alguna ability
	// se concede o se remueve (GAS puede hacerlo en respuesta a tags), el cambio se pone en
	// cola y se aplica al destruirse este lock (al salir de la función), evitando corromper
	// la iteración o causar un crash. El lock requiere el ASC por referencia, de ahí *this.
	FScopedAbilityListLock ActiveScopeLock(*this);

	for (const FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		// ExecuteIfBound ejecuta el callback solo si hay uno bindeado, y devuelve false si no.
		// Si nadie bindeó el delegate es un error de programación, así que lo logueamos.
		if (!Delegate.ExecuteIfBound(AbilitySpec))
		{
			// __FUNCTION__ se expande al nombre de esta función — útil para rastrear el error.
			UE_LOG(LogTemp, Error, TEXT("Fallo al ejecutar el delegate en %hs"), __FUNCTION__);
		}
	}
}

FGameplayTag UPantheliaAbilitySystemComponent::GetAbilityTagFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
	// La spec referencia la CDO de la ability (AbilitySpec.Ability). De ahí leemos sus
	// AbilityTags (los tags estáticos que definen QUÉ es la ability, no su input).
	if (AbilitySpec.Ability)
	{
		// GetAssetTags() reemplaza al antiguo AbilityTags (deprecado en UE5.4+).
		// Devuelve el contenedor de tags estáticos de la ability.
		for (const FGameplayTag& Tag : AbilitySpec.Ability->GetAssetTags())
		{
			// Buscamos el tag que sea hijo de "Abilities" (p. ej. "Abilities.Fire.Firebolt").
			// MatchesTag (no Exact) acepta cualquier descendiente del tag padre.
			// Usamos la raíz nativa del singleton en vez de RequestGameplayTag por string:
			// el miembro no puede tener typos (no compilaría) y no hace lookup por FName.
			if (Tag.MatchesTag(FPantheliaGameplayTags::Get().Abilities))
			{
				return Tag;
			}
		}
	}
	// Ninguna ability tag encontrada: devolvemos un tag vacío (el caller debe comprobarlo).
	return FGameplayTag();
}

FGameplayTag UPantheliaAbilitySystemComponent::GetInputTagFromSpec(const FGameplayAbilitySpec& AbilitySpec)
{
	// El input tag NO está en los AbilityTags estáticos, sino en los DynamicSpecSourceTags,
	// que es donde AddCharacterAbilities (y el futuro árbol) escriben el input asignado.
	// GetDynamicSpecSourceTags() reemplaza a DynamicAbilityTags (deprecado en UE5.3+).
	for (const FGameplayTag& Tag : AbilitySpec.GetDynamicSpecSourceTags())
	{
		// Buscamos el tag que sea hijo de "InputTag" (p. ej. "InputTag.Spell.1").
		// Raíz nativa del singleton en vez de RequestGameplayTag por string (ver arriba).
		if (Tag.MatchesTag(FPantheliaGameplayTags::Get().InputTag))
		{
			return Tag;
		}
	}
	// Ningún input tag asignado: tag vacío.
	return FGameplayTag();
}

FGameplayAbilitySpec* UPantheliaAbilitySystemComponent::GetSpecFromAbilityTag(const FGameplayTag& AbilityTag)
{
	// Tag inválido: no hay nada que buscar. Guard estándar de todas las funciones por tag.
	if (!AbilityTag.IsValid()) return nullptr;

	// Mismo lock que ForEachAbility y por el mismo motivo: si GAS concede o remueve una
	// ability durante la iteración (puede hacerlo en respuesta a tags), el cambio se
	// encola y se aplica al salir, evitando corromper el array mientras lo recorremos.
	FScopedAbilityListLock ActiveScopeLock(*this);

	// Referencia NO-const: el caller (SetAbilityLevel) necesita escribir en la spec.
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		// La spec referencia la CDO de la ability; sin ella no hay tags que comparar.
		if (!AbilitySpec.Ability) continue;

		// Comparamos contra los AbilityTags estáticos de la ability (GetAssetTags,
		// que reemplaza al deprecado AbilityTags). MatchesTag acepta descendientes:
		// pedir "Abilities.Spell.Fire.Firebolt" (hoja) hace match exacto, y pedir un
		// padre como "Abilities.Spell.Fire" devolvería el PRIMER hechizo de fuego que
		// se encuentre. Para localizar una ability concreta, pasar siempre el tag hoja.
		for (const FGameplayTag& Tag : AbilitySpec.Ability->GetAssetTags())
		{
			if (Tag.MatchesTag(AbilityTag))
			{
				return &AbilitySpec;
			}
		}
	}
	// El ASC no tiene ninguna ability con ese tag.
	return nullptr;
}

int32 UPantheliaAbilitySystemComponent::GetAbilityLevelFromTag(const FGameplayTag& AbilityTag)
{
	// Convención del proyecto: 0 = "el jugador no tiene esta ability" (no desbloqueada).
	// Toda ability otorgada tiene nivel >= 1, así que el 0 nunca es ambiguo.
	const FGameplayAbilitySpec* AbilitySpec = GetSpecFromAbilityTag(AbilityTag);
	return AbilitySpec ? AbilitySpec->Level : 0;
}

bool UPantheliaAbilitySystemComponent::SetAbilityLevel(const FGameplayTag& AbilityTag, int32 NewLevel)
{
	// Nivel mínimo 1: el 0 está reservado para "no otorgada" y las curvas de escalado
	// (GetValueAtLevel) están definidas desde nivel 1. Clampeamos en vez de fallar para
	// que un error de datos del árbol (nodo con rango 0) degrade con gracia.
	NewLevel = FMath::Max(NewLevel, 1);

	FGameplayAbilitySpec* AbilitySpec = GetSpecFromAbilityTag(AbilityTag);
	if (!AbilitySpec)
	{
		// El árbol intentó subir de nivel una ability que el ASC no tiene. Es un error
		// de flujo (debería desbloquearse antes de subirse), así que lo dejamos logueado.
		UE_LOG(LogTemp, Warning,
			TEXT("[ASC] SetAbilityLevel: no existe ninguna ability con el tag '%s'. ¿Se desbloqueó antes de subir su nivel?"),
			*AbilityTag.ToString());
		return false;
	}

	// Si el nivel ya es el pedido, no hay nada que hacer. Evitamos un MarkAbilitySpecDirty
	// innecesario (y sus notificaciones) cuando el árbol re-aplica nodos, p. ej. al cargar
	// una partida guardada con ReapplyAllNodes.
	if (AbilitySpec->Level == NewLevel) return true;

	AbilitySpec->Level = NewLevel;

	// MarkAbilitySpecDirty es la función oficial de GAS para avisar de que una spec ya
	// otorgada fue modificada. Dispara las notificaciones internas del ASC para que el
	// cambio sea visible de inmediato (la próxima activación de la ability leerá el
	// nivel nuevo en GetAbilityLevel(), y con él escalarán DamageTypes, PoiseDamage y
	// cualquier curva evaluada con GetValueAtLevel).
	MarkAbilitySpecDirty(*AbilitySpec);

	UE_LOG(LogTemp, Log, TEXT("[ASC] Ability '%s' ahora es nivel %d."),
		*AbilityTag.ToString(), NewLevel);
	return true;
}

void UPantheliaAbilitySystemComponent::AbilityInputTagPressed(const FGameplayTag& InputTag)
{
	// Edge-triggered: esta función se llama una sola vez por pulsación real. Aquí vive
	// la activación normal de todas las abilities actuales de Panthelia.
	if (!InputTag.IsValid()) return;

	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		// Match exacto: InputTag.Spell no debe activar InputTag.Spell.1.
		if (!AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag)) continue;

		// Notificar el press real una sola vez. GAS mantiene con esto el estado interno
		// de input de la spec y puede reenviar el evento a una ability ya activa.
		AbilitySpecInputPressed(AbilitySpec);

		const UPantheliaGameplayAbility* PantheliaAbility =
			Cast<UPantheliaGameplayAbility>(AbilitySpec.Ability);

		// Toda ability del proyecto debería heredar de UPantheliaGameplayAbility. Si una
		// clase externa no lo hace, no inventamos una política implícita ni la activamos
		// por esta ruta.
		if (!PantheliaAbility) continue;

		if (PantheliaAbility->InputActivationPolicy ==
			EPantheliaAbilityInputActivationPolicy::OnInputTriggered &&
			!AbilitySpec.IsActive())
		{
			TryActivateAbility(AbilitySpec.Handle);
		}
	}
}

void UPantheliaAbilitySystemComponent::AbilityInputTagHeld(const FGameplayTag& InputTag)
{
	// Held se ejecuta cada frame. Solo las abilities que declaren explícitamente
	// WhileInputActive pueden reintentar su activación desde esta ruta.
	if (!InputTag.IsValid()) return;

	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (!AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag)) continue;

		const UPantheliaGameplayAbility* PantheliaAbility =
			Cast<UPantheliaGameplayAbility>(AbilitySpec.Ability);
		if (!PantheliaAbility) continue;

		if (PantheliaAbility->InputActivationPolicy ==
			EPantheliaAbilityInputActivationPolicy::WhileInputActive &&
			!AbilitySpec.IsActive())
		{
			TryActivateAbility(AbilitySpec.Handle);
		}
	}
}

void UPantheliaAbilitySystemComponent::AbilityInputTagReleased(const FGameplayTag& InputTag)
{
	// Misma validación que en Held.
	if (!InputTag.IsValid()) return;

	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			// Notificamos que el input fue soltado.
			// Esto NO cancela ni termina la ability — solo actualiza su estado interno.
			// Cada ability decide qué hacer cuando su input es soltado
			// (overrideando InputReleased en su Blueprint o C++).
			AbilitySpecInputReleased(AbilitySpec);
		}
	}
}

bool UPantheliaAbilitySystemComponent::TryActivateAbilityByInputTag(const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return false;

	// Activamos una sola spec. Esto evita que dos abilities que compartan accidentalmente
	// el mismo input se ejecuten a la vez desde un follow-up. Si una spec no satisface
	// costes/tags, continuamos buscando otra candidata con ese InputTag.
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (!AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag)) continue;

		if (TryActivateAbility(AbilitySpec.Handle))
		{
			return true;
		}
	}

	return false;
}

void UPantheliaAbilitySystemComponent::SetPendingAttackEntryContext(
	EPantheliaAttackEntryContext NewContext)
{
	PendingAttackEntryContext = NewContext;
}

EPantheliaAttackEntryContext UPantheliaAbilitySystemComponent::ConsumeAttackEntryContext()
{
	// Consumo con reset en la misma operación: una activación posterior jamás puede
	// heredar por accidente el contexto especial de un ataque anterior.
	const EPantheliaAttackEntryContext ConsumedContext = PendingAttackEntryContext;
	PendingAttackEntryContext = EPantheliaAttackEntryContext::Normal;
	return ConsumedContext;
}

void UPantheliaAbilitySystemComponent::ResetPendingAttackEntryContext()
{
	PendingAttackEntryContext = EPantheliaAttackEntryContext::Normal;
}

bool UPantheliaAbilitySystemComponent::NotifyDodgeFollowupInputPressed(
	const FGameplayTag& InputTag)
{
	if (!InputTag.IsValid()) return false;

	// El input se ofrece únicamente al dodge del jugador que esté activo. La propia
	// ability valida la ventana, distingue ligero/pesado y aplica la regla primer-input-gana.
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (UPantheliaPlayerDodgeAbility* DodgeAbility =
			Cast<UPantheliaPlayerDodgeAbility>(AbilitySpec.GetPrimaryInstance()))
		{
			if (DodgeAbility->IsActive())
			{
				const bool bAccepted = DodgeAbility->TryBufferFollowupInput(InputTag);

				UE_LOG(LogPanthelia, Log,
					TEXT("[DODGE] Input '%s' ofrecido al follow-up | Ventana abierta: %s | Aceptado: %s"),
					*InputTag.ToString(),
					DodgeAbility->IsFollowupWindowOpen() ? TEXT("Sí") : TEXT("No"),
					bAccepted ? TEXT("Sí") : TEXT("No"));

				return bAccepted;
			}
		}
	}

	return false;
}

void UPantheliaAbilitySystemComponent::NotifyComboInputPressed(const FGameplayTag& InputTag)
{
	// Validación: tag inválido, nada que hacer.
	if (!InputTag.IsValid()) return;

	// Buscamos la ability que tenga este InputTag y que sea de ataque del jugador.
	// Si esta activa (reproduciendo el combo), le marcamos el buffer de combo.
	// Esto se llama una sola vez por pulsacion real (AbilityInputTagPressed), no cada
	// frame, evitando la desincronizacion que causaba el override InputPressed con Held.
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			if (UPantheliaPlayerAttackAbility* AttackAbility =
				Cast<UPantheliaPlayerAttackAbility>(AbilitySpec.GetPrimaryInstance()))
			{
				if (AttackAbility->IsActive())
				{
					AttackAbility->TryBufferComboInput();
				}
			}
		}
	}
}

void UPantheliaAbilitySystemComponent::NotifyHeavyInputReleased(const FGameplayTag& InputTag)
{
	// Validación: tag inválido, nada que hacer.
	if (!InputTag.IsValid()) return;

	// Busca la ability de ataque PESADO activa y le notifica que el boton se solto.
	// Esto alimenta la deteccion tap-vs-hold del cargado (sustituye a WaitInputRelease,
	// que no detectaba el release con el sistema de input custom del proyecto).
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			if (UPantheliaPlayerHeavyAttackAbility* HeavyAbility =
				Cast<UPantheliaPlayerHeavyAttackAbility>(AbilitySpec.GetPrimaryInstance()))
			{
				if (HeavyAbility->IsActive())
				{
					HeavyAbility->NotifyHeavyInputReleased();
				}
			}
		}
	}
}

void UPantheliaAbilitySystemComponent::NotifyBlockInputReleased(const FGameplayTag& InputTag)
{
	// Validación: tag inválido, nada que hacer.
	if (!InputTag.IsValid()) return;

	// Busca la ability de parry/bloqueo activa con este InputTag y le notifica el release.
	// La ability decide: si estaba en bloqueo sostenido, termina la guardia.
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (AbilitySpec.GetDynamicSpecSourceTags().HasTagExact(InputTag))
		{
			if (UPantheliaParryAbility* ParryAbility =
				Cast<UPantheliaParryAbility>(AbilitySpec.GetPrimaryInstance()))
			{
				if (ParryAbility->IsActive())
				{
					ParryAbility->NotifyBlockInputReleased();
				}
			}
		}
	}
}

void UPantheliaAbilitySystemComponent::NotifyParryImpact(bool bParried, bool bGuardBroken)
{
	// A diferencia de las otras funciones Notify*, aquí NO filtramos por InputTag porque
	// esta notificación viene del pipeline de daño (AttributeSet::HandleParryReaction),
	// no del input del jugador. Simplemente buscamos la única ability de parry que pueda
	// estar activa en este momento y le pasamos el resultado del impacto.
	//
	// Puede haber hasta dos abilities de parry concedidas (GA_Parry_Physical y GA_Parry_Magic),
	// pero solo una puede estar activa a la vez (el jugador no puede hacer ambas a la vez).
	// En cuanto encontramos la activa, llamamos y salimos con return para no iterar de más.
	for (FGameplayAbilitySpec& AbilitySpec : GetActivatableAbilities())
	{
		if (UPantheliaParryAbility* ParryAbility =
			Cast<UPantheliaParryAbility>(AbilitySpec.GetPrimaryInstance()))
		{
			if (ParryAbility->IsActive())
			{
				ParryAbility->NotifyParryImpact(bParried, bGuardBroken);
				return;
			}
		}
	}
}

void UPantheliaAbilitySystemComponent::TriggerGuardBreak()
{
	const FPantheliaGameplayTags& Tags = FPantheliaGameplayTags::Get();

	// El tag semántico se concede antes de activar Stagger para que cualquier listener
	// que reaccione al comienzo de la animación ya conozca la causa correcta.
	if (!HasMatchingGameplayTag(Tags.State_GuardBroken))
	{
		AddLooseGameplayTag(Tags.State_GuardBroken);
	}

	bGuardBreakObservedStagger = HasMatchingGameplayTag(Tags.Effects_Stagger);

	if (!GuardBreakStaggerTagDelegateHandle.IsValid())
	{
		GuardBreakStaggerTagDelegateHandle =
			RegisterGameplayTagEvent(Tags.Effects_Stagger, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UPantheliaAbilitySystemComponent::OnGuardBreakStaggerTagChanged);
	}

	FGameplayTagContainer StaggerTags;
	StaggerTags.AddTag(Tags.Effects_Stagger);
	const bool bActivationRequested = TryActivateAbilitiesByTag(StaggerTags);

	// La activación puede añadir el tag sincrónicamente. También cubrimos el caso donde
	// ya existía un Stagger activo y la guardia rota debe compartir su ventana restante.
	bGuardBreakObservedStagger =
		bGuardBreakObservedStagger || HasMatchingGameplayTag(Tags.Effects_Stagger);

	if (!bActivationRequested && !bGuardBreakObservedStagger)
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("[GUARD BREAK] No se pudo activar Stagger; se retira State.GuardBroken."));
		ClearGuardBreakState();
	}
}

void UPantheliaAbilitySystemComponent::OnGuardBreakStaggerTagChanged(
	const FGameplayTag /*Tag*/,
	int32 NewCount)
{
	if (NewCount > 0)
	{
		bGuardBreakObservedStagger = true;
		return;
	}

	if (bGuardBreakObservedStagger)
	{
		ClearGuardBreakState();
	}
}

void UPantheliaAbilitySystemComponent::ClearGuardBreakState()
{
	const FPantheliaGameplayTags& Tags = FPantheliaGameplayTags::Get();
	RemoveLooseGameplayTag(Tags.State_GuardBroken);
	bGuardBreakObservedStagger = false;

	if (GuardBreakStaggerTagDelegateHandle.IsValid())
	{
		RegisterGameplayTagEvent(Tags.Effects_Stagger, EGameplayTagEventType::NewOrRemoved)
			.Remove(GuardBreakStaggerTagDelegateHandle);
		GuardBreakStaggerTagDelegateHandle.Reset();
	}
}

void UPantheliaAbilitySystemComponent::UpgradeAttribute(const FGameplayTag& AttributeTag)
{
	AActor* Avatar = GetAvatarActor();
	APantheliaPlayerState* PantheliaPlayerState = Cast<APantheliaPlayerState>(GetOwner());
	const UPantheliaAttributeSet* PantheliaAS = GetSet<UPantheliaAttributeSet>();
	if (!Avatar || !PantheliaPlayerState || !PantheliaAS ||
		!Avatar->Implements<UPantheliaPlayerInterface>())
	{
		return;
	}

	// Solo se permite mejorar uno de los cinco atributos primarios. Tags secundarios
	// o desconocidos no deben poder consumir puntos mediante esta API pública.
	const FPantheliaGameplayTags& Tags = FPantheliaGameplayTags::Get();
	const bool bSupportedPrimaryAttribute =
		AttributeTag.MatchesTagExact(Tags.Attributes_Primary_Hardness) ||
		AttributeTag.MatchesTagExact(Tags.Attributes_Primary_Resonance) ||
		AttributeTag.MatchesTagExact(Tags.Attributes_Primary_Resilience) ||
		AttributeTag.MatchesTagExact(Tags.Attributes_Primary_Endurance) ||
		AttributeTag.MatchesTagExact(Tags.Attributes_Primary_Spirit);
	if (!bSupportedPrimaryAttribute)
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("[Attributes] UpgradeAttribute rechazó el tag no primario '%s'."),
			*AttributeTag.ToString());
		return;
	}

	const TStaticFuncPtr<FGameplayAttribute()>* AttributeGetter =
		PantheliaAS->TagsToAttributes.Find(AttributeTag);
	if (!AttributeGetter)
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[Attributes] No existe mapping TagsToAttributes para '%s'."),
			*AttributeTag.ToString());
		return;
	}

	const FGameplayAttribute Attribute = (*AttributeGetter)();
	const float PreviousValue = GetNumericAttribute(Attribute);

	// Reservamos el punto antes de disparar el evento. Si el listener/GE no confirma
	// un aumento síncrono, lo reembolsamos: nunca se pierde saldo por una configuración
	// rota, y tampoco se concede una mejora gratis.
	// CONTRATO: GA_ListenForXPEvents debe aplicar su GE Instant dentro de esta misma
	// llamada. No introducir Delay, tareas asíncronas ni aplicación diferida: eso
	// permitiría que el reembolso ocurra antes de una mejora tardía.
	if (!PantheliaPlayerState->TrySpendAttributePoints(1))
	{
		return;
	}

	FGameplayEventData Payload;
	Payload.EventTag = AttributeTag;
	Payload.EventMagnitude = 1.f;
	UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(Avatar, AttributeTag, Payload);

	const float NewValue = GetNumericAttribute(Attribute);
	if (NewValue <= PreviousValue + KINDA_SMALL_NUMBER)
	{
		PantheliaPlayerState->GrantAttributePoints(1);
		UE_LOG(LogPanthelia, Error,
			TEXT("[Attributes] Mejora revertida para '%s': el Gameplay Event no aumentó "
			     "el atributo (%.2f -> %.2f). Revisa GA_ListenForXPEvents y GE_EventBasedEffect."),
			*AttributeTag.ToString(), PreviousValue, NewValue);
		return;
	}

	UE_LOG(LogPanthelia, Log,
		TEXT("[Attributes] Mejora confirmada '%s': %.2f -> %.2f. Puntos restantes=%d."),
		*AttributeTag.ToString(), PreviousValue, NewValue,
		PantheliaPlayerState->GetAttributePoints());
}
