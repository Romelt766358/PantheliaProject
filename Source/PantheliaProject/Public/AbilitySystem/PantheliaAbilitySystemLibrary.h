// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "AbilitySystem/Data/PantheliaCharacterClassInfo.h"
#include "AbilitySystem/PantheliaAbilityTypes.h"
#include "PantheliaAbilitySystemLibrary.generated.h"

class UOverlayWidgetController;
class UAttributeMenuWidgetController;
class UAbilitySystemComponent;

/**
 * UPantheliaAbilitySystemLibrary
 *
 * Blueprint Function Library con funciones estáticas para acceder fácilmente
 * a los widget controllers y otros sistemas del juego desde Blueprint o C++.
 */
UCLASS()
class PANTHELIAPROJECT_API UPantheliaAbilitySystemLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:

	// Devuelve el OverlayWidgetController del HUD.
	UFUNCTION(BlueprintPure, Category = "PantheliaAbilitySystemLibrary|WidgetController")
	static UOverlayWidgetController* GetOverlayWidgetController(const UObject* WorldContextObject);

	// Devuelve el AttributeMenuWidgetController del HUD.
	UFUNCTION(BlueprintPure, Category = "PantheliaAbilitySystemLibrary|WidgetController")
	static UAttributeMenuWidgetController* GetAttributeMenuWidgetController(const UObject* WorldContextObject);

	// Inicializa los atributos de un personaje según su arquetipo y nivel.
	UFUNCTION(BlueprintCallable, Category = "PantheliaAbilitySystemLibrary|CharacterClassDefaults")
	static void InitializeDefaultAttributes(const UObject* WorldContextObject,
		EPantheliaCharacterClass CharacterClass,
		float Level,
		UAbilitySystemComponent* ASC);

	// Otorga a un ASC las CommonAbilities y las StartupAbilities del arquetipo indicado.
	// CharacterClass determina qué abilities específicas de clase se conceden (además de las comunes).
	UFUNCTION(BlueprintCallable, Category = "PantheliaAbilitySystemLibrary|CharacterClassDefaults")
	static void GiveStartupAbilities(const UObject* WorldContextObject,
		UAbilitySystemComponent* ASC,
		EPantheliaCharacterClass CharacterClass);

	// Devuelve el CharacterClassInfo Data Asset desde el GameMode.
	UFUNCTION(BlueprintCallable, Category = "PantheliaAbilitySystemLibrary|CharacterClassDefaults")
	static UPantheliaCharacterClassInfo* GetCharacterClassInfo(const UObject* WorldContextObject);

	// ============================================================
	// GETTERS/SETTERS del Gameplay Effect Context custom
	// ============================================================
	// Estas funciones encapsulan el static cast a FPantheliaGameplayEffectContext
	// para que el resto del código no tenga que hacerlo manualmente.
	// Se usan en ExecCalc_Damage (setters) y AttributeSet (getters).

	// Devuelve true si el golpe registrado en el context fue crítico.
	// BlueprintPure: no tiene efectos secundarios, puede usarse en cualquier momento.
	// Útil en Blueprint para consultar el resultado del golpe en medio de una ability.
	UFUNCTION(BlueprintPure, Category = "PantheliaAbilitySystemLibrary|GameplayEffects")
	static bool IsCriticalHit(const FGameplayEffectContextHandle& EffectContextHandle);

	// Escribe en el context si el golpe fue crítico.
	// UPARAM(ref): indica a Blueprint que EffectContextHandle es un INPUT (no output).
	// Sin esto Blueprint lo muestra como pin de salida, que es incorrecto.
	// Usado exclusivamente por ExecCalc_Damage después de calcular el crítico.
	UFUNCTION(BlueprintCallable, Category = "PantheliaAbilitySystemLibrary|GameplayEffects")
	static void SetIsCriticalHit(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle,
		bool bInIsCriticalHit);

	// ============================================================
	// LECTURA DEL RESULTADO DE DEBUFF (clase 307-308)
	// ============================================================
	// Solo GETTERS aquí — los setters no hacen falta en la librería porque solo
	// ExecCalc_Damage escribe estos valores, y lo hace directo via static_cast (igual
	// que ya hace con SetIsCriticalHit/SetParryResult/SetWasBlocked). Estos getters
	// existen para que OTRO código (PostGameplayEffectExecute, el AttributeSet) pueda
	// leer el resultado sin necesitar conocer el tipo concreto FPantheliaGameplayEffectContext.

	// Devuelve true si ExecCalc_Damage determinó un debuff exitoso en este golpe.
	UFUNCTION(BlueprintPure, Category = "PantheliaAbilitySystemLibrary|GameplayEffects|DamageEffect")
	static bool IsSuccessfulDebuff(const FGameplayEffectContextHandle& EffectContextHandle);

	// Devuelve el daño que debe tiquear el debuff exitoso cada GetDebuffFrequency segundos.
	UFUNCTION(BlueprintPure, Category = "PantheliaAbilitySystemLibrary|GameplayEffects|DamageEffect")
	static float GetDebuffDamage(const FGameplayEffectContextHandle& EffectContextHandle);

	// Devuelve la duración total en segundos del debuff exitoso.
	UFUNCTION(BlueprintPure, Category = "PantheliaAbilitySystemLibrary|GameplayEffects|DamageEffect")
	static float GetDebuffDuration(const FGameplayEffectContextHandle& EffectContextHandle);

	// Devuelve cada cuántos segundos tiquea el daño del debuff exitoso.
	UFUNCTION(BlueprintPure, Category = "PantheliaAbilitySystemLibrary|GameplayEffects|DamageEffect")
	static float GetDebuffFrequency(const FGameplayEffectContextHandle& EffectContextHandle);

	// Devuelve el tipo de daño (elemental) que causó el debuff exitoso.
	// Gestiona el TSharedPtr internamente: si no hay uno válido (nunca se asignó),
	// devuelve un FGameplayTag vacío en vez de crashear al desreferenciar un puntero nulo.
	UFUNCTION(BlueprintPure, Category = "PantheliaAbilitySystemLibrary|GameplayEffects|DamageEffect")
	static FGameplayTag GetDamageType(const FGameplayEffectContextHandle& EffectContextHandle);

	// Devuelve el vector de impulso a aplicar si el golpe resultó fatal (clase 313).
	// Si el context no es válido, devuelve FVector::ZeroVector (nunca crashea).
	UFUNCTION(BlueprintPure, Category = "PantheliaAbilitySystemLibrary|GameplayEffects|DamageEffect")
	static FVector GetDeathImpulse(const FGameplayEffectContextHandle& EffectContextHandle);

	// Devuelve el vector de fuerza de knockback a aplicar si el golpe NO resultó fatal
	// (clase 315). Si el context no es válido, devuelve FVector::ZeroVector.
	UFUNCTION(BlueprintPure, Category = "PantheliaAbilitySystemLibrary|GameplayEffects|DamageEffect")
	static FVector GetKnockbackForce(const FGameplayEffectContextHandle& EffectContextHandle);

	// Devuelve el vector de lanzamiento aéreo (post-315, Nivel 3). Independiente de
	// GetKnockbackForce — ver la nota de diseño en FPantheliaGameplayTags.
	UFUNCTION(BlueprintPure, Category = "PantheliaAbilitySystemLibrary|GameplayEffects|DamageEffect")
	static FVector GetLaunchForce(const FGameplayEffectContextHandle& EffectContextHandle);

	// Devuelve true si el knockback de este golpe está marcado como "pesado" (Nivel 2,
	// a petición) — bloquea GA_HitReact y dispara GA_HeavyKnockback en su lugar.
	UFUNCTION(BlueprintPure, Category = "PantheliaAbilitySystemLibrary|GameplayEffects|DamageEffect")
	static bool IsKnockbackHeavy(const FGameplayEffectContextHandle& EffectContextHandle);

	// ============================================================
	// ESCRITURA DEL RESULTADO DE DEBUFF (clase 309)
	// ============================================================
	// Estas 5 funciones las llama exclusivamente DetermineDebuff (ExecCalc_Damage) cuando
	// confirma un debuff exitoso — es el único sitio del código que decide que un debuff
	// ocurrió, así que es el único que necesita escribir estos valores en el context.
	// UPARAM(ref) en todas: EffectContextHandle es un INPUT que se modifica, no un output
	// (sin esto Blueprint lo mostraría como pin de salida, incorrectamente).

	UFUNCTION(BlueprintCallable, Category = "PantheliaAbilitySystemLibrary|GameplayEffects|DamageEffect")
	static void SetIsSuccessfulDebuff(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle,
		bool bInSuccessfulDebuff);

	UFUNCTION(BlueprintCallable, Category = "PantheliaAbilitySystemLibrary|GameplayEffects|DamageEffect")
	static void SetDebuffDamage(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle,
		float InDebuffDamage);

	UFUNCTION(BlueprintCallable, Category = "PantheliaAbilitySystemLibrary|GameplayEffects|DamageEffect")
	static void SetDebuffDuration(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle,
		float InDebuffDuration);

	UFUNCTION(BlueprintCallable, Category = "PantheliaAbilitySystemLibrary|GameplayEffects|DamageEffect")
	static void SetDebuffFrequency(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle,
		float InDebuffFrequency);

	// A diferencia de las otras 4, esta recibe un FGameplayTag simple (no un TSharedPtr):
	// crea el TSharedPtr internamente con MakeShared antes de asignarlo al context, para
	// que quien llama (ExecCalc_Damage) no tenga que lidiar con smart pointers.
	UFUNCTION(BlueprintCallable, Category = "PantheliaAbilitySystemLibrary|GameplayEffects|DamageEffect")
	static void SetDamageType(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle,
		const FGameplayTag& InDamageType);

	// Asigna el vector de impulso de muerte (clase 313). Llamada desde
	// APantheliaProjectile::OnSphereOverlap (y, en el futuro, desde melee/weapon trace
	// con el mismo patrón) justo ANTES de aplicar el gameplay effect — el impulso debe
	// estar en el context antes de que HandleIncomingDamage llegue a leerlo, o llegaría
	// demasiado tarde.
	UFUNCTION(BlueprintCallable, Category = "PantheliaAbilitySystemLibrary|GameplayEffects|DamageEffect")
	static void SetDeathImpulse(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle,
		const FVector& InImpulse);

	// Asigna el vector de fuerza de knockback (clase 315). Llamada desde
	// APantheliaProjectile::OnSphereOverlap y UWeaponTraceComponent::PerformTrace, justo
	// ANTES de aplicar el gameplay effect — mismo motivo que SetDeathImpulse arriba.
	UFUNCTION(BlueprintCallable, Category = "PantheliaAbilitySystemLibrary|GameplayEffects|DamageEffect")
	static void SetKnockbackForce(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle,
		const FVector& InForce);

	// Asigna el vector de lanzamiento aéreo (post-315, Nivel 3). Mismo patrón e
	// idénticos sitios de llamada que SetKnockbackForce, con su propio campo
	// independiente en el context.
	UFUNCTION(BlueprintCallable, Category = "PantheliaAbilitySystemLibrary|GameplayEffects|DamageEffect")
	static void SetLaunchForce(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle,
		const FVector& InForce);

	// Marca este knockback como "pesado" (Nivel 2, a petición). Llamada desde
	// APantheliaProjectile::OnSphereOverlap y UWeaponTraceComponent::PerformTrace, en el
	// mismo bloque donde ya se calcula/escribe KnockbackForce.
	UFUNCTION(BlueprintCallable, Category = "PantheliaAbilitySystemLibrary|GameplayEffects|DamageEffect")
	static void SetKnockbackIsHeavy(UPARAM(ref) FGameplayEffectContextHandle& EffectContextHandle,
		bool bInHeavy);

	// ============================================================
	// UTILIDAD DE DIRECCIÓN CON PITCH FIJO (clase 315)
	// ============================================================
	// Rota InDirection para que apunte con un pitch (ángulo vertical) FIJO, conservando
	// su yaw (dirección horizontal) original. Útil para cualquier combat trick que lance
	// algo hacia arriba en un ángulo consistente sin importar hacia dónde se mira — el
	// knockback de esta clase lo usa para que los enemigos salgan despedidos hacia
	// arriba y hacia atrás, no en línea recta horizontal (que se vería raro).
	//
	// Ejemplo: GetDirectionWithPitchOverride(GetActorForwardVector(), 45.f) devuelve un
	// vector que apunta hacia donde miras, pero angulado 45° hacia arriba.
	//
	// Se expone aquí (en vez de duplicar la lógica en cada sitio que lance algo) porque
	// es exactamente el tipo de utilidad pequeña y genérica que un árbol de habilidades
	// complejo va a querer reutilizar una y otra vez (saltos, lanzamientos, empujones...).
	// BlueprintPure: sin efectos secundarios, solo calcula — se puede usar libremente
	// también desde Blueprint para futuros combat tricks sin tocar C++.
	UFUNCTION(BlueprintPure, Category = "PantheliaAbilitySystemLibrary|Combat")
	static FVector GetDirectionWithPitchOverride(const FVector& InDirection, float PitchOverrideDegrees);

	// ============================================================
	// I-FRAMES GENÉRICOS (post-315, a petición explícita)
	// ============================================================
	// Concede el tag State.Invulnerable a ASC durante Duration segundos, creando un
	// UGameplayEffect dinámicamente en C++ (mismo patrón que UPantheliaAttributeSet::Debuff,
	// clase 310 — un GE con duración simple, sin modificadores de atributo, que solo
	// concede un tag). ExecCalc_Damage comprueba este tag al principio de
	// Execute_Implementation y anula CUALQUIER daño mientras esté activo.
	//
	// GENÉRICO A PROPÓSITO: no pertenece a ninguna ability concreta — cualquier sistema
	// puede llamarlo. Hoy lo usa GA_GetUp (al levantarse tras un lanzamiento aéreo); en
	// el futuro, GA_Dodge hará exactamente lo mismo con su propia duración, sin que haga
	// falta tocar esta función ni el chequeo del ExecCalc — ya están listos para eso.
	UFUNCTION(BlueprintCallable, Category = "PantheliaAbilitySystemLibrary|Combat")
	static void GrantTemporaryInvulnerability(UAbilitySystemComponent* ASC, float Duration);

	// ============================================================
	// TAG TEMPORAL GENÉRICO (Nivel 2 de knockback, a petición)
	// ============================================================
	// Generalización de GrantTemporaryInvulnerability: mismo mecanismo (GE dinámico con
	// duración, sin modificadores, que solo concede un tag), pero para CUALQUIER tag,
	// no solo State.Invulnerable. GrantTemporaryInvulnerability ahora llama a esta
	// función por dentro (ver el .cpp) — se mantiene como función aparte, con su firma
	// intacta, para no romper el nodo ya cableado en GA_GetUp.
	//
	// Primer uso: State.HeavyKnockback (Nivel 2), concedido brevemente desde
	// HandleIncomingDamage cuando un knockback "pesado" se activa, para bloquear
	// GA_HitReact mientras dura la reacción de GA_HeavyKnockback.
	UFUNCTION(BlueprintCallable, Category = "PantheliaAbilitySystemLibrary|Combat")
	static void GrantTemporaryGameplayTag(UAbilitySystemComponent* ASC, FGameplayTag Tag, float Duration);

	// ============================================================
	// APLICACIÓN DE DAÑO SECUNDARIO/DEBUFF (clase 305)
	// ============================================================
	// Aplica un Gameplay Effect de daño usando un FDamageEffectParams ya construido
	// (ver el struct para la explicación completa de para qué sirve y en qué se
	// diferencia del pipeline de la ability principal).
	//
	// Construye el spec desde DamageEffectParams.DamageGameplayEffectClass, asigna
	// el SetByCaller de DamageType (si es válido) y los 4 SetByCaller de debuff, y
	// aplica el spec resultante a TargetASC.
	//
	// Devuelve el FGameplayEffectContextHandle usado, por si el llamador necesita
	// inspeccionarlo o guardarlo (ej. para leer más tarde si fue crítico).
	//
	// Si SourceASC o TargetASC no son válidos, loguea un Error y devuelve un handle
	// vacío sin crashear (el curso original deja que crashee aquí; preferimos fallar
	// de forma controlada).
	UFUNCTION(BlueprintCallable, Category = "PantheliaAbilitySystemLibrary|GameplayEffects|DamageEffect")
	static FGameplayEffectContextHandle ApplyDamageEffect(const FDamageEffectParams& DamageEffectParams);

	// Devuelve los actores vivos (que implementan ICombatInterface y no están muertos)
	// dentro de un radio alrededor de un punto.
	// ActorsToIgnore: lista de actores a excluir (típicamente el atacante).
	// OutOverlappingActors: resultado. Se llena con actores únicos vivos.
	UFUNCTION(BlueprintCallable, Category = "PantheliaAbilitySystemLibrary|GameplayMechanics")
	static void GetLivePlayersWithinRadius(const UObject* WorldContextObject,
		TArray<AActor*>& OutOverlappingActors,
		const TArray<AActor*>& ActorsToIgnore,
		float Radius,
		const FVector& SphereOrigin);

	// Devuelve true si los dos actores son enemigos entre sí (equipos distintos).
	// Usa Actor Tags "Player" y "Enemy" para determinar el equipo.
	UFUNCTION(BlueprintCallable, Category = "PantheliaAbilitySystemLibrary|GameplayMechanics")
	static bool IsNotFriend(AActor* FirstActor, AActor* SecondActor);

	// ============================================================
	// SISTEMA DE RENDIMIENTOS DECRECIENTES DE XP
	// ============================================================

	// Devuelve el multiplicador de XP según cuántas veces el jugador ha matado
	// a ese enemigo antes (KillCount = número de muertes PREVIAS, no incluye la actual).
	//
	// Tabla de multiplicadores:
	//   KillCount = 0 → primera muerte  → 1.00 (100%)
	//   KillCount = 1 → segunda muerte  → 0.60  (60%)
	//   KillCount = 2 → tercera muerte  → 0.35  (35%)
	//   KillCount = 3 → cuarta muerte   → 0.20  (20%)
	//   KillCount ≥ 4 → quinta+ muerte  → 0.10  (10%, piso permanente)
	//
	// No es UFUNCTION: solo lo llama el AttributeSet en C++ al procesar IncomingXP.
	// KillCount se obtiene con APantheliaPlayerState::GetEnemyKillCount(EnemyID),
	// y se incrementa DESPUÉS de otorgar la XP con RecordEnemyKill(EnemyID).
	static float GetXPMultiplierForKillCount(int32 KillCount);
};