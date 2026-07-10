// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/PantheliaDamageGameplayAbility.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "PantheliaGameplayTags.h"
#include "AbilitySystem/PantheliaAttributeSet.h"

FGameplayEffectSpecHandle UPantheliaDamageGameplayAbility::MakeDamageSpec()
{
	// ASC del caster (owner de la ability). Necesario para crear el spec y para
	// leer sus atributos durante el escalado.
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC) return FGameplayEffectSpecHandle();

	// Crear el spec de GE desde DamageEffectClass al nivel actual de la ability.
	// Añadimos el SourceObject al contexto para que el escalado y el ExecCalc
	// puedan identificar al caster si lo necesitan.
	FGameplayEffectContextHandle EffectContextHandle = SourceASC->MakeEffectContext();
	EffectContextHandle.AddSourceObject(GetAvatarActorFromActorInfo());

	FGameplayEffectSpecHandle DamageSpecHandle = SourceASC->MakeOutgoingSpec(
		DamageEffectClass, GetAbilityLevel(), EffectContextHandle);

	// Aplicar TODO el escalado de daño (base + atributos + postura) al spec.
	// Misma lógica que usan los proyectiles — centralizada en la clase base
	// para que melee, proyectiles y weapon trace sean consistentes (hallazgo D1).
	ApplyDamageScalingToSpec(DamageSpecHandle, SourceASC);

	return DamageSpecHandle;
}

void UPantheliaDamageGameplayAbility::CauseDamage(AActor* TargetActor)
{
	// Construimos el spec con todo el escalado (reutilizando MakeDamageSpec)...
	FGameplayEffectSpecHandle DamageSpecHandle = MakeDamageSpec();
	if (!DamageSpecHandle.IsValid()) return;

	// ...y lo aplicamos de inmediato al target conocido.
	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
	if (!SourceASC || !TargetASC) return;

	SourceASC->ApplyGameplayEffectSpecToTarget(*DamageSpecHandle.Data.Get(), TargetASC);
}

FTaggedMontage UPantheliaDamageGameplayAbility::GetRandomTaggedMontageFromArray(const TArray<FTaggedMontage>& TaggedMontages) const
{
	// Si el array está vacío devolvemos un struct vacío.
	// PlayMontageAndWait recibe Montage=null y completa inmediatamente sin crashear,
	// lo que hace que la ability termine limpiamente sin reproducir nada.
	if (TaggedMontages.Num() <= 0)
	{
		return FTaggedMontage();
	}

	const int32 Selection = FMath::RandRange(0, TaggedMontages.Num() - 1);
	return TaggedMontages[Selection];
}

void UPantheliaDamageGameplayAbility::ApplyDamageScalingToSpec(
	FGameplayEffectSpecHandle& SpecHandle, UAbilitySystemComponent* SourceASC, float DamageMultiplier) const
{
	if (!SpecHandle.IsValid() || !SourceASC) return;

	const FPantheliaGameplayTags& GameplayTags = FPantheliaGameplayTags::Get();

	// ----------------------------------------------------------------
	// ESCALADO POR ATRIBUTOS (spec §1.7)
	// ----------------------------------------------------------------
	// 1. Calcular el daño base total (Σ daños base por tipo)
	// 2. Calcular el escalado (Σ ratio × atributo del caster)
	// 3. Multiplicador = (DañoBase + Escalado) / DañoBase
	// 4. Distribuir el multiplicador proporcionalmente entre los tipos
	// 5. Asignar cada tipo escalado al SetByCaller
	// 6. Reservado: los parámetros del estado viven en la configuración global
	// 7. Asignar la magnitud del impulso de muerte al SetByCaller (clase 313)
	// 8. Asignar los parámetros de knockback al SetByCaller (clase 315)
	// 9. Asignar los parámetros de launch/Nivel 3 al SetByCaller (post-315)
	//
	// El ExecCalc recibe valores YA escalados — no sabe del escalado.
	// ----------------------------------------------------------------

	// PASO 1: Daño base de cada tipo a su nivel actual
	TMap<FGameplayTag, float> ScaledDamages;
	float TotalBaseDamage = 0.f;

	for (const auto& DamageTypePair : DamageTypes)
	{
		const float BaseValue = DamageTypePair.Value.GetValueAtLevel(GetAbilityLevel());
		if (BaseValue > 0.f)
		{
			ScaledDamages.Add(DamageTypePair.Key, BaseValue);
			TotalBaseDamage += BaseValue;
		}
	}

	// PASO 2: Escalado por atributos secundarios/vitales del caster.
	// Usamos TagsToAttributes del AttributeSet para lookup genérico sin if-else.
	float TotalScaling = 0.f;

	if (!AttributeScalings.IsEmpty() && TotalBaseDamage > 0.f)
	{
		const UPantheliaAttributeSet* PAS = SourceASC->GetSet<UPantheliaAttributeSet>();
		if (PAS)
		{
			for (const FAbilityAttributeScaling& Scaling : AttributeScalings)
			{
				if (!Scaling.AttributeTag.IsValid() || Scaling.Ratio <= 0.f) continue;

				auto* FuncPtr = PAS->TagsToAttributes.Find(Scaling.AttributeTag);
				if (FuncPtr)
				{
					const FGameplayAttribute Attribute = (*FuncPtr)();
					const float AttributeValue = SourceASC->GetNumericAttribute(Attribute);
					TotalScaling += Scaling.Ratio * AttributeValue;
				}
			}
		}
	}

	// PASO 3-4: Multiplicador proporcional aplicado a cada tipo.
	// Si no hay escalado (o daño base = 0), el multiplicador es 1.0 → sin cambio.
	if (TotalBaseDamage > 0.f && TotalScaling > 0.f)
	{
		const float Multiplier = (TotalBaseDamage + TotalScaling) / TotalBaseDamage;
		for (auto& DamagePair : ScaledDamages)
		{
			DamagePair.Value *= Multiplier;
		}
	}

	// PASO 5: Asignar cada tipo de daño escalado al SetByCaller.
	// DamageMultiplier (1.0 por defecto) escala el daño final — lo usa el ataque pesado
	// cargado del jugador (hold) para pegar más fuerte que el especial/ligero. Se aplica
	// aquí, sobre el daño ya escalado por atributos, para que el multiplicador afecte al
	// total final de forma predecible.
	for (const auto& DamagePair : ScaledDamages)
	{
		UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
			SpecHandle, DamagePair.Key, DamagePair.Value * DamageMultiplier);
	}

	// Daño a la postura (no se escala — es un valor de arma/habilidad directo)
	const float ScaledPoiseDamage = PoiseDamage.GetValueAtLevel(GetAbilityLevel());
	if (ScaledPoiseDamage > 0.f)
	{
		UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
			SpecHandle, GameplayTags.Damage_Poise, ScaledPoiseDamage);
	}

	// PASO 6: reservado. Los parámetros del estado elemental ya no viajan en
	// el spec del golpe. TriggerElementalStatus los obtiene de la configuración
	// global y del Status Power del source cuando la barra llega a 100.

	// PASO 7 (clase 313): Asignar la magnitud del impulso de muerte al SetByCaller.
	// Por qué un SetByCaller y no directamente el context (a diferencia del VECTOR final,
	// que sí se escribe directo al context — ver PantheliaProjectile.cpp): aquí, en la
	// ability, solo conocemos la MAGNITUD (un escalar). La DIRECCIÓN del impulso depende
	// de dónde y cómo impacta el golpe — algo que la ability, en este punto del pipeline,
	// todavía no sabe (el proyectil ni siquiera ha volado todavía). Por eso el escalar
	// viaja como SetByCaller. Quien SÍ conoce la dirección en el momento del
	// impacto (proyectil o WeaponTrace) lo lee, calcula el vector final y lo escribe
	// directamente en el context con UPantheliaAbilitySystemLibrary::SetDeathImpulse.
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		SpecHandle, GameplayTags.CombatTricks_DeathImpulseMagnitude,
		DeathImpulseMagnitude.GetValueAtLevel(GetAbilityLevel()));

	// PASO 8 (clase 315): Asignar los parámetros de knockback al SetByCaller.
	// Mismo razonamiento que el PASO 7 (impulso de muerte): aquí solo conocemos la
	// MAGNITUD y la PROBABILIDAD (dos escalares); la DIRECCIÓN del knockback depende
	// de dónde/cómo impacta el golpe, algo que se resuelve en el punto de impacto
	// (PantheliaProjectile.cpp / WeaponTraceComponent.cpp), no aquí en la ability.
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		SpecHandle, GameplayTags.CombatTricks_KnockbackForceMagnitude,
		KnockbackForceMagnitude.GetValueAtLevel(GetAbilityLevel()));
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		SpecHandle, GameplayTags.CombatTricks_KnockbackChance,
		KnockbackChance.GetValueAtLevel(GetAbilityLevel()));

	// PASO 9 (post-315): Asignar los parámetros de Launch/Nivel 3 al SetByCaller.
	// Sistema independiente de Knockback (PASO 8) — ver la nota de diseño completa en
	// FPantheliaGameplayTags sobre por qué NO comparten tags ni campos de contexto.
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		SpecHandle, GameplayTags.CombatTricks_LaunchForceMagnitude,
		LaunchForceMagnitude.GetValueAtLevel(GetAbilityLevel()));
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		SpecHandle, GameplayTags.CombatTricks_LaunchChance,
		LaunchChance.GetValueAtLevel(GetAbilityLevel()));
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		SpecHandle, GameplayTags.CombatTricks_LaunchPitchOverride,
		LaunchPitchOverride.GetValueAtLevel(GetAbilityLevel()));

	// PASO 10 (Nivel 2 de knockback, a petición): Asignar bKnockbackIsHeavy al
	// SetByCaller. SetByCaller solo mueve floats — no hay un tipo "bool" nativo para
	// esto, así que se transporta como 1.0 (true) / 0.0 (false), igual que hacen otros
	// booleanos de GAS cuando necesitan viajar por este mecanismo.
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		SpecHandle, GameplayTags.CombatTricks_KnockbackIsHeavy,
		bKnockbackIsHeavy ? 1.f : 0.f);

	// PASO 11 (sistema de buildup): colapsar BuildupAmounts (por tipo de daño) a
	// nivel de ELEMENTO y transportarlo como SetByCaller. Mismo colapso de dos saltos
	// del sistema de estados (DamageTypeToElement): dos tipos que comparten elemento
	// SUMAN sus buildups en la misma barra — correcto por diseño (un golpe
	// Hielo+Agua llena la barra de Saturación con la suma de ambos, sin dobles
	// tiradas porque ya no hay tiradas). Los 4 tags se asignan SIEMPRE (0 si la
	// ability no acumula ese elemento), coherente con el resto de SetByCallers.
	float ElementBuildup[4] = { 0.f, 0.f, 0.f, 0.f }; // Fire, Storm, Water, Nature
	for (const TTuple<FGameplayTag, FScalableFloat>& BuildupPair : BuildupAmounts)
	{
		const EPantheliaElement* ElementPtr = GameplayTags.DamageTypeToElement.Find(BuildupPair.Key);
		if (!ElementPtr) continue;
		const float Amount = BuildupPair.Value.GetValueAtLevel(GetAbilityLevel());
		switch (*ElementPtr)
		{
			case EPantheliaElement::Fire:   ElementBuildup[0] += Amount; break;
			case EPantheliaElement::Storm:  ElementBuildup[1] += Amount; break;
			case EPantheliaElement::Water:  ElementBuildup[2] += Amount; break;
			case EPantheliaElement::Nature: ElementBuildup[3] += Amount; break;
			default: break; // None: daño sin elemento, no acumula estado
		}
	}
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		SpecHandle, GameplayTags.CombatTricks_Buildup_Fire, ElementBuildup[0]);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		SpecHandle, GameplayTags.CombatTricks_Buildup_Storm, ElementBuildup[1]);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		SpecHandle, GameplayTags.CombatTricks_Buildup_Water, ElementBuildup[2]);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		SpecHandle, GameplayTags.CombatTricks_Buildup_Nature, ElementBuildup[3]);
}
