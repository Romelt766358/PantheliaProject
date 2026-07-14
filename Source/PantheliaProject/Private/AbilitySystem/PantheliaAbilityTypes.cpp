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
    //   bit 13 — bKnockbackIsHeavy (bool, solo si true)
    //   bit 14 — DodgeResponse (2 bits, solo si difiere de AvoidableNoReward)
    //   bit 15 — HitOutcome (3 bits, solo si difiere de Unresolved)
    //   bit 16 — DefenseAttackType (2 bits, solo si difiere de Normal)
    //   bit 17 — bWasGuardBroken (bool, solo si true)
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

        // --- Nivel 2 de knockback ---
        if (bKnockbackIsHeavy)
        {
            RepBits |= 1 << 13;
        }

        // AvoidableNoReward es el default seguro del context. Solo transportamos
        // el enum cuando un productor explícito eligió Dodgeable o Unavoidable.
        if (DodgeResponse != EPantheliaDodgeResponse::AvoidableNoReward)
        {
            RepBits |= 1 << 14;
        }

        if (HitOutcome != EPantheliaHitOutcome::Unresolved)
        {
            RepBits |= 1 << 15;
        }

        if (DefenseAttackType != EPantheliaDefenseAttackType::Normal)
        {
            RepBits |= 1 << 16;
        }

        if (bWasGuardBroken)
        {
            RepBits |= 1 << 17;
        }
    }

    // 18 bits usados en total. Los bits 14-17 cubren las reglas defensivas
    // añadidas al context sin alterar los campos base anteriores.
    Ar.SerializeBits(&RepBits, 18);

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

    // --- Nivel 2 de knockback ---
    if (RepBits & (1 << 13))
    {
        Ar << bKnockbackIsHeavy;
    }

    // DodgeResponse tiene tres valores, por lo que dos bits son suficientes.
    // Ante un valor corrupto/desconocido caemos al default seguro: se puede
    // evitar con i-frames, pero no concede perfecto.
    if (RepBits & (1 << 14))
    {
        uint8 DodgeResponseValue = static_cast<uint8>(DodgeResponse);
        Ar.SerializeBits(&DodgeResponseValue, 2);

        if (Ar.IsLoading())
        {
            switch (DodgeResponseValue)
            {
                case static_cast<uint8>(EPantheliaDodgeResponse::Dodgeable):
                    DodgeResponse = EPantheliaDodgeResponse::Dodgeable;
                    break;

                case static_cast<uint8>(EPantheliaDodgeResponse::Unavoidable):
                    DodgeResponse = EPantheliaDodgeResponse::Unavoidable;
                    break;

                case static_cast<uint8>(EPantheliaDodgeResponse::AvoidableNoReward):
                default:
                    DodgeResponse = EPantheliaDodgeResponse::AvoidableNoReward;
                    break;
            }
        }
    }
    else if (Ar.IsLoading())
    {
        DodgeResponse = EPantheliaDodgeResponse::AvoidableNoReward;
    }

    // HitOutcome tiene cinco valores; tres bits dejan margen para ampliar el enum sin
    // cambiar el formato inmediatamente. Ante un valor desconocido volvemos a Unresolved.
    if (RepBits & (1 << 15))
    {
        uint8 HitOutcomeValue = static_cast<uint8>(HitOutcome);
        Ar.SerializeBits(&HitOutcomeValue, 3);

        if (Ar.IsLoading())
        {
            switch (HitOutcomeValue)
            {
                case static_cast<uint8>(EPantheliaHitOutcome::Accepted):
                    HitOutcome = EPantheliaHitOutcome::Accepted;
                    break;

                case static_cast<uint8>(EPantheliaHitOutcome::NegatedInvulnerability):
                    HitOutcome = EPantheliaHitOutcome::NegatedInvulnerability;
                    break;

                case static_cast<uint8>(EPantheliaHitOutcome::NegatedPerfectParry):
                    HitOutcome = EPantheliaHitOutcome::NegatedPerfectParry;
                    break;

                case static_cast<uint8>(EPantheliaHitOutcome::MitigatedBlock):
                    HitOutcome = EPantheliaHitOutcome::MitigatedBlock;
                    break;

                case static_cast<uint8>(EPantheliaHitOutcome::Unresolved):
                default:
                    HitOutcome = EPantheliaHitOutcome::Unresolved;
                    break;
            }
        }
    }
    else if (Ar.IsLoading())
    {
        HitOutcome = EPantheliaHitOutcome::Unresolved;
    }

    // DefenseAttackType tiene tres valores; dos bits son suficientes.
    if (RepBits & (1 << 16))
    {
        uint8 DefenseAttackTypeValue = static_cast<uint8>(DefenseAttackType);
        Ar.SerializeBits(&DefenseAttackTypeValue, 2);

        if (Ar.IsLoading())
        {
            switch (DefenseAttackTypeValue)
            {
                case static_cast<uint8>(EPantheliaDefenseAttackType::Heavy):
                    DefenseAttackType = EPantheliaDefenseAttackType::Heavy;
                    break;

                case static_cast<uint8>(EPantheliaDefenseAttackType::Fury):
                    DefenseAttackType = EPantheliaDefenseAttackType::Fury;
                    break;

                case static_cast<uint8>(EPantheliaDefenseAttackType::Normal):
                default:
                    DefenseAttackType = EPantheliaDefenseAttackType::Normal;
                    break;
            }
        }
    }
    else if (Ar.IsLoading())
    {
        DefenseAttackType = EPantheliaDefenseAttackType::Normal;
    }

    if (RepBits & (1 << 17))
    {
        Ar << bWasGuardBroken;
    }
    else if (Ar.IsLoading())
    {
        bWasGuardBroken = false;
    }

    // Al cargar, reconstruir InstigatorAbilitySystemComponent desde el Instigator.
    if (Ar.IsLoading())
    {
        AddInstigator(Instigator.Get(), EffectCauser.Get());
    }

    bOutSuccess = true;
    return true;
}
