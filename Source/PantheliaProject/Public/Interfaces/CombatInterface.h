// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "PantheliaElementTypes.h"
#include "GameplayTagContainer.h"
// Necesario para exponer EPantheliaCharacterClass en GetCharacterClass.
// La interfaz lo necesita en su firma de función (no basta forward declaration
// porque es un enum por valor, no un puntero).
#include "AbilitySystem/Data/PantheliaCharacterClassInfo.h"
#include "CombatInterface.generated.h"

class UAnimMontage;
class USoundBase;
class UNiagaraSystem;
class UAbilitySystemComponent;

// ============================================================
// Delegates de la interfaz de combate (clase 311)
// ============================================================
//
// Por qué viven AQUÍ y no en APantheliaCharacterBase directamente: el nuevo
// UPantheliaDebuffNiagaraComponent (clase 311) necesita saber cuándo el ASC de su
// propietario ya está listo, y cuándo su propietario muere — pero NO debe depender
// directamente de APantheliaCharacterBase (evitamos que un componente reutilizable
// conozca una clase concreta del juego; solo conoce la interfaz). Cualquier actor que
// implemente ICombatInterface puede ofrecer estos dos delegates sin acoplarse a nada más.
//
// FOnASCRegistered — NO dynamic (no hace falta Blueprint, solo se usa desde C++ con
// AddUObject/AddWeakLambda). Se dispara UNA vez, justo después de que el ASC del
// propietario queda inicializado (InitAbilityActorInfo, en PantheliaEnemy y
// MainCharacter). Por qué hace falta: un componente puede hacer BeginPlay() ANTES de
// que el ASC del personaje esté listo (el orden entre componentes y el ASC no está
// garantizado) — sin este delegate, intentar leer el ASC en ese instante devolvería
// nullptr y el componente nunca llegaría a suscribirse a los cambios de tag.
DECLARE_MULTICAST_DELEGATE_OneParam(FOnASCRegistered, UAbilitySystemComponent*);

// FOnDeath — SÍ dynamic (DYNAMIC_MULTICAST): a diferencia del anterior, este delegate
// necesita poder bindearse con UFUNCTION()+AddDynamic (lo que usa
// UPantheliaDebuffNiagaraComponent para desactivarse al morir su propietario), y en el
// futuro cualquier sistema en Blueprint (VFX, sonido, lógica de loot) podría querer
// suscribirse sin tocar C++. DeadActor: el actor que murió, para quien escuche pueda
// saber de quién se trata sin tener que guardar una referencia aparte.
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnDeath, AActor*, DeadActor);

/**
 * FTaggedMontage
 *
 * Vincula un montage de ataque con su gameplay tag de socket.
 * El tag determina qué socket usar para hit detection en GetCombatSocketLocation:
 *   Montage.Attack.Weapon    → socket en el arma (FinalWeaponMesh)
 *   Montage.Attack.RightHand → socket en la mano derecha (mesh del personaje)
 *   Montage.Attack.LeftHand  → socket en la mano izquierda (mesh del personaje)
 *
 * Configurable por Blueprint en el array AttackMontages de cada personaje.
 * El notify AN_MontageEvent del montage debe enviar el mismo MontageTag.
 */
USTRUCT(BlueprintType)
struct FTaggedMontage
{
    GENERATED_BODY()

    // El montage de ataque a reproducir.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TObjectPtr<UAnimMontage> Montage = nullptr;

    // Tag que identifica qué socket usar para este ataque.
    // Debe coincidir con el tag enviado por AN_MontageEvent en el montage.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    FGameplayTag MontageTag;

    // GANCHO (pendiente de assets): sonido de impacto a reproducir si ESTE ataque conecta.
    // Va por montage (no por personaje) porque cada animación puede golpear con algo distinto
    // (puño, arma, patada) y sonar diferente. Vacío por ahora; cuando se asigne, el
    // WeaponTraceComponent lo reproducirá en el punto de impacto solo si hubo hit.
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    TObjectPtr<USoundBase> ImpactSound = nullptr;
};

UINTERFACE(MinimalAPI, BlueprintType)
class UCombatInterface : public UInterface
{
    GENERATED_BODY()
};

class PANTHELIAPROJECT_API ICombatInterface
{
    GENERATED_BODY()

public:
    // Devuelve el nivel del personaje. Convertido a BlueprintNativeEvent para poder
    // llamarse con Execute_GetPlayerLevel(Actor) sin necesidad de castear previamente
    // a ICombatInterface. Esto evita crashes si el actor no implementa la interfaz
    // (p.ej. un EffectActor que aplica daño de área no tiene nivel).
    // Default: 0 (UHT genera la implementación base que devuelve el default del tipo).
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
    int32 GetPlayerLevel() const;

    // Devuelve la ubicación del socket del arma (tip socket).
    // Usada para spawn de proyectiles y para detectar hits de melee.
    // MontageTag identifica qué socket usar:
    //   Montage.Attack.Weapon    → socket en el arma (FinalWeaponMesh)
    //   Montage.Attack.LeftHand  → socket en la mano izquierda del mesh
    //   Montage.Attack.RightHand → socket en la mano derecha del mesh
    // En C++ llamar via Execute_GetCombatSocketLocation(Actor, MontageTag).
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
    FVector GetCombatSocketLocation(const FGameplayTag& MontageTag);

    // Actualiza el warp target de Motion Warping.
    UFUNCTION(BlueprintCallable, BlueprintImplementableEvent)
    void UpdateFacingTarget(const FVector& Target);

    // Devuelve el montage de hit react del personaje.
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
    UAnimMontage* GetHitReactMontage();

    // Devuelve el montage de stagger (aturdimiento cuando postura llega a 0).
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
    UAnimMontage* GetStaggerMontage();

    // Devuelve el montage de "levantarse" tras un lanzamiento aéreo (post-315, Nivel 3
    // de knockback). Lo reproduce GA_GetUp al activarse — mismo patrón exacto que
    // GetHitReactMontage/GetStaggerMontage arriba.
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
    UAnimMontage* GetGetUpMontage();

    // Devuelve el montage de reacción al knockback "pesado" (Nivel 2, a petición) — el
    // personaje se ve empujado con más fuerza y una animación dedicada, en vez del
    // HitReact normal. Lo reproduce GA_HeavyKnockback — mismo patrón que los anteriores.
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
    UAnimMontage* GetHeavyKnockbackMontage();

    // Maneja la muerte del personaje (ragdoll + dissolve + lifespan).
    // DeathImpulse (clase 314): vector ya calculado (dirección × magnitud) que se aplica
    // físicamente al ragdoll/arma en MulticastHandleDeath. Se obtiene del context del
    // golpe fatal (ver UPantheliaAbilitySystemLibrary::GetDeathImpulse) y se pasa por
    // parámetro en vez de leerlo el propio Die() — así este método no necesita saber
    // NADA sobre GAS ni sobre de dónde viene ese vector, solo qué hacer con él una vez
    // que lo tiene. Mantiene la interfaz limpia de dependencias que no le corresponden.
    virtual void Die(const FVector& DeathImpulse) = 0;

    // --- Delegates de la clase 311 (ver declaración arriba para la explicación completa) ---
    // Pura virtual, NO expuesta a Blueprint (solo C++): quien implemente esta interfaz
    // DEBE devolver una referencia a su propio delegate (normalmente un miembro que ya
    // tiene declarado, como hace APantheliaCharacterBase). Devolver por referencia (no
    // por valor) es clave: así quien llama a .AddUObject()/.AddWeakLambda()/.Broadcast()
    // está operando sobre el delegate REAL del objeto, no sobre una copia inútil.
    virtual FOnASCRegistered& GetOnASCRegisteredDelegate() = 0;
    virtual FOnDeath& GetOnDeathDelegate() = 0;

    // Devuelve true si el personaje ha muerto.
    // Usado por GetLivePlayersWithinRadius para filtrar cadáveres.
    // BlueprintNativeEvent: en C++ usar ICombatInterface::Execute_IsDead(Actor).
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
    bool IsDead() const;

    // Devuelve el actor que implementa esta interfaz (el avatar).
    // Permite obtener el AActor* sin necesidad de cast cuando solo
    // tenemos un ICombatInterface*.
    // BlueprintNativeEvent: en C++ usar ICombatInterface::Execute_GetAvatar(Actor).
    // No es const porque devolver 'this' desde const requeriría const AActor*.
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
    AActor* GetAvatar();

    // Devuelve el array de montages de ataque disponibles para este personaje.
    // Cada entrada contiene el montage + el tag del socket de hit detection.
    // GA_MeleeAttack selecciona uno al azar para variedad de ataques.
    // Configurable por Blueprint en AttackMontages de cada personaje.
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
    TArray<FTaggedMontage> GetAttackMontages();

    // GANCHO (pendiente de assets): devuelve el efecto de impacto (Niagara) de ESTE personaje.
    // Se llama sobre la VÍCTIMA del golpe, para que cada personaje aporte su propio efecto
    // (sangre roja/verde, chispas, etc.). El WeaponTraceComponent lo usará al detectar un hit.
    // Devuelve null hasta que se asigne un BloodEffect en el Blueprint del personaje.
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
    UNiagaraSystem* GetBloodEffect();

    // Devuelve el % de MaxPoise que un golpe debe infligir para activar HitReact.
    // Enemigos normales: 10% (default). Bosses: 15%. Configurable por Blueprint.
    virtual float GetFlinchThreshold() const { return 10.f; }

    // Reinicia el timer de regeneración de postura.
    // Llamado por el AttributeSet cada vez que se recibe daño de postura.
    virtual void ResetPoiseRegenTimer() {}

    // Devuelve el elemento defensivo del personaje.
    // Afecta a la tabla de afinidades de daño elemental en ExecCalc_Damage.
    //
    // Enemigos: valor fijo configurable en el Blueprint del enemigo.
    // Jugador: determinado por el corazón elemental equipado (Sistema Elemental, pendiente).
    //          Devuelve None hasta que ese sistema esté implementado.
    //
    // None = neutro — ningún tipo de daño recibe modificador de tabla.
    virtual EPantheliaElement GetDefensiveElement() const { return EPantheliaElement::None; }

    // Devuelve el arquetipo de personaje (Warrior, Ranger, Elementalist).
    // Usado por el sistema de XP para identificar el tipo de enemigo muerto.
    //
    // GANCHO DE AUTOLEVEL: cuando el sistema de escalado de zonas esté implementado,
    // este método junto con Level será la entrada principal para calcular la XP reescalada.
    //
    // Jugador: devuelve Elementalist (seteado en AMainCharacter constructor).
    // Enemigos: valor configurable por Blueprint en Details → Panthelia|Combat.
    // Default: Warrior.
    UFUNCTION(BlueprintCallable, BlueprintNativeEvent)
    EPantheliaCharacterClass GetCharacterClass() const;
};