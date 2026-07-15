// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/PantheliaAbilityTypes.h"
#include "PantheliaAbilitySystemComponent.generated.h"

// Delegate que broadcast los Asset Tags de cualquier GE aplicado a este ASC.
// El widget controller se suscribirá a este para saber qué efectos se aplicaron
// y mostrar mensajes en la UI (por ejemplo, "Recogiste una poción de vida").
DECLARE_MULTICAST_DELEGATE_OneParam(FEffectAssetTags, const FGameplayTagContainer& /*AssetTags*/);

// Delegate que se broadcasta cuando todas las startup abilities han sido otorgadas.
// Pasa un puntero al propio ASC para que el receptor pueda iterar las abilities
// sin necesitar hacer un cast adicional.
// No es DYNAMIC porque solo lo usamos en C++ (el widget controller lo suscribe via AddUObject).
DECLARE_MULTICAST_DELEGATE_OneParam(FAbilitiesGiven, UPantheliaAbilitySystemComponent* /*AbilitySystemComponent*/);

// Delegate de un solo parámetro (NO multicast) que recibe un FGameplayAbilitySpec.
// Sirve como "callback genérico" para aplicar una operación a cada ability del ASC:
// el WidgetController bindea un lambda a este delegate y se lo pasa a ForEachAbility,
// que lo ejecuta sobre cada ability activable. No es multicast porque solo se necesita
// un único callback a la vez (lo ejecuta ForEachAbility internamente).
DECLARE_DELEGATE_OneParam(FForEachAbility, const FGameplayAbilitySpec& /*AbilitySpec*/);

UCLASS()
class PANTHELIAPROJECT_API UPantheliaAbilitySystemComponent : public UAbilitySystemComponent
{
	GENERATED_BODY()

public:
	// Llamado desde el Character/Enemy una vez que AbilityActorInfo ha sido inicializado.
	// Es el momento seguro para bindear delegates.
	void AbilityActorInfoSet();

	// Delegate público para que el widget controller pueda suscribirse.
	FEffectAssetTags EffectAssetTags;

	// Delegate público que se broadcasta al terminar AddCharacterAbilities.
	// Permite que el widget controller sepa cuándo puede acceder a las abilities
	// del ASC para inicializar la UI de hechizos.
	FAbilitiesGiven AbilitiesGivenDelegate;

	// True una vez que AddCharacterAbilities ha terminado de otorgar todas las abilities.
	// Se usa en BindCallbacksToDependencies para resolver la carrera de inicialización:
	// si el widget controller llega tarde (las abilities ya fueron dadas), puede llamar
	// directamente el callback en lugar de esperar al delegate.
	// Desde la Etapa 4 también actúa como GUARDA dentro de AddCharacterAbilities: en un
	// futuro respawn del jugador, el Pawn nuevo volverá a llamarla sobre este MISMO ASC
	// persistente (vive en el PlayerState), y sin la guarda las abilities se otorgarían
	// por duplicado.
	bool bStartupAbilitiesGiven = false;

	// === ESTADO PERSISTENTE DE INICIALIZACIÓN DE ATRIBUTOS (Etapa 4) ===
	//
	// POR QUÉ ESTE ESTADO VIVE AQUÍ Y NO EN EL PAWN (explicación extendida):
	// La regla sana de arquitectura es que el estado viva junto a lo que describe. Estos
	// dos miembros describen el estado de LOS DATOS de GAS ("¿ya se aplicaron los GEs de
	// atributos por defecto?", "¿qué instancia del GE de secundarios está activa?"), no
	// el estado del cuerpo físico. El ASC tiene exactamente la vida correcta en ambos
	// mundos del juego, sin código especial para cada uno:
	//   - JUGADOR: su ASC vive en APantheliaPlayerState, que SOBREVIVE a la muerte y
	//     respawn del Pawn. El flag y el handle sobreviven con él → el Pawn nuevo NO
	//     re-inicializa atributos (no borra los puntos gastados ni duplica el GE
	//     Infinite de secundarios).
	//   - ENEMIGOS: su ASC vive en el propio Character y muere con él. Cada enemigo
	//     nuevo nace con flag = false → inicializa sus atributos con normalidad.
	// Cuando estos miembros vivían en el Pawn (APantheliaCharacterBase / AMainCharacter),
	// un respawn creaba un Pawn nuevo con flag=false y handle inválido → reaplicaba los
	// GEs por defecto sobre el ASC persistente (reseteando atributos primarios ya
	// mejorados) y apilaba una segunda instancia Infinite de secundarios que ya no se
	// podía remover (el handle de la primera murió con el Pawn viejo).

	// True cuando InitializeDefaultAttributes ya aplicó los GEs de atributos por defecto
	// sobre este ASC. La comprueba y la escribe APantheliaCharacterBase::InitializeDefaultAttributes.
	bool bAttributesInitialized = false;

	// Handle de la instancia activa de DefaultSecondaryAttributes, guardado para poder
	// quitarla explícitamente antes de reaplicarla en RefreshSecondaryAttributes()
	// (patrón QUITAR + REAPLICAR — explicación extendida en PantheliaCharacterBase.h).
	// El tipo FActiveGameplayEffectHandle por valor exige conocer el struct completo;
	// aquí llega a través de AbilitySystemComponent.h (incluido arriba), así que no
	// hace falta un include adicional de GameplayEffectTypes.h en este header.
	FActiveGameplayEffectHandle SecondaryAttributesEffectHandle;

	// Itera el array de clases de abilities y las otorga al ASC una por una.
	// Además, agrega el StartupInputTag de cada ability a sus DynamicAbilityTags
	// para que pueda ser activada por input desde el primer frame.
	// Llamado desde APantheliaCharacterBase::AddCharacterAbilities().
	void AddCharacterAbilities(const TArray<TSubclassOf<UGameplayAbility>>& StartupAbilities);

	// Itera el array de abilities pasivas y las otorga Y activa de inmediato (una vez).
	// Diferencias clave con AddCharacterAbilities:
	//   - Llama a GiveAbilityAndActivateOnce en vez de GiveAbility (se activan al darse).
	//   - No asigna InputTag: las abilities pasivas no tienen input del jugador.
	//   - No broadcastea AbilitiesGivenDelegate: solo las startup abilities activan ese flujo.
	// Llamado desde AMainCharacter::PossessedBy() después de AddCharacterAbilities().
	// Solo se usa en el jugador — los enemigos no tienen abilities pasivas permanentes.
	void AddCharacterPassiveAbilities(const TArray<TSubclassOf<UGameplayAbility>>& PassiveAbilities);

	// Ejecuta el delegate dado sobre CADA ability activable del ASC.
	// Bloquea la lista de abilities con un FScopedAbilityListLock durante la iteración:
	// si alguna ability se concede o remueve mientras iteramos (cosa que GAS puede hacer
	// en respuesta a tags), el cambio se pone en cola y se aplica al terminar, evitando
	// corromper la iteración. Por eso esta iteración vive AQUÍ y no en el WidgetController.
	void ForEachAbility(const FForEachAbility& Delegate);

	// Utilidad estática: extrae el AbilityTag (el que empieza por "Abilities") de un spec.
	// Recorre los AbilityTags de la CDO de la ability y devuelve el primero que sea hijo
	// de "Abilities". Es static porque no lee estado del ASC: solo recibe y devuelve.
	static FGameplayTag GetAbilityTagFromSpec(const FGameplayAbilitySpec& AbilitySpec);

	// Utilidad estática: extrae el InputTag (el que empieza por "InputTag") de un spec.
	// Recorre los DynamicSpecSourceTags (donde se guarda el input asignado en runtime)
	// y devuelve el primero que sea hijo de "InputTag". Es static por la misma razón.
	static FGameplayTag GetInputTagFromSpec(const FGameplayAbilitySpec& AbilitySpec);

	// === SISTEMA DE NIVEL DE ABILITIES (infraestructura del árbol de habilidades) ===
	//
	// DECISIÓN DE DISEÑO (cerrada): el nivel de una ability es COMPLETAMENTE INDEPENDIENTE
	// del nivel del personaje. Toda ability se otorga a nivel 1 (su rango inicial) y SOLO
	// el árbol de habilidades lo sube, gastando puntos de habilidad nodo a nodo.
	//
	// Contexto para entender qué es "el nivel de una ability": cada ability otorgada vive
	// en el ASC como una FGameplayAbilitySpec — su "ficha de registro" (clase, input
	// asignado, y nivel). Cuando un hechizo llama a GetAbilityLevel() para escalar su daño
	// con GetValueAtLevel(), lo que lee es el campo Level de esa ficha. Estas funciones
	// son la ventanilla para consultar y modificar esa ficha por su AbilityTag.

	// Localiza la spec cuya ability lleva el AbilityTag indicado (o un hijo suyo).
	// Devuelve nullptr si el ASC no tiene ninguna ability con ese tag.
	//
	// ADVERTENCIA sobre el puntero devuelto: apunta DENTRO del array interno de specs
	// del ASC. Úsalo y suéltalo en el mismo scope — no lo guardes como miembro ni lo
	// uses tras conceder/remover abilities (el array puede realojarse y el puntero
	// quedaría colgando). Por eso es solo C++ (no UFUNCTION): Blueprint no puede
	// manejar punteros a structs con esta semántica de vida corta.
	FGameplayAbilitySpec* GetSpecFromAbilityTag(const FGameplayTag& AbilityTag);

	// Devuelve el nivel actual de la ability con ese tag, o 0 si el ASC no la tiene.
	// El 0 es deliberado y útil: para el árbol de habilidades y la UI del Spell Menu,
	// "nivel 0" significa "no desbloqueada" (toda ability otorgada tiene nivel >= 1).
	UFUNCTION(BlueprintCallable, Category = "GAS|Abilities")
	int32 GetAbilityLevelFromTag(const FGameplayTag& AbilityTag);

	// Cambia el nivel de la ability con ese tag y notifica a GAS con MarkAbilitySpecDirty
	// (la función oficial para avisar de que una spec otorgada fue modificada).
	// Devuelve true si la ability existía y el nivel quedó fijado; false si no se encontró.
	//
	// NewLevel se clampea a mínimo 1: el nivel 0 está reservado para "no otorgada" y una
	// spec viva nunca debe tenerlo (los GetValueAtLevel de las curvas empiezan en 1).
	//
	// Momento seguro de llamada: desde menús (Spell Menu / árbol), con la ability NO
	// activa. Si la ability estuviera a mitad de ejecución, el nivel nuevo se leerá en
	// su SIGUIENTE activación — GAS no re-evalúa una activación en curso, lo cual es
	// el comportamiento deseado (un ataque ya lanzado no cambia de daño en el aire).
	UFUNCTION(BlueprintCallable, Category = "GAS|Abilities")
	bool SetAbilityLevel(const FGameplayTag& AbilityTag, int32 NewLevel);

	// Llamado desde el PlayerController cuando el input se PULSA (edge-triggered, una
	// vez por pulsación real). Notifica el press a GAS e intenta activar únicamente las
	// abilities configuradas con la política OnInputTriggered.
	void AbilityInputTagPressed(const FGameplayTag& InputTag);

	// Llamado desde el PlayerController cada frame que el input está presionado.
	// Solo intenta activar abilities configuradas con WhileInputActive. No vuelve a
	// notificar AbilitySpecInputPressed cada frame: esa notificación pertenece al edge
	// real y se procesa en AbilityInputTagPressed.
	void AbilityInputTagHeld(const FGameplayTag& InputTag);

	// Llamado desde el PlayerController cuando el input es soltado.
	// Busca abilities con este InputTag y les notifica que el input fue soltado.
	// NO cancela ni termina la ability — la ability decide qué hacer con esa info.
	void AbilityInputTagReleased(const FGameplayTag& InputTag);

	// Llamado desde el PlayerController cuando el input se PULSA (edge-triggered, una
	// vez por pulsacion real, no cada frame). Busca la ability de ataque del jugador
	// que este activa y le marca el buffer de combo. Esto reemplaza la dependencia del
	// override InputPressed de la ability, que con input Held se disparaba cada frame.
	void NotifyComboInputPressed(const FGameplayTag& InputTag);

	// Llamado desde el PlayerController en el mismo edge de pulsación. Si hay un dodge
	// del jugador activo, le ofrece el input para la ventana de follow-up. El dodge solo
	// lo acepta si la ventana está abierta y todavía no existe otro input bufferizado.
	bool NotifyDodgeFollowupInputPressed(const FGameplayTag& InputTag);

	// Intenta activar la primera ability cuyo DynamicSpecSourceTag coincida exactamente
	// con InputTag. Se usa para ejecutar un follow-up almacenado sin duplicar la lógica
	// de búsqueda de specs. Devuelve true solo si alguna activación fue aceptada por GAS.
	bool TryActivateAbilityByInputTag(const FGameplayTag& InputTag);

	// Contexto efímero de la próxima activación de ataque. El dodge lo escribe justo
	// antes de activar el ataque y la propia ability lo consume con reset al comenzar.
	void SetPendingAttackEntryContext(EPantheliaAttackEntryContext NewContext);
	EPantheliaAttackEntryContext ConsumeAttackEntryContext();
	EPantheliaAttackEntryContext GetPendingAttackEntryContext() const
	{
		return PendingAttackEntryContext;
	}
	void ResetPendingAttackEntryContext();

	// Llamado desde el PlayerController cuando se SUELTA un input de ability. Notifica a
	// la ability de ataque pesado activa para la deteccion tap-vs-hold del cargado.
	void NotifyHeavyInputReleased(const FGameplayTag& InputTag);

	// Llamado desde el PlayerController cuando se suelta el boton de bloqueo. Notifica a
	// la ability de parry/bloqueo activa para que termine la guardia sostenida.
	void NotifyBlockInputReleased(const FGameplayTag& InputTag);

	// Llamado desde el AttributeSet (HandleParryReaction) cuando el sistema de daño
	// detecta que una ability de parry activa paró un golpe.
	// A diferencia de las otras funciones Notify*, NO busca por InputTag porque la
	// notificación no viene del input del jugador sino del pipeline de daño: simplemente
	// encuentra la parry ability activa por tipo y le pasa el resultado.
	//   bParried = true  → parry perfecto
	//   bParried = false → bloqueo imperfecto
	//   bGuardBroken     → el bloqueo no pudo pagar stamina; termina la guardia antes
	//                      de reutilizar la ability/animación de Stagger.
	void NotifyParryImpact(bool bParried, bool bGuardBroken);

	// Reutiliza el pipeline de Stagger para una guardia rota y concede
	// State.GuardBroken durante la misma ventana. El tag adicional permite a UI,
	// enemigos y perks distinguir la causa sin duplicar la implementación de stagger.
	void TriggerGuardBreak();

	// Gasta un punto de atributo disponible para mejorar el atributo primario indicado.
	// Llamado desde UAttributeMenuWidgetController::UpgradeAttribute cuando el jugador
	// pulsa el botón "+" de una fila del menú de atributos.
	//
	// Adaptación respecto al curso: el curso envía esto por RPC al servidor
	// (Server_UpgradeAttribute) porque es multiplayer. Panthelia es single-player,
	// así que toda la lógica ocurre aquí directamente, sin RPC.
	//
	// Flujo:
	//   1. Valida que el tag corresponda a uno de los cinco atributos primarios.
	//   2. Reserva un punto mediante TrySpendAttributePoints.
	//   3. Envía el Gameplay Event que procesa GA_ListenForXPEvents + GE_EventBasedEffect.
	//   4. Confirma que el valor del atributo aumentó sincrónicamente. Si no ocurrió,
	//      reembolsa el punto y registra el error de configuración.
	// CONTRATO: el listener debe aplicar un GE Instant en esta misma llamada; no puede
	// introducir Delay, Ability Tasks asíncronas ni aplicación diferida.
	UFUNCTION(BlueprintCallable, Category = "GAS|Attributes")
	void UpgradeAttribute(const FGameplayTag& AttributeTag);

protected:
	// Callback que se dispara cada vez que se aplica cualquier GE a este ASC.
	// Incluye efectos Instant y Duration/Infinite.
	void EffectApplied(UAbilitySystemComponent* AbilitySystemComponent,
		const FGameplayEffectSpec& EffectSpec,
		FActiveGameplayEffectHandle ActiveEffectHandle);

	// Guarda contra el re-bind de EffectApplied en AbilityActorInfoSet (Etapa 4).
	// DETALLE IMPORTANTE que hace necesaria esta guarda: el bind se hace con
	// AddUObject(this, ...) donde "this" es ESTE ASC — un objeto persistente en el caso
	// del jugador. A diferencia de los binds cuyo objeto es el Pawn (que se autolimpian
	// cuando el Pawn muere), este bind SOBREVIVE al respawn; si el Pawn nuevo volviera a
	// llamar AbilityActorInfoSet sin guarda, habría DOS binds y cada GE aplicado se
	// broadcastearía dos veces a la UI.
	bool bEffectAppliedDelegateBound = false;

	// Guarda contra la re-concesión de abilities pasivas en un futuro respawn (Etapa 4).
	// Simétrica a bStartupAbilitiesGiven, pero para AddCharacterPassiveAbilities: las
	// pasivas se otorgan Y ACTIVAN (GiveAbilityAndActivateOnce), así que duplicarlas
	// significaría dos GA_ListenForXPEvents activas procesando cada evento dos veces.
	bool bPassiveAbilitiesGiven = false;

private:
	// Observa el ciclo del tag Effects.Stagger para retirar State.GuardBroken cuando
	// termina la misma reacción. Un único guard break puede estar activo por ASC.
	void OnGuardBreakStaggerTagChanged(const FGameplayTag Tag, int32 NewCount);
	void ClearGuardBreakState();

	FDelegateHandle GuardBreakStaggerTagDelegateHandle;
	bool bGuardBreakObservedStagger = false;

	// Solo existe entre SetPendingAttackEntryContext y el comienzo de la próxima ability
	// de ataque. ConsumeAttackEntryContext lo devuelve y lo resetea en la misma llamada.
	EPantheliaAttackEntryContext PendingAttackEntryContext =
		EPantheliaAttackEntryContext::Normal;
};
