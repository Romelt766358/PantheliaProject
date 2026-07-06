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

	// Resistencias (inicialmente 0 — el GE de secundarios las calcula desde Resilience)
	InitFireResistance(0.f); InitWaterResistance(0.f);
	InitStormResistance(0.f); InitNatureResistance(0.f);

	// Vitales
	InitHealth(75.f); InitMana(50.f); InitStamina(50.f); InitPoise(50.f);

	// Meta
	InitIncomingDamage(0.f); InitIncomingPoiseDamage(0.f); InitIncomingXP(0.f);

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

	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, Health, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, Mana, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, Stamina, COND_None, REPNOTIFY_Always);
	DOREPLIFETIME_CONDITION_NOTIFY(UPantheliaAttributeSet, Poise, COND_None, REPNOTIFY_Always);
	// IncomingDamage, IncomingPoiseDamage, IncomingXP: meta atributos, NO se replican.
}

void UPantheliaAttributeSet::PreAttributeChange(const FGameplayAttribute& Attribute, float& NewValue)
{
	Super::PreAttributeChange(Attribute, NewValue);

	if (Attribute == GetHealthAttribute() && GetMaxHealth() > 0.f) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxHealth());
	else if (Attribute == GetManaAttribute() && GetMaxMana() > 0.f) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxMana());
	else if (Attribute == GetStaminaAttribute() && GetMaxStamina() > 0.f) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxStamina());
	else if (Attribute == GetPoiseAttribute() && GetMaxPoise() > 0.f) NewValue = FMath::Clamp(NewValue, 0.f, GetMaxPoise());
	// Las resistencias se clampean a 0-100 aquí para que nunca sean negativas ni > 100%
	else if (Attribute == GetFireResistanceAttribute())   NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
	else if (Attribute == GetWaterResistanceAttribute())  NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
	else if (Attribute == GetStormResistanceAttribute())  NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
	else if (Attribute == GetNatureResistanceAttribute()) NewValue = FMath::Clamp(NewValue, 0.f, 100.f);
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
		return;
	}

	// Clamp de Health para GEs que lo modifican DIRECTAMENTE (curaciones: pociones,
	// hechizos de vida, hoguera futura). El daño NO pasa por aquí — todo el daño entra
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
		HandleIncomingDamage(Props);
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
void UPantheliaAttributeSet::HandleIncomingDamage(const FEffectProperties& Props)
{
	const float LocalIncomingDamage = GetIncomingDamage();
	SetIncomingDamage(0.f);

	// --- REACCION DE PARRY / BLOQUEO (modelo Lies of P) ---
	// Leemos el resultado que el ExecCalc escribio en el context. Si hubo PARRY
	// perfecto del tipo correcto, aplicamos dano de postura AL ATACANTE y disparamos
	// los efectos elementales (gancho). El bloqueo no daña la postura del atacante.
	HandleParryReaction(Props);

	// --- DEBUFF (clases 307-309) ---
	// Leemos el resultado que ExecCalc_Damage (DetermineDebuff) escribió en el context.
	// Si hubo un debuff exitoso, Debuff(Props) es donde reaccionamos — todavía vacía,
	// ver la función más abajo y su comentario.
	if (UPantheliaAbilitySystemLibrary::IsSuccessfulDebuff(Props.EffectContextHandle))
	{
		Debuff(Props);
	}

	if (LocalIncomingDamage > 0.f)
	{
		const float NewHealth = FMath::Clamp(GetHealth() - LocalIncomingDamage, 0.f, GetMaxHealth());
		SetHealth(NewHealth);

		// Log de diagnóstico: todo daño que llega por GAS.
		// Nos confirma que el sistema de daño está procesando al target.
		// Quitar cuando el sistema XP funcione correctamente.
		UE_LOG(LogPanthelia, Log,
			TEXT("[DAMAGE] Target: '%s' | Damage: %.1f | HP: %.1f → %.1f | Fatal: %s"),
			Props.TargetCharacter ? *Props.TargetCharacter->GetName() : TEXT("null"),
			LocalIncomingDamage, GetHealth() + LocalIncomingDamage, NewHealth,
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

			// --- KNOCKBACK (clase 315) ---
			// Solo tiene sentido en el golpe NO fatal — un golpe fatal usa el impulso de
			// muerte (bloque bFatal arriba), que es físico de verdad (ragdoll). El
			// knockback, en cambio, lanza a un personaje VIVO por el aire — por eso usa
			// LaunchCharacter (un método de movimiento de ACharacter) y NO AddImpulse.
			//
			// POR QUÉ LaunchCharacter Y NO AddImpulse (la pista del propio curso, que sí
			// vale la pena entender): AddImpulse necesita que el mesh esté simulando
			// física (ragdoll) para tener cualquier efecto — eso es exactamente lo que
			// pasa al morir (MulticastHandleDeath activa el ragdoll primero). Pero un
			// personaje VIVO sigue controlado por su CharacterMovementComponent, no por
			// física de ragdoll — un impulso físico no haría absolutamente nada mientras
			// el personaje sigue caminando/atacando normalmente. LaunchCharacter, en
			// cambio, es un método pensado exactamente para esto: le dice al sistema de
			// movimiento del personaje "muévete con esta velocidad ahora", sin tocar
			// física ni ragdoll — funciona perfectamente en un personaje vivo y
			// completamente controlado.
			const FVector KnockbackForce = UPantheliaAbilitySystemLibrary::GetKnockbackForce(Props.EffectContextHandle);

			// IsNearlyZero con tolerancia 1.0: comparar un FVector contra el cero exacto
			// es frágil con floats (imprecisión de punto flotante). Una tolerancia pequeña
			// (1 unidad de Unreal, prácticamente imperceptible) evita procesar un
			// "knockback" que en la práctica es cero por error de redondeo.
			if (!KnockbackForce.IsNearlyZero(1.f) && Props.TargetCharacter)
			{
				// bXYOverride=true, bZOverride=true: reemplazamos la velocidad actual del
				// personaje en los 3 ejes por completo (no la sumamos) — así el knockback
				// es siempre igual de contundente sin importar si el personaje ya se estaba
				// moviendo o no. Ver LaunchCharacter en la documentación de ACharacter.
				Props.TargetCharacter->LaunchCharacter(KnockbackForce, true, true);
			}

			// --- LAUNCH / NIVEL 3 (post-315, a petición) ---
			// Sistema independiente del Knockback de arriba — su propio campo de contexto
			// (LaunchForce), y además concede State.Airborne, que hace dos cosas: (1)
			// bloquea GA_HitReact mientras el personaje está en el aire (hay que añadir
			// State.Airborne a los Activation Blocked Tags de GA_HitReact en el editor,
			// mismo patrón que ya hiciste con Debuff.Burn en la clase 314); (2) le dice a
			// Landed() (PantheliaCharacterBase.cpp) que ESTE aterrizaje debe disparar
			// GA_GetUp, a diferencia de un salto o caída normal.
			const FVector LaunchForce = UPantheliaAbilitySystemLibrary::GetLaunchForce(Props.EffectContextHandle);

			if (!LaunchForce.IsNearlyZero(1.f) && Props.TargetCharacter && Props.TargetASC)
			{
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
		}
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
// Debuff (clase 310) — creación dinámica de un GameplayEffect en C++
// ============================================================
// A diferencia de todo lo hecho hasta ahora, aquí NO usamos un GE ya configurado en un
// Blueprint (como DamageEffectClass) — lo CONSTRUIMOS desde cero en C++, en tiempo de
// ejecución. Un UGameplayEffect es normalmente un asset inmutable creado en el editor,
// pero como cualquier UObject también se puede crear "de la nada" con NewObject. Hace
// falta aquí porque cada debuff necesita su Duration/Period/Modifier ajustados a los 3
// números concretos de ESTE golpe (DebuffDamage/Duration/Frequency, ya guardados en el
// context desde la clase 309) — no tiene sentido crear un asset Blueprint por cada
// combinación posible de esos 3 números.
//
// LIMITACIÓN IMPORTANTE (viene del motor, no es un descuido nuestro): los GE creados así
// en C++ no soportan replicación de red correctamente. Como el proyecto es sin
// multiplayer (regla del proyecto), esto no nos afecta.
void UPantheliaAttributeSet::Debuff(const FEffectProperties& Props)
{
	// El SourceASC crea el contexto — es quien origina el debuff (el atacante original).
	FGameplayEffectContextHandle EffectContextHandle = Props.SourceASC->MakeEffectContext();
	EffectContextHandle.AddSourceObject(Props.SourceAvatarActor);

	// El tipo de daño que causó el debuff — lo guardamos en el context del golpe ORIGINAL
	// en la clase 309 (ExecCalc_Damage → SetDamageType). Lo leemos de vuelta aquí para
	// saber qué debuff construir.
	const FGameplayTag DamageType = UPantheliaAbilitySystemLibrary::GetDamageType(Props.EffectContextHandle);

	// Nombre descriptivo del GE dinámico — solo para identificarlo en el debugger de GAS
	// (showdebug abilitysystem) en vez de ver un "None" genérico. No afecta la lógica.
	const FString DebuffName = FString::Printf(TEXT("DynamicDebuff_%s"), *DamageType.ToString());
	UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage(), FName(DebuffName));

	// Duración: el debuff dura un tiempo (no es instantáneo ni infinito).
	Effect->DurationPolicy = EGameplayEffectDurationType::HasDuration;

	// Los 3 parámetros restantes viajan desde el context del golpe original — los
	// escribimos ahí en la clase 309 (ExecCalc_Damage → SetDebuffDamage/Duration/Frequency).
	const float DebuffDamage = UPantheliaAbilitySystemLibrary::GetDebuffDamage(Props.EffectContextHandle);
	const float DebuffDuration = UPantheliaAbilitySystemLibrary::GetDebuffDuration(Props.EffectContextHandle);
	const float DebuffFrequency = UPantheliaAbilitySystemLibrary::GetDebuffFrequency(Props.EffectContextHandle);

	// Period = cada cuántos segundos tiquea el daño (0 significaría no periódico).
	Effect->Period = DebuffFrequency;
	// DurationMagnitude: cuánto dura el efecto activo. FGameplayEffectModifierMagnitude
	// envuelve varios tipos posibles de magnitud; usamos un FScalableFloat inicializado
	// con un valor fijo, como una curva que siempre devuelve el mismo número sin importar
	// el nivel (el nivel de este GE dinámico siempre es 1, ver más abajo).
	Effect->DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(DebuffDuration));

	// --- TAG CONCEDIDO AL TARGET (dos adaptaciones respecto al curso — leer ambas) ---
	//
	// ADAPTACIÓN DE DISEÑO (ya establecida desde la clase 303): el curso resuelve el tag
	// de debuff con Tags.DamageTypesToDebuffs[DamageType] — un mapa que decidimos NO
	// construir porque varios tipos de daño colapsan al mismo elemento (8 tipos → 4
	// elementos). Aquí reutilizamos la misma cadena de dos saltos que ya usa
	// DetermineDebuff en ExecCalc_Damage: tipo de daño → elemento → debuff.
	//
	// ADAPTACIÓN DE API DE MOTOR (esto NO es una decisión de diseño nuestra — es un
	// requisito de UE 5.8): el curso usa "InheritableOwnedTagsContainer.AddTag(tag)"
	// directamente sobre el GameplayEffect. Esa propiedad está DEPRECADA desde UE 5.3 —
	// Epic movió la gestión de tags concedidos a un sistema de "Gameplay Effect
	// Components". La forma correcta en 5.8 es añadir un UTargetTagsGameplayEffectComponent
	// al efecto y llamar a SetAndApplyTargetTagChanges() con un FInheritedTagContainer
	// ya relleno con AddTag().
	const FPantheliaGameplayTags& GameplayTags = FPantheliaGameplayTags::Get();
	const EPantheliaElement* ElementPtr = GameplayTags.DamageTypeToElement.Find(DamageType);
	if (ElementPtr && *ElementPtr != EPantheliaElement::None)
	{
		if (const FGameplayTag* DebuffTagPtr = GameplayTags.ElementToDebuff.Find(*ElementPtr))
		{
			FInheritedTagContainer TagContainer;
			TagContainer.AddTag(*DebuffTagPtr);

			UTargetTagsGameplayEffectComponent& TagComponent =
				Effect->AddComponent<UTargetTagsGameplayEffectComponent>();
			TagComponent.SetAndApplyTargetTagChanges(TagContainer);
		}
	}

	// --- STACKING: activo, con fecha de caducidad conocida (confirmado, no especulación) ---
	//
	// CONFIRMADO tras compilar (ver conversación): estas dos líneas compilan en tu UE 5.8
	// con un warning C4996 ("Stacking Type will be made private, Please use
	// GetStackingType — actualiza tu código antes de subir de versión, o tu proyecto
	// dejará de compilar"). Investigué la alternativa que sugiere ese mensaje y hay un
	// problema real: sí existe un SetStackingType() de reemplazo, pero está marcado
	// "solo editor" — no se puede llamar desde código que corre en pleno gameplay (como
	// esta función). Es decir: hoy no existe ninguna forma "a prueba de futuro" de fijar
	// el stacking en un GameplayEffect creado dinámicamente en runtime. Es una limitación
	// real y actual del motor, no algo que se resuelva reescribiendo esto de otra forma.
	//
	// DEJO estas líneas activas porque funcionan HOY y el stacking es el comportamiento
	// correcto (dos Quemaduras seguidas en el mismo enemigo se combinan en una sola, en
	// vez de correr en paralelo). Pero queda pendiente: el día que actualices de UE 5.8 a
	// una versión más nueva, ESTA es la primera línea que revisar si el proyecto deja de
	// compilar — para entonces Epic probablemente ya habrá publicado la forma correcta de
	// hacerlo en runtime, y solo hará falta sustituir estas dos líneas por lo que sea que
	// hayan definido.
	Effect->StackingType = EGameplayEffectStackingType::AggregateBySource;
	Effect->StackLimitCount = 1;

	// --- MODIFICADOR: cuánto daño tiquea cada Period ---
	const int32 Index = Effect->Modifiers.Num();
	Effect->Modifiers.SetNum(Index + 1);
	FGameplayModifierInfo& ModifierInfo = Effect->Modifiers[Index];

	ModifierInfo.ModifierMagnitude = FScalableFloat(DebuffDamage);
	ModifierInfo.ModifierOp = EGameplayModOp::Additive;
	ModifierInfo.Attribute = UPantheliaAttributeSet::GetIncomingDamageAttribute();

	// --- CONSTRUIR EL SPEC Y APLICARLO ---
	//
	// CORRECCIÓN respecto al curso (no es una adaptación de diseño, es una mejora
	// técnica): el curso crea el spec con "new FGameplayEffectSpec(...)" y nunca lo
	// libera — eso es una fuga de memoria permanente en CADA debuff exitoso, que se
	// acumularía durante una sesión larga de juego. FGameplayEffectSpec es un struct
	// normal (no un UObject), así que no hay ninguna razón técnica para reservarlo en el
	// heap con "new": lo creamos como variable local normal (en el stack), y se destruye
	// solo al salir de la función — sin fuga.
	//
	// Nivel 1 siempre (simplificación deliberada, igual que el curso): los debuffs no
	// escalan por nivel de ability en esta primera versión.
	FGameplayEffectSpec MutableSpec(Effect, EffectContextHandle, 1.f);

	// Este es un context NUEVO (el que creamos arriba con MakeEffectContext), NO el
	// context del golpe original que disparó el debuff. Le asignamos el DamageType aquí
	// también, por si algo más adelante en el pipeline de ESTE debuff necesitara leerlo
	// igual que hicimos con el golpe original.
	if (FPantheliaGameplayEffectContext* PantheliaContext =
		static_cast<FPantheliaGameplayEffectContext*>(MutableSpec.GetContext().Get()))
	{
		const TSharedPtr<FGameplayTag> DebuffDamageTypePtr = MakeShareable(new FGameplayTag(DamageType));
		PantheliaContext->SetDamageType(DebuffDamageTypePtr);
	}

	// IMPORTANTE (evita un bucle infinito): NO establecemos IsSuccessfulDebuff en este
	// context nuevo. Cuando el propio debuff tiquee daño (cada Period), disparará su
	// propio PostGameplayEffectExecute → HandleIncomingDamage, y ese chequeo leerá
	// IsSuccessfulDebuff de ESTE context nuevo — que nunca lo pusimos en true, así que
	// Debuff() no se vuelve a llamar a sí misma. Si algún día quisieras que los debuffs
	// pudieran encadenar otros debuffs, este es el sitio exacto a tocar — con mucho
	// cuidado de no crear un ciclo infinito.
	Props.TargetASC->ApplyGameplayEffectSpecToSelf(MutableSpec);
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