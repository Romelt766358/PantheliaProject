// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/PantheliaAbilityTypes.h"

bool FPantheliaGameplayEffectContext::NetSerialize(FArchive& Ar, UPackageMap* Map, bool& bOutSuccess)
{
    // ----------------------------------------------------------------
    // NetSerialize simplificado para Panthelia (single player).
    //
    // Serializamos solo los campos que realmente usamos en el juego.
    // Los campos Actors, HitResult y WorldOrigin fueron omitidos porque:
    //   - No los usamos en nuestro pipeline de daño
    //   - Sus funciones helper del engine (SafeNetSerializeArray_Default,
    //     MAX_NUM_ACTORS_PER_ACTORCUE) no son accesibles en UE 5.7
    //
    // Campos serializados:
    //   bit 0 — Instigator    (propietario del ASC, ej: PlayerState)
    //   bit 1 — EffectCauser  (avatar actor, ej: el personaje)
    //   bit 2 — AbilityCDO    (la ability que causó el efecto)
    //   bit 3 — SourceObject  (objeto fuente, ej: el proyectil)
    //   bit 4 — bIsCriticalHit (nuestro campo custom)
    //   bit 5 — bIsSuccessfulDebuff (clase 307-308)
    //   bit 6 — DebuffDamage        (solo si > 0)
    //   bit 7 — DebuffDuration      (solo si > 0)
    //   bit 8 — DebuffFrequency     (solo si > 0)
    //   bit 9 — DamageType (FGameplayTag vía TSharedPtr, solo si está asignado)
    //   bit 10 — DeathImpulse (FVector, solo si no es el vector cero)
    //   bit 11 — KnockbackForce (FVector, solo si no es el vector cero)
    //   bit 12 — LaunchForce (FVector, solo si no es el vector cero)
    // ----------------------------------------------------------------

    uint32 RepBits = 0;

    if (Ar.IsSaving())
    {
        if (bReplicateInstigator && Instigator.IsValid())
        {
            RepBits |= 1 << 0;
        }
        if (bReplicateEffectCauser && EffectCauser.IsValid())
        {
            RepBits |= 1 << 1;
        }
        if (AbilityCDO.IsValid())
        {
            RepBits |= 1 << 2;
        }
        if (bReplicateSourceObject && SourceObject.IsValid())
        {
            RepBits |= 1 << 3;
        }
        if (bIsCriticalHit)
        {
            RepBits |= 1 << 4;
        }

        // --- Debuff (clase 307-308) ---
        // Solo replicamos si el debuff fue exitoso, y solo los parámetros no-cero —
        // igual optimización de ancho de banda que ya usa el resto de esta función.
        if (bIsSuccessfulDebuff)
        {
            RepBits |= 1 << 5;
        }
        if (DebuffDamage > 0.f)
        {
            RepBits |= 1 << 6;
        }
        if (DebuffDuration > 0.f)
        {
            RepBits |= 1 << 7;
        }
        if (DebuffFrequency > 0.f)
        {
            RepBits |= 1 << 8;
        }
        if (DamageType.IsValid())
        {
            RepBits |= 1 << 9;
        }

        // --- Impulso de muerte (clase 313) ---
        // FVector::IsZero() comprueba los 3 componentes contra 0 — si nunca se asignó
        // (sigue en su valor inicial FVector::ZeroVector), no tiene sentido replicarlo.
        if (!DeathImpulse.IsZero())
        {
            RepBits |= 1 << 10;
        }

        // --- Knockback (clase 315) ---
        if (!KnockbackForce.IsZero())
        {
            RepBits |= 1 << 11;
        }

        // --- Launch (post-315, Nivel 3) ---
        if (!LaunchForce.IsZero())
        {
            RepBits |= 1 << 12;
        }
    }

    // 13 bits usados en total (antes 12 — Nivel 3 añade LaunchForce).
    Ar.SerializeBits(&RepBits, 13);

    if (RepBits & (1 << 0))
    {
        Ar << Instigator;
    }
    if (RepBits & (1 << 1))
    {
        Ar << EffectCauser;
    }
    if (RepBits & (1 << 2))
    {
        Ar << AbilityCDO;
        Ar << AbilityLevel;
    }
    if (RepBits & (1 << 3))
    {
        Ar << SourceObject;
    }
    if (RepBits & (1 << 4))
    {
        Ar << bIsCriticalHit;
    }

    // --- Debuff (clase 307-308) ---
    if (RepBits & (1 << 5))
    {
        Ar << bIsSuccessfulDebuff;
    }
    if (RepBits & (1 << 6))
    {
        Ar << DebuffDamage;
    }
    if (RepBits & (1 << 7))
    {
        Ar << DebuffDuration;
    }
    if (RepBits & (1 << 8))
    {
        Ar << DebuffFrequency;
    }
    if (RepBits & (1 << 9))
    {
        // DamageType es un TSharedPtr, no un valor simple — mismo patrón que usa el
        // engine para serializar HitResult (pista de la clase original). Si estamos
        // cargando (leyendo) y todavía no existe el puntero, hay que crearlo primero
        // (MakeShareable) antes de poder escribirle algo dentro con NetSerialize.
        // Si estamos guardando, el puntero ya existe (por eso este bit se flippeó arriba).
        if (Ar.IsLoading())
        {
            if (!DamageType.IsValid())
            {
                DamageType = MakeShareable(new FGameplayTag());
            }
        }
        DamageType->NetSerialize(Ar, Map, bOutSuccess);
    }

    // --- Impulso de muerte (clase 313) ---
    // A diferencia de DamageType (un FGameplayTag, que necesitó todo el mecanismo de
    // TSharedPtr), un FVector NO necesita nada especial: ya tiene su propio NetSerialize
    // incorporado (heredado de su naturaleza como tipo "pequeño" del motor), así que
    // simplemente lo tratamos como cualquier otro tipo con NetSerialize propio — sin
    // punteros, sin comprobar IsValid(), sin crear nada al cargar.
    if (RepBits & (1 << 10))
    {
        DeathImpulse.NetSerialize(Ar, Map, bOutSuccess);
    }

    // --- Knockback (clase 315) ---
    if (RepBits & (1 << 11))
    {
        KnockbackForce.NetSerialize(Ar, Map, bOutSuccess);
    }

    // --- Launch (post-315, Nivel 3) ---
    if (RepBits & (1 << 12))
    {
        LaunchForce.NetSerialize(Ar, Map, bOutSuccess);
    }

    // Al cargar, reconstruir InstigatorAbilitySystemComponent desde el Instigator.
    if (Ar.IsLoading())
    {
        AddInstigator(Instigator.Get(), EffectCauser.Get());
    }

    bOutSuccess = true;
    return true;
}