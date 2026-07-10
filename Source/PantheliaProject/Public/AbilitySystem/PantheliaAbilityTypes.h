// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameplayEffectTypes.h"
#include "PantheliaAbilityTypes.generated.h"

// Forward declarations — evitan incluir headers pesados aquí (AbilitySystemComponent.h,
// GameplayEffect.h) cuando este .h solo necesita punteros/TSubclassOf, no la clase completa.
class UAbilitySystemComponent;
class UGameplayEffect;

// ============================================================
// FDamageEffectParams
// ============================================================
//
// Paquete de datos "autocontenido" para aplicar un Gameplay Effect de daño desde
// CUALQUIER sitio del código — no solo desde dentro de una UGameplayAbility activa.
//
// ¿Por qué hace falta si ya existe MakeDamageSpec()/CauseDamage() en
// UPantheliaDamageGameplayAbility? Porque esas dos funciones dependen de estar
// ejecutándose DENTRO de una ability (usan GetAbilitySystemComponentFromActorInfo(),
// GetAbilityLevel(), etc.). Este struct, en cambio, no depende de ninguna ability:
// carga todo lo necesario (ASCs, clase de GE, nivel, tipo de daño, parámetros de
// debuff) como datos planos, para poder aplicarse desde sitios donde NO hay una
// ability activa — el caso de uso venidero es el tick periódico de un debuff
// (Quemadura, Electrocución...) que se dispara solo, sin que ninguna ability lo
// esté ejecutando en ese instante.
//
// DECISIÓN DE DISEÑO IMPORTANTE (clase 305, adaptada — leer con atención):
// El curso de Druid Mechanics le da a este struct UN solo DamageType + BaseDamage.
// En la clase 303 rechazamos ese modelo "1 tipo por ability" para
// UPantheliaDamageGameplayAbility porque un ataque principal puede llevar varios
// tipos de daño a la vez (TMap DamageTypes, sigue así, sin cambios).
//
// Aquí NO aplica el mismo rechazo, y por eso este struct SÍ mantiene un solo
// DamageType: este paquete no está pensado para el golpe principal (ese ya tiene
// su propio pipeline completo en ApplyDamageScalingToSpec). Está pensado para
// daño SECUNDARIO de un debuff, y un debuff SIEMPRE corresponde a un solo elemento
// por diseño (ver ElementToDebuff en FPantheliaGameplayTags: Fuego→Quemadura,
// Agua→Saturación, etc. — relación 1 a 1 real, no forzada). Por eso "un solo tipo"
// aquí es correcto, no una simplificación que sacrifique nada.
//
// Los 4 campos Debuff* (Chance/Damage/Duration/Frequency) vienen de la clase 304
// (ver DebuffDamage/Frequency/Duration en UPantheliaDamageGameplayAbility) y
// viajan aquí para poder asignarse como SetByCaller en el GE que se aplique.
//
// NO se implementa (todavía) una función "MakeFromClassDefaults" que copie esto
// desde UPantheliaDamageGameplayAbility automáticamente, porque esa ability tiene
// VARIOS tipos de daño (TMap) y este struct solo admite UNO — elegir cuál copiar
// requeriría inventar una regla que no está definida aún. Se construirá a mano,
// con el tipo correcto ya resuelto, en el sitio de la clase futura que dispare el
// debuff (probablemente resolviendo el tipo vía ElementToDebuff).
// ============================================================
USTRUCT(BlueprintType)
struct FDamageEffectParams
{
    GENERATED_BODY()

    FDamageEffectParams() {}

    // Necesario para operaciones que requieren un UObject "de contexto" (ej. GetWorld
    // desde una función estática de librería). Normalmente el AvatarActor del source.
    UPROPERTY()
    TObjectPtr<UObject> WorldContextObject = nullptr;

    // El Gameplay Effect que realmente contiene el ExecCalc_Damage (o el GE del debuff).
    UPROPERTY()
    TSubclassOf<UGameplayEffect> DamageGameplayEffectClass = nullptr;

    // ASC de quien origina el daño (para MakeOutgoingSpec/MakeEffectContext).
    UPROPERTY()
    TObjectPtr<UAbilitySystemComponent> SourceASC = nullptr;

    // ASC de quien recibe el daño (a quien se le aplica el spec).
    UPROPERTY()
    TObjectPtr<UAbilitySystemComponent> TargetASC = nullptr;

    // Nivel al que se evalúa el GE (ScalableFloats dentro del GE, si los usa).
    UPROPERTY()
    float AbilityLevel = 1.f;

    // Tipo de daño único de este paquete (ver nota de diseño arriba: por qué es
    // singular aquí y no un TMap como en UPantheliaDamageGameplayAbility).
    UPROPERTY()
    FGameplayTag DamageType = FGameplayTag();

    // Magnitud del daño para DamageType, ya evaluada/escalada por quien construye el struct.
    UPROPERTY()
    float BaseDamage = 0.f;

    // --- Parámetros de efecto de estado (clase 304, actualizado por buildup) ---
    // DebuffChance fue ELIMINADO (decisión cerrada): los estados no tienen azar —
    // se disparan por umbral de acumulación (buildup), como en Elden Ring/Lies of P.

    // Daño que tiquea el debuff cada DebuffFrequency segundos.
    UPROPERTY()
    float DebuffDamage = 0.f;

    // Duración total en segundos del debuff activo.
    UPROPERTY()
    float DebuffDuration = 0.f;

    // Cada cuántos segundos tiquea DebuffDamage (el "Period" del GE periódico).
    UPROPERTY()
    float DebuffFrequency = 0.f;

    // --- Impulso de muerte (clase 312) ---
    // Magnitud del impulso físico a aplicar si este golpe resulta fatal. Float simple
    // (no FScalableFloat, a diferencia del campo equivalente en la ability): este struct
    // representa valores YA EVALUADOS en tiempo de ejecución, no curvas — mismo motivo
    // por el que BaseDamage y los parámetros de debuff arriba también son floats simples.
    //
    // Igual que el resto de campos de este struct para daño secundario/debuff (ver la
    // nota de diseño al principio del struct): nadie lo rellena ni lo lee todavía. Ver
    // DeathImpulseMagnitude en UPantheliaDamageGameplayAbility.h para el porqué completo
    // y qué falta para que esto haga algo.
    UPROPERTY()
    float DeathImpulseMagnitude = 0.f;

    // Vector de impulso ya calculado (dirección × magnitud) — clase 313. Al igual que
    // DeathImpulseMagnitude arriba, este campo existe por completitud del struct pero
    // NO es el camino que usamos en Panthelia para propagar esto en la práctica: nuestro
    // proyectil (PantheliaProjectile.cpp) calcula y escribe el vector DIRECTAMENTE sobre
    // el contexto del spec existente, sin pasar por este struct — porque nuestra
    // ability principal usa el pipeline de spec handle multi-tipo (ApplyDamageScalingToSpec),
    // no FDamageEffectParams/ApplyDamageEffect (ver la nota de diseño de la clase 305/306).
    UPROPERTY()
    FVector DeathImpulse = FVector::ZeroVector;

    // --- Knockback (clase 315) ---
    // Misma lógica que DeathImpulseMagnitude/DeathImpulse arriba: existen aquí por
    // completitud del struct, pero el camino real de propagación en Panthelia es el
    // SetByCaller (CombatTricks_KnockbackForceMagnitude/Chance en ApplyDamageScalingToSpec)
    // + escritura directa en el context desde PantheliaProjectile.cpp/WeaponTraceComponent.cpp
    // — no desde aquí.
    UPROPERTY()
    float KnockbackForceMagnitude = 0.f;

    UPROPERTY()
    FVector KnockbackForce = FVector::ZeroVector;

    UPROPERTY()
    float KnockbackChance = 0.f;

    // --- Launch / Nivel 3 (post-315) ---
    // Mismo razonamiento que Knockback arriba: existen aquí por completitud del struct,
    // pero el camino real de propagación en Panthelia es el SetByCaller
    // (CombatTricks_Launch* en ApplyDamageScalingToSpec) + escritura directa en el
    // context desde PantheliaProjectile.cpp/WeaponTraceComponent.cpp.
    UPROPERTY()
    float LaunchForceMagnitude = 0.f;

    UPROPERTY()
    FVector LaunchForce = FVector::ZeroVector;

    UPROPERTY()
    float LaunchChance = 0.f;

    // Nivel 2 de knockback ("empujón fuerte"). Igual que el resto de campos de este
    // struct para daño secundario: existe por completitud, no es el camino real de
    // propagación en Panthelia (eso es el SetByCaller CombatTricks_KnockbackIsHeavy).
    UPROPERTY()
    bool bKnockbackIsHeavy = false;
};

// ============================================================
// FPantheliaGameplayEffectContext
// ============================================================
//
// Subclase del FGameplayEffectContext de GAS que añade campos
// custom necesarios para el sistema de daño de Panthelia.
//
// ¿Por qué subclasear?
// FGameplayEffectContext no tiene campos para información de combate
// como si el golpe fue crítico. Sin este custom context, el ExecCalc
// no tiene forma de comunicarle a PostGameplayEffectExecute si el
// golpe fue crítico — los dos sistemas no se comunican directamente.
//
// Flujo de uso:
//   1. ExecCalc_Damage calcula bIsCriticalHit
//   2. Lo escribe en FPantheliaGameplayEffectContext
//   3. PostGameplayEffectExecute lo lee y dispara VFX/SFX de crítico
//
// Requerimientos de GAS para un context custom:
//   - Sobrescribir GetScriptStruct() (identificador para reflexión)
//   - Sobrescribir Duplicate() devolviendo el tipo derivado (UE 5.3+)
//   - Sobrescribir NetSerialize() (serialización a red/disco)
//   - Declarar TStructOpsTypeTraits con WithNetSerializer y WithCopy
// ============================================================
USTRUCT(BlueprintType)
struct FPantheliaGameplayEffectContext : public FGameplayEffectContext
{
    GENERATED_BODY()

public:

    // Getters — usados en PostGameplayEffectExecute para leer el resultado del golpe
    bool IsCriticalHit() const { return bIsCriticalHit; }

    // Resultado del parry/bloqueo (lo escribe el ExecCalc, lo lee PostGameplayEffectExecute).
    bool WasParried() const { return bWasParried; }
    bool WasBlocked() const { return bWasBlocked; }
    float GetParryPoiseDamageToAttacker() const { return ParryPoiseDamageToAttacker; }

    // --- Resultado del debuff (clase 307-308) ---
    // Lo escribe ExecCalc_Damage (DetermineDebuff decide bIsSuccessfulDebuff; los 4
    // parámetros ya venían en el spec desde la clase 306). PostGameplayEffectExecute
    // (o el AttributeSet) los lee de aquí para decidir si aplicar el debuff realmente.
    bool IsSuccessfulDebuff() const { return bIsSuccessfulDebuff; }
    float GetDebuffDamage() const { return DebuffDamage; }
    float GetDebuffDuration() const { return DebuffDuration; }
    float GetDebuffFrequency() const { return DebuffFrequency; }

    // Tipo de daño (elemental) que causó el debuff. TSharedPtr en vez de FGameplayTag
    // simple: mismo patrón que usa HitResult en la clase base FGameplayEffectContext
    // (ver el include GameplayEffectTypes.h). Puede ser inválido/nulo si nunca se asignó
    // — por eso este getter devuelve el puntero crudo, sin desreferenciar; quien lo lea
    // debe comprobar IsValid() primero (la librería lo hace por ti — ver
    // UPantheliaAbilitySystemLibrary::GetDamageType).
    TSharedPtr<FGameplayTag> GetDamageType() const { return DamageType; }

    // Vector de impulso a aplicar al ragdoll si este golpe resulta fatal (clase 313).
    // A diferencia de DamageType, NO es un puntero — un FVector no necesita el mecanismo
    // de TSharedPtr porque no hay ambigüedad "nunca se asignó" vs "vacío": simplemente
    // se inicializa a FVector::ZeroVector, y un impulso cero es indistinguible en la
    // práctica de "no hay impulso" (aplicar un impulso de longitud cero no hace nada).
    FVector GetDeathImpulse() const { return DeathImpulse; }

    // Vector de fuerza de knockback a aplicar si el golpe NO resulta fatal (clase 315).
    // Mismo razonamiento que DeathImpulse: FVector simple, cero = "sin knockback".
    // La diferencia semántica con DeathImpulse no es el TIPO de dato, es CUÁNDO se usa
    // (HandleIncomingDamage los lee en ramas opuestas: bFatal vs. no bFatal).
    FVector GetKnockbackForce() const { return KnockbackForce; }

    // Vector de lanzamiento aéreo (post-315, Nivel 3). Independiente de KnockbackForce
    // — ver la nota de diseño completa en FPantheliaGameplayTags sobre por qué Knockback
    // y Launch NO comparten campo aunque ambos terminen en un LaunchCharacter.
    FVector GetLaunchForce() const { return LaunchForce; }

    // Nivel 2 de knockback ("empujón fuerte", a petición): true si ESTE knockback debe
    // bloquear GA_HitReact y disparar GA_HeavyKnockback en vez del comportamiento
    // normal (que convive con HitReact). Bool simple, no TSharedPtr — mismo motivo que
    // DeathImpulse: no hay ambigüedad "nunca asignado" vs "vacío" (false por defecto ya
    // significa "knockback normal", que es el caso común).
    bool IsKnockbackHeavy() const { return bKnockbackIsHeavy; }

    // Setters — usados en ExecCalc_Damage para escribir el resultado del golpe
    void SetIsCriticalHit(bool bInIsCriticalHit) { bIsCriticalHit = bInIsCriticalHit; }

    // Marca que hubo un PARRY perfecto del tipo correcto (fisico vs fisico o magico vs
    // magico). PostGameplayEffectExecute lo usa para aplicar dano de postura al atacante
    // y disparar los efectos elementales (gancho). PoiseToAttacker = cuanto aplicar.
    void SetParryResult(bool bInParried, float InPoiseToAttacker)
    {
        bWasParried = bInParried;
        ParryPoiseDamageToAttacker = InPoiseToAttacker;
    }

    // Marca que hubo un BLOQUEO (imperfecto). Sin dano de postura al atacante.
    void SetWasBlocked(bool bInBlocked) { bWasBlocked = bInBlocked; }

    // --- Setters del resultado del debuff (clase 307-308) ---
    // Los llama ExecCalc_Damage directamente via static_cast, igual que SetIsCriticalHit
    // y SetParryResult — no necesitan wrapper en la librería porque solo el ExecCalc
    // escribe estos valores (la librería solo expone LECTURA, para PostGameplayEffectExecute
    // y el AttributeSet, que sí pueden necesitar leerlos sin conocer el tipo concreto del
    // context).
    void SetIsSuccessfulDebuff(bool bInSuccessfulDebuff) { bIsSuccessfulDebuff = bInSuccessfulDebuff; }
    void SetDebuffDamage(float InDebuffDamage) { DebuffDamage = InDebuffDamage; }
    void SetDebuffDuration(float InDebuffDuration) { DebuffDuration = InDebuffDuration; }
    void SetDebuffFrequency(float InDebuffFrequency) { DebuffFrequency = InDebuffFrequency; }

    // Asigna el tipo de daño que causó el debuff.
    // CORRECCIÓN (clase 309): en la clase 308 este setter recibía un FGameplayTag y
    // creaba el TSharedPtr internamente (era mi inferencia, marcada como tal — la
    // transcripción de esa clase no dictaba este cuerpo). La clase 309 revela la firma
    // real que se usa: este setter recibe el TSharedPtr YA CREADO y solo lo asigna; el
    // MakeShared vive en el llamador (UPantheliaAbilitySystemLibrary::SetDamageType, ver
    // ese .cpp). Se corrige aquí para que ambos lados coincidan.
    void SetDamageType(TSharedPtr<FGameplayTag> InDamageType)
    {
        DamageType = InDamageType;
    }

    // Asigna el vector de impulso de muerte (clase 313).
    void SetDeathImpulse(const FVector& InImpulse)
    {
        DeathImpulse = InImpulse;
    }

    // Asigna el vector de fuerza de knockback (clase 315).
    void SetKnockbackForce(const FVector& InForce)
    {
        KnockbackForce = InForce;
    }

    // Asigna el vector de lanzamiento aéreo (post-315, Nivel 3).
    void SetLaunchForce(const FVector& InForce)
    {
        LaunchForce = InForce;
    }

    // Asigna si este knockback es "pesado" (Nivel 2, a petición).
    void SetKnockbackIsHeavy(bool bInHeavy)
    {
        bKnockbackIsHeavy = bInHeavy;
    }

    // GAS requiere que las subclases sobreescriban GetScriptStruct().
    // Devuelve el UScriptStruct de reflexión de este tipo (equivalente
    // al StaticClass() de los UObjects, pero para structs).
    // NOTA UE 5.3+: NO calificar el nombre — usar StaticStruct() directamente,
    // no FPantheliaGameplayEffectContext::StaticStruct(). Evita errores de serialización.
    virtual UScriptStruct* GetScriptStruct() const override
    {
        return StaticStruct();
    }

    // Crea una copia de este context.
    // NOTA UE 5.3+: El tipo de retorno DEBE ser el tipo derivado
    // (FPantheliaGameplayEffectContext*), no el base (FGameplayEffectContext*).
    // Esto es un covariant return type — válido en C++ al sobreescribir con
    // un puntero a clase derivada. Sin esto UE 5.7 da error al serializar.
    virtual FPantheliaGameplayEffectContext* Duplicate() const override
    {
        FPantheliaGameplayEffectContext* NewContext = new FPantheliaGameplayEffectContext();
        *NewContext = *this;
        return NewContext;
    }

    // NetSerialize especifica cómo este struct se serializa para guardado
    // o replicación por red. GAS lo requiere en todas las subclases.
    // Implementación completa en PantheliaAbilityTypes.cpp.
    virtual bool NetSerialize(FArchive& Ar, class UPackageMap* Map, bool& bOutSuccess) override;

protected:

    // True si el golpe fue crítico (multiplicador 1.5x + CritDamage flat).
    // Calculado en ExecCalc_Damage, leído en PostGameplayEffectExecute
    // para disparar VFX y SFX distintos en golpes críticos.
    UPROPERTY()
    bool bIsCriticalHit = false;

    // True si el defensor hizo un PARRY perfecto del tipo correcto (anula dano y daña
    // la postura del atacante). Lo escribe el ExecCalc, lo lee PostGameplayEffectExecute.
    UPROPERTY()
    bool bWasParried = false;

    // True si el defensor BLOQUEO (imperfecto). Mitiga dano pero no daña postura enemiga.
    UPROPERTY()
    bool bWasBlocked = false;

    // Dano de postura a aplicar AL ATACANTE en un parry perfecto. 0 si no hubo parry.
    UPROPERTY()
    float ParryPoiseDamageToAttacker = 0.f;

    // --- Resultado del debuff (clase 307-308) ---
    // True si DetermineDebuff (ExecCalc_Damage) confirmó un debuff exitoso en este golpe.
    UPROPERTY()
    bool bIsSuccessfulDebuff = false;

    // Los 3 parámetros que necesita el debuff exitoso para aplicarse (el 4to, Chance,
    // ya cumplió su función en la tirada de dado — no hace falta transportarlo más allá).
    UPROPERTY()
    float DebuffDamage = 0.f;

    UPROPERTY()
    float DebuffDuration = 0.f;

    UPROPERTY()
    float DebuffFrequency = 0.f;

    // Tipo de daño (elemental) que causó el debuff. TSharedPtr, NO UPROPERTY: los shared
    // pointers no los rastrea el garbage collector de UPROPERTY — TSharedPtr gestiona su
    // propia memoria automáticamente (ref-counting), por eso NO se le pone UPROPERTY()
    // como a los campos de arriba. Empieza sin asignar (nullptr implícito de TSharedPtr);
    // NetSerialize lo crea sobre la marcha si hace falta (ver el .cpp).
    TSharedPtr<FGameplayTag> DamageType;

    // Vector de impulso a aplicar al ragdoll si este golpe resulta fatal (clase 313).
    // Se inicializa a ZeroVector: mientras nadie lo asigne, un impulso de longitud cero
    // no hace nada si algo llegara a leerlo antes de tiempo — no hace falta la
    // distinción "nunca asignado" vs "vacío" que sí necesitaba DamageType (por eso este
    // campo SÍ es UPROPERTY() normal, no un TSharedPtr).
    UPROPERTY()
    FVector DeathImpulse = FVector::ZeroVector;

    // Vector de fuerza de knockback a aplicar si el golpe NO resulta fatal (clase 315).
    // Mismo patrón que DeathImpulse (UPROPERTY normal, ZeroVector por defecto).
    UPROPERTY()
    FVector KnockbackForce = FVector::ZeroVector;

    // Vector de lanzamiento aéreo (post-315, Nivel 3). Campo separado de KnockbackForce
    // a propósito — ver la nota de diseño en FPantheliaGameplayTags.
    UPROPERTY()
    FVector LaunchForce = FVector::ZeroVector;

    // Nivel 2 de knockback ("empujón fuerte", a petición). false = knockback normal
    // (convive con HitReact); true = knockback pesado (bloquea HitReact, dispara
    // GA_HeavyKnockback).
    UPROPERTY()
    bool bKnockbackIsHeavy = false;
};

// ============================================================
// TStructOpsTypeTraits
// ============================================================
//
// Especialización de plantilla que le dice al sistema de reflexión
// de Unreal qué capacidades tiene FPantheliaGameplayEffectContext.
//
// WithNetSerializer = true → el struct tiene NetSerialize() y puede
//   ser serializado para red o guardado a disco.
// WithCopy = true → el struct puede copiarse via operador de asignación
//   (necesario para Duplicate()).
//
// Sin esta declaración GAS no sabría que puede serializar este struct
// y fallaría silenciosamente al intentar replicarlo.
// ============================================================
template<>
struct TStructOpsTypeTraits<FPantheliaGameplayEffectContext>
    : public TStructOpsTypeTraitsBase2<FPantheliaGameplayEffectContext>
{
    enum
    {
        WithNetSerializer = true,
        WithCopy = true
    };
};
