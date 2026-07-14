// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/ExecCalc/ExecCalc_Damage.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/PantheliaAttributeSet.h"
#include "AbilitySystem/PantheliaAbilitySystemLibrary.h"
#include "AbilitySystem/Data/PantheliaCharacterClassInfo.h"
#include "Interfaces/CombatInterface.h"
#include "PantheliaGameplayTags.h"
#include "PantheliaElementTypes.h"
#include "PantheliaLogChannels.h"
#include "Combat/PantheliaEquipmentComponent.h"
#include "Combat/PantheliaWeaponDefinition.h"
#include "AbilitySystem/PantheliaAbilityTypes.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Abilities/GameplayAbilityTypes.h"

// ============================================================
// PantheliaDamageStatics
// ============================================================
// NOTA ASIMETRÍA (spec §1.7, modelo LoL):
//   PhysicalDamage (AD): capturado del SOURCE → sumado como addend genérico al daño físico.
//   MagicDamage    (AP): NO se suma automáticamente dentro del ExecCalc. El daño directo
//                        lo usa mediante AttributeScalings de la ability; los estados
//                        globales lo leen al detonar desde PantheliaAttributeSet.

struct PantheliaDamageStatics
{
    DECLARE_ATTRIBUTE_CAPTUREDEF(Armor);
    DECLARE_ATTRIBUTE_CAPTUREDEF(MagicResistance);
    DECLARE_ATTRIBUTE_CAPTUREDEF(CritChance);
    DECLARE_ATTRIBUTE_CAPTUREDEF(CritDamage);
    DECLARE_ATTRIBUTE_CAPTUREDEF(ArmorPenetration);
    DECLARE_ATTRIBUTE_CAPTUREDEF(MagicPenetration);
    DECLARE_ATTRIBUTE_CAPTUREDEF(Stamina);

    // Sumando genérico de daño físico (modelo AD de LoL).
    // Se añade ANTES de la mitigación de Armor a TODOS los tipos físicos.
    DECLARE_ATTRIBUTE_CAPTUREDEF(PhysicalDamage);

    // Resistencias elementales del TARGET
    DECLARE_ATTRIBUTE_CAPTUREDEF(FireResistance);
    DECLARE_ATTRIBUTE_CAPTUREDEF(WaterResistance);
    DECLARE_ATTRIBUTE_CAPTUREDEF(StormResistance);
    DECLARE_ATTRIBUTE_CAPTUREDEF(NatureResistance);

    PantheliaDamageStatics()
    {
        DEFINE_ATTRIBUTE_CAPTUREDEF(UPantheliaAttributeSet, Armor, Target, false);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UPantheliaAttributeSet, MagicResistance, Target, false);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UPantheliaAttributeSet, CritChance, Source, false);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UPantheliaAttributeSet, CritDamage, Source, false);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UPantheliaAttributeSet, ArmorPenetration, Source, false);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UPantheliaAttributeSet, MagicPenetration, Source, false);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UPantheliaAttributeSet, Stamina, Target, false);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UPantheliaAttributeSet, PhysicalDamage, Source, false);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UPantheliaAttributeSet, FireResistance, Target, false);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UPantheliaAttributeSet, WaterResistance, Target, false);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UPantheliaAttributeSet, StormResistance, Target, false);
        DEFINE_ATTRIBUTE_CAPTUREDEF(UPantheliaAttributeSet, NatureResistance, Target, false);
    }
};

static const PantheliaDamageStatics& DamageStatics()
{
    static PantheliaDamageStatics Statics;
    return Statics;
}

// ============================================================
// Tabla de afinidades (spec cerrada, §1.4)
// ============================================================

static float GetTypeChartMultiplier(EPantheliaElement AttackerElement, EPantheliaElement DefenderElement)
{
    if (AttackerElement == EPantheliaElement::None || DefenderElement == EPantheliaElement::None) return 1.f;
    if (AttackerElement == DefenderElement) return 1.f;

    if (AttackerElement == EPantheliaElement::Fire && DefenderElement == EPantheliaElement::Storm)  return 1.15f;
    if (AttackerElement == EPantheliaElement::Storm && DefenderElement == EPantheliaElement::Water)  return 1.15f;
    if (AttackerElement == EPantheliaElement::Water && DefenderElement == EPantheliaElement::Fire)   return 1.15f;
    if (AttackerElement == EPantheliaElement::Fire && DefenderElement == EPantheliaElement::Water)  return 0.85f;
    if (AttackerElement == EPantheliaElement::Storm && DefenderElement == EPantheliaElement::Fire)   return 0.85f;
    if (AttackerElement == EPantheliaElement::Water && DefenderElement == EPantheliaElement::Storm)  return 0.85f;
    if (DefenderElement == EPantheliaElement::Nature && AttackerElement == EPantheliaElement::Storm) return 0.85f;

    return 1.f;
}

// ============================================================

UExecCalc_Damage::UExecCalc_Damage()
{
    RelevantAttributesToCapture.Add(DamageStatics().ArmorDef);
    RelevantAttributesToCapture.Add(DamageStatics().MagicResistanceDef);
    RelevantAttributesToCapture.Add(DamageStatics().CritChanceDef);
    RelevantAttributesToCapture.Add(DamageStatics().CritDamageDef);
    RelevantAttributesToCapture.Add(DamageStatics().ArmorPenetrationDef);
    RelevantAttributesToCapture.Add(DamageStatics().MagicPenetrationDef);
    RelevantAttributesToCapture.Add(DamageStatics().StaminaDef);
    RelevantAttributesToCapture.Add(DamageStatics().PhysicalDamageDef);
    RelevantAttributesToCapture.Add(DamageStatics().FireResistanceDef);
    RelevantAttributesToCapture.Add(DamageStatics().WaterResistanceDef);
    RelevantAttributesToCapture.Add(DamageStatics().StormResistanceDef);
    RelevantAttributesToCapture.Add(DamageStatics().NatureResistanceDef);
}

// ================================================================
// Helper: mitigacion de Parry / Bloqueo (modelo Lies of P)
// ================================================================
// Lee el estado defensivo del defensor (TargetTags) y aplica la matriz de mitigacion
// a los subtotales fisico/magico (modificados por referencia). Los multiplicadores
// salen del arma equipada del defensor (WeaponDefinition). Si no hay arma o no hay
// estado defensivo, no toca nada (dano completo).
//
// Comunica el resultado por referencia:
//   bOutWasPerfectParry  -> hubo parry perfecto del tipo correcto (daña postura enemiga).
//   bOutWasBlocked       -> hubo bloqueo o parry cruzado (mitiga, sin postura enemiga).
//   OutPoiseToAttacker   -> dano de postura a aplicar al atacante (solo en parry correcto).
//
// bIncomingIsPhysical / bIncomingIsMagic: que tipos de dano trae el golpe entrante.
static void ApplyParryBlockMitigation(
    AActor* TargetAvatar,
    const FGameplayTagContainer& TargetTags,
    const FPantheliaGameplayTags& Tags,
    EPantheliaDefenseAttackType DefenseAttackType,
    float CurrentStamina,
    bool bIncomingIsPhysical,
    bool bIncomingIsMagic,
    float& PhysicalSubtotal,
    float& MagicSubtotal,
    bool& bOutWasPerfectParry,
    bool& bOutWasBlocked,
    bool& bOutWasGuardBroken,
    float& OutPoiseToAttacker,
    float& OutDefenseStaminaCost)
{
    bOutWasPerfectParry = false;
    bOutWasBlocked = false;
    bOutWasGuardBroken = false;
    OutPoiseToAttacker = 0.f;
    OutDefenseStaminaCost = 0.f;

    // Detectar el estado defensivo activo (solo uno deberia estar activo a la vez).
    const bool bParryPhysical = TargetTags.HasTagExact(Tags.State_Parry_Physical);
    const bool bParryMagic = TargetTags.HasTagExact(Tags.State_Parry_Magic);
    const bool bBlockPhysical = TargetTags.HasTagExact(Tags.State_Block_Physical);
    const bool bBlockMagic = TargetTags.HasTagExact(Tags.State_Block_Magic);

    // Sin estado defensivo: dano completo, no hacemos nada.
    if (!bParryPhysical && !bParryMagic && !bBlockPhysical && !bBlockMagic)
    {
        return;
    }

    // Leer los multiplicadores, costes y daño de postura del arma equipada del defensor.
    // Si no hay arma, usamos defaults seguros que coinciden con la definición base.
    float PhysImperfect = 0.6f;
    float MagicOnPhysParry = 0.8f;
    float MagicImperfect = 0.5f;
    float PhysOnMagicParry = 0.7f;
    float PhysParryPoise = 30.f;
    float MagicParryPoise = 12.f;
    float BaseBlockStaminaCost = 10.f;
    float HeavyBlockStaminaMultiplier = 4.f;
    float PerfectParryStaminaMultiplier = 0.5f;

    if (TargetAvatar)
    {
        if (UPantheliaEquipmentComponent* Equip =
            TargetAvatar->FindComponentByClass<UPantheliaEquipmentComponent>())
        {
            if (const UPantheliaWeaponDefinition* WeaponDef =
                Equip->GetEquippedWeaponDefinition())
            {
                PhysImperfect = WeaponDef->PhysicalImperfectBlockMultiplier;
                MagicOnPhysParry = WeaponDef->MagicOnPhysicalParryMultiplier;
                MagicImperfect = WeaponDef->MagicImperfectBlockMultiplier;
                PhysOnMagicParry = WeaponDef->PhysicalOnMagicParryMultiplier;
                PhysParryPoise = WeaponDef->PhysicalParryPoiseDamage;
                MagicParryPoise = WeaponDef->MagicParryPoiseDamage;
                BaseBlockStaminaCost = WeaponDef->BlockStaminaCost;
                HeavyBlockStaminaMultiplier =
                    WeaponDef->HeavyBlockStaminaCostMultiplier;
                PerfectParryStaminaMultiplier =
                    WeaponDef->PerfectParryStaminaCostMultiplier;
            }
        }
    }

    const bool bIsFury = DefenseAttackType == EPantheliaDefenseAttackType::Fury;

    // --- PARRY FISICO (perfecto) ---
    if (bParryPhysical)
    {
        // Un parry físico solo es perfecto si el golpe contiene componente físico.
        // Una vez reconocido como perfecto, anula TODO el paquete ofensivo, también Fury.
        if (bIncomingIsPhysical)
        {
            PhysicalSubtotal = 0.f;
            MagicSubtotal = 0.f;
            bOutWasPerfectParry = true;
            OutPoiseToAttacker = PhysParryPoise;
        }
        else if (!bIsFury)
        {
            // La mitigación cruzada cuenta como bloqueo imperfecto en ataques Normal/Heavy.
            // Fury no admite bloqueo ni parry cruzado: atraviesa la guardia con daño completo.
            MagicSubtotal *= MagicOnPhysParry;
            bOutWasBlocked = true;
        }

        UE_LOG(LogTemp, Warning,
            TEXT("[Parry] MITIGACION Parry Fisico: Tipo=%d Perfecto=%s Bloqueado=%s | Fis=%.1f Mag=%.1f | Postura enemigo=%.1f"),
            static_cast<int32>(DefenseAttackType),
            bOutWasPerfectParry ? TEXT("true") : TEXT("false"),
            bOutWasBlocked ? TEXT("true") : TEXT("false"),
            PhysicalSubtotal, MagicSubtotal, OutPoiseToAttacker);
    }
    // --- PARRY MAGICO (perfecto) ---
    else if (bParryMagic)
    {
        if (bIncomingIsMagic)
        {
            PhysicalSubtotal = 0.f;
            MagicSubtotal = 0.f;
            bOutWasPerfectParry = true;
            OutPoiseToAttacker = MagicParryPoise;
        }
        else if (!bIsFury)
        {
            PhysicalSubtotal *= PhysOnMagicParry;
            bOutWasBlocked = true;
        }

        UE_LOG(LogTemp, Warning,
            TEXT("[Parry] MITIGACION Parry Magico: Tipo=%d Perfecto=%s Bloqueado=%s | Fis=%.1f Mag=%.1f | Postura enemigo=%.1f"),
            static_cast<int32>(DefenseAttackType),
            bOutWasPerfectParry ? TEXT("true") : TEXT("false"),
            bOutWasBlocked ? TEXT("true") : TEXT("false"),
            PhysicalSubtotal, MagicSubtotal, OutPoiseToAttacker);
    }
    // --- BLOQUEO FISICO IMPERFECTO ---
    else if (bBlockPhysical && !bIsFury)
    {
        PhysicalSubtotal *= PhysImperfect;
        MagicSubtotal *= MagicOnPhysParry;
        bOutWasBlocked = true;

        UE_LOG(LogTemp, Warning,
            TEXT("[Parry] MITIGACION Bloqueo Fisico: Tipo=%d Fis*%.2f Mag*%.2f"),
            static_cast<int32>(DefenseAttackType),
            PhysImperfect,
            MagicOnPhysParry);
    }
    // --- BLOQUEO MAGICO IMPERFECTO ---
    else if (bBlockMagic && !bIsFury)
    {
        MagicSubtotal *= MagicImperfect;
        PhysicalSubtotal *= PhysOnMagicParry;
        bOutWasBlocked = true;

        UE_LOG(LogTemp, Warning,
            TEXT("[Parry] MITIGACION Bloqueo Magico: Tipo=%d Mag*%.2f Fis*%.2f"),
            static_cast<int32>(DefenseAttackType),
            MagicImperfect,
            PhysOnMagicParry);
    }
    else if (bIsFury)
    {
        UE_LOG(LogTemp, Warning,
            TEXT("[Parry] Ataque Fury/Ultimate atraviesa la guardia: solo un parry perfecto valido puede anularlo."));
    }

    // El parry perfecto paga siempre el mismo coste, independientemente del tipo de ataque.
    if (bOutWasPerfectParry)
    {
        OutDefenseStaminaCost =
            FMath::Max(0.f, BaseBlockStaminaCost * PerfectParryStaminaMultiplier);
        return;
    }

    if (!bOutWasBlocked)
    {
        return;
    }

    const float AttackCostMultiplier =
        DefenseAttackType == EPantheliaDefenseAttackType::Heavy
            ? FMath::Max(1.f, HeavyBlockStaminaMultiplier)
            : 1.f;

    OutDefenseStaminaCost =
        FMath::Max(0.f, BaseBlockStaminaCost * AttackCostMultiplier);

    // Igualdad exacta es válida: puede dejar stamina en 0 y la guardia continúa.
    // Solo una cantidad estrictamente insuficiente rompe la guardia en ESTE impacto.
    bOutWasGuardBroken =
        CurrentStamina + KINDA_SMALL_NUMBER < OutDefenseStaminaCost;
}

// ================================================================
// NOTA HISTÓRICA: aquí vivía DetermineDebuff (clases 307-309) — ELIMINADA.
// ================================================================
// Era el "dado" del modelo del curso: por cada tipo de daño del golpe, tiraba una
// probabilidad (DebuffChance mitigada por resistencia) y escribía el resultado en el
// context para que HandleIncomingDamage aplicara el debuff. DECISIÓN CERRADA que la
// jubila: en Panthelia los efectos de estado NO tienen azar — funcionan por UMBRAL DE
// ACUMULACIÓN (buildup), como en Elden Ring/Lies of P: cada golpe elemental suma una
// cantidad FIJA a la barra del elemento en la víctima, y al llegar a 100 el estado se
// dispara con certeza. Lo único aleatorio del combate es el crítico. El reemplazo vive
// en la sección BUILDUP de Execute_Implementation (abajo) + HandleElementalBuildup en
// el AttributeSet. Con ella murieron: el tag Debuff.Chance, el campo DebuffChance de
// la ability, y las escrituras de debuff en el context (sus campos quedan en el struct
// como infraestructura reservada, sin escritores).

void UExecCalc_Damage::Execute_Implementation(
    const FGameplayEffectCustomExecutionParameters& ExecutionParams,
    FGameplayEffectCustomExecutionOutput& OutExecutionOutput) const
{


    const UAbilitySystemComponent* SourceASC = ExecutionParams.GetSourceAbilitySystemComponent();
    const UAbilitySystemComponent* TargetASC = ExecutionParams.GetTargetAbilitySystemComponent();

    AActor* SourceAvatar = SourceASC ? SourceASC->GetAvatarActor() : nullptr;
    AActor* TargetAvatar = TargetASC ? TargetASC->GetAvatarActor() : nullptr;

    // Obtenemos los niveles via Execute_ (BlueprintNativeEvent, no requiere cast previo).
    // Si el actor no implementa la interfaz (ej. EffectActor de área), usamos nivel 1.
    int32 SourceLevel = 1;
    if (IsValid(SourceAvatar) && SourceAvatar->Implements<UCombatInterface>())
    {
        SourceLevel = ICombatInterface::Execute_GetPlayerLevel(SourceAvatar);
    }

    int32 TargetLevel = 1;
    if (IsValid(TargetAvatar) && TargetAvatar->Implements<UCombatInterface>())
    {
        TargetLevel = ICombatInterface::Execute_GetPlayerLevel(TargetAvatar);
    }

    // Para GetDefensiveElement y GetFlinchThreshold seguimos usando cast (no son BlueprintNativeEvent).
    ICombatInterface* SourceCombatInterface = Cast<ICombatInterface>(SourceAvatar);
    ICombatInterface* TargetCombatInterface = Cast<ICombatInterface>(TargetAvatar);

    const FGameplayEffectSpec& Spec = ExecutionParams.GetOwningSpec();
    FGameplayEffectContextHandle EffectContextHandle = Spec.GetContext();

    // Un mismo context puede reutilizarse al aplicar el spec a varios objetivos. Antes
    // de cualquier salida temprana limpiamos SIEMPRE los resultados del objetivo previo.
    if (FPantheliaGameplayEffectContext* PantheliaContext =
        static_cast<FPantheliaGameplayEffectContext*>(EffectContextHandle.Get()))
    {
        PantheliaContext->SetIsCriticalHit(false);
        PantheliaContext->SetParryResult(false, 0.f);
        PantheliaContext->SetWasBlocked(false);
        PantheliaContext->SetWasGuardBroken(false);
        PantheliaContext->SetHitOutcome(EPantheliaHitOutcome::Unresolved);
    }

    const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
    const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

    const EPantheliaDefenseAttackType DefenseAttackType =
        UPantheliaAbilitySystemLibrary::GetDefenseAttackType(EffectContextHandle);

    // --- INVULNERABILIDAD ABSOLUTA E I-FRAMES DE EVASIÓN ---
    // La jerarquía separa dos conceptos:
    //   State.Invulnerable exacto       -> inmunidad absoluta (GetUp hoy).
    //   State.Invulnerable.Dodge/Jump   -> i-frames de evasión.
    //
    // HasTag comprueba la raíz jerárquica y detecta tanto el padre como sus hijos.
    // HasTagExact distingue después la inmunidad absoluta del padre. Unavoidable solo
    // atraviesa los hijos de evasión; nunca atraviesa el padre exacto.
    //
    // Este chequeo sigue afectando SOLO al daño directo que pasa por ExecCalc_Damage.
    // Los ticks de DoT ya aplicados modifican IncomingDamage por otra ruta y continúan
    // tiqueando durante una evasión, por decisión de diseño.
    const FPantheliaGameplayTags& InvulnerabilityTags = FPantheliaGameplayTags::Get();
    if (TargetTags && TargetTags->HasTag(InvulnerabilityTags.State_Invulnerable))
    {
        // El padre exacto gana incluso si, por una situación excepcional, también hay
        // un hijo activo. Es invulnerabilidad absoluta: ningún tipo de daño la atraviesa.
        if (TargetTags->HasTagExact(InvulnerabilityTags.State_Invulnerable))
        {
            UPantheliaAbilitySystemLibrary::SetHitOutcome(
                EffectContextHandle, EPantheliaHitOutcome::NegatedInvulnerability);
            return;
        }

        EPantheliaDodgeResponse EffectiveDodgeResponse =
            UPantheliaAbilitySystemLibrary::GetDodgeResponse(EffectContextHandle);

        // Fury/Ultimate ignora los i-frames normales. Un perk u objeto puede conceder
        // Capability.Dodge.Fury; en ese caso vuelve a comportarse como Dodgeable y
        // participa en el mismo sistema de esquive perfecto del resto de ataques.
        if (DefenseAttackType == EPantheliaDefenseAttackType::Fury)
        {
            const bool bCanDodgeFury =
                TargetTags->HasTagExact(InvulnerabilityTags.Capability_Dodge_Fury);
            EffectiveDodgeResponse =
                bCanDodgeFury
                    ? EPantheliaDodgeResponse::Dodgeable
                    : EPantheliaDodgeResponse::Unavoidable;
        }

        if (EffectiveDodgeResponse != EPantheliaDodgeResponse::Unavoidable)
        {
            // Solo el hijo exacto del dodge puede generar el evento crudo. Jump y
            // cualquier otra evasión futura anulan el golpe sin premiar un perfecto.
            if (EffectiveDodgeResponse == EPantheliaDodgeResponse::Dodgeable &&
                TargetTags->HasTagExact(InvulnerabilityTags.State_Invulnerable_Dodge) &&
                IsValid(SourceAvatar) && IsValid(TargetAvatar) &&
                SourceAvatar != TargetAvatar &&
                UPantheliaAbilitySystemLibrary::IsNotFriend(SourceAvatar, TargetAvatar))
            {
                FGameplayEventData Payload;
                Payload.EventTag = InvulnerabilityTags.Event_Dodge_HitAvoided;
                Payload.Instigator = SourceAvatar;
                Payload.Target = TargetAvatar;
                Payload.ContextHandle = EffectContextHandle;

                UAbilitySystemBlueprintLibrary::SendGameplayEventToActor(
                    TargetAvatar,
                    InvulnerabilityTags.Event_Dodge_HitAvoided,
                    Payload);

                UE_LOG(LogPanthelia, Log,
                    TEXT("[DODGE] HitAvoided crudo | Atacante: %s | Objetivo: %s"),
                    *SourceAvatar->GetName(), *TargetAvatar->GetName());
            }

            // Dodgeable y AvoidableNoReward quedan anulados antes de resistencias,
            // crítico, parry, postura, buildup o cualquier otro resultado del golpe.
            UPantheliaAbilitySystemLibrary::SetHitOutcome(
                EffectContextHandle, EPantheliaHitOutcome::NegatedInvulnerability);
            return;
        }

        // Unavoidable continúa por el pipeline normal y atraviesa solo esta evasión.
    }

    FAggregatorEvaluateParameters EvalParams;
    EvalParams.SourceTags = SourceTags;
    EvalParams.TargetTags = TargetTags;

    // --- ATRIBUTOS DEL TARGET ---
    float Armor = 0.f;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().ArmorDef, EvalParams, Armor);
    Armor = FMath::Max(0.f, Armor);

    float MagicResistance = 0.f;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().MagicResistanceDef, EvalParams, MagicResistance);
    MagicResistance = FMath::Max(0.f, MagicResistance);

    float CurrentStamina = 0.f;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(
        DamageStatics().StaminaDef, EvalParams, CurrentStamina);
    CurrentStamina = FMath::Max(0.f, CurrentStamina);

    // --- ATRIBUTOS DEL SOURCE ---
    float CritChance = 0.f;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().CritChanceDef, EvalParams, CritChance);
    CritChance = FMath::Max(0.f, CritChance);

    float CritDamage = 0.f;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().CritDamageDef, EvalParams, CritDamage);
    CritDamage = FMath::Max(0.f, CritDamage);

    float ArmorPenetration = 0.f;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().ArmorPenetrationDef, EvalParams, ArmorPenetration);
    ArmorPenetration = FMath::Max(0.f, ArmorPenetration);

    float MagicPenetration = 0.f;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().MagicPenetrationDef, EvalParams, MagicPenetration);
    MagicPenetration = FMath::Max(0.f, MagicPenetration);

    // PhysicalDamage del SOURCE: addend genérico al daño físico (modelo AD de LoL).
    // Se suma a TODOS los tipos físicos ANTES de la mitigación de Armor.
    float PhysicalDamage = 0.f;
    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(DamageStatics().PhysicalDamageDef, EvalParams, PhysicalDamage);
    PhysicalDamage = FMath::Max(0.f, PhysicalDamage);

    // --- COEFICIENTES DESDE CURVE TABLE ---
    const UPantheliaCharacterClassInfo* ClassInfo = UPantheliaAbilitySystemLibrary::GetCharacterClassInfo(SourceAvatar);

    // Defaults seguros (neutros) por si falta el ClassInfo, las curvas o los CombatInterface:
    //   - ArmorPenCoeff/MagicPenCoeff = 0 → sin penetración si faltan todas las curvas.
    //   - EffArmorCoeff/EffMagicResistanceCoeff = 1 → mitigación lineal estándar.
    // Así, ante una mala configuración (GameMode incorrecto, Data Asset sin asignar,
    // nombres de fila cambiados) el cálculo no crashea ni anula el daño: degrada con gracia.
    float ArmorPenCoeff = 0.f;
    float EffArmorCoeff = 1.f;
    float MagicPenCoeff = 0.f;
    float EffMagicResistanceCoeff = 1.f;

    // Niveles del Source/Target. Si el cast a CombatInterface falló, usamos nivel 1 por defecto.
    // SourceLevel ya calculado arriba vía Execute_GetPlayerLevel
    // TargetLevel ya calculado arriba vía Execute_GetPlayerLevel

    if (ClassInfo && ClassInfo->DamageCalculationCoefficients)
    {
        const FRealCurve* ArmorPenCurve = ClassInfo->DamageCalculationCoefficients->FindCurve(FName("ArmorPenetration"), FString());
        const FRealCurve* EffArmorCurve = ClassInfo->DamageCalculationCoefficients->FindCurve(FName("EffectiveArmor"), FString());
        const FRealCurve* MagicPenCurve = ClassInfo->DamageCalculationCoefficients->FindCurve(FName("MagicPenetration"), FString());
        const FRealCurve* EffMagicResistanceCurve = ClassInfo->DamageCalculationCoefficients->FindCurve(FName("EffectiveMagicResistance"), FString());

        if (ArmorPenCurve)
        {
            ArmorPenCoeff = ArmorPenCurve->Eval(SourceLevel);
        }
        if (EffArmorCurve)
        {
            EffArmorCoeff = EffArmorCurve->Eval(TargetLevel);
        }

        // Si las filas mágicas todavía no existen, se reutilizan temporalmente las
        // curvas físicas. Cuando se creen, el sistema las usa automáticamente.
        MagicPenCoeff = MagicPenCurve ? MagicPenCurve->Eval(SourceLevel) : ArmorPenCoeff;
        EffMagicResistanceCoeff = EffMagicResistanceCurve
            ? EffMagicResistanceCurve->Eval(TargetLevel)
            : EffArmorCoeff;
    }

    const float EffectiveArmor = FMath::Max(
        0.f, Armor * ((100.f - ArmorPenetration * ArmorPenCoeff) / 100.f));
    const float EffectiveMagicResistance = FMath::Max(
        0.f, MagicResistance * ((100.f - MagicPenetration * MagicPenCoeff) / 100.f));

    // --- ELEMENTO DEFENSIVO DEL TARGET ---
    const EPantheliaElement DefenderElement = TargetCombatInterface
        ? TargetCombatInterface->GetDefensiveElement()
        : EPantheliaElement::None;

    // --- MAPA ResistanceTag → CaptureDef (lazy init) ---
    static const TMap<FGameplayTag, FGameplayEffectAttributeCaptureDefinition> TagToCaptureDef =
        []() -> TMap<FGameplayTag, FGameplayEffectAttributeCaptureDefinition>
        {
            TMap<FGameplayTag, FGameplayEffectAttributeCaptureDefinition> Map;
            const FPantheliaGameplayTags& T = FPantheliaGameplayTags::Get();
            Map.Add(T.Attributes_Resistance_Fire, DamageStatics().FireResistanceDef);
            Map.Add(T.Attributes_Resistance_Water, DamageStatics().WaterResistanceDef);
            Map.Add(T.Attributes_Resistance_Storm, DamageStatics().StormResistanceDef);
            Map.Add(T.Attributes_Resistance_Nature, DamageStatics().NatureResistanceDef);
            return Map;
        }();

    // ================================================================
    // NOTA HISTÓRICA: aquí vivió la llamada a DetermineDebuff (clase 307) — el
    // sistema de debuff por dado fue REEMPLAZADO por el buildup de umbral. Ver la
    // sección BUILDUP ELEMENTAL más abajo (después de parry y crítico, porque
    // necesita ambos resultados).

    // ================================================================
    // LOOP POR TIPO DE DAÑO (spec §1.8)
    // ================================================================
    const FPantheliaGameplayTags& Tags = FPantheliaGameplayTags::Get();
    float TotalDamage = 0.f;

    // Rastreamos por separado cuanto dano es fisico y cuanto magico, porque el sistema
    // de parry/bloqueo (mas abajo) mitiga distinto cada tipo segun el estado defensivo.
    float PhysicalSubtotal = 0.f;
    float MagicSubtotal = 0.f;

    for (const auto& DamageTypePair : Tags.DamageTypesToResistances)
    {
        const FGameplayTag& DamageTypeTag = DamageTypePair.Key;
        const FGameplayTag& ResistanceTag = DamageTypePair.Value;

        // El daño ya viene escalado desde SpawnProjectile (spec §1.8)
        float TypeDamage = Spec.GetSetByCallerMagnitude(DamageTypeTag, false);
        if (TypeDamage <= 0.f) continue;

        const bool bIsPhysical = DamageTypeTag.MatchesTag(Tags.DamageParent_Physical);

        if (bIsPhysical)
        {
            // PASO 2 (spec §1.8): PhysicalDamage como addend genérico al daño físico.
            // Se suma ANTES de la mitigación de Armor porque es daño bruto.
            // Modelo: AD en LoL — escala auto-ataques, armas y cualquier daño físico.
            // Si la ability también usa PhysicalDamage como ratio en AttributeScalings,
            // cuenta dos veces (comportamiento intencional — igual que AD en LoL).
            TypeDamage += PhysicalDamage;

            // PASO 3: Mitigación de Armor
            TypeDamage *= FMath::Max(0.f, (100.f - EffectiveArmor * EffArmorCoeff) / 100.f);
        }
        else
        {
            // PASO 3 (mágico): mitigación real de MagicResistance después de
            // MagicPenetration. MagicDamage no se suma aquí: el daño directo lo usa
            // como ratio en la ability y los estados lo consultan en su payload global.
            TypeDamage *= FMath::Max(
                0.f,
                (100.f - EffectiveMagicResistance * EffMagicResistanceCoeff) / 100.f);
        }

        // PASO 4: Tabla de afinidades (±15%)
        const EPantheliaElement* AttackerElementPtr = Tags.DamageTypeToElement.Find(DamageTypeTag);
        const EPantheliaElement AttackerElement = AttackerElementPtr ? *AttackerElementPtr : EPantheliaElement::None;
        TypeDamage *= GetTypeChartMultiplier(AttackerElement, DefenderElement);

        // PASO 5: Resistencia elemental del target
        if (ResistanceTag.IsValid())
        {
            const FGameplayEffectAttributeCaptureDefinition* CaptureDef = TagToCaptureDef.Find(ResistanceTag);
            if (CaptureDef)
            {
                float Resistance = 0.f;
                ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(*CaptureDef, EvalParams, Resistance);
                Resistance = FMath::Clamp(Resistance, 0.f, 100.f);
                // PLACEHOLDER: actualmente derivada de Resilience vía GE_SecondaryAttributes.
                // Cuando existan equipo y árbol de habilidades, el GE modificador se elimina
                // y las resistencias se configuran directamente desde esos sistemas.
                TypeDamage *= FMath::Max(0.f, (100.f - Resistance) / 100.f);
            }
        }

        // Acumular en el subtotal correspondiente (para la matriz de parry).
        if (bIsPhysical)
        {
            PhysicalSubtotal += TypeDamage;
        }
        else
        {
            MagicSubtotal += TypeDamage;
        }

        TotalDamage += TypeDamage;
    }

    // ================================================================
    // PARRY / BLOQUEO (modelo Lies of P) — mitigacion segun estado defensivo
    // ================================================================
    // Leemos el estado defensivo del TARGET (el que recibe el dano) de sus tags.
    // Matriz vigente:
    //   Parry Fisico + golpe con componente fisico -> parry perfecto; anula TODO el golpe.
    //   Parry Magico + golpe con componente magico -> parry perfecto; anula TODO el golpe.
    //   Parry Fisico vs golpe puramente magico -> mitigacion cruzada, no parry perfecto.
    //   Parry Magico vs golpe puramente fisico -> mitigacion cruzada, no parry perfecto.
    //   Bloqueos imperfectos -> aplican los multiplicadores configurados de ambos subtotales.
    // Sin estado defensivo -> dano completo.
    // Booleanos del resultado defensivo, IZADOS fuera del if (TargetTags) — dos motivos:
    // (1) el fix de contaminación de contexto de abajo necesita escribirlos SIEMPRE,
    // incluso cuando TargetTags es nullptr; (2) la sección de BUILDUP (más abajo)
    // necesita bWasPerfectParry: un parry perfecto niega el buildup del golpe.
    bool bWasPerfectParry = false;
    bool bWasBlocked = false;
    bool bWasGuardBroken = false;
    float PoiseToAttacker = 0.f;
    float DefenseStaminaCost = 0.f;

    if (TargetTags)
    {
        // Que tipos trae el golpe entrante (antes de mitigar).
        const bool bIncomingIsPhysical = PhysicalSubtotal > 0.f;
        const bool bIncomingIsMagic = MagicSubtotal > 0.f;

        ApplyParryBlockMitigation(
            TargetAvatar, *TargetTags, Tags,
            DefenseAttackType, CurrentStamina,
            bIncomingIsPhysical, bIncomingIsMagic,
            PhysicalSubtotal, MagicSubtotal,
            bWasPerfectParry, bWasBlocked, bWasGuardBroken,
            PoiseToAttacker, DefenseStaminaCost);

        // Recalcular el total tras la mitigacion defensiva.
        TotalDamage = PhysicalSubtotal + MagicSubtotal;
    }

    // === ESCRIBIR SIEMPRE el resultado del parry/bloqueo (fix de contaminación) ===
    //
    // Antes, SetParryResult(true, ...) / SetWasBlocked(true) solo se escribían en caso
    // de ÉXITO, dentro del if (TargetTags). En un melee multi-objetivo que comparte el
    // mismo contexto entre aplicaciones (WeaponTraceComponent aplica el mismo spec a
    // varios objetivos; el handle del contexto es ref-counting, no copia por objetivo),
    // eso dejaba flags "pegados": si el enemigo A parreaba el swing,
    // el contexto conservaba bWasParried=true al aplicar sobre el enemigo B (que no
    // parreó), y HandleParryReaction re-aplicaba el daño de postura al atacante una
    // segunda vez. La regla del fix — la misma que ya seguía SetIsCriticalHit — es
    // escribir el resultado calculado SIEMPRE, sea true o false, en cada aplicación.
    if (FPantheliaGameplayEffectContext* PantheliaContext =
        static_cast<FPantheliaGameplayEffectContext*>(EffectContextHandle.Get()))
    {
        // SetParryResult escribe ambos campos a la vez (flag + postura al atacante):
        // en el caso "no hubo parry" esto resetea correctamente a (false, 0).
        PantheliaContext->SetParryResult(bWasPerfectParry, bWasPerfectParry ? PoiseToAttacker : 0.f);
        PantheliaContext->SetWasBlocked(bWasBlocked);
        PantheliaContext->SetWasGuardBroken(bWasGuardBroken);
    }

    // Resultado defensivo final. Se escribe antes del crítico para que un parry perfecto
    // no pueda quedar marcado como crítico ni recrear daño mediante el término plano.
    const EPantheliaHitOutcome HitOutcome =
        bWasPerfectParry
            ? EPantheliaHitOutcome::NegatedPerfectParry
            : (bWasBlocked
                ? EPantheliaHitOutcome::MitigatedBlock
                : EPantheliaHitOutcome::Accepted);
    UPantheliaAbilitySystemLibrary::SetHitOutcome(EffectContextHandle, HitOutcome);

    // --- CRÍTICO (sobre el total) ---
    // REUBICADO (sistema de buildup): antes vivía después de esta posición; ahora va
    // ANTES de la sección de buildup porque esta necesita bCriticalHit (el crítico
    // multiplica también el buildup que deposita el golpe — decisión cerrada).
    //
    // Un parry perfecto anula el golpe completo. No hacemos la tirada ni escribimos
    // un crítico fantasma en el context, y el CritDamage plano no puede recrear daño.
    const bool bCriticalHit = !bWasPerfectParry && FMath::RandRange(1, 100) <= CritChance;
    TotalDamage = bCriticalHit ? 1.5f * TotalDamage + CritDamage : TotalDamage;
    UPantheliaAbilitySystemLibrary::SetIsCriticalHit(EffectContextHandle, bCriticalHit);

    // --- COSTE DEFENSIVO DE STAMINA ---
    // Parry perfecto: coste fijo relativo al coste base, sin importar Normal/Heavy/Fury.
    // Bloqueo: coste base para Normal y multiplicador pesado para Heavy.
    // Si el bloqueo no puede pagar, consume la stamina restante, mantiene el daño de
    // vida mitigado y marca guardia rota. Llegar EXACTAMENTE a 0 no rompe la guardia.
    if ((bWasPerfectParry || bWasBlocked) && DefenseStaminaCost > 0.f)
    {
        const float StaminaToConsume =
            bWasGuardBroken
                ? CurrentStamina
                : FMath::Min(CurrentStamina, DefenseStaminaCost);

        if (StaminaToConsume > 0.f)
        {
            OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
                UPantheliaAttributeSet::GetStaminaAttribute(),
                EGameplayModOp::Additive,
                -StaminaToConsume));
        }
    }

    // --- ESCRIBIR IncomingDamage PRIMERO ---
    // Dentro del paquete ofensivo, el orden de los modifiers importa: primero se
    // resuelve el golpe directo y su posible muerte; después postura y buildup.
    // El coste defensivo anterior es un recurso del defensor y debe quedar cobrado
    // antes de que HandleIncomingDamage procese la reacción de guardia rota.
    OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
        UPantheliaAttributeSet::GetIncomingDamageAttribute(),
        EGameplayModOp::Additive, TotalDamage));

    // --- DAÑO A POSTURA ---
    const float BasePoiseDamage = Spec.GetSetByCallerMagnitude(Tags.Damage_Poise, false);
    if (!bWasPerfectParry && BasePoiseDamage > 0.f)
    {
        OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
            UPantheliaAttributeSet::GetIncomingPoiseDamageAttribute(),
            EGameplayModOp::Additive, BasePoiseDamage));
    }

    // ================================================================
    // BUILDUP ELEMENTAL (sistema de umbral — reemplaza al dado del debuff)
    // ================================================================
    // Por cada elemento con buildup entrante en este golpe (SetByCaller
    // CombatTricks.Buildup.*, colapsado por la ability desde su TMap BuildupAmounts),
    // calculamos el buildup EFECTIVO y lo depositamos como output modifier en el
    // atributo XBuildup del target. La fórmula, término a término:
    //
    //   Efectivo = Base × (1 − Resistencia/100) × (crítico ? 1.5 : 1)
    //
    //   - Resistencia (0-100): reduce el intake linealmente. A 100, el intake es 0:
    //     INMUNIDAD real al estado (la barra jamás sube). Es la MISMA resistencia
    //     que ya mitiga el daño elemental — doble rol, decisión cerrada.
    //   - Crítico ×1.5: el mismo multiplicador BASE del daño (decisión cerrada: un
    //     crítico con arma elemental acerca más al enemigo a su estado). El flat de
    //     CritDamage NO se aplica aquí — es una estadística de daño, no de estado.
    //   - Parry perfecto: buildup CERO (coherente con "parry perfecto niega el
    //     estado"). El bloqueo imperfecto NO reduce el buildup — mitigar no es negar.
    //
    // Vive DESPUÉS del parry y del crítico a propósito: necesita ambos resultados.
    // El disparo del umbral NO ocurre aquí — ocurre en HandleElementalBuildup
    // (AttributeSet) cuando estos modifiers lleguen al target.
    if (!bWasPerfectParry)
    {
        struct FBuildupRoute
        {
            EPantheliaElement Element;
            FGameplayTag SetByCallerTag;
            FGameplayAttribute TargetAttribute;
        };
        const FBuildupRoute BuildupRoutes[] =
        {
            { EPantheliaElement::Fire,   Tags.CombatTricks_Buildup_Fire,   UPantheliaAttributeSet::GetFireBuildupAttribute() },
            { EPantheliaElement::Storm,  Tags.CombatTricks_Buildup_Storm,  UPantheliaAttributeSet::GetStormBuildupAttribute() },
            { EPantheliaElement::Water,  Tags.CombatTricks_Buildup_Water,  UPantheliaAttributeSet::GetWaterBuildupAttribute() },
            { EPantheliaElement::Nature, Tags.CombatTricks_Buildup_Nature, UPantheliaAttributeSet::GetNatureBuildupAttribute() },
        };

        for (const FBuildupRoute& Route : BuildupRoutes)
        {
            // false = no avisar si el tag no está asignado (abilities antiguas que aún
            // no re-guardaron su spec); 0 = sin buildup de este elemento.
            const float BaseBuildup = Spec.GetSetByCallerMagnitude(Route.SetByCallerTag, false, 0.f);
            if (BaseBuildup <= 0.f) continue;

            // Resistencia del elemento en el TARGET, por la vía canónica:
            // elemento --(ElementToResistance)--> tag de resistencia --(TagToCaptureDef)--> captura.
            // Reutiliza las MISMAS capturas que ya usa la mitigación de daño elemental.
            float Resistance = 0.f;
            if (const FGameplayTag* ResistanceTag = Tags.ElementToResistance.Find(Route.Element))
            {
                if (const FGameplayEffectAttributeCaptureDefinition* CaptureDef = TagToCaptureDef.Find(*ResistanceTag))
                {
                    ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(*CaptureDef, EvalParams, Resistance);
                    Resistance = FMath::Clamp(Resistance, 0.f, 100.f);
                }
            }

            const float CritMultiplier = bCriticalHit ? 1.5f : 1.f;
            const float EffectiveBuildup = BaseBuildup * ((100.f - Resistance) / 100.f) * CritMultiplier;

            // Resistencia 100 (o redondeos) → nada que depositar: inmunidad silenciosa.
            if (EffectiveBuildup <= 0.f) continue;

            // Output modifier Additive sobre el atributo de barra del target. GAS lo
            // aplicará junto al daño, y PostGameplayEffectExecute (rama del atributo)
            // hará el clamp + chequeo de umbral + disparo en HandleElementalBuildup.
            OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
                Route.TargetAttribute, EGameplayModOp::Additive, EffectiveBuildup));
        }
    }

}
