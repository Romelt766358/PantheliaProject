#include "AbilitySystem/PantheliaAttributeSet.h"
#include "GameplayEffectExtension.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/Character.h"
#include "Net/UnrealNetwork.h"
#include "PantheliaGameplayTags.h"
#include "Interfaces/CombatInterface.h"
#include "AbilitySystem/PantheliaAbilitySystemLibrary.h"
#include "AbilitySystem/PantheliaAbilityTypes.h"
#include "AbilitySystem/PantheliaAbilitySystemComponent.h"
#include "AbilitySystem/Data/PantheliaCharacterClassInfo.h"
#include "AbilitySystem/Data/PantheliaElementalStatusConfig.h"
// Necesario para AddToXP() y GetEnemyKillCount() / RecordEnemyKill() en el bloque IncomingXP
#include "Player/PantheliaPlayerState.h"
// Necesario para GetBaseXPReward() y GetEnemyID() en SendXPEvent
#include "Characters/PantheliaEnemy.h"
// Desacopla el bloque IncomingXP del AttributeSet del PlayerState concreto.
// En vez de castear a APantheliaPlayerState directamente, usamos la interfaz.
#include "Interfaces/PantheliaPlayerInterface.h"
// Necesario para UAbilitySystemBlueprintLibrary::SendGameplayEventToActor en SendXPEvent
#include "AbilitySystemBlueprintLibrary.h"
#include "GameFramework/PlayerController.h"
#include "PantheliaLogChannels.h"
// Necesario para crear un UGameplayEffect dinámicamente en C++ (clase 310): UGameplayEffect,
// FGameplayModifierInfo, FGameplayEffectSpec, EGameplayEffectDurationType, FInheritedTagContainer.
#include "GameplayEffect.h"
// Necesario para conceder tags desde un GE creado en C++ (clase 310). InheritableOwnedTagsContainer
// (lo que usa el curso original) está DEPRECADO desde UE 5.3 — este componente es el reemplazo oficial.
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UPantheliaAttributeSet::UPantheliaAttributeSet()
{
	// Primarios
	InitHardness(10.f); InitResonance(10.f); InitResilience(10.f); InitEndurance(10.f); InitSpirit(10.f);

	// Secundarios
	InitMaxHealth(100.f); InitArmor(0.f); InitMaxMana(50.f); InitMagicResistance(0.f);
	InitMaxStamina(100.f); InitMaxPoise(50.f); InitTenacity(0.f);
	InitPhysicalDamage(0.f); InitMagicDamage(0.f);
	InitArmorPenetration(0.f); InitMagicPenetration(0.f);
	InitCritChance(0.f); InitCritDamage(0.f);

	// Resistencias (inicialmente 0). ORIGEN (decisión cerrada): árbol de habilidades,
	// accesorios y armaduras — NO derivan de primarios. El Override placeholder desde
	// Resilience en GE_SecondaryAttributes debe eliminarse en el editor (Etapa 3).
	InitFireResistance(0.f); InitWaterResistance(0.f);
	InitStormResistance(0.f); InitNatureResistance(0.f);

	// Potencia ofensiva de los estados (porcentaje adicional). Empieza en 0 y
	// la modifican árbol/equipamiento mediante GameplayEffects Infinite Add.
	InitFireStatusPower(0.f); InitStormStatusPower(0.f);
	InitWaterStatusPower(0.f); InitNatureStatusPower(0.f);

	// Modificadores de payload desbloqueables por árbol/equipamiento.
	InitFireMaxHealthDamagePercent(0.f); InitNatureMaxHealthDamagePercent(0.f);
	InitStormCurrentHealthDamagePercent(0.f); InitStormMissingHealthDamagePercent(0.f);
	InitFireArmorReduction(0.f); InitFireMagicResistanceReduction(0.f);
	InitNatureArmorReduction(0.f); InitNatureMagicResistanceReduction(0.f);
	InitGrievousWounds(0.f);
	InitGrievousWoundsOnHitPercent(0.f); InitGrievousWoundsOnHitDuration(0.f);
	InitGrievousWoundsIntensityBonus(0.f); InitGrievousWoundsDurationBonus(0.f);

	// Barras de buildup elemental (inicialmente 0 — vacías). Solo las llena el
	// ExecCalc con golpes elementales; solo las vacía el decay o el disparo del estado.
	InitFireBuildup(0.f); InitStormBuildup(0.f);
	InitWaterBuildup(0.f); InitNatureBuildup(0.f);

	// Vitales
	InitHealth(75.f); InitMana(50.f); InitStamina(50.f); InitPoise(50.f);

	// Meta
	InitIncomingDamage(0.f); InitIncomingPoiseDamage(0.f); InitIncomingXP(0.f); InitIncomingHealing(0.f);

	// ===== TagsToAttributes =====
	const FPantheliaGameplayTags& Tags = FPantheliaGameplayTags::Get();

	TagsToAttributes.Add(Tags.Attributes_Primary_Hardness, GetHardnessAttribute);
	TagsToAttributes.Add(Tags.Attributes_Primary_Resonance, GetResonanceAttribute);
	TagsToAttributes.Add(Tags.Attributes_Primary_Resilience, GetResilienceAttribute);
	TagsToAttributes.Add(Tags.Attributes_Primary_Endurance, GetEnduranceAttribute);
	TagsToAttributes.Add(Tags.Attributes_Primary_Spirit, GetSpiritAttribute);

	TagsToAttributes.Add(Tags.Attributes_Secondary_MaxHealth, GetMaxHealthAttribute);
	TagsToAttributes.Add(Tags.Attributes_Secondary_MaxMana, GetMaxManaAttribute);
	TagsToAttributes.Add(Tags.Attributes_Secondary_MaxStamina, GetMaxStaminaAttribute);
	TagsToAttributes.Add(Tags.Attributes_Secondary_MaxPoise, GetMaxPoiseAttribute);
	TagsToAttributes.Add(Tags.Attributes_Secondary_Armor, GetArmorAttribute);
	TagsToAttributes.Add(Tags.Attributes_Secondary_MagicResistance, GetMagicResistanceAttribute);
	TagsToAttributes.Add(Tags.Attributes_Secondary_Tenacity, GetTenacityAttribute);
	TagsToAttributes.Add(Tags.Attributes_Secondary_PhysicalDamage, GetPhysicalDamageAttribute);
	TagsToAttributes.Add(Tags.Attributes_Secondary_MagicDamage, GetMagicDamageAttribute);
	TagsToAttributes.Add(Tags.Attributes_Secondary_ArmorPenetration, GetArmorPenetrationAttribute);
	TagsToAttributes.Add(Tags.Attributes_Secondary_MagicPenetration, GetMagicPenetrationAttribute);
	TagsToAttributes.Add(Tags.Attributes_Secondary_CritChance, GetCritChanceAttribute);
	TagsToAttributes.Add(Tags.Attributes_Secondary_CritDamage, GetCritDamageAttribute);

	// Resistencias elementales — visibles en el menú de atributos
	TagsToAttributes.Add(Tags.Attributes_Resistance_Fire, GetFireResistanceAttribute);
	TagsToAttributes.Add(Tags.Attributes_Resistance_Water, GetWaterResistanceAttribute);
	TagsToAttributes.Add(Tags.Attributes_Resistance_Storm, GetStormResistanceAttribute);
	TagsToAttributes.Add(Tags.Attributes_Resistance_Nature, GetNatureResistanceAttribute);

	// Potencia de estado del atacante — visible para UI, árbol y equipamiento.
	TagsToAttributes.Add(Tags.Attributes_StatusPower_Fire, GetFireStatusPowerAttribute);
	TagsToAttributes.Add(Tags.Attributes_StatusPower_Storm, GetStormStatusPowerAttribute);
	TagsToAttributes.Add(Tags.Attributes_StatusPower_Water, GetWaterStatusPowerAttribute);
	TagsToAttributes.Add(Tags.Attributes_StatusPower_Nature, GetNatureStatusPowerAttribute);

	// Modificadores de payload de estados y antiheal.
	TagsToAttributes.Add(Tags.Attributes_StatusDamage_Fire_MaxHealthPercent, GetFireMaxHealthDamagePercentAttribute);
	TagsToAttributes.Add(Tags.Attributes_StatusDamage_Nature_MaxHealthPercent, GetNatureMaxHealthDamagePercentAttribute);
	TagsToAttributes.Add(Tags.Attributes_StatusDamage_Storm_CurrentHealthPercent, GetStormCurrentHealthDamagePercentAttribute);
	TagsToAttributes.Add(Tags.Attributes_StatusDamage_Storm_MissingHealthPercent, GetStormMissingHealthDamagePercentAttribute);
	TagsToAttributes.Add(Tags.Attributes_StatusDebuff_Fire_ArmorReduction, GetFireArmorReductionAttribute);
	TagsToAttributes.Add(Tags.Attributes_StatusDebuff_Fire_MagicResistanceReduction, GetFireMagicResistanceReductionAttribute);
	TagsToAttributes.Add(Tags.Attributes_StatusDebuff_Nature_ArmorReduction, GetNatureArmorReductionAttribute);
	TagsToAttributes.Add(Tags.Attributes_StatusDebuff_Nature_MagicResistanceReduction, GetNatureMagicResistanceReductionAttribute);
	TagsToAttributes.Add(Tags.Attributes_Debuff_GrievousWounds_OnHitPercent, GetGrievousWoundsOnHitPercentAttribute);
	TagsToAttributes.Add(Tags.Attributes_Debuff_GrievousWounds_OnHitDuration, GetGrievousWoundsOnHitDurationAttribute);
	TagsToAttributes.Add(Tags.Attributes_Debuff_GrievousWounds_IntensityBonus, GetGrievousWoundsIntensityBonusAttribute);
	TagsToAttributes.Add(Tags.Attributes_Debuff_GrievousWounds_DurationBonus, GetGrievousWoundsDurationBonusAttribute);

	// Barras de buildup elemental — registradas desde el primer día para que la
	// futura UI de barras de estado del enemigo se bindee por tag sin tocar C++.
	TagsToAttributes.Add(Tags.Attributes_Buildup_Fire, GetFireBuildupAttribute);
	TagsToAttributes.Add(Tags.Attributes_Buildup_Storm, GetStormBuildupAttribute);
	TagsToAttributes.Add(Tags.Attributes_Buildup_Water, GetWaterBuildupAttribute);
	TagsToAttributes.Add(Tags.Attributes_Buildup_Nature, GetNatureBuildupAttribute);
}

void UPantheliaAttributeSet::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);

	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, Hardness, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, Resonance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, Resilience, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, Endurance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, Spirit, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, MaxHealth, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, Armor, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, MaxMana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, MagicResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, MaxStamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, MaxPoise, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, Tenacity, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, PhysicalDamage, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, MagicDamage, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, ArmorPenetration, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, MagicPenetration, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, CritChance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, CritDamage, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, FireResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, WaterResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, StormResistance, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, NatureResistance, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, FireStatusPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, StormStatusPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, WaterStatusPower, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, NatureStatusPower, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, FireMaxHealthDamagePercent, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, NatureMaxHealthDamagePercent, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, StormCurrentHealthDamagePercent, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, StormMissingHealthDamagePercent, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, FireArmorReduction, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, FireMagicResistanceReduction, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, NatureArmorReduction, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, NatureMagicResistanceReduction, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, GrievousWounds, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, GrievousWoundsOnHitPercent, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, GrievousWoundsOnHitDuration, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, GrievousWoundsIntensityBonus, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, GrievousWoundsDurationBonus, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, FireBuildup, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, StormBuildup, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, WaterBuildup, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, NatureBuildup, COND_None, REPNOTIFY_Always);

	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, Mana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, Poise, COND_None, REPNOTIFY_Always);
	// IncomingDamage, IncomingPoiseDamage, IncomingXP e IncomingHealing: meta atributos, NO se replican.
}

void UPantheliaAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute() && GetMaxHealth() > 0.f) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	else if (Attribute == GetManaAttribute() && GetMaxMana() > 0.f) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxMana());
	else if (Attribute == GetStaminaAttribute() && GetMaxStamina() > 0.f) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
	else if (Attribute == GetPoiseAttribute() && GetMaxPoise() > 0.f) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxPoise());
	// Armor y MagicResistance pueden bajar temporalmente por estados, pero nunca
	// deben presentar un valor efectivo negativo en GAS/UI.
	else if (Attribute == GetArmorAttribute()) NewValue = FMath::Max(NewValue, 0.f);
	else if (Attribute == GetMagicResistanceAttribute()) NewValue = FMath::Max(NewValue, 0.f);
	// Las resistencias se clampean a 0-100 aquí para que nunca sean negativas ni > 100%
	else if (Attribute == GetFireResistanceAttribute())   NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
	else if (Attribute == GetWaterResistanceAttribute())  NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
	else if (Attribute == GetStormResistanceAttribute())  NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
	else if (Attribute == GetNatureResistanceAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
	// Status Power puede acumularse sin cap superior fijo: el balance vive en los
	// GEs del árbol/equipamiento. Sí impedimos valores negativos, que invertirían
	// el payload y podrían convertir daño en curación accidental.
	else if (Attribute == GetFireStatusPowerAttribute())   NewValue = FMath::Max(NewValue, 0.f);
	else if (Attribute == GetStormStatusPowerAttribute())  NewValue = FMath::Max(NewValue, 0.f);
	else if (Attribute == GetWaterStatusPowerAttribute())  NewValue = FMath::Max(NewValue, 0.f);
	else if (Attribute == GetNatureStatusPowerAttribute()) NewValue = FMath::Max(NewValue, 0.f);

	// Los porcentajes desbloqueables son puntos porcentuales. Se impide que sean
	// negativos y se clampean a 100 para evitar daño porcentual inválido.
	else if (Attribute == GetFireMaxHealthDamagePercentAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
	else if (Attribute == GetNatureMaxHealthDamagePercentAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
	else if (Attribute == GetStormCurrentHealthDamagePercentAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
	else if (Attribute == GetStormMissingHealthDamagePercentAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
	else if (Attribute == GetFireArmorReductionAttribute()) NewValue = FMath::Max(NewValue, 0.f);
	else if (Attribute == GetFireMagicResistanceReductionAttribute()) NewValue = FMath::Max(NewValue, 0.f);
	else if (Attribute == GetNatureArmorReductionAttribute()) NewValue = FMath::Max(NewValue, 0.f);
	else if (Attribute == GetNatureMagicResistanceReductionAttribute()) NewValue = FMath::Max(NewValue, 0.f);
	else if (Attribute == GetGrievousWoundsAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
	else if (Attribute == GetGrievousWoundsOnHitPercentAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
	else if (Attribute == GetGrievousWoundsOnHitDurationAttribute()) NewValue = FMath::Max(NewValue, 0.f);
	else if (Attribute == GetGrievousWoundsIntensityBonusAttribute()) NewValue = FMath::Max(NewValue, 0.f);
	else if (Attribute == GetGrievousWoundsDurationBonusAttribute()) NewValue = FMath::Max(NewValue, 0.f);

	// Barras de buildup: clamp 0..BuildupThreshold. El disparo del umbral NO vive
	// aquí (PreAttributeChange no debe tener lógica de juego, solo clamps) — vive en
	// HandleElementalBuildup, que solo corre cuando el buildup entra por el ExecCalc.
	else if (Attribute == GetFireBuildupAttribute())   NewValue = FMath::Clamp(NewValue, 0.f, BuildupThreshold);
	else if (Attribute == GetStormBuildupAttribute())  NewValue = FMath::Clamp(NewValue, 0.f, BuildupThreshold);
	else if (Attribute == GetWaterBuildupAttribute())  NewValue = FMath::Clamp(NewValue, 0.f, BuildupThreshold);
	else if (Attribute == GetNatureBuildupAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, BuildupThreshold);
}

void UPantheliaAttributeSet::PostAttributeChange(const FGameplayAttribute& Attribute, float OldValue, float NewValue)
{
	Super::PostAttributeChange(Attribute, OldValue, NewValue);

	// === CLAMP DE VITALES CUANDO CAMBIA SU MAX ===
	//
	// (Ver la explicación completa del porqué en el .h, sobre la declaración.)
	//
	// Regla: si un Max BAJA y deja al vital por encima del nuevo techo, recortamos
	// el vital al techo. Si el Max SUBE, no tocamos nada (decisión soulslike).
	//
	// Detalles de implementación importantes:
	//
	// 1. Usamos los setters del macro ATTRIBUTE_ACCESSORS (SetHealth, etc.), que
	//    internamente llaman a SetNumericAttributeBase en el ASC. Eso modifica el
	//    BASE value — necesario, porque el estado corrupto (vida > máximo) vive en
	//    el base, no solo en el current. Un clamp solo del current dejaría el base
	//    corrupto esperando a reaparecer.
	//
	// 2. Guard NewValue > 0.f: durante la construcción del AttributeSet los Max
	//    pueden pasar transitoriamente por 0 antes de que los GEs de inicialización
	//    los fijen. Sin este guard, clampearíamos los vitales a 0 en ese instante
	//    (matando al personaje al spawnear). Es el mismo guard que ya usa
	//    PreAttributeChange con GetMaxHealth() > 0.f, por el mismo motivo.
	//
	// 3. Solo escribimos si hace falta (vital > nuevo max). Escribir siempre
	//    dispararía deltas de atributo innecesarios hacia la UI en cada recálculo
	//    de secundarios (RefreshSecondaryAttributes remueve y reaplica el GE, lo
	//    que pasa por aquí aunque el valor final no cambie).
	if (Attribute == GetMaxHealthAttribute() && NewValue > 0.f)
	{
		if (GetHealth() > NewValue) SetHealth(NewValue);
	}
	else if (Attribute == GetMaxManaAttribute() && NewValue > 0.f)
	{
		if (GetMana() > NewValue) SetMana(NewValue);
	}
	else if (Attribute == GetMaxStaminaAttribute() && NewValue > 0.f)
	{
		if (GetStamina() > NewValue) SetStamina(NewValue);
	}
	else if (Attribute == GetMaxPoiseAttribute() && NewValue > 0.f)
	{
		if (GetPoise() > NewValue) SetPoise(NewValue);
	}
}

void UPantheliaAttributeSet::SetEffectProperties(const FGameplayEffectModCallbackData& Data, FEffectProperties& Props) const
{
	Props.EffectContextHandle = Data.EffectSpec.GetContext();
	Props.SourceASC = Props.EffectContextHandle.GetOriginalInstigatorAbilitySystemComponent();

	if (IsValid(Props.SourceASC) && Props.SourceASC->AbilityActorInfo.IsValid() && Props.SourceASC->AbilityActorInfo->AvatarActor.IsValid())
	{
		Props.SourceAvatarActor = Props.SourceASC->AbilityActorInfo->AvatarActor.Get();
		Props.SourceController = Props.SourceASC->AbilityActorInfo->PlayerController.Get();
		if (!Props.SourceController)
			if (const APawn* Pawn = Cast<APawn>(Props.SourceAvatarActor))
				Props.SourceController = Pawn->GetController();
		if (Props.SourceController)
			Props.SourceCharacter = Cast<ACharacter>(Props.SourceController->GetPawn());
	}

	if (Data.Target.AbilityActorInfo.IsValid() && Data.Target.AbilityActorInfo->AvatarActor.IsValid())
	{
		Props.TargetAvatarActor = Data.Target.AbilityActorInfo->AvatarActor.Get();
		Props.TargetController = Data.Target.AbilityActorInfo->PlayerController.Get();
		Props.TargetCharacter = Cast<ACharacter>(Props.TargetAvatarActor);
		Props.TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Props.TargetAvatarActor);
	}
}

void UPantheliaAttributeSet::PostGameplayEffectExecute(const FGameplayEffectModCallbackData& Data)
{
	Super::PostGameplayEffectExecute(Data);

	FEffectProperties Props;
	SetEffectProperties(Data, Props);

	// --- BLOQUEO GLOBAL: nada de esto se procesa si el TARGET ya está muerto (clase 310) ---
	// Sin este check, un debuff cuyo GE periódico todavía no ha expirado seguiría restando
	// vida, disparando HitReact, etc. sobre un personaje ya muerto — el bug real que
	// describe la clase 310 ("los enemigos siguen recibiendo daño de debuff después de
	// morir, porque siguen 'rondando' unos instantes"). Comprobamos el TARGET (quien
	// recibe este GE en concreto), no el Source — un error fácil de cometer (el propio
	// curso lo comete primero y lo corrige en la misma clase; aquí vamos directo a la
	// versión correcta sin repetir ese paso en falso).
	// IsValid() primero: TargetAvatarActor puede no existir para ciertos GEs sin target claro.
	if (IsValid(Props.TargetAvatarActor) &&
		Props.TargetAvatarActor->Implements<UCombatInterface>() &&
		ICombatInterface::Execute_IsDead(Props.TargetAvatarActor))
	{
		// Un ExecCalc puede producir varios output modifiers. Si IncomingDamage mata
		// primero, los outputs posteriores todavía llegan uno a uno a este callback.
		// Limpiamos el atributo evaluado antes de salir para que un cadáver no conserve
		// buildup/meta valores que puedan reaparecer tras un respawn con ASC persistente.
		const FGameplayAttribute& EvaluatedAttribute = Data.EvaluatedData.Attribute;
		if (EvaluatedAttribute == GetFireBuildupAttribute()) SetFireBuildup(0.f);
		else if (EvaluatedAttribute == GetStormBuildupAttribute()) SetStormBuildup(0.f);
		else if (EvaluatedAttribute == GetWaterBuildupAttribute()) SetWaterBuildup(0.f);
		else if (EvaluatedAttribute == GetNatureBuildupAttribute()) SetNatureBuildup(0.f);
		else if (EvaluatedAttribute == GetIncomingDamageAttribute()) SetIncomingDamage(0.f);
		else if (EvaluatedAttribute == GetIncomingPoiseDamageAttribute()) SetIncomingPoiseDamage(0.f);
		else if (EvaluatedAttribute == GetIncomingXPAttribute()) SetIncomingXP(0.f);
		else if (EvaluatedAttribute == GetIncomingHealingAttribute()) SetIncomingHealing(0.f);
		return;
	}

	// Clamp de Health para GEs legacy que lo modifican DIRECTAMENTE. Las curaciones
	// nuevas deben entrar por IncomingHealing para que Heridas Graves pueda reducirlas.
	// El daño NO pasa por aquí — todo el daño entra
	// por el meta atributo IncomingDamage (rama de abajo), que ya clampea y además
	// decide la muerte. Por eso esta rama solo recorta a [0, MaxHealth] y NO llama a
	// Die(): la muerte es responsabilidad exclusiva del pipeline de IncomingDamage.
	// Sin esta rama, una curación con Add que sobrepase MaxHealth dependería solo del
	// clamp del CurrentValue en PreAttributeChange, dejando el BaseValue por encima
	// del máximo (mismo problema de base corrupto explicado en PostAttributeChange).
	if (Data.EvaluatedData.Attribute == GetHealthAttribute())   SetHealth(FMath::Clamp(GetHealth(), 0.f, GetMaxHealth()));
	else if (Data.EvaluatedData.Attribute == GetManaAttribute())     SetMana(FMath::Clamp(GetMana(), 0.f, GetMaxMana()));
	else if (Data.EvaluatedData.Attribute == GetStaminaAttribute())  SetStamina(FMath::Clamp(GetStamina(), 0.f, GetMaxStamina()));
	else if (Data.EvaluatedData.Attribute == GetPoiseAttribute())    SetPoise(FMath::Clamp(GetPoise(), 0.f, GetMaxPoise()));
	else if (Data.EvaluatedData.Attribute == GetIncomingDamageAttribute())
	{
		HandleIncomingDamage(Props, Data.EffectSpec);
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingHealingAttribute())
	{
		HandleIncomingHealing(Props);
	}
	// --- BARRAS DE BUILDUP ELEMENTAL (sistema de umbral, sin azar) ---
	// Estas ramas SOLO se ejecutan cuando el buildup entra como output modifier del
	// ExecCalc (un golpe elemental). El decay (TickBuildupDecay en CharacterBase)
	// escribe la base con SetNumericAttributeBase, que NO dispara
	// PostGameplayEffectExecute — por diseño: vaciar la barra jamás debe re-evaluar
	// el umbral. Una rama por elemento porque GAS llama a esta función una vez por
	// modificador de salida, con Data.EvaluatedData señalando el atributo concreto.
	else if (Data.EvaluatedData.Attribute == GetFireBuildupAttribute())
	{
		HandleElementalBuildup(Data, EPantheliaElement::Fire);
	}
	else if (Data.EvaluatedData.Attribute == GetStormBuildupAttribute())
	{
		HandleElementalBuildup(Data, EPantheliaElement::Storm);
	}
	else if (Data.EvaluatedData.Attribute == GetWaterBuildupAttribute())
	{
		HandleElementalBuildup(Data, EPantheliaElement::Water);
	}
	else if (Data.EvaluatedData.Attribute == GetNatureBuildupAttribute())
	{
		HandleElementalBuildup(Data, EPantheliaElement::Nature);
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingPoiseDamageAttribute())
	{
		const float LocalIncomingPoiseDamage = GetIncomingPoiseDamage();
		SetIncomingPoiseDamage(0.f);

		if (LocalIncomingPoiseDamage > 0.f && IsValid(Props.TargetAvatarActor))
		{
			float FlinchThreshold = 10.f;
			if (ICombatInterface* CI = Cast<ICombatInterface>(Props.TargetAvatarActor))
				FlinchThreshold = CI->GetFlinchThreshold();

			if (GetMaxPoise() > 0.f)
			{
				const float PoiseDamagePercent = (LocalIncomingPoiseDamage / GetMaxPoise()) * 100.f;
				if (PoiseDamagePercent >= FlinchThreshold)
				{
					FGameplayTagContainer HitReactContainer;
					HitReactContainer.AddTag(FPantheliaGameplayTags::Get().Effects_HitReact);
					Props.TargetASC->TryActivateAbilitiesByTag(HitReactContainer);
				}
			}

			const float NewPoise = FMath::Clamp(GetPoise() - LocalIncomingPoiseDamage, 0.f, GetMaxPoise());
			SetPoise(NewPoise);

			if (NewPoise <= 0.f)
			{
				FGameplayTagContainer StaggerContainer;
				StaggerContainer.AddTag(FPantheliaGameplayTags::Get().Effects_Stagger);
				Props.TargetASC->TryActivateAbilitiesByTag(StaggerContainer);
				SetPoise(GetMaxPoise()); // reset postura al stagear (como Elden Ring)
			}

			if (ICombatInterface* CI = Cast<ICombatInterface>(Props.TargetAvatarActor))
				CI->ResetPoiseRegenTimer();
		}
	}
	else if (Data.EvaluatedData.Attribute == GetIncomingXPAttribute())
	{
		HandleIncomingXP(Props);
	}
}

// ============================================================
// HandleIncomingDamage (clase 309)
// ============================================================
// Extraído tal cual del bloque IncomingDamage que antes vivía inline en
// PostGameplayEffectExecute. Único cambio de comportamiento: la llamada a Debuff(Props)
// insertada justo después de HandleParryReaction — si ExecCalc_Damage (DetermineDebuff)
// confirmó un debuff exitoso en este golpe, reaccionamos aquí. Se coloca en ese punto
// (no dentro del "if LocalIncomingDamage > 0") porque un debuff se resuelve por su propio
// SetByCaller de tipo de daño en el spec — es información independiente de si el daño
// PRINCIPAL de este golpe en particular fue > 0.
void UPantheliaAttributeSet::HandleIncomingDamage(const FEffectProperties& Props, const FGameplayEffectSpec& EffectSpec)
{
	const float LocalIncomingDamage = GetIncomingDamage();
	SetIncomingDamage(0.f);

	// --- REACCION DE PARRY / BLOQUEO (modelo Lies of P) ---
	// Leemos el resultado que el ExecCalc escribio en el context. Si hubo PARRY
	// perfecto del tipo correcto, aplicamos dano de postura AL ATACANTE y disparamos
	// los efectos elementales (gancho). El bloqueo no daña la postura del atacante.
	HandleParryReaction(Props);

	// --- DEBUFF: la ruta antigua por context (clases 307-309) FUE ELIMINADA ---
	// El disparador de los efectos de estado ya no es un dado en el ExecCalc leído
	// aquí vía IsSuccessfulDebuff — es el UMBRAL de las barras de buildup (sistema
	// soulslike, sin azar). Ver HandleElementalBuildup más abajo: el ExecCalc deposita
	// buildup en los atributos XBuildup, y cuando una barra llega a BuildupThreshold,
	// ESE es el momento en que el estado se dispara, con certeza.
	// (Los campos de debuff del FPantheliaGameplayEffectContext se conservan en el
	// struct pero ya nadie los escribe — quedan como infraestructura reservada.)

	if (LocalIncomingDamage > 0.f)
	{
		// Guardamos la vida previa ANTES de escribirla — antes el log la reconstruía
		// como GetHealth() + daño, lo que en un overkill (daño 100 sobre 30 de vida,
		// clampeado a 0) imprimía "100.0 → 0.0" en vez de "30.0 → 0.0". Cosmético,
		// pero un log de diagnóstico que miente es peor que no tener log.
		const float OldHealth = GetHealth();
		const float NewHealth = FMath::Clamp(OldHealth - LocalIncomingDamage, 0.f, GetMaxHealth());
		SetHealth(NewHealth);

		// Log de diagnóstico: todo daño que llega por GAS.
		// Nos confirma que el sistema de daño está procesando al target.
		// Quitar cuando el sistema XP funcione correctamente.
		UE_LOG(LogPanthelia, Log,
			TEXT("[DAMAGE] Target: '%s' | Damage: %.1f | HP: %.1f → %.1f | Fatal: %s"),
			Props.TargetCharacter ? *Props.TargetCharacter->GetName() : TEXT("null"),
			LocalIncomingDamage, OldHealth, NewHealth,
			NewHealth <= 0.f ? TEXT("SI") : TEXT("no"));

		const bool bFatal = NewHealth <= 0.f;
		const bool bIsCriticalHit = UPantheliaAbilitySystemLibrary::IsCriticalHit(Props.EffectContextHandle);

		if (bFatal)
		{
			// DeathImpulse (clase 314): lo escribió el proyectil (u otra fuente de daño en
			// el futuro) directamente en el context, ANTES de aplicar el efecto — ver
			// APantheliaProjectile::OnSphereOverlap (clase 313). Props.EffectContextHandle
			// es el mismo context de ESE golpe (SetEffectProperties lo copia desde Data al
			// principio de PostGameplayEffectExecute), así que llega intacto hasta aquí.
			if (ICombatInterface* CI = Cast<ICombatInterface>(Props.TargetAvatarActor))
			{
				CI->Die(UPantheliaAbilitySystemLibrary::GetDeathImpulse(Props.EffectContextHandle));
			}
			// El enemigo ha muerto: enviamos la recompensa de XP al atacante.
			// SendXPEvent calcula la XP final (base × multiplicador de rendimientos
			// decrecientes) y la concede directamente en C++ puro: detecta las subidas
			// de nivel, rellena vida/maná y llama a AddToXP (ver SendXPEvent abajo).
			SendXPEvent(Props);
		}
		else
		{
			// HitReact lo activa IncomingPoiseDamage según FlinchThreshold
			// TODO: VFX/SFX de crítico cuando bIsCriticalHit == true

			// --- LAUNCH / NIVEL 3 primero, KNOCKBACK después: EXCLUYENTES (fix auditoría) ---
			//
			// Antes eran dos bloques independientes que podían dispararse AMBOS en el mismo
			// golpe (dados independientes): dos LaunchCharacter seguidos, donde el segundo
			// sobreescribía al primero silenciosamente. Ahora se evalúa primero el Launch
			// (la reacción más fuerte: lanzamiento aéreo + GA_GetUp) y, SOLO si no procede,
			// el Knockback (empujón a ras de suelo, Nivel 1/2). El lanzamiento "contiene"
			// al empujón — aplicar los dos a la vez nunca fue un resultado deseable.
			//
			// Ambos usan LaunchCharacter y NO AddImpulse. POR QUÉ (la pista del propio
			// curso, que sí vale la pena entender): AddImpulse necesita que el mesh esté
			// simulando física (ragdoll) para tener cualquier efecto — eso es exactamente
			// lo que pasa al morir (MulticastHandleDeath activa el ragdoll primero). Pero
			// un personaje VIVO sigue controlado por su CharacterMovementComponent, no por
			// física de ragdoll — un impulso físico no haría absolutamente nada mientras
			// el personaje sigue caminando/atacando normalmente. LaunchCharacter, en
			// cambio, es un método pensado exactamente para esto: le dice al sistema de
			// movimiento del personaje "muévete con esta velocidad ahora", sin tocar
			// física ni ragdoll. Por eso ambos viven en la rama NO fatal — un golpe fatal
			// usa el impulso de muerte (bloque bFatal arriba), que es físico de verdad.
			//
			// IsNearlyZero con tolerancia 1.0: comparar un FVector contra el cero exacto
			// es frágil con floats (imprecisión de punto flotante). Una tolerancia pequeña
			// (1 unidad de Unreal, prácticamente imperceptible) evita procesar un vector
			// que en la práctica es cero por error de redondeo. Además, tras el fix de
			// "escribir siempre" (proyectil/WeaponTrace), el cero es ahora el valor
			// explícito de "esta tirada no salió" — este chequeo es el consumidor de esa
			// convención.
			const FVector LaunchForce = UPantheliaAbilitySystemLibrary::GetLaunchForce(Props.EffectContextHandle);
			const bool bLaunchApplies = !LaunchForce.IsNearlyZero(1.f) && Props.TargetCharacter && Props.TargetASC;

			if (bLaunchApplies)
			{
				// Sistema del Nivel 3 — su propio campo de contexto (LaunchForce), y además
				// concede State.Airborne, que hace dos cosas: (1) bloquea GA_HitReact
				// mientras el personaje está en el aire (State.Airborne debe estar en los
				// Activation Blocked Tags de GA_HitReact en el editor); (2) le dice a
				// Landed() (PantheliaCharacterBase.cpp) que ESTE aterrizaje debe disparar
				// GA_GetUp, a diferencia de un salto o caída normal.
				//
				// bXYOverride=true, bZOverride=true: reemplazamos la velocidad actual del
				// personaje en los 3 ejes por completo (no la sumamos) — así el lanzamiento
				// es siempre igual de contundente sin importar si el personaje ya se estaba
				// moviendo o no. Ver LaunchCharacter en la documentación de ACharacter.
				Props.TargetCharacter->LaunchCharacter(LaunchForce, true, true);

				// SetLooseGameplayTagCount(Tag, 1) en vez de AddLooseGameplayTag: un tag
				// suelto normal se concede con un contador que hay que igualar al quitarlo
				// (si lo concedes 2 veces, hay que quitarlo 2 veces). Fijar el contador
				// directamente a 1 evita ese problema — no importa cuántas veces se llame
				// esto mientras el personaje sigue en el aire, el contador se queda en 1,
				// y Landed() lo apaga de un solo golpe con SetLooseGameplayTagCount(0).
				Props.TargetASC->SetLooseGameplayTagCount(
					FPantheliaGameplayTags::Get().State_Airborne, 1);
			}
			else
			{
				// --- KNOCKBACK (clase 315) — solo si el Launch NO procedió ---
				const FVector KnockbackForce = UPantheliaAbilitySystemLibrary::GetKnockbackForce(Props.EffectContextHandle);

				if (!KnockbackForce.IsNearlyZero(1.f) && Props.TargetCharacter)
				{
					Props.TargetCharacter->LaunchCharacter(KnockbackForce, true, true);

					// --- NIVEL 2: KNOCKBACK PESADO (a petición) ---
					// Si la ability marcó este knockback como "pesado" (bKnockbackIsHeavy en
					// UPantheliaDamageGameplayAbility), en vez de dejar que HitReact conviva con
					// el empujón (comportamiento normal, Nivel 1), lo bloqueamos brevemente y
					// disparamos una reacción dedicada (GA_HeavyKnockback) — un ataque fuerte
					// merece una animación de "salir despedido con fuerza", no la mueca genérica
					// de HitReact mientras el cuerpo patina varios metros.
					if (UPantheliaAbilitySystemLibrary::IsKnockbackHeavy(Props.EffectContextHandle) && Props.TargetASC)
					{
						// Concede State.HeavyKnockback durante 1 segundo — tiempo suficiente para
						// cubrir la reacción sin tener que sincronizarlo a mano con la duración
						// exacta del montage (si la reacción termina antes, el tag simplemente
						// sigue activo un instante más sin bloquear nada nuevo relevante; si
						// necesitas más margen, sube este número). A diferencia de State.Airborne
						// (Nivel 3), aquí no hay un evento físico como "aterrizar" que nos diga
						// cuándo quitarlo, así que una duración fija tiene sentido.
						UPantheliaAbilitySystemLibrary::GrantTemporaryGameplayTag(
							Props.TargetASC, FPantheliaGameplayTags::Get().State_HeavyKnockback, 1.f);

						// Dispara GA_HeavyKnockback — mismo patrón que GA_HitReact/GA_GetUp
						// (activación externa vía TryActivateAbilitiesByTag con su Ability Tag).
						FGameplayTagContainer HeavyKnockbackTags;
						HeavyKnockbackTags.AddTag(FPantheliaGameplayTags::Get().Effects_HeavyKnockback);
						Props.TargetASC->TryActivateAbilitiesByTag(HeavyKnockbackTags);
					}
				}
			}
		}
	}

	// Heridas Graves directas: cualquier golpe/tick que realmente causó daño puede
	// aplicarlas inmediatamente. Un parry perfecto las niega junto con el golpe.
	const FPantheliaGameplayTags& GameplayTags = FPantheliaGameplayTags::Get();
	const float SpecGrievousPercent = EffectSpec.GetSetByCallerMagnitude(
		GameplayTags.CombatTricks_GrievousWoundsPercent, false, 0.f);
	const float SpecGrievousDuration = EffectSpec.GetSetByCallerMagnitude(
		GameplayTags.CombatTricks_GrievousWoundsDuration, false, 4.f);

	float EquipmentGrievousPercent = 0.f;
	float EquipmentGrievousDuration = 0.f;
	if (Props.SourceASC)
	{
		EquipmentGrievousPercent = Props.SourceASC->GetNumericAttribute(
			GetGrievousWoundsOnHitPercentAttribute());
		EquipmentGrievousDuration = Props.SourceASC->GetNumericAttribute(
			GetGrievousWoundsOnHitDurationAttribute());
	}

	// Intensidad y duración se eligen como una pareja. Tomar el máximo de cada
	// columna por separado podría combinar el porcentaje de un arma con la duración
	// de una ability distinta, creando una fuente que nunca existió realmente.
	float RequestedGrievousPercent = SpecGrievousPercent;
	float RequestedGrievousDuration = SpecGrievousDuration;
	if (EquipmentGrievousPercent > SpecGrievousPercent)
	{
		RequestedGrievousPercent = EquipmentGrievousPercent;
		RequestedGrievousDuration = EquipmentGrievousDuration;
	}
	else if (FMath::IsNearlyEqual(EquipmentGrievousPercent, SpecGrievousPercent) &&
		EquipmentGrievousPercent > 0.f)
	{
		RequestedGrievousDuration = FMath::Max(
			SpecGrievousDuration, EquipmentGrievousDuration);
	}

	const FPantheliaGameplayEffectContext* PantheliaContext =
		static_cast<const FPantheliaGameplayEffectContext*>(Props.EffectContextHandle.Get());
	const bool bWasPerfectParry = PantheliaContext && PantheliaContext->WasParried();
	const bool bTargetIsDead = IsValid(Props.TargetAvatarActor) &&
		Props.TargetAvatarActor->Implements<UCombatInterface>() &&
		ICombatInterface::Execute_IsDead(Props.TargetAvatarActor);

	if (LocalIncomingDamage > 0.f && RequestedGrievousPercent > 0.f &&
		!bWasPerfectParry && !bTargetIsDead)
	{
		UPantheliaAbilitySystemLibrary::ApplyGrievousWounds(
			Props.SourceASC, Props.TargetASC,
			RequestedGrievousPercent, RequestedGrievousDuration,
			GameplayTags.Effects_GrievousWounds_Direct, true);
	}
}

// ============================================================
// HandleIncomingXP (clase 309)
// ============================================================
// Extraído tal cual del bloque IncomingXP que antes vivía inline en
// PostGameplayEffectExecute. Sin cambios de comportamiento — solo reubicación.
void UPantheliaAttributeSet::HandleIncomingXP(const FEffectProperties& Props)
{
	// === META ATRIBUTO IncomingXP ===
	//
	// Patrón estándar: guardar localmente y zerear de inmediato.
	// El multiplicador de rendimientos decrecientes ya fue aplicado en SendXPEvent.
	const float LocalIncomingXP = GetIncomingXP();
	SetIncomingXP(0.f);

	if (LocalIncomingXP <= 0.f) return;

	// Solo el jugador implementa IPantheliaPlayerInterface.
	if (!Props.SourceCharacter->Implements<UPantheliaPlayerInterface>()) return;

	// === DETECCIÓN DE NIVEL ASCENDENTE ===
	//
	// Calculamos cuántos niveles subirá el jugador con esta XP ANTES de añadirla.
	// Esto nos permite:
	//   a) Llamar Execute_LevelUp para los efectos visuales/sonidos de cada subida.
	//   b) Rellenar salud y maná al subir de nivel (comportamiento soulslike estándar).
	//
	// La lógica de datos (incrementar Level, AttributePoints, SkillPoints y broadcastear
	// delegates) la maneja Execute_AddToXP → UpdateLevelFromXP internamente.
	// NO llamamos Execute_AddToPlayerLevel/AddToAttributePoints/AddToSkillPoints desde
	// aquí para evitar doble conteo con UpdateLevelFromXP.
	//
	// Source character es el dueño de GA_ListenForXPEvents que aplicó el GE sobre sí mismo.
	const int32 CurrentLevel = ICombatInterface::Execute_GetPlayerLevel(Props.SourceCharacter);
	const int32 CurrentXP = IPantheliaPlayerInterface::Execute_GetXP(Props.SourceCharacter);
	const int32 FinalXP = FMath::RoundToInt(LocalIncomingXP);
	const int32 NewLevel = IPantheliaPlayerInterface::Execute_FindLevelForXP(
		Props.SourceCharacter, CurrentXP + FinalXP);
	const int32 NumLevelUps = NewLevel - CurrentLevel;

	UE_LOG(LogPanthelia, Log,
		TEXT("[XP] IncomingXP: %.1f → FinalXP: %d | Nivel actual: %d | Nivel nuevo: %d | Subidas: %d"),
		LocalIncomingXP, FinalXP, CurrentLevel, NewLevel, NumLevelUps);

	if (NumLevelUps > 0)
	{
		// Efectos visuales/sonidos de subida de nivel (implementados en Blueprint).
		// Vacío en C++ — se sobreescribe en BP_ThirdPersonCharacter.
		IPantheliaPlayerInterface::Execute_LevelUp(Props.SourceCharacter);

		// Al subir de nivel, rellenamos salud y maná completamente.
		// Comportamiento soulslike estándar (Elden Ring, Dark Souls, Black Myth Wukong).
		// Nota: los cambios de SetHealth/SetMana aquí son locales al AttributeSet del
		// atacante (Props.SourceCharacter tiene su propio ASC y AttributeSet).
		// Como no hay multiplicador, accedemos directamente.
		if (UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Props.SourceCharacter))
		{
			if (const UPantheliaAttributeSet* SourceAS = SourceASC->GetSet<UPantheliaAttributeSet>())
			{
				// Usamos SetAttributeBaseValue para modificar el atributo del Source, no del Self.
				SourceASC->SetNumericAttributeBase(GetHealthAttribute(), SourceAS->GetMaxHealth());
				SourceASC->SetNumericAttributeBase(GetManaAttribute(), SourceAS->GetMaxMana());
			}
		}
	}

	// AddToXP → UpdateLevelFromXP gestiona: XP, Level, AttributePoints, SkillPoints, delegates.
	IPantheliaPlayerInterface::Execute_AddToXP(Props.SourceCharacter, FinalXP);
}

// ============================================================
// HandleIncomingHealing — pipeline central de curación
// ============================================================
void UPantheliaAttributeSet::HandleIncomingHealing(const FEffectProperties& Props)
{
	const float RawHealing = GetIncomingHealing();
	SetIncomingHealing(0.f);

	if (RawHealing <= 0.f) return;

	const float HealingReductionPercent =
		UPantheliaAbilitySystemLibrary::GetActiveGrievousWoundsPercent(Props.TargetASC);
	const float FinalHealing = RawHealing * (1.f - HealingReductionPercent / 100.f);

	const float OldHealth = GetHealth();
	const float NewHealth = FMath::Clamp(OldHealth + FinalHealing, 0.f, GetMaxHealth());
	SetHealth(NewHealth);

	UE_LOG(LogPanthelia, Log,
		TEXT("[HEALING] Target '%s' | Raw %.2f | Grievous %.1f%% | Final %.2f | HP %.2f -> %.2f"),
		Props.TargetCharacter ? *Props.TargetCharacter->GetName() : TEXT("null"),
		RawHealing, HealingReductionPercent, FinalHealing, OldHealth, NewHealth);
}

// ============================================================
// ESTADOS ELEMENTALES — GameplayEffects dinámicos cacheados
// ============================================================
// La definición de diseño (magnitud base, duración, frecuencia y tipo de payload)
// vive en DA_ElementalStatusConfig. Aquí solo se materializa esa definición como
// un GameplayEffect runtime cuando la barra alcanza el umbral.
//
// El GE se construye una vez por AttributeSet/tag/frecuencia y se reutiliza.
// Los valores finales de cada aplicación viajan por SetByCaller después de aplicar
// el Status Power del atacante; un proc nuevo reemplaza al anterior por tag.
//
// LIMITACIÓN DEL MOTOR: los GameplayEffects creados en runtime no son una solución
// apropiada para replicación. Panthelia es single-player, así que no afecta al diseño.
// ============================================================
// SISTEMA DE BUILDUP — disparo por umbral (reemplaza al dado del curso)
// ============================================================

void UPantheliaAttributeSet::HandleElementalBuildup(const FGameplayEffectModCallbackData& Data, EPantheliaElement Element)
{
	// Props: mismas propiedades de efecto que usa el resto de handlers (fuente,
	// target, ASCs, context) — las necesita el disparador para construir el estado.
	FEffectProperties Props;
	SetEffectProperties(Data, Props);

	// Leer/clampear/reescribir la barra del elemento. El clamp de PreAttributeChange
	// ya recortó el CurrentValue, pero reescribimos la BASE explícitamente (mismo
	// razonamiento del base corrupto documentado en PostAttributeChange): la barra
	// es estado persistente y su base jamás debe superar el umbral.
	float Current = 0.f;
	switch (Element)
	{
		case EPantheliaElement::Fire:   Current = GetFireBuildup();   break;
		case EPantheliaElement::Storm:  Current = GetStormBuildup();  break;
		case EPantheliaElement::Water:  Current = GetWaterBuildup();  break;
		case EPantheliaElement::Nature: Current = GetNatureBuildup(); break;
		default: return;
	}

	const float Clamped = FMath::Clamp(Current, 0.f, BuildupThreshold);

	// ¿Se llenó la barra? El estado se dispara CON CERTEZA (sin dado) y la barra se
	// resetea a 0 — exactamente el ciclo de Elden Ring/Lies of P: acumular, procar,
	// volver a empezar. Nota: >= y no ==, porque el clamp puede dejar el valor
	// EXACTAMENTE en el umbral desde cualquier exceso.
	const bool bTriggered = Clamped >= BuildupThreshold;
	const float NewValue = bTriggered ? 0.f : Clamped;

	switch (Element)
	{
		case EPantheliaElement::Fire:   SetFireBuildup(NewValue);   break;
		case EPantheliaElement::Storm:  SetStormBuildup(NewValue);  break;
		case EPantheliaElement::Water:  SetWaterBuildup(NewValue);  break;
		case EPantheliaElement::Nature: SetNatureBuildup(NewValue); break;
		default: break;
	}

	// Avisar al personaje de que recibió buildup, para que (re)arranque su timer de
	// decay — mismo patrón exacto que ResetPoiseRegenTimer para la postura: el
	// AttributeSet no gestiona timers, se lo pide al Character vía la interfaz.
	if (ICombatInterface* CombatInterface = Cast<ICombatInterface>(Props.TargetAvatarActor))
	{
		CombatInterface->NotifyElementalBuildupReceived();
	}

	if (bTriggered)
	{
		TriggerElementalStatus(Props, Element);
	}
}

void UPantheliaAttributeSet::TriggerElementalStatus(const FEffectProperties& Props, EPantheliaElement Element)
{
	const FPantheliaGameplayTags& GameplayTags = FPantheliaGameplayTags::Get();

	// Identidad del estado (Debuff.Burn/Shock/Saturation/Poison).
	const FGameplayTag* DebuffTagPtr = GameplayTags.ElementToDebuff.Find(Element);
	if (!DebuffTagPtr || !DebuffTagPtr->IsValid())
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("[STATUS] El elemento %d no tiene tag en ElementToDebuff."),
			static_cast<uint8>(Element));
		return;
	}

	// La definición del estado es GLOBAL. Ya no se lee daño/duración/frecuencia del
	// spec del último golpe, por lo que todas las fuentes del mismo elemento disparan
	// la misma Quemadura/Electrocución/Saturación/Veneno base.
	const UPantheliaCharacterClassInfo* ClassInfo =
		UPantheliaAbilitySystemLibrary::GetCharacterClassInfo(Props.TargetAvatarActor);
	if (!ClassInfo || !ClassInfo->ElementalStatusConfig)
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[STATUS] Falta ElementalStatusConfig en DA_CharacterClassInfo. No se puede disparar '%s'."),
			*DebuffTagPtr->ToString());
		return;
	}

	const FPantheliaElementalStatusDefinition* Definition =
		ClassInfo->ElementalStatusConfig->FindStatusDefinition(Element, true);
	if (!Definition) return;

	// El source puede aumentar la potencia mediante árbol/equipamiento. Además,
	// todo daño ofensivo de Quemadura, Veneno y Electrocución pasa por MagicDamage:
	// los estados son payloads mágicos globales, aunque los haya detonado un arma.
	float StatusPower = 0.f;
	float SourceMagicDamage = 0.f;
	if (Props.SourceASC)
	{
		SourceMagicDamage = FMath::Max(
			Props.SourceASC->GetNumericAttribute(GetMagicDamageAttribute()), 0.f);

		if (const FGameplayTag* StatusPowerTag = GameplayTags.ElementToStatusPower.Find(Element))
		{
			if (const auto* AttributeGetter = TagsToAttributes.Find(*StatusPowerTag))
			{
				StatusPower = FMath::Max(
					Props.SourceASC->GetNumericAttribute((*AttributeGetter)()), 0.f);
			}
		}
	}

	const float MagnitudeMultiplier = FMath::Max(
		0.f,
		1.f + (StatusPower * Definition->MagnitudePercentPerStatusPower / 100.f));
	const float DurationMultiplier = FMath::Max(
		0.f,
		1.f + (StatusPower * Definition->DurationPercentPerStatusPower / 100.f));

	const float MagicDamageContribution = SourceMagicDamage * Definition->FlatDamagePerMagicDamage;
	const float FinalMagnitude = FMath::Max(
		(Definition->BaseMagnitude + MagicDamageContribution) * MagnitudeMultiplier, 0.f);
	const float FinalDuration = FMath::Max(Definition->BaseDuration * DurationMultiplier, 0.f);
	const float TickFrequency = FMath::Max(Definition->TickFrequency, 0.f);

	// ===== PORCENTAJES DE VIDA DESBLOQUEADOS POR EL BUILD =====
	// Los atributos base empiezan en 0. Mientras sigan en 0, el coeficiente de
	// Status Power NO activa la rama por sí solo. El perk/equipo es la llave y el
	// Status Power solo escala una mecánica ya desbloqueada.
	float BaseMaxHealthPercent = 0.f;
	float BaseCurrentHealthPercent = 0.f;
	float BaseMissingHealthPercent = 0.f;

	if (Props.SourceASC)
	{
		switch (Element)
		{
			case EPantheliaElement::Fire:
				BaseMaxHealthPercent = Props.SourceASC->GetNumericAttribute(
					GetFireMaxHealthDamagePercentAttribute());
				break;

			case EPantheliaElement::Nature:
				BaseMaxHealthPercent = Props.SourceASC->GetNumericAttribute(
					GetNatureMaxHealthDamagePercentAttribute());
				break;

			case EPantheliaElement::Storm:
				BaseCurrentHealthPercent = Props.SourceASC->GetNumericAttribute(
					GetStormCurrentHealthDamagePercentAttribute());
				BaseMissingHealthPercent = Props.SourceASC->GetNumericAttribute(
					GetStormMissingHealthDamagePercentAttribute());
				break;

			default:
				break;
		}
	}

	auto CalculateUnlockedPercent = [StatusPower, SourceMagicDamage](
		float BasePercent,
		float PercentPerStatusPower,
		float PercentPerMagicDamage)
	{
		if (BasePercent <= 0.f) return 0.f;
		return FMath::Clamp(
			BasePercent +
			(StatusPower * PercentPerStatusPower) +
			(SourceMagicDamage * PercentPerMagicDamage),
			0.f, 100.f);
	};

	const float FinalMaxHealthPercent = CalculateUnlockedPercent(
		BaseMaxHealthPercent,
		Definition->MaxHealthPercentPerStatusPower,
		Definition->MaxHealthPercentPerMagicDamage);
	const float FinalCurrentHealthPercent = CalculateUnlockedPercent(
		BaseCurrentHealthPercent,
		Definition->CurrentHealthPercentPerStatusPower,
		Definition->CurrentHealthPercentPerMagicDamage);
	const float FinalMissingHealthPercent = CalculateUnlockedPercent(
		BaseMissingHealthPercent,
		Definition->MissingHealthPercentPerStatusPower,
		Definition->MissingHealthPercentPerMagicDamage);

	// Snapshot al momento de detonar el estado. Para DoT, el porcentaje de vida
	// máxima queda fijado durante esa aplicación; si MaxHealth cambia a mitad del
	// efecto, la siguiente aplicación recalculará con el nuevo valor.
	const float TargetMaxHealth = FMath::Max(GetMaxHealth(), 0.f);
	const float TargetCurrentHealth = FMath::Clamp(GetHealth(), 0.f, TargetMaxHealth);
	const float TargetMissingHealth = FMath::Max(TargetMaxHealth - TargetCurrentHealth, 0.f);

	const float MaxHealthDamage = TargetMaxHealth * FinalMaxHealthPercent / 100.f;
	const float CurrentHealthDamage = TargetCurrentHealth * FinalCurrentHealthPercent / 100.f;
	const float MissingHealthDamage = TargetMissingHealth * FinalMissingHealthPercent / 100.f;
	const float PercentageHealthDamage = MaxHealthDamage + CurrentHealthDamage + MissingHealthDamage;

	const float FinalGrievousWoundsPercent = Definition->bAppliesGrievousWounds
		? FMath::Clamp(
			Definition->GrievousWoundsPercent +
			(StatusPower * Definition->GrievousWoundsPercentPerStatusPower),
			0.f, 100.f)
		: 0.f;

	// Reducción defensiva: atributos del source desbloqueados por perks/objetos.
	float FinalArmorReduction = 0.f;
	float FinalMagicResistanceReduction = 0.f;
	FGameplayTag DefenseShredTag;
	if (Props.SourceASC)
	{
		switch (Element)
		{
			case EPantheliaElement::Fire:
				FinalArmorReduction = FMath::Max(Props.SourceASC->GetNumericAttribute(GetFireArmorReductionAttribute()), 0.f);
				FinalMagicResistanceReduction = FMath::Max(Props.SourceASC->GetNumericAttribute(GetFireMagicResistanceReductionAttribute()), 0.f);
				DefenseShredTag = GameplayTags.Effects_DefenseShred_Burn;
				break;
			case EPantheliaElement::Nature:
				FinalArmorReduction = FMath::Max(Props.SourceASC->GetNumericAttribute(GetNatureArmorReductionAttribute()), 0.f);
				FinalMagicResistanceReduction = FMath::Max(Props.SourceASC->GetNumericAttribute(GetNatureMagicResistanceReductionAttribute()), 0.f);
				DefenseShredTag = GameplayTags.Effects_DefenseShred_Poison;
				break;
			default:
				break;
		}
	}

	// Daño de postura de la detonación: una sola aplicación, no por tick.
	const float FinalPoiseDamage = FMath::Max(
		Definition->BasePoiseDamage +
		(StatusPower * Definition->PoiseDamagePerStatusPower) +
		(SourceMagicDamage * Definition->PoiseDamagePerMagicDamage),
		0.f);

	switch (Definition->PayloadType)
	{
		case EPantheliaElementalStatusPayload::DamageOverTime:
		{
			// Si el estado aplica Heridas Graves (Veneno), ambos efectos comparten
			// exactamente la misma duración. Se garantiza el mínimo global de 4s
			// elevando la duración del propio Veneno, no dejando antiheal huérfano.
			const float AppliedDuration = FinalGrievousWoundsPercent > 0.f
				? FMath::Max(FinalDuration, 4.f)
				: FinalDuration;

			// Quemadura/Veneno: el componente porcentual se suma a CADA tick.
			// El porcentaje base vive en el atributo concedido por el perk; el daño
			// plano y los coeficientes globales viven en el Data Asset.
			const float FinalTickDamage = FinalMagnitude + PercentageHealthDamage;
			ApplyElementalDebuff(
				Props, *DebuffTagPtr, FinalTickDamage, AppliedDuration, TickFrequency);

			const bool bTargetDiedFromInitialTick =
				IsValid(Props.TargetAvatarActor) &&
				Props.TargetAvatarActor->Implements<UCombatInterface>() &&
				ICombatInterface::Execute_IsDead(Props.TargetAvatarActor);

			if (!bTargetDiedFromInitialTick)
			{
				if (FinalGrievousWoundsPercent > 0.f)
				{
					ApplyGrievousWounds(Props, AppliedDuration, FinalGrievousWoundsPercent);
				}
				ApplyElementalDefenseShred(
					Props, *DebuffTagPtr, DefenseShredTag, AppliedDuration,
					FinalArmorReduction, FinalMagicResistanceReduction);
			}
			break;
		}

		case EPantheliaElementalStatusPayload::BurstDamage:
		{
			// Electrocución: daño plano global + vida actual + vida faltante. Cada
			// rama porcentual puede estar apagada (atributo 0), encendida sola o
			// combinarse con la otra según las decisiones del árbol.
			const float FinalBurstDamage = FinalMagnitude + PercentageHealthDamage;
			ApplyInstantElementalDamage(Props, *DebuffTagPtr, FinalBurstDamage);

			// BaseDuration > 0 permite mantener Debuff.Shock para Niagara/UI después
			// de la detonación. Con 0, Electrocución es puramente instantánea.
			const bool bTargetDiedFromBurst =
				IsValid(Props.TargetAvatarActor) &&
				Props.TargetAvatarActor->Implements<UCombatInterface>() &&
				ICombatInterface::Execute_IsDead(Props.TargetAvatarActor);

			if (FinalDuration > 0.f && !bTargetDiedFromBurst)
			{
				ApplyElementalDebuff(Props, *DebuffTagPtr, 0.f, FinalDuration, 0.f);
			}
			break;
		}

		case EPantheliaElementalStatusPayload::AttributeDebuff:
			// El payload defensivo principal de Saturación sigue pendiente, pero su
			// tag ya puede vivir durante la duración configurada para Niagara/UI/pasivas.
			if (FinalDuration > 0.f)
			{
				ApplyElementalDebuff(Props, *DebuffTagPtr, 0.f, FinalDuration, 0.f);
			}
			UE_LOG(LogPanthelia, Warning,
				TEXT("[STATUS] '%s': tag y postura implementados; AttributeDebuff principal pendiente."),
				*DebuffTagPtr->ToString());
			break;

		default:
			UE_LOG(LogPanthelia, Warning,
				TEXT("[STATUS] Payload desconocido para '%s'."), *DebuffTagPtr->ToString());
			break;
	}

	const bool bTargetDeadAfterPayload =
		IsValid(Props.TargetAvatarActor) &&
		Props.TargetAvatarActor->Implements<UCombatInterface>() &&
		ICombatInterface::Execute_IsDead(Props.TargetAvatarActor);
	if (FinalPoiseDamage > 0.f && !bTargetDeadAfterPayload)
	{
		ApplyInstantElementalPoiseDamage(Props, *DebuffTagPtr, FinalPoiseDamage);
	}

	UE_LOG(LogPanthelia, Log,
		TEXT("[STATUS] '%s' en '%s' | Flat %.2f | MagicDamage %.2f (contrib %.2f) | StatusPower %.2f | MaxHP %.3f%% | CurrentHP %.3f%% | MissingHP %.3f%% | PercentDamage %.2f | Poise %.2f | ArmorShred %.2f | MRShred %.2f | Dur %.2f | Freq %.2f | Grievous %.1f%%"),
		*DebuffTagPtr->ToString(),
		Props.TargetCharacter ? *Props.TargetCharacter->GetName() : TEXT("null"),
		FinalMagnitude, SourceMagicDamage, MagicDamageContribution, StatusPower,
		FinalMaxHealthPercent, FinalCurrentHealthPercent, FinalMissingHealthPercent,
		PercentageHealthDamage, FinalPoiseDamage, FinalArmorReduction,
		FinalMagicResistanceReduction, FinalDuration, TickFrequency,
		FinalGrievousWoundsPercent);
}

void UPantheliaAttributeSet::ApplyElementalDebuff(const FEffectProperties& Props, const FGameplayTag& DebuffTag,
	float Damage, float Duration, float Frequency)
{
	if (!DebuffTag.IsValid() || !Props.SourceASC || !Props.TargetASC) return;

	// Un estado sin duración no tiene sentido — dato malo, rechazado con aviso.
	if (Duration <= 0.f)
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("[DEBUFF] Duracion invalida (%.2f) para '%s' — no se aplica. Revisa BaseDuration en DA_ElementalStatusConfig."),
			Duration, *DebuffTag.ToString());
		return;
	}

	// ¿Este estado tiquea daño (DoT) o es solo-tag? Con Damage <= 0 construimos la
	// variante SOLO-TAG: sin modificador, sin Period — el GE existe únicamente para
	// conceder el tag de identidad durante la duración (lo que necesitará Saturación).
	const bool bIsDoT = Damage > 0.f;

	// === GUARD DE FRECUENCIA (solo aplica a la variante DoT) ===
	// Period = 0 en un GE con un modificador sobre IncomingDamage NO significa "sin
	// ticks": convierte el modificador Additive en un AGREGADO CONTINUO silencioso
	// sobre el meta atributo durante toda la duración. Dato malo → rechazado.
	if (bIsDoT && Frequency <= 0.f)
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("[DEBUFF] Frecuencia invalida (%.2f) para el DoT de '%s' — no se aplica. Revisa TickFrequency en DA_ElementalStatusConfig."),
			Frequency, *DebuffTag.ToString());
		return;
	}

	// El SourceASC crea el contexto — quien llenó la barra es quien origina el estado.
	FGameplayEffectContextHandle EffectContextHandle = Props.SourceASC->MakeEffectContext();
	EffectContextHandle.AddSourceObject(Props.SourceAvatarActor);

	const FPantheliaGameplayTags& GameplayTags = FPantheliaGameplayTags::Get();

	// === DEFINICIÓN CACHEADA (ver la explicación completa sobre CachedDebuffEffects
	// en el .h: arregla el stacking real y elimina la colisión de nombres) ===
	//
	// La clave incluye el tag Y la variante: para los DoT también la frecuencia (el
	// Period es propiedad FIJA de la definición, no puede ser SetByCaller); la
	// variante solo-tag no tiene Period y usa su propia clave. Daño y duración SÍ
	// son SetByCaller del spec (abajo), por eso no fragmentan el caché.
	const FName CacheKey = bIsDoT
		? FName(*FString::Printf(TEXT("DynamicDebuff_%s_P%.2f"), *DebuffTag.ToString(), Frequency))
		: FName(*FString::Printf(TEXT("DynamicStatus_%s"), *DebuffTag.ToString()));

	TObjectPtr<UGameplayEffect>& CachedEffect = CachedDebuffEffects.FindOrAdd(CacheKey);
	if (!CachedEffect)
	{
		// Primera vez que ESTE AttributeSet necesita esta combinación: se construye
		// una definición y queda cacheada. Usar this como Outer evita colisiones entre
		// personajes que creen el mismo CacheKey dentro de GetTransientPackage.
		UGameplayEffect* Effect = NewObject<UGameplayEffect>(this, CacheKey);

		// Duración: el estado dura un tiempo (no es instantáneo ni infinito).
		Effect->DurationPolicy = EGameplayEffectDurationType::HasDuration;

		// DurationMagnitude por SetByCaller: cada APLICACIÓN trae su propia duración
		// en el spec — imprescindible para que la definición sea reutilizable entre
		// abilities con duraciones distintas. Tag Debuff.Duration (clase 304).
		FSetByCallerFloat DurationSetByCaller;
		DurationSetByCaller.DataTag = GameplayTags.Debuff_Duration;
		Effect->DurationMagnitude = FGameplayEffectModifierMagnitude(DurationSetByCaller);

		// --- TAG CONCEDIDO AL TARGET ---
		// ADAPTACIÓN DE API DE MOTOR (requisito de UE 5.8, no decisión nuestra): el
		// curso usa "InheritableOwnedTagsContainer.AddTag(tag)", DEPRECADO desde
		// UE 5.3. Forma correcta en 5.8: UTargetTagsGameplayEffectComponent +
		// FInheritedTagContainer + SetAndApplyTargetTagChanges().
		FInheritedTagContainer TagContainer;
		TagContainer.AddTag(DebuffTag);
		UTargetTagsGameplayEffectComponent& TagComponent =
			Effect->AddComponent<UTargetTagsGameplayEffectComponent>();
		TagComponent.SetAndApplyTargetTagChanges(TagContainer);

		// No configuramos StackingType en la definición dinámica. Esa propiedad está
		// deprecada para escritura directa en UE 5.8 y, además, AggregateBySource
		// permitiría Quemaduras paralelas de atacantes distintos. La unicidad se
		// resuelve de forma determinista justo antes de aplicar: se elimina cualquier
		// estado activo con este tag y se aplica el spec nuevo.

		if (bIsDoT)
		{
			// Period = cada cuántos segundos tiquea el daño. Fijo en la definición
			// (por eso forma parte de la clave del caché).
			Effect->Period = Frequency;

			// EXPLÍCITO (decisión documentada): el primer tick se ejecuta EN EL
			// INSTANTE de aplicarse — el estado "muerde" junto al golpe que llenó la
			// barra, y luego cada Period. Interruptor por si se prefiere lo contrario.
			Effect->bExecutePeriodicEffectOnApplication = true;

			// --- MODIFICADOR: cuánto daño tiquea cada Period ---
			// Magnitud por SetByCaller (mismo motivo que la duración). Tag
			// Debuff.Damage (clase 304). NOTA: el tick modifica IncomingDamage
			// DIRECTAMENTE, sin pasar por el ExecCalc — por eso los i-frames NO
			// bloquean los ticks (decisión cerrada, ver el chequeo de State.Invulnerable
			// en ExecCalc_Damage).
			const int32 Index = Effect->Modifiers.Num();
			Effect->Modifiers.SetNum(Index + 1);
			FGameplayModifierInfo& ModifierInfo = Effect->Modifiers[Index];

			FSetByCallerFloat DamageSetByCaller;
			DamageSetByCaller.DataTag = GameplayTags.Debuff_Damage;
			ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(DamageSetByCaller);
			ModifierInfo.ModifierOp = EGameplayModOp::Additive;
			ModifierInfo.Attribute = UPantheliaAttributeSet::GetIncomingDamageAttribute();
		}
		// (Variante solo-tag: sin Period ni modificadores — el GE solo concede el tag.)

		CachedEffect = Effect;
	}

	// --- CONSTRUIR EL SPEC (con los valores de ESTE estado) Y APLICARLO ---
	//
	// CORRECCIÓN respecto al curso (conservada del refactor original): el curso crea
	// el spec con "new FGameplayEffectSpec(...)" y nunca lo libera — fuga de memoria.
	// FGameplayEffectSpec es un struct normal: variable local en el stack.
	//
	// Nivel 1 siempre (simplificación deliberada): los estados no escalan por nivel
	// de ability en esta primera versión.
	FGameplayEffectSpec MutableSpec(CachedEffect, EffectContextHandle, 1.f);

	// Los valores concretos viajan como SetByCaller del spec — la definición cacheada
	// es genérica; el spec es lo específico de cada aplicación. El de daño se asigna
	// también en la variante solo-tag (nadie lo lee ahí, pero un SetByCaller asignado
	// de más es inofensivo y mantiene el código sin ramas extra).
	MutableSpec.SetSetByCallerMagnitude(GameplayTags.Debuff_Duration, Duration);
	MutableSpec.SetSetByCallerMagnitude(GameplayTags.Debuff_Damage, Damage);

	// Un solo estado elemental de cada tipo puede estar activo por target. Quitamos
	// el anterior por tag antes de aplicar el nuevo: esto refresca duración, actualiza
	// la magnitud si cambió el Status Power y evita stacks paralelos incluso cuando
	// diferentes enemigos contribuyen a la misma barra del jugador.
	FGameplayTagContainer ExistingStatusTag;
	ExistingStatusTag.AddTag(DebuffTag);
	Props.TargetASC->RemoveActiveEffectsWithGrantedTags(ExistingStatusTag);

	// IMPORTANTE (evita un bucle infinito): este context NUEVO no lleva buildup ni
	// flags de estado. Cuando el DoT tiquee daño (cada Period), disparará su propio
	// PostGameplayEffectExecute → HandleIncomingDamage — pero ese golpe interno no
	// deposita buildup (no trae SetByCallers de buildup ni pasa por el ExecCalc), así
	// que un estado jamás llena barras ni encadena otros estados. Si algún día se
	// quisiera lo contrario (estados que alimentan otras barras), este es el sitio
	// exacto — con mucho cuidado de no crear un ciclo.
	Props.TargetASC->ApplyGameplayEffectSpecToSelf(MutableSpec);
}

void UPantheliaAttributeSet::ApplyInstantElementalDamage(
	const FEffectProperties& Props,
	const FGameplayTag& DebuffTag,
	float Damage)
{
	if (!DebuffTag.IsValid() || !Props.SourceASC || !Props.TargetASC || Damage <= 0.f) return;

	FGameplayEffectContextHandle EffectContextHandle = Props.SourceASC->MakeEffectContext();
	EffectContextHandle.AddSourceObject(Props.SourceAvatarActor);

	const FPantheliaGameplayTags& GameplayTags = FPantheliaGameplayTags::Get();
	const FName CacheKey(*FString::Printf(TEXT("DynamicStatusBurst_%s"), *DebuffTag.ToString()));

	TObjectPtr<UGameplayEffect>& CachedEffect = CachedDebuffEffects.FindOrAdd(CacheKey);
	if (!CachedEffect)
	{
		UGameplayEffect* Effect = NewObject<UGameplayEffect>(this, CacheKey);
		Effect->DurationPolicy = EGameplayEffectDurationType::Instant;

		const int32 Index = Effect->Modifiers.Num();
		Effect->Modifiers.SetNum(Index + 1);
		FGameplayModifierInfo& ModifierInfo = Effect->Modifiers[Index];

		FSetByCallerFloat DamageSetByCaller;
		DamageSetByCaller.DataTag = GameplayTags.Debuff_Damage;
		ModifierInfo.ModifierMagnitude = FGameplayEffectModifierMagnitude(DamageSetByCaller);
		ModifierInfo.ModifierOp = EGameplayModOp::Additive;
		ModifierInfo.Attribute = UPantheliaAttributeSet::GetIncomingDamageAttribute();

		CachedEffect = Effect;
	}

	FGameplayEffectSpec MutableSpec(CachedEffect, EffectContextHandle, 1.f);
	MutableSpec.SetSetByCallerMagnitude(GameplayTags.Debuff_Damage, Damage);
	Props.TargetASC->ApplyGameplayEffectSpecToSelf(MutableSpec);
}

void UPantheliaAttributeSet::ApplyInstantElementalPoiseDamage(
	const FEffectProperties& Props,
	const FGameplayTag& DebuffTag,
	float PoiseDamage)
{
	if (!DebuffTag.IsValid() || !Props.SourceASC || !Props.TargetASC || PoiseDamage <= 0.f) return;

	FGameplayEffectContextHandle EffectContextHandle = Props.SourceASC->MakeEffectContext();
	EffectContextHandle.AddSourceObject(Props.SourceAvatarActor);

	const FPantheliaGameplayTags& GameplayTags = FPantheliaGameplayTags::Get();
	const FName CacheKey(*FString::Printf(TEXT("DynamicStatusPoise_%s"), *DebuffTag.ToString()));
	TObjectPtr<UGameplayEffect>& CachedEffect = CachedDebuffEffects.FindOrAdd(CacheKey);
	if (!CachedEffect)
	{
		UGameplayEffect* Effect = NewObject<UGameplayEffect>(this, CacheKey);
		Effect->DurationPolicy = EGameplayEffectDurationType::Instant;
		Effect->Modifiers.SetNum(1);

		FSetByCallerFloat PoiseSetByCaller;
		PoiseSetByCaller.DataTag = GameplayTags.Damage_Poise;
		Effect->Modifiers[0].ModifierMagnitude = FGameplayEffectModifierMagnitude(PoiseSetByCaller);
		Effect->Modifiers[0].ModifierOp = EGameplayModOp::Additive;
		Effect->Modifiers[0].Attribute = UPantheliaAttributeSet::GetIncomingPoiseDamageAttribute();
		CachedEffect = Effect;
	}

	FGameplayEffectSpec MutableSpec(CachedEffect, EffectContextHandle, 1.f);
	MutableSpec.SetSetByCallerMagnitude(GameplayTags.Damage_Poise, PoiseDamage);
	Props.TargetASC->ApplyGameplayEffectSpecToSelf(MutableSpec);
}

void UPantheliaAttributeSet::ApplyElementalDefenseShred(
	const FEffectProperties& Props,
	const FGameplayTag& DebuffTag,
	const FGameplayTag& DefenseShredTag,
	float Duration,
	float ArmorReduction,
	float MagicResistanceReduction)
{
	if (!DebuffTag.IsValid() || !DefenseShredTag.IsValid() ||
		!Props.SourceASC || !Props.TargetASC || Duration <= 0.f)
	{
		return;
	}

	ArmorReduction = FMath::Max(ArmorReduction, 0.f);
	MagicResistanceReduction = FMath::Max(MagicResistanceReduction, 0.f);
	if (ArmorReduction <= 0.f && MagicResistanceReduction <= 0.f) return;

	const FPantheliaGameplayTags& GameplayTags = FPantheliaGameplayTags::Get();

	// Refresca solo la reducción del mismo estado. Quemadura y Veneno usan hijos
	// distintos, así que pueden coexistir y sus modifiers Add se acumulan.
	FGameplayTagContainer ExistingShredTag;
	ExistingShredTag.AddTag(DefenseShredTag);
	Props.TargetASC->RemoveActiveEffectsWithGrantedTags(ExistingShredTag);

	FString SafeTagName = DefenseShredTag.ToString();
	SafeTagName.ReplaceInline(TEXT("."), TEXT("_"));
	const FName CacheKey(*FString::Printf(TEXT("DynamicDefenseShred_%s"), *SafeTagName));
	TObjectPtr<UGameplayEffect>& CachedEffect = CachedDebuffEffects.FindOrAdd(CacheKey);
	if (!CachedEffect)
	{
		UGameplayEffect* Effect = NewObject<UGameplayEffect>(this, CacheKey);
		Effect->DurationPolicy = EGameplayEffectDurationType::HasDuration;

		FSetByCallerFloat DurationSetByCaller;
		DurationSetByCaller.DataTag = GameplayTags.Debuff_Duration;
		Effect->DurationMagnitude = FGameplayEffectModifierMagnitude(DurationSetByCaller);

		FInheritedTagContainer GrantedTags;
		GrantedTags.AddTag(DebuffTag);
		GrantedTags.AddTag(GameplayTags.Effects_DefenseShred);
		GrantedTags.AddTag(DefenseShredTag);
		UTargetTagsGameplayEffectComponent& TagComponent =
			Effect->AddComponent<UTargetTagsGameplayEffectComponent>();
		TagComponent.SetAndApplyTargetTagChanges(GrantedTags);

		Effect->Modifiers.SetNum(2);

		FSetByCallerFloat ArmorSetByCaller;
		ArmorSetByCaller.DataTag = GameplayTags.Debuff_DefenseShred_ArmorReduction;
		Effect->Modifiers[0].ModifierMagnitude = FGameplayEffectModifierMagnitude(ArmorSetByCaller);
		Effect->Modifiers[0].ModifierOp = EGameplayModOp::Additive;
		Effect->Modifiers[0].Attribute = UPantheliaAttributeSet::GetArmorAttribute();

		FSetByCallerFloat MagicResistanceSetByCaller;
		MagicResistanceSetByCaller.DataTag = GameplayTags.Debuff_DefenseShred_MagicResistanceReduction;
		Effect->Modifiers[1].ModifierMagnitude = FGameplayEffectModifierMagnitude(MagicResistanceSetByCaller);
		Effect->Modifiers[1].ModifierOp = EGameplayModOp::Additive;
		Effect->Modifiers[1].Attribute = UPantheliaAttributeSet::GetMagicResistanceAttribute();
		CachedEffect = Effect;
	}

	FGameplayEffectContextHandle EffectContextHandle = Props.SourceASC->MakeEffectContext();
	EffectContextHandle.AddSourceObject(Props.SourceAvatarActor);
	FGameplayEffectSpec MutableSpec(CachedEffect, EffectContextHandle, 1.f);
	MutableSpec.SetSetByCallerMagnitude(GameplayTags.Debuff_Duration, Duration);
	MutableSpec.SetSetByCallerMagnitude(GameplayTags.Debuff_DefenseShred_ArmorReduction, -ArmorReduction);
	MutableSpec.SetSetByCallerMagnitude(
		GameplayTags.Debuff_DefenseShred_MagicResistanceReduction,
		-MagicResistanceReduction);
	Props.TargetASC->ApplyGameplayEffectSpecToSelf(MutableSpec);

	UE_LOG(LogPanthelia, Log,
		TEXT("[DEFENSE SHRED] '%s' en '%s' | Armor -%.2f | MagicResistance -%.2f | %.2fs"),
		*DefenseShredTag.ToString(),
		Props.TargetCharacter ? *Props.TargetCharacter->GetName() : TEXT("null"),
		ArmorReduction, MagicResistanceReduction, Duration);
}

void UPantheliaAttributeSet::ApplyGrievousWounds(
	const FEffectProperties& Props,
	float Duration,
	float ReductionPercent)
{
	if (!Props.SourceASC || !Props.TargetASC) return;

	UPantheliaAbilitySystemLibrary::ApplyGrievousWounds(
		Props.SourceASC,
		Props.TargetASC,
		ReductionPercent,
		Duration,
		FPantheliaGameplayTags::Get().Effects_GrievousWounds_Poison,
		false); // la duración debe seguir exactamente al Veneno
}

// ===== OnRep Primarios =====
void UPantheliaAttributeSet::OnRep_Hardness(const FGameplayAttributeData& O)  const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, Hardness, O); }
void UPantheliaAttributeSet::OnRep_Resonance(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, Resonance, O); }
void UPantheliaAttributeSet::OnRep_Resilience(const FGameplayAttributeData& O)const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, Resilience, O); }
void UPantheliaAttributeSet::OnRep_Endurance(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, Endurance, O); }
void UPantheliaAttributeSet::OnRep_Spirit(const FGameplayAttributeData& O)    const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, Spirit, O); }

// ===== OnRep Secundarios =====
void UPantheliaAttributeSet::OnRep_MaxHealth(const FGameplayAttributeData& O)        const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, MaxHealth, O); }
void UPantheliaAttributeSet::OnRep_Armor(const FGameplayAttributeData& O)            const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, Armor, O); }
void UPantheliaAttributeSet::OnRep_MaxMana(const FGameplayAttributeData& O)          const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, MaxMana, O); }
void UPantheliaAttributeSet::OnRep_MagicResistance(const FGameplayAttributeData& O)  const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, MagicResistance, O); }
void UPantheliaAttributeSet::OnRep_MaxStamina(const FGameplayAttributeData& O)       const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, MaxStamina, O); }
void UPantheliaAttributeSet::OnRep_MaxPoise(const FGameplayAttributeData& O)         const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, MaxPoise, O); }
void UPantheliaAttributeSet::OnRep_Tenacity(const FGameplayAttributeData& O)         const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, Tenacity, O); }
void UPantheliaAttributeSet::OnRep_PhysicalDamage(const FGameplayAttributeData& O)   const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, PhysicalDamage, O); }
void UPantheliaAttributeSet::OnRep_MagicDamage(const FGameplayAttributeData& O)      const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, MagicDamage, O); }
void UPantheliaAttributeSet::OnRep_ArmorPenetration(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, ArmorPenetration, O); }
void UPantheliaAttributeSet::OnRep_MagicPenetration(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, MagicPenetration, O); }
void UPantheliaAttributeSet::OnRep_CritChance(const FGameplayAttributeData& O)       const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, CritChance, O); }
void UPantheliaAttributeSet::OnRep_CritDamage(const FGameplayAttributeData& O)       const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, CritDamage, O); }

// ===== OnRep Resistencias =====
void UPantheliaAttributeSet::OnRep_FireResistance(const FGameplayAttributeData& O)   const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, FireResistance, O); }
void UPantheliaAttributeSet::OnRep_WaterResistance(const FGameplayAttributeData& O)  const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, WaterResistance, O); }
void UPantheliaAttributeSet::OnRep_StormResistance(const FGameplayAttributeData& O)  const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, StormResistance, O); }
void UPantheliaAttributeSet::OnRep_NatureResistance(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, NatureResistance, O); }

// ===== OnRep Potencia de estados =====
void UPantheliaAttributeSet::OnRep_FireStatusPower(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, FireStatusPower, O); }
void UPantheliaAttributeSet::OnRep_StormStatusPower(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, StormStatusPower, O); }
void UPantheliaAttributeSet::OnRep_WaterStatusPower(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, WaterStatusPower, O); }
void UPantheliaAttributeSet::OnRep_NatureStatusPower(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, NatureStatusPower, O); }

// ===== OnRep Modificadores de payload / antiheal =====
void UPantheliaAttributeSet::OnRep_FireMaxHealthDamagePercent(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, FireMaxHealthDamagePercent, O); }
void UPantheliaAttributeSet::OnRep_NatureMaxHealthDamagePercent(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, NatureMaxHealthDamagePercent, O); }
void UPantheliaAttributeSet::OnRep_StormCurrentHealthDamagePercent(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, StormCurrentHealthDamagePercent, O); }
void UPantheliaAttributeSet::OnRep_StormMissingHealthDamagePercent(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, StormMissingHealthDamagePercent, O); }
void UPantheliaAttributeSet::OnRep_FireArmorReduction(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, FireArmorReduction, O); }
void UPantheliaAttributeSet::OnRep_FireMagicResistanceReduction(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, FireMagicResistanceReduction, O); }
void UPantheliaAttributeSet::OnRep_NatureArmorReduction(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, NatureArmorReduction, O); }
void UPantheliaAttributeSet::OnRep_NatureMagicResistanceReduction(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, NatureMagicResistanceReduction, O); }
void UPantheliaAttributeSet::OnRep_GrievousWounds(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, GrievousWounds, O); }
void UPantheliaAttributeSet::OnRep_GrievousWoundsOnHitPercent(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, GrievousWoundsOnHitPercent, O); }
void UPantheliaAttributeSet::OnRep_GrievousWoundsOnHitDuration(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, GrievousWoundsOnHitDuration, O); }
void UPantheliaAttributeSet::OnRep_GrievousWoundsIntensityBonus(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, GrievousWoundsIntensityBonus, O); }
void UPantheliaAttributeSet::OnRep_GrievousWoundsDurationBonus(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, GrievousWoundsDurationBonus, O); }

void UPantheliaAttributeSet::OnRep_FireBuildup(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, FireBuildup, O); }
void UPantheliaAttributeSet::OnRep_StormBuildup(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, StormBuildup, O); }
void UPantheliaAttributeSet::OnRep_WaterBuildup(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, WaterBuildup, O); }
void UPantheliaAttributeSet::OnRep_NatureBuildup(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, NatureBuildup, O); }

// ===== OnRep Vitales =====
void UPantheliaAttributeSet::OnRep_Health(const FGameplayAttributeData& O)  const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, Health, O); }
void UPantheliaAttributeSet::OnRep_Mana(const FGameplayAttributeData& O)    const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, Mana, O); }
void UPantheliaAttributeSet::OnRep_Stamina(const FGameplayAttributeData& O) const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, Stamina, O); }
void UPantheliaAttributeSet::OnRep_Poise(const FGameplayAttributeData& O)   const { GAMEPLAYATTRIBUTE_REPNOTIFY(UPantheliaAttributeSet, Poise, O); }

void UPantheliaAttributeSet::HandleParryReaction(const FEffectProperties& Props)
{
	// Leer el resultado del parry/bloqueo que el ExecCalc escribio en el context.
	const FPantheliaGameplayEffectContext* PantheliaContext =
		static_cast<const FPantheliaGameplayEffectContext*>(Props.EffectContextHandle.Get());
	if (!PantheliaContext) return;

	const bool bParried = PantheliaContext->WasParried();
	const bool bBlocked = PantheliaContext->WasBlocked();

	// Si hubo parry o bloqueo, avisar a la ability de parry del DEFENSOR (el que recibe el
	// golpe = TargetASC) para que reproduzca el retroceso (ParryHit o BlockHit). Esto pasa
	// tanto en parry como en bloqueo; la diferencia (postura al atacante) se maneja abajo.
	if ((bParried || bBlocked) && IsValid(Props.TargetASC))
	{
		if (UPantheliaAbilitySystemComponent* PantheliaASC =
			Cast<UPantheliaAbilitySystemComponent>(Props.TargetASC))
		{
			PantheliaASC->NotifyParryImpact(bParried);
		}
	}

	// Solo el PARRY perfecto del tipo correcto daña la postura del atacante.
	// El bloqueo (imperfecto) mitiga dano pero no genera reaccion ofensiva.
	if (!bParried) return;

	const float PoiseToAttacker = PantheliaContext->GetParryPoiseDamageToAttacker();
	if (PoiseToAttacker <= 0.f) return;

	// El atacante es el Source del dano entrante. Le aplicamos dano de postura.
	UAbilitySystemComponent* AttackerASC = Props.SourceASC;
	AActor* AttackerAvatar = Props.SourceAvatarActor;
	if (!IsValid(AttackerASC) || !IsValid(AttackerAvatar)) return;

	// Obtener el AttributeSet del atacante para leer/escribir su postura.
	// GetSet<T>() es la API canonica de GAS para obtener un AttributeSet tipado del ASC.
	const UPantheliaAttributeSet* AttackerAttributes =
		AttackerASC->GetSet<UPantheliaAttributeSet>();
	if (!AttackerAttributes) return;

	const float AttackerMaxPoise = AttackerAttributes->GetMaxPoise();
	const float AttackerCurrentPoise = AttackerAttributes->GetPoise();
	const float NewAttackerPoise = FMath::Clamp(AttackerCurrentPoise - PoiseToAttacker, 0.f, AttackerMaxPoise);

	// Aplicar el dano de postura directamente al atributo del atacante.
	AttackerASC->SetNumericAttributeBase(UPantheliaAttributeSet::GetPoiseAttribute(), NewAttackerPoise);

	UE_LOG(LogTemp, Warning, TEXT("[Parry] PARRY PERFECTO -> dano de postura al atacante %s: %.1f (postura %.1f -> %.1f)"),
		*AttackerAvatar->GetName(), PoiseToAttacker, AttackerCurrentPoise, NewAttackerPoise);

	// Si la postura del atacante llega a 0, se aturde (stagger) — abierto a riposte.
	if (NewAttackerPoise <= 0.f)
	{
		FGameplayTagContainer StaggerContainer;
		StaggerContainer.AddTag(FPantheliaGameplayTags::Get().Effects_Stagger);
		AttackerASC->TryActivateAbilitiesByTag(StaggerContainer);
		UE_LOG(LogTemp, Warning, TEXT("[Parry] El atacante %s quedo STAGGER por el parry."), *AttackerAvatar->GetName());
	}

	// --- GANCHOS (sin implementar todavia) ---
	// TODO[Elemental]: disparar el efecto del corazon elemental equipado en parry perfecto.
	// TODO[SkillTree]: aplicar modificadores de arbol (mas dano de postura, devolver estamina).
	// TODO[Estado]: si el ataque era de fuego/rayo, el parry magico perfecto anula la barra
	//   de efecto de estado del jugador (cuando exista el sistema de estados).
	// TODO[Imbuir]: contar parrys magicos consecutivos para auto-imbuir el arma.
}

void UPantheliaAttributeSet::SendXPEvent(const FEffectProperties& Props)
{
	// === FLUJO DE RECOMPENSA DE XP ===
	//
	// Este método es el punto de partida de toda la cadena de XP:
	//
	//   1. Obtenemos BaseXPReward y EnemyID del enemigo muerto (target).
	//   2. Calculamos la XP final aplicando el multiplicador de rendimientos decrecientes.
	//   3. Registramos la muerte para futuras visitas del mismo enemigo.
	//   4. Concedemos la XP directamente al jugador en C++ (sin GA/GE intermedios).
	//      Detectamos subidas de nivel, llamamos LevelUp(), rellenamos vida/maná y
	//      llamamos AddToXP() para actualizar todo el estado de progresión.

	// Solo los enemigos dan XP. Si el target no es APantheliaEnemy (por ejemplo si
	// hubiera daño entre jugadores en el futuro), salimos silenciosamente.
	const APantheliaEnemy* Enemy = Cast<APantheliaEnemy>(Props.TargetCharacter);
	if (!Enemy)
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("[XP] SendXPEvent: '%s' no es APantheliaEnemy. Clase: %s. Sin recompensa de XP."),
			Props.TargetCharacter ? *Props.TargetCharacter->GetName() : TEXT("null"),
			Props.TargetCharacter ? *Props.TargetCharacter->GetClass()->GetName() : TEXT("null"));
		return;
	}

	const int32 BaseXP = Enemy->GetBaseXPReward();
	if (BaseXP <= 0)
	{
		// Enemigo sin recompensa configurada (BaseXPReward == 0 en el Blueprint).
		UE_LOG(LogPanthelia, Warning,
			TEXT("[XP] SendXPEvent: '%s' tiene BaseXPReward=0. Configura el valor en Details > Panthelia|XP."),
			*Enemy->GetName());
		return;
	}

	// --- Rendimientos decrecientes ---
	// Calculamos el multiplicador según cuántas veces el jugador ya ha matado
	// a ESTA instancia (identificada por EnemyID). Bosses/minibosses dejan EnemyID
	// en NAME_None → multiplier siempre 1.0 y sin registro de kills.
	const FName EnemyID = Enemy->GetEnemyID();
	int32 FinalXP = BaseXP;

	if (!EnemyID.IsNone())
	{
		// Obtenemos el PlayerState del atacante para leer y escribir el kill count.
		APlayerController* PC = Cast<APlayerController>(Props.SourceController);
		APantheliaPlayerState* PS = PC ? PC->GetPlayerState<APantheliaPlayerState>() : nullptr;

		if (PS)
		{
			const int32 KillCount = PS->GetEnemyKillCount(EnemyID);
			const float Multiplier = UPantheliaAbilitySystemLibrary::GetXPMultiplierForKillCount(KillCount);
			FinalXP = FMath::RoundToInt(BaseXP * Multiplier);

			// Registramos la muerte DESPUÉS de calcular la XP de esta visita.
			// Si lo registrásemos antes, la primera muerte contaría como segunda.
			PS->RecordEnemyKill(EnemyID);

			UE_LOG(LogPanthelia, Verbose,
				TEXT("[XP] Enemigo '%s' (ID='%s'): BaseXP=%d, KillCount=%d, x%.2f → FinalXP=%d"),
				*Props.TargetCharacter->GetName(), *EnemyID.ToString(),
				BaseXP, KillCount, Multiplier, FinalXP);
		}
	}

	// --- Conceder XP directamente al jugador (C++ puro, sin GA/GE intermedios) ---
	//
	// Arquitectura original usaba: SendGameplayEvent → GA_ListenForXPEvents → GE_EventBasedEffect → IncomingXP.
	// Esa cadena Blueprint era frágil (errores de compilación en GA_ListenForXPEvents, nodo deprecado
	// BP_ApplyGameplayEffectSpecToSelf, conexiones incorrectas de then/EventReceived).
	//
	// La nueva arquitectura hace todo en C++:
	//   1. Detecta cuántos niveles subirá el jugador con esta XP.
	//   2. Llama a LevelUp() para los efectos visuales/sonidos de cada subida.
	//   3. Rellena salud y maná si el jugador subió de nivel (comportamiento soulslike estándar).
	//   4. Llama a AddToXP() para actualizar XP, Level, AttributePoints, SkillPoints y broadcasts.
	//
	// GA_ListenForXPEvents y GE_EventBasedEffect quedan en el proyecto como gancho para
	// modificadores de XP externos (áreas de bonificación, consumibles, árbol de habilidades)
	// pero ya NO son la ruta principal de procesamiento de XP de muerte de enemigos.

	if (!Props.SourceCharacter) return;

	// Solo el jugador implementa IPantheliaPlayerInterface e ICombatInterface.
	const bool bImplementsPlayerInterface = Props.SourceCharacter->Implements<UPantheliaPlayerInterface>();
	const bool bImplementsCombatInterface = Props.SourceCharacter->Implements<UCombatInterface>();
	if (!bImplementsPlayerInterface || !bImplementsCombatInterface) return;

	// === DETECCIÓN DE NIVEL ASCENDENTE ===
	const int32 CurrentLevel = ICombatInterface::Execute_GetPlayerLevel(Props.SourceCharacter);
	const int32 CurrentXP = IPantheliaPlayerInterface::Execute_GetXP(Props.SourceCharacter);
	const int32 NewLevel = IPantheliaPlayerInterface::Execute_FindLevelForXP(
		Props.SourceCharacter, CurrentXP + FinalXP);
	const int32 NumLevelUps = NewLevel - CurrentLevel;

	UE_LOG(LogPanthelia, Log,
		TEXT("[XP] Concediendo %d XP a '%s' | XP actual: %d | Nivel: %d → %d"),
		FinalXP, *Props.SourceCharacter->GetName(), CurrentXP, CurrentLevel, NewLevel);

	if (NumLevelUps > 0)
	{
		// Efectos visuales/sonidos de subida de nivel (implementados en BP_ThirdPersonCharacter).
		IPantheliaPlayerInterface::Execute_LevelUp(Props.SourceCharacter);

		// Rellenar salud y maná al subir de nivel (soulslike estándar).
		if (UAbilitySystemComponent* SourceASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Props.SourceCharacter))
		{
			if (const UPantheliaAttributeSet* SourceAS = SourceASC->GetSet<UPantheliaAttributeSet>())
			{
				SourceASC->SetNumericAttributeBase(GetHealthAttribute(), SourceAS->GetMaxHealth());
				SourceASC->SetNumericAttributeBase(GetManaAttribute(), SourceAS->GetMaxMana());
			}
		}
	}

	// AddToXP → UpdateLevelFromXP gestiona: XP, Level, AttributePoints, SkillPoints, delegates UI.
	IPantheliaPlayerInterface::Execute_AddToXP(Props.SourceCharacter, FinalXP);
}
