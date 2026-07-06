// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/PantheliaAbilitySystemLibrary.h"
#include "UI/HUD/PantheliaHUD.h"
#include "UI/WidgetController/OverlayWidgetController.h"
#include "UI/WidgetController/AttributeMenuWidgetController.h"
#include "UI/WidgetController/PantheliaWidgetController.h"
#include "Player/PantheliaPlayerState.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Game/PantheliaGameModeBase.h"
#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbility.h"
// Necesario para el static cast a FPantheliaGameplayEffectContext
#include "AbilitySystem/PantheliaAbilityTypes.h"
// Necesario para GetPlayerLevel() en GiveStartupAbilities
#include "Interfaces/CombatInterface.h"
// Necesario para FOverlapResult en GetLivePlayersWithinRadius
#include "Engine/OverlapResult.h"
// Necesario para AssignTagSetByCallerMagnitude en ApplyDamageEffect
#include "AbilitySystemBlueprintLibrary.h"
// Necesario para leer los tags SetByCaller de debuff (Debuff_Chance, etc.) en ApplyDamageEffect
#include "PantheliaGameplayTags.h"
// Necesario para UE_LOG(LogPanthelia, ...) en ApplyDamageEffect
#include "PantheliaLogChannels.h"
// Necesario para crear un UGameplayEffect dinámicamente en GrantTemporaryInvulnerability
// (post-315, i-frames genéricos) — mismo patrón que UPantheliaAttributeSet::Debuff (clase 310).
#include "GameplayEffect.h"
#include "GameplayEffectComponents/TargetTagsGameplayEffectComponent.h"

UOverlayWidgetController* UPantheliaAbilitySystemLibrary::GetOverlayWidgetController(const UObject* WorldContextObject)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	if (!PC) { return nullptr; }

	if (APantheliaHUD* HUD = Cast<APantheliaHUD>(PC->GetHUD()))
	{
		APantheliaPlayerState* PS = PC->GetPlayerState<APantheliaPlayerState>();
		if (!PS) { return nullptr; }

		UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
		UAttributeSet* AS = PS->GetAttributeSet();

		const FWidgetControllerParams WCParams(PC, PS, ASC, AS);
		return HUD->GetOverlayWidgetController(WCParams);
	}

	return nullptr;
}

UAttributeMenuWidgetController* UPantheliaAbilitySystemLibrary::GetAttributeMenuWidgetController(const UObject* WorldContextObject)
{
	APlayerController* PC = UGameplayStatics::GetPlayerController(WorldContextObject, 0);
	if (!PC) { return nullptr; }

	if (APantheliaHUD* HUD = Cast<APantheliaHUD>(PC->GetHUD()))
	{
		APantheliaPlayerState* PS = PC->GetPlayerState<APantheliaPlayerState>();
		if (!PS) { return nullptr; }

		UAbilitySystemComponent* ASC = PS->GetAbilitySystemComponent();
		UAttributeSet* AS = PS->GetAttributeSet();

		const FWidgetControllerParams WCParams(PC, PS, ASC, AS);
		return HUD->GetAttributeMenuWidgetController(WCParams);
	}

	return nullptr;
}

UPantheliaCharacterClassInfo* UPantheliaAbilitySystemLibrary::GetCharacterClassInfo(const UObject* WorldContextObject)
{
	AGameModeBase* GameMode = UGameplayStatics::GetGameMode(WorldContextObject);
	if (!GameMode) { return nullptr; }

	APantheliaGameModeBase* PantheliaGM = Cast<APantheliaGameModeBase>(GameMode);
	if (!PantheliaGM) { return nullptr; }

	return PantheliaGM->CharacterClassInfo;
}

void UPantheliaAbilitySystemLibrary::InitializeDefaultAttributes(
	const UObject* WorldContextObject,
	EPantheliaCharacterClass CharacterClass,
	float Level,
	UAbilitySystemComponent* ASC)
{
	UPantheliaCharacterClassInfo* ClassInfo = GetCharacterClassInfo(WorldContextObject);
	if (!ClassInfo) { return; }

	AActor* AvatarActor = ASC->GetAvatarActor();
	FCharacterClassDefaultInfo ClassDefaultInfo = ClassInfo->GetClassDefaultInfo(CharacterClass);

	FGameplayEffectContextHandle PrimaryContextHandle = ASC->MakeEffectContext();
	PrimaryContextHandle.AddSourceObject(AvatarActor);
	const FGameplayEffectSpecHandle PrimarySpecHandle = ASC->MakeOutgoingSpec(
		ClassDefaultInfo.DefaultPrimaryAttributes, Level, PrimaryContextHandle);
	ASC->ApplyGameplayEffectSpecToSelf(*PrimarySpecHandle.Data.Get());

	FGameplayEffectContextHandle SecondaryContextHandle = ASC->MakeEffectContext();
	SecondaryContextHandle.AddSourceObject(AvatarActor);
	const FGameplayEffectSpecHandle SecondarySpecHandle = ASC->MakeOutgoingSpec(
		ClassInfo->DefaultSecondaryAttributes, Level, SecondaryContextHandle);
	ASC->ApplyGameplayEffectSpecToSelf(*SecondarySpecHandle.Data.Get());

	FGameplayEffectContextHandle VitalContextHandle = ASC->MakeEffectContext();
	VitalContextHandle.AddSourceObject(AvatarActor);
	const FGameplayEffectSpecHandle VitalSpecHandle = ASC->MakeOutgoingSpec(
		ClassInfo->DefaultVitalAttributes, Level, VitalContextHandle);
	ASC->ApplyGameplayEffectSpecToSelf(*VitalSpecHandle.Data.Get());
}

void UPantheliaAbilitySystemLibrary::GiveStartupAbilities(const UObject* WorldContextObject, UAbilitySystemComponent* ASC, EPantheliaCharacterClass CharacterClass)
{
	UPantheliaCharacterClassInfo* ClassInfo = GetCharacterClassInfo(WorldContextObject);
	if (!ClassInfo) { return; }

	// 1 — Common abilities (HitReact, etc.) — otorgadas a nivel 1 a todos los enemigos
	for (TSubclassOf<UGameplayAbility> AbilityClass : ClassInfo->CommonAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, 1);
		ASC->GiveAbility(AbilitySpec);
	}

	// 2 — StartupAbilities específicas del arquetipo (GA_MeleeAttack para Warrior, etc.)
	// Se otorgan al nivel del personaje para que escalen con él.
	// El nivel se obtiene del avatar via ICombatInterface para no acoplar esta librería
	// a ninguna clase concreta de personaje.
	const FCharacterClassDefaultInfo& DefaultInfo = ClassInfo->GetClassDefaultInfo(CharacterClass);

	// Obtenemos el nivel del avatar via Execute_GetPlayerLevel (BlueprintNativeEvent).
	// No requiere cast previo: verificamos Implements<> y llamamos la versión estática.
	int32 CharacterLevel = 1;
	if (AActor* AvatarActor = ASC->GetAvatarActor())
	{
		if (AvatarActor->Implements<UCombatInterface>())
		{
			CharacterLevel = ICombatInterface::Execute_GetPlayerLevel(AvatarActor);
		}
	}

	for (TSubclassOf<UGameplayAbility> AbilityClass : DefaultInfo.StartupAbilities)
	{
		FGameplayAbilitySpec AbilitySpec = FGameplayAbilitySpec(AbilityClass, CharacterLevel);
		ASC->GiveAbility(AbilitySpec);
	}
}

// ============================================================
// Context custom — getters y setters
// ============================================================

bool UPantheliaAbilitySystemLibrary::IsCriticalHit(const FGameplayEffectContextHandle& EffectContextHandle)
{
	// Static cast: sabemos que todos los contexts del proyecto son FPantheliaGameplayEffectContext
	// gracias a que UPantheliaAbilitySystemGlobals::AllocGameplayEffectContext() los crea así.
	// El const en el puntero es necesario porque EffectContextHandle es const ref.
	const FPantheliaGameplayEffectContext* PantheliaContext =
		static_cast<const FPantheliaGameplayEffectContext*>(EffectContextHandle.Get());

	if (PantheliaContext)
	{
		return PantheliaContext->IsCriticalHit();
	}

	return false;
}

void UPantheliaAbilitySystemLibrary::SetIsCriticalHit(
	FGameplayEffectContextHandle& EffectContextHandle,
	bool bInIsCriticalHit)
{
	// El handle es no-const así que podemos obtener un puntero mutable al context.
	FPantheliaGameplayEffectContext* PantheliaContext =
		static_cast<FPantheliaGameplayEffectContext*>(EffectContextHandle.Get());

	if (PantheliaContext)
	{
		PantheliaContext->SetIsCriticalHit(bInIsCriticalHit);
	}
}

// ============================================================
// LECTURA DEL RESULTADO DE DEBUFF (clase 307-308)
// ============================================================

bool UPantheliaAbilitySystemLibrary::IsSuccessfulDebuff(const FGameplayEffectContextHandle& EffectContextHandle)
{
	const FPantheliaGameplayEffectContext* PantheliaContext =
		static_cast<const FPantheliaGameplayEffectContext*>(EffectContextHandle.Get());

	if (PantheliaContext)
	{
		return PantheliaContext->IsSuccessfulDebuff();
	}

	return false;
}

float UPantheliaAbilitySystemLibrary::GetDebuffDamage(const FGameplayEffectContextHandle& EffectContextHandle)
{
	const FPantheliaGameplayEffectContext* PantheliaContext =
		static_cast<const FPantheliaGameplayEffectContext*>(EffectContextHandle.Get());

	if (PantheliaContext)
	{
		return PantheliaContext->GetDebuffDamage();
	}

	return 0.f;
}

float UPantheliaAbilitySystemLibrary::GetDebuffDuration(const FGameplayEffectContextHandle& EffectContextHandle)
{
	const FPantheliaGameplayEffectContext* PantheliaContext =
		static_cast<const FPantheliaGameplayEffectContext*>(EffectContextHandle.Get());

	if (PantheliaContext)
	{
		return PantheliaContext->GetDebuffDuration();
	}

	return 0.f;
}

float UPantheliaAbilitySystemLibrary::GetDebuffFrequency(const FGameplayEffectContextHandle& EffectContextHandle)
{
	const FPantheliaGameplayEffectContext* PantheliaContext =
		static_cast<const FPantheliaGameplayEffectContext*>(EffectContextHandle.Get());

	if (PantheliaContext)
	{
		return PantheliaContext->GetDebuffFrequency();
	}

	return 0.f;
}

FGameplayTag UPantheliaAbilitySystemLibrary::GetDamageType(const FGameplayEffectContextHandle& EffectContextHandle)
{
	const FPantheliaGameplayEffectContext* PantheliaContext =
		static_cast<const FPantheliaGameplayEffectContext*>(EffectContextHandle.Get());

	if (PantheliaContext)
	{
		// GetDamageType() en el context devuelve el TSharedPtr crudo (puede ser nulo si
		// nunca se asignó). Aquí es donde comprobamos IsValid() antes de desreferenciar,
		// para que el resto del código pueda llamar a esta función sin preocuparse por
		// punteros nulos — siempre recibe un FGameplayTag válido (vacío en el peor caso).
		const TSharedPtr<FGameplayTag> DamageTypePtr = PantheliaContext->GetDamageType();
		if (DamageTypePtr.IsValid())
		{
			return *DamageTypePtr;
		}
	}

	return FGameplayTag();
}

FVector UPantheliaAbilitySystemLibrary::GetDeathImpulse(const FGameplayEffectContextHandle& EffectContextHandle)
{
	const FPantheliaGameplayEffectContext* PantheliaContext =
		static_cast<const FPantheliaGameplayEffectContext*>(EffectContextHandle.Get());

	if (PantheliaContext)
	{
		return PantheliaContext->GetDeathImpulse();
	}

	return FVector::ZeroVector;
}

FVector UPantheliaAbilitySystemLibrary::GetKnockbackForce(const FGameplayEffectContextHandle& EffectContextHandle)
{
	const FPantheliaGameplayEffectContext* PantheliaContext =
		static_cast<const FPantheliaGameplayEffectContext*>(EffectContextHandle.Get());

	if (PantheliaContext)
	{
		return PantheliaContext->GetKnockbackForce();
	}

	return FVector::ZeroVector;
}

FVector UPantheliaAbilitySystemLibrary::GetLaunchForce(const FGameplayEffectContextHandle& EffectContextHandle)
{
	const FPantheliaGameplayEffectContext* PantheliaContext =
		static_cast<const FPantheliaGameplayEffectContext*>(EffectContextHandle.Get());

	if (PantheliaContext)
	{
		return PantheliaContext->GetLaunchForce();
	}

	return FVector::ZeroVector;
}

// ============================================================
// ESCRITURA DEL RESULTADO DE DEBUFF (clase 309)
// ============================================================

void UPantheliaAbilitySystemLibrary::SetIsSuccessfulDebuff(
	FGameplayEffectContextHandle& EffectContextHandle,
	bool bInSuccessfulDebuff)
{
	FPantheliaGameplayEffectContext* PantheliaContext =
		static_cast<FPantheliaGameplayEffectContext*>(EffectContextHandle.Get());

	if (PantheliaContext)
	{
		PantheliaContext->SetIsSuccessfulDebuff(bInSuccessfulDebuff);
	}
}

void UPantheliaAbilitySystemLibrary::SetDebuffDamage(
	FGameplayEffectContextHandle& EffectContextHandle,
	float InDebuffDamage)
{
	FPantheliaGameplayEffectContext* PantheliaContext =
		static_cast<FPantheliaGameplayEffectContext*>(EffectContextHandle.Get());

	if (PantheliaContext)
	{
		PantheliaContext->SetDebuffDamage(InDebuffDamage);
	}
}

void UPantheliaAbilitySystemLibrary::SetDebuffDuration(
	FGameplayEffectContextHandle& EffectContextHandle,
	float InDebuffDuration)
{
	FPantheliaGameplayEffectContext* PantheliaContext =
		static_cast<FPantheliaGameplayEffectContext*>(EffectContextHandle.Get());

	if (PantheliaContext)
	{
		PantheliaContext->SetDebuffDuration(InDebuffDuration);
	}
}

void UPantheliaAbilitySystemLibrary::SetDebuffFrequency(
	FGameplayEffectContextHandle& EffectContextHandle,
	float InDebuffFrequency)
{
	FPantheliaGameplayEffectContext* PantheliaContext =
		static_cast<FPantheliaGameplayEffectContext*>(EffectContextHandle.Get());

	if (PantheliaContext)
	{
		PantheliaContext->SetDebuffFrequency(InDebuffFrequency);
	}
}

void UPantheliaAbilitySystemLibrary::SetDamageType(
	FGameplayEffectContextHandle& EffectContextHandle,
	const FGameplayTag& InDamageType)
{
	FPantheliaGameplayEffectContext* PantheliaContext =
		static_cast<FPantheliaGameplayEffectContext*>(EffectContextHandle.Get());

	if (PantheliaContext)
	{
		// Esta función SÍ crea el TSharedPtr (a diferencia del setter en el struct, que
		// solo asigna uno ya creado) — así ExecCalc_Damage puede llamar a esta función
		// pasando un FGameplayTag normal, sin lidiar con smart pointers directamente.
		const TSharedPtr<FGameplayTag> DamageTypePtr = MakeShared<FGameplayTag>(InDamageType);
		PantheliaContext->SetDamageType(DamageTypePtr);
	}
}

void UPantheliaAbilitySystemLibrary::SetDeathImpulse(
	FGameplayEffectContextHandle& EffectContextHandle,
	const FVector& InImpulse)
{
	FPantheliaGameplayEffectContext* PantheliaContext =
		static_cast<FPantheliaGameplayEffectContext*>(EffectContextHandle.Get());

	if (PantheliaContext)
	{
		PantheliaContext->SetDeathImpulse(InImpulse);
	}
}

void UPantheliaAbilitySystemLibrary::SetKnockbackForce(
	FGameplayEffectContextHandle& EffectContextHandle,
	const FVector& InForce)
{
	FPantheliaGameplayEffectContext* PantheliaContext =
		static_cast<FPantheliaGameplayEffectContext*>(EffectContextHandle.Get());

	if (PantheliaContext)
	{
		PantheliaContext->SetKnockbackForce(InForce);
	}
}

void UPantheliaAbilitySystemLibrary::SetLaunchForce(
	FGameplayEffectContextHandle& EffectContextHandle,
	const FVector& InForce)
{
	FPantheliaGameplayEffectContext* PantheliaContext =
		static_cast<FPantheliaGameplayEffectContext*>(EffectContextHandle.Get());

	if (PantheliaContext)
	{
		PantheliaContext->SetLaunchForce(InForce);
	}
}

FVector UPantheliaAbilitySystemLibrary::GetDirectionWithPitchOverride(
	const FVector& InDirection, float PitchOverrideDegrees)
{
	// Convertimos el vector a un FRotator (yaw + pitch + roll que representarían esa
	// dirección), sobreescribimos SOLO el pitch, y convertimos de vuelta a vector.
	// El yaw (hacia dónde apunta horizontalmente) se conserva intacto — solo cambia
	// cuánto "hacia arriba" apunta.
	FRotator Rotation = InDirection.Rotation();
	Rotation.Pitch = PitchOverrideDegrees;
	return Rotation.Vector();
}

void UPantheliaAbilitySystemLibrary::GrantTemporaryInvulnerability(UAbilitySystemComponent* ASC, float Duration)
{
	if (!ASC || Duration <= 0.f)
	{
		return;
	}

	// Mismo patrón de creación dinámica que UPantheliaAttributeSet::Debuff (clase 310),
	// simplificado: aquí NO hace falta ningún modificador de atributo (Effect->Modifiers
	// se queda vacío) — el único propósito de este GE es existir durante Duration
	// segundos y conceder un tag mientras tanto.
	UGameplayEffect* Effect = NewObject<UGameplayEffect>(GetTransientPackage(), FName("DynamicInvulnerability"));
	Effect->DurationPolicy = EGameplayEffectDurationType::HasDuration;
	Effect->DurationMagnitude = FGameplayEffectModifierMagnitude(FScalableFloat(Duration));

	// Conceder State.Invulnerable — vía el componente moderno (UTargetTagsGameplayEffectComponent),
	// NO vía InheritableOwnedTagsContainer (deprecado desde UE 5.3, ver la explicación
	// completa en UPantheliaAttributeSet::Debuff, clase 310).
	FInheritedTagContainer TagContainer;
	TagContainer.AddTag(FPantheliaGameplayTags::Get().State_Invulnerable);
	UTargetTagsGameplayEffectComponent& TagComponent = Effect->AddComponent<UTargetTagsGameplayEffectComponent>();
	TagComponent.SetAndApplyTargetTagChanges(TagContainer);

	FGameplayEffectContextHandle EffectContextHandle = ASC->MakeEffectContext();
	EffectContextHandle.AddSourceObject(ASC->GetAvatarActor());

	// FGameplayEffectSpec en el stack (no "new"): mismo motivo que en Debuff() —
	// FGameplayEffectSpec es un struct normal, reservarlo en el heap sería una fuga de
	// memoria sin ningún beneficio real.
	FGameplayEffectSpec Spec(Effect, EffectContextHandle, 1.f);
	ASC->ApplyGameplayEffectSpecToSelf(Spec);
}

// ============================================================
// APLICACIÓN DE DAÑO SECUNDARIO/DEBUFF (clase 305)
// ============================================================
// Ver el header (declaración de esta función) y FDamageEffectParams (en
// PantheliaAbilityTypes.h) para la explicación completa de diseño.
FGameplayEffectContextHandle UPantheliaAbilitySystemLibrary::ApplyDamageEffect(
	const FDamageEffectParams& DamageEffectParams)
{
	UAbilitySystemComponent* SourceASC = DamageEffectParams.SourceASC;
	UAbilitySystemComponent* TargetASC = DamageEffectParams.TargetASC;

	// A diferencia del curso original (que deja crashear aquí si el target ASC no es
	// válido), preferimos fallar de forma controlada: logueamos el error y devolvemos
	// un handle vacío. Quien llame a esta función puede comprobar EffectContextHandle.IsValid()
	// si necesita saber si la aplicación tuvo éxito.
	if (!SourceASC || !TargetASC)
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("ApplyDamageEffect: SourceASC o TargetASC inválido. No se aplica el efecto."));
		return FGameplayEffectContextHandle();
	}

	// El context se crea desde el SourceASC (quien origina el daño), igual que en
	// MakeDamageSpec() de UPantheliaDamageGameplayAbility.
	FGameplayEffectContextHandle EffectContextHandle = SourceASC->MakeEffectContext();
	EffectContextHandle.AddSourceObject(SourceASC->GetAvatarActor());

	const FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		DamageEffectParams.DamageGameplayEffectClass, DamageEffectParams.AbilityLevel, EffectContextHandle);

	if (!SpecHandle.IsValid())
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("ApplyDamageEffect: MakeOutgoingSpec devolvió un spec inválido (¿DamageGameplayEffectClass sin asignar?)."));
		return EffectContextHandle;
	}

	// SetByCaller del daño principal — solo si DamageEffectParams trae un tipo válido.
	// (Puede no traerlo si este struct se usa solo para transportar parámetros de debuff
	// hacia un GE que no necesita un tipo de daño propio.)
	if (DamageEffectParams.DamageType.IsValid())
	{
		UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
			SpecHandle, DamageEffectParams.DamageType, DamageEffectParams.BaseDamage);
	}

	// SetByCaller de los 4 parámetros de debuff (clase 304).
	const FPantheliaGameplayTags& GameplayTags = FPantheliaGameplayTags::Get();
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		SpecHandle, GameplayTags.Debuff_Chance, DamageEffectParams.DebuffChance);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		SpecHandle, GameplayTags.Debuff_Damage, DamageEffectParams.DebuffDamage);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		SpecHandle, GameplayTags.Debuff_Duration, DamageEffectParams.DebuffDuration);
	UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(
		SpecHandle, GameplayTags.Debuff_Frequency, DamageEffectParams.DebuffFrequency);

	// A diferencia de CauseDamage() (que usa ApplyGameplayEffectSpecToTarget desde el
	// SourceASC), aquí aplicamos directamente al TargetASC con ApplyGameplayEffectSpecToSelf.
	// Ambos caminos son válidos en GAS; este es el que usa el curso para esta función.
	TargetASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());

	return EffectContextHandle;
}

void UPantheliaAbilitySystemLibrary::GetLivePlayersWithinRadius(
	const UObject* WorldContextObject,
	TArray<AActor*>& OutOverlappingActors,
	const TArray<AActor*>& ActorsToIgnore,
	float Radius,
	const FVector& SphereOrigin)
{
	// SphereParams define qué colisiones ignorar.
	// AddIgnoredActors: típicamente el atacante, para que no se dañe a sí mismo.
	FCollisionQueryParams SphereParams;
	SphereParams.AddIgnoredActors(ActorsToIgnore);

	TArray<FOverlapResult> Overlaps;

	// Obtenemos el mundo via GEngine para ser consistentes con el engine en BP function libraries.
	// LogAndReturnNull: si falla, loguea el error y devuelve null — no crashea.
	if (UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull))
	{
		// OverlapMultiByObjectType: detecta todos los objetos dinámicos dentro de la esfera.
		// AllDynamicObjects incluye personajes y pawns — exactamente lo que necesitamos.
		// No usamos Static porque los personajes son dinámicos.
		World->OverlapMultiByObjectType(
			Overlaps,
			SphereOrigin,
			FQuat::Identity,
			FCollisionObjectQueryParams(FCollisionObjectQueryParams::InitType::AllDynamicObjects),
			FCollisionShape::MakeSphere(Radius),
			SphereParams
		);

		for (FOverlapResult& Overlap : Overlaps)
		{
			// Primero verificamos que implementa la interfaz (short-circuit evaluation):
			// si no implementa, el segundo check (IsDead) no se evalúa y no hay crash.
			// Esto previene el assert que ocurre al llamar Execute_IsDead en actores
			// que no implementan ICombatInterface (como props o effect actors).
			if (Overlap.GetActor()->Implements<UCombatInterface>() &&
				!ICombatInterface::Execute_IsDead(Overlap.GetActor()))
			{
				// Usamos GetAvatar para obtener el actor correcto (por si el overlap
				// golpea un componente hijo en lugar del actor raíz).
				// AddUnique evita duplicados si múltiples componentes del mismo actor
				// están dentro de la esfera.
				OutOverlappingActors.AddUnique(ICombatInterface::Execute_GetAvatar(Overlap.GetActor()));
			}
		}
	}
}

bool UPantheliaAbilitySystemLibrary::IsNotFriend(AActor* FirstActor, AActor* SecondActor)
{
	// Determinamos el equipo de cada actor usando Actor Tags.
	// Los tags "Player" y "Enemy" deben estar configurados en los Blueprints
	// de cada personaje (BP_PantheliaCharacter, BP_PantheliaEnemy, etc.).
	const bool bBothPlayers = FirstActor->ActorHasTag(FName("Player")) && SecondActor->ActorHasTag(FName("Player"));
	const bool bBothEnemies = FirstActor->ActorHasTag(FName("Enemy")) && SecondActor->ActorHasTag(FName("Enemy"));

	// Son amigos si están en el mismo equipo.
	// Son enemigos (IsNotFriend = true) si están en equipos distintos.
	const bool bFriends = bBothPlayers || bBothEnemies;
	return !bFriends;
}

// ============================================================
// SISTEMA DE RENDIMIENTOS DECRECIENTES DE XP
// ============================================================

float UPantheliaAbilitySystemLibrary::GetXPMultiplierForKillCount(int32 KillCount)
{
	// Tabla de multiplicadores por número de muertes previas (0-based).
	// KillCount es cuántas veces el jugador ha matado a ESTE enemigo ANTES de esta muerte.
	//
	//   KillCount 0 = primera muerte    = 100%  (nunca lo había matado)
	//   KillCount 1 = segunda muerte    =  60%
	//   KillCount 2 = tercera muerte    =  35%
	//   KillCount 3 = cuarta muerte     =  20%
	//   KillCount ≥ 4 = quinta o más   =  10%  (piso permanente)
	//
	// La caída es pronunciada al principio (40% en la primera repetición) y se
	// suaviza hacia el piso, dando la sensación de que matar el mismo enemigo
	// muchas veces sigue siendo algo pero trivial.
	//
	// GANCHO DE AUTOLEVEL / POKEMON-STYLE: si en el futuro se implementa un
	// multiplicador de XP dinámico por diferencia de nivel (más XP si el enemigo
	// es de nivel superior, menos si es inferior), ese multiplicador debería
	// COMBINARSE con éste multiplicando ambos antes de aplicar a BaseXPReward.
	static const float Multipliers[] = { 1.0f, 0.6f, 0.35f, 0.2f, 0.1f };
	static const int32 NumMultipliers = UE_ARRAY_COUNT(Multipliers);

	const int32 Index = FMath::Clamp(KillCount, 0, NumMultipliers - 1);
	return Multipliers[Index];
}