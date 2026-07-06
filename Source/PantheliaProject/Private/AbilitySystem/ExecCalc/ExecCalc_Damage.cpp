// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/ExecCalc/ExecCalc_Damage.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/PantheliaAttributeSet.h"
#include "AbilitySystem/PantheliaAbilitySystemLibrary.h"
#include "AbilitySystem/Data/PantheliaCharacterClassInfo.h"
#include "Interfaces/CombatInterface.h"
#include "PantheliaGameplayTags.h"
#include "PantheliaElementTypes.h"
#include "Combat/PantheliaEquipmentComponent.h"
#include "Combat/PantheliaWeaponDefinition.h"
#include "AbilitySystem/PantheliaAbilityTypes.h"

// ============================================================
// PantheliaDamageStatics
// ============================================================
// NOTA ASIMETRÍA (spec §1.7, modelo LoL):
//   PhysicalDamage (AD): capturado del SOURCE → sumado como addend genérico al daño físico.
//   MagicDamage    (AP): NO capturado aquí — su único uso es como ratio en las abilities
//                        (se calcula en SpawnProjectile antes del SetByCaller).

struct PantheliaDamageStatics
{
    DECLARE_ATTRIBUTE_CAPTUREDEF(Armor);
    DECLARE_ATTRIBUTE_CAPTUREDEF(MagicResistance);
    DECLARE_ATTRIBUTE_CAPTUREDEF(CritChance);
    DECLARE_ATTRIBUTE_CAPTUREDEF(CritDamage);
    DECLARE_ATTRIBUTE_CAPTUREDEF(ArmorPenetration);
    DECLARE_ATTRIBUTE_CAPTUREDEF(MagicPenetration);

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
    bool bIncomingIsPhysical,
    bool bIncomingIsMagic,
    float& PhysicalSubtotal,
    float& MagicSubtotal,
    bool& bOutWasPerfectParry,
    bool& bOutWasBlocked,
    float& OutPoiseToAttacker)
{
    bOutWasPerfectParry = false;
    bOutWasBlocked = false;
    OutPoiseToAttacker = 0.f;

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

    // Leer los multiplicadores y danos de postura del arma equipada del defensor.
    // Si no hay arma, usamos defaults neutros conservadores para no crashear.
    float PhysImperfect = 0.6f;
    float MagicOnPhysParry = 0.8f;
    float MagicImperfect = 0.5f;
    float PhysOnMagicParry = 0.7f;
    float PhysParryPoise = 30.f;
    float MagicParryPoise = 12.f;

    if (TargetAvatar)
    {
        if (UPantheliaEquipmentComponent* Equip = TargetAvatar->FindComponentByClass<UPantheliaEquipmentComponent>())
        {
            if (const UPantheliaWeaponDefinition* WeaponDef = Equip->GetEquippedWeaponDefinition())
            {
                PhysImperfect = WeaponDef->PhysicalImperfectBlockMultiplier;
                MagicOnPhysParry = WeaponDef->MagicOnPhysicalParryMultiplier;
                MagicImperfect = WeaponDef->MagicImperfectBlockMultiplier;
                PhysOnMagicParry = WeaponDef->PhysicalOnMagicParryMultiplier;
                PhysParryPoise = WeaponDef->PhysicalParryPoiseDamage;
                MagicParryPoise = WeaponDef->MagicParryPoiseDamage;
            }
        }
    }

    // --- PARRY FISICO (perfecto) ---
    if (bParryPhysical)
    {
        PhysicalSubtotal = 0.f;                  // anula el dano fisico
        MagicSubtotal *= MagicOnPhysParry;     // mitiga el magico (sin anular)
        // Solo es parry "correcto" (con dano de postura al enemigo) si el ataque era fisico.
        if (bIncomingIsPhysical)
        {
            bOutWasPerfectParry = true;
            OutPoiseToAttacker = PhysParryPoise;
        }
        else
        {
            // Parar magia con parry fisico mitiga pero NO da postura ni beneficios.
            bOutWasBlocked = true;
        }
        UE_LOG(LogTemp, Warning, TEXT("[Parry] MITIGACION Parry Fisico: Fis=0 Mag*%.2f | Postura enemigo=%.1f"), MagicOnPhysParry, OutPoiseToAttacker);
    }
    // --- PARRY MAGICO (perfecto) ---
    else if (bParryMagic)
    {
        MagicSubtotal = 0.f;                  // anula el dano magico
        PhysicalSubtotal *= PhysOnMagicParry;    // mitiga el fisico (sin anular)
        if (bIncomingIsMagic)
        {
            bOutWasPerfectParry = true;
            OutPoiseToAttacker = MagicParryPoise;
        }
        else
        {
            bOutWasBlocked = true;
        }
        UE_LOG(LogTemp, Warning, TEXT("[Parry] MITIGACION Parry Magico: Mag=0 Fis*%.2f | Postura enemigo=%.1f"), PhysOnMagicParry, OutPoiseToAttacker);
    }
    // --- BLOQUEO FISICO (imperfecto) ---
    else if (bBlockPhysical)
    {
        PhysicalSubtotal *= PhysImperfect;
        MagicSubtotal *= MagicOnPhysParry;
        bOutWasBlocked = true;
        UE_LOG(LogTemp, Warning, TEXT("[Parry] MITIGACION Bloqueo Fisico: Fis*%.2f Mag*%.2f"), PhysImperfect, MagicOnPhysParry);
    }
    // --- BLOQUEO MAGICO (imperfecto) ---
    else if (bBlockMagic)
    {
        MagicSubtotal *= MagicImperfect;
        PhysicalSubtotal *= PhysOnMagicParry;
        bOutWasBlocked = true;
        UE_LOG(LogTemp, Warning, TEXT("[Parry] MITIGACION Bloqueo Magico: Mag*%.2f Fis*%.2f"), MagicImperfect, PhysOnMagicParry);
    }
}

// ================================================================
// Helper: determinar si un debuff elemental se activa (clase 307)
// ================================================================
// Recorre los tipos de daño presentes en este golpe (via SetByCaller) y, para
// cada uno que resuelva a un elemento con debuff asociado, calcula si el debuff
// se activa (probabilidad del source, mitigada por la resistencia del target).
//
// DECISIÓN DE DISEÑO (adaptada del curso): el curso itera un TMap "DamageTypesToDebuffs"
// (tipo de daño -> debuff, relación 1 a 1) que en la clase 303 decidimos NO construir,
// porque en Panthelia varios tipos de daño colapsan en el mismo elemento. En su lugar,
// iteramos DamageTypesToResistances (ya existente, enumera los 8 tipos de daño
// registrados) y resolvemos el debuff en dos saltos, reutilizando los mapas ya
// construidos en las clases 303-304:
//   tipo de daño --(DamageTypeToElement)--> elemento --(ElementToDebuff)--> debuff
// Un tipo de daño físico genérico (elemento None) no tiene entrada en ElementToDebuff
// y se salta sin más — sin debuff, correctamente.
//
// OJO (a vigilar en clases futuras, no resuelto aquí): si una ability tuviera DOS tipos
// de daño que colapsan al MISMO elemento (ej. Damage.Physical.Ice y Damage.Magical.Water,
// ambos Agua), este loop haría la tirada de Saturación dos veces en el mismo golpe. Hoy
// ninguna ability de Panthelia hace esto, así que no es un problema real todavía — se
// resuelve si llega a serlo (ej. deduplicando por debuff ya procesado en este golpe).
//
// Por ahora esta función SOLO determina si hubo debuff (bDebuff) — qué hacer al
// respecto es la pregunta que responde la clase siguiente (ver TODO al final).
static void DetermineDebuff(
    const FGameplayEffectCustomExecutionParameters& ExecutionParams,
    const FGameplayEffectSpec& Spec,
    const FAggregatorEvaluateParameters& EvaluationParameters,
    const TMap<FGameplayTag, FGameplayEffectAttributeCaptureDefinition>& TagToCaptureDef)
{
    const FPantheliaGameplayTags& Tags = FPantheliaGameplayTags::Get();

    for (const TTuple<FGameplayTag, FGameplayTag>& DamageTypePair : Tags.DamageTypesToResistances)
    {
        const FGameplayTag& DamageType = DamageTypePair.Key;

        // ¿Este tipo de daño está presente en el golpe? Spec.GetSetByCallerMagnitude con
        // WarnIfNotFound=false y DefaultIfNotFound=-1.f: como nunca asignamos daño negativo
        // (convención del proyecto), -1.f solo puede significar "este tipo no está en el golpe".
        // Ver ApplyDamageScalingToSpec (clase 303): solo se asignan al SetByCaller los tipos
        // con BaseValue > 0, así que un tipo ausente del golpe nunca tendrá SetByCaller.
        const float TypeDamage = Spec.GetSetByCallerMagnitude(DamageType, false, -1.f);
        if (TypeDamage <= -1.f)
        {
            continue;
        }

        // PASO 1 (adaptado): resolver tipo de daño -> elemento -> debuff.
        const EPantheliaElement* ElementPtr = Tags.DamageTypeToElement.Find(DamageType);
        if (!ElementPtr || *ElementPtr == EPantheliaElement::None)
        {
            // Daño físico genérico: sin elemento, sin debuff elemental. Comportamiento
            // correcto, no un caso de error — la mayoría de golpes físicos puros caen aquí.
            continue;
        }

        const FGameplayTag* DebuffTypePtr = Tags.ElementToDebuff.Find(*ElementPtr);
        if (!DebuffTypePtr)
        {
            // No debería ocurrir (los 4 elementos con entrada en DamageTypeToElement tienen
            // su contraparte en ElementToDebuff), pero no crasheamos ante un mapa incompleto.
            continue;
        }
        const FGameplayTag DebuffType = *DebuffTypePtr;

        // PASO 2: probabilidad base del source (viene del SetByCaller asignado en la
        // clase 306, siempre presente — por eso WarnIfNotFound=false es solo una red de
        // seguridad, no algo que se espere disparar en la práctica).
        const float SourceDebuffChance = Spec.GetSetByCallerMagnitude(Tags.Debuff_Chance, false, -1.f);

        // Optimización: si esta ability no tiene ninguna probabilidad de debuff configurada
        // (0 o el sentinel -1 de "no encontrado"), no tiene sentido capturar la resistencia
        // del target ni tirar el dado — el resultado sería "no" de todas formas. No cambia
        // el comportamiento, solo evita trabajo innecesario en cada golpe.
        if (SourceDebuffChance <= 0.f)
        {
            continue;
        }

        // PASO 3: resistencia elemental del target (mismo patrón que el loop principal
        // de daño: tipo de daño -> tag de resistencia -> CaptureDef -> valor capturado).
        float TargetDebuffResistance = 0.f;
        const FGameplayTag* ResistanceTagPtr = Tags.DamageTypesToResistances.Find(DamageType);
        if (ResistanceTagPtr && ResistanceTagPtr->IsValid())
        {
            if (const FGameplayEffectAttributeCaptureDefinition* CaptureDef = TagToCaptureDef.Find(*ResistanceTagPtr))
            {
                ExecutionParams.AttemptCalculateCapturedAttributeMagnitude(*CaptureDef, EvaluationParameters, TargetDebuffResistance);
            }
        }
        TargetDebuffResistance = FMath::Max(TargetDebuffResistance, 0.f);

        // PASO 4: probabilidad efectiva. Cada punto de resistencia resta 1% de probabilidad
        // (mismo criterio simple que usa el curso — ajustable a curva más adelante si hace falta).
        const float EffectiveDebuffChance = SourceDebuffChance * (100.f - TargetDebuffResistance) / 100.f;

        // PASO 5: tirada de dado de 100 caras.
        const bool bDebuff = FMath::RandRange(1, 100) < EffectiveDebuffChance;

        if (bDebuff)
        {
            // Escribimos el resultado en el context del spec para que llegue hasta el
            // AttributeSet (clase 309). Spec.GetContext() devuelve un HANDLE que comparte
            // el mismo objeto de contexto subyacente (ref-counting) — no es una copia
            // independiente, así que escribir en este handle es visible más adelante en
            // el pipeline, donde sea que se lea el mismo FGameplayEffectSpec/contexto.
            FGameplayEffectContextHandle ContextHandle = Spec.GetContext();

            // Los 3 parámetros que el debuff necesita para aplicarse de verdad (Chance ya
            // cumplió su función en la tirada de dado de arriba — no hace falta transportarlo).
            const float SuccessfulDebuffDamage = Spec.GetSetByCallerMagnitude(Tags.Debuff_Damage, false, -1.f);
            const float SuccessfulDebuffDuration = Spec.GetSetByCallerMagnitude(Tags.Debuff_Duration, false, -1.f);
            const float SuccessfulDebuffFrequency = Spec.GetSetByCallerMagnitude(Tags.Debuff_Frequency, false, -1.f);

            UPantheliaAbilitySystemLibrary::SetIsSuccessfulDebuff(ContextHandle, true);
            UPantheliaAbilitySystemLibrary::SetDamageType(ContextHandle, DamageType);
            UPantheliaAbilitySystemLibrary::SetDebuffDamage(ContextHandle, SuccessfulDebuffDamage);
            UPantheliaAbilitySystemLibrary::SetDebuffDuration(ContextHandle, SuccessfulDebuffDuration);
            UPantheliaAbilitySystemLibrary::SetDebuffFrequency(ContextHandle, SuccessfulDebuffFrequency);

            // Nota de diseño (clase 307, sigue vigente): NO guardamos DebuffType (el tag de
            // identidad — Debuff.Burn, etc.) en el context. Es derivable de DamageType vía
            // la misma cadena DamageTypeToElement -> ElementToDebuff que ya usamos arriba,
            // así que guardarlo aquí sería duplicar información. Quien aplique el debuff más
            // adelante puede re-derivarlo con una línea de código en vez de leerlo de aquí.
        }
    }
}

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

    const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
    const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

    // --- I-FRAMES GENÉRICOS (post-315, a petición) ---
    // Si el target tiene el tag de invulnerabilidad activo (concedido por
    // UPantheliaAbilitySystemLibrary::GrantTemporaryInvulnerability — hoy usado por
    // GA_GetUp al levantarse tras un lanzamiento aéreo; en el futuro también por
    // GA_Dodge), el daño es CERO, sin excepciones. Este chequeo va ANTES de absolutamente
    // todo lo demás (resistencias, crítico, parry, debuff...) — durante los i-frames,
    // nada de eso debería importar: el golpe simplemente no cuenta. HasTagExact (no
    // HasTag): buscamos el tag exacto, no una jerarquía de padres/hijos — no hay
    // sub-tags de State.Invulnerable que debieran coincidir también.
    //
    // Pedimos el singleton aquí, localmente: el "Tags" que se usa más abajo en esta
    // misma función se declara más adelante (justo antes del loop de daño) — no está
    // disponible todavía en este punto tan temprano. FPantheliaGameplayTags::Get() es
    // barato (un singleton ya construido, no hace trabajo real) así que pedirlo dos
    // veces en la misma función no tiene coste — este archivo ya lo hace en otro punto.
    if (TargetTags && TargetTags->HasTagExact(FPantheliaGameplayTags::Get().State_Invulnerable))
    {
        return;
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
    //   - ArmorPenCoeff = 0  → no aplica penetración de armadura (EffectiveArmor = Armor).
    //   - EffArmorCoeff = 1  → la armadura mitiga de forma estándar.
    // Así, ante una mala configuración (GameMode incorrecto, Data Asset sin asignar,
    // nombres de fila cambiados) el cálculo no crashea ni anula el daño: degrada con gracia.
    float ArmorPenCoeff = 0.f;
    float EffArmorCoeff = 1.f;

    // Niveles del Source/Target. Si el cast a CombatInterface falló, usamos nivel 1 por defecto.
    // SourceLevel ya calculado arriba vía Execute_GetPlayerLevel
    // TargetLevel ya calculado arriba vía Execute_GetPlayerLevel

    if (ClassInfo && ClassInfo->DamageCalculationCoefficients)
    {
        const FRealCurve* ArmorPenCurve = ClassInfo->DamageCalculationCoefficients->FindCurve(FName("ArmorPenetration"), FString());
        const FRealCurve* EffArmorCurve = ClassInfo->DamageCalculationCoefficients->FindCurve(FName("EffectiveArmor"), FString());

        if (ArmorPenCurve)
        {
            ArmorPenCoeff = ArmorPenCurve->Eval(SourceLevel);
        }
        if (EffArmorCurve)
        {
            EffArmorCoeff = EffArmorCurve->Eval(TargetLevel);
        }
    }

    const float EffectiveArmor = FMath::Max(
        0.f, Armor * ((100.f - ArmorPenetration * ArmorPenCoeff) / 100.f));

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
    // DEBUFF (clase 307) — se calcula ANTES del daño porque no depende de él.
    // ================================================================
    // Determina, por cada tipo de daño presente en el golpe, si el debuff elemental
    // correspondiente se activa. Por ahora solo determina (bDebuff) — la clase 307
    // no implementa todavía qué pasa cuando se activa (ver TODO dentro de la función).
    DetermineDebuff(ExecutionParams, Spec, EvalParams, TagToCaptureDef);

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
            // PASO 3 (mágico): MagicResistance — PENDIENTE chat de matemáticas
            // MagicDamage NO se suma aquí — solo se usa como ratio en las abilities
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
    // Matriz (Fase 2 — solo mitigacion de dano a la vida; postura del enemigo y
    // efectos elementales llegan en Fase 3):
    //   Parry Fisico  perfecto vs Fisico -> anula 100% del fisico.
    //   Parry Fisico  perfecto vs Magico -> mitiga (MagicOnPhysicalParryMultiplier).
    //   Parry Magico  perfecto vs Magico -> anula 100% del magico.
    //   Parry Magico  perfecto vs Fisico -> mitiga (PhysicalOnMagicParryMultiplier).
    //   Bloqueo Fisico (imperfecto) vs Fisico -> PhysicalImperfectBlockMultiplier.
    //   Bloqueo Magico (imperfecto) vs Magico -> MagicImperfectBlockMultiplier.
    // Sin estado defensivo -> dano completo.
    if (TargetTags)
    {
        // Que tipos trae el golpe entrante (antes de mitigar).
        const bool bIncomingIsPhysical = PhysicalSubtotal > 0.f;
        const bool bIncomingIsMagic = MagicSubtotal > 0.f;

        bool bWasPerfectParry = false;
        bool bWasBlocked = false;
        float PoiseToAttacker = 0.f;

        ApplyParryBlockMitigation(
            TargetAvatar, *TargetTags, Tags,
            bIncomingIsPhysical, bIncomingIsMagic,
            PhysicalSubtotal, MagicSubtotal,
            bWasPerfectParry, bWasBlocked, PoiseToAttacker);

        // Recalcular el total tras la mitigacion defensiva.
        TotalDamage = PhysicalSubtotal + MagicSubtotal;

        // Escribir el resultado del parry/bloqueo en el context para que
        // PostGameplayEffectExecute aplique el dano de postura al atacante y los efectos.
        if (FPantheliaGameplayEffectContext* PantheliaContext =
            static_cast<FPantheliaGameplayEffectContext*>(EffectContextHandle.Get()))
        {
            if (bWasPerfectParry)
            {
                PantheliaContext->SetParryResult(true, PoiseToAttacker);
            }
            else if (bWasBlocked)
            {
                PantheliaContext->SetWasBlocked(true);
            }
        }
    }

    // --- CRÍTICO (sobre el total) ---
    const bool bCriticalHit = FMath::RandRange(1, 100) <= CritChance;
    TotalDamage = bCriticalHit ? 1.5f * TotalDamage + CritDamage : TotalDamage;
    UPantheliaAbilitySystemLibrary::SetIsCriticalHit(EffectContextHandle, bCriticalHit);

    // --- DAÑO A POSTURA ---
    const float BasePoiseDamage = Spec.GetSetByCallerMagnitude(Tags.Damage_Poise, false);
    if (BasePoiseDamage > 0.f)
    {
        OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
            UPantheliaAttributeSet::GetIncomingPoiseDamageAttribute(),
            EGameplayModOp::Additive, BasePoiseDamage));
    }

    // --- ESCRIBIR IncomingDamage ---
    OutExecutionOutput.AddOutputModifier(FGameplayModifierEvaluatedData(
        UPantheliaAttributeSet::GetIncomingDamageAttribute(),
        EGameplayModOp::Additive, TotalDamage));
}