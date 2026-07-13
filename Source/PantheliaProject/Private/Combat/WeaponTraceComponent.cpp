// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/WeaponTraceComponent.h"

#include "AbilitySystem/PantheliaAbilitySystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Combat/LockonComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "PantheliaGameplayTags.h"
#include "PantheliaLogChannels.h"
#include "GameplayCueManager.h"

UWeaponTraceComponent::UWeaponTraceComponent()
{
	// Necesitamos tick para hacer el sweep continuo durante la ventana de daño.
	// Arranca desactivado; solo trabaja de verdad cuando bIsTracing es true.
	PrimaryComponentTick.bCanEverTick = true;
}

void UWeaponTraceComponent::BeginPlay()
{
	Super::BeginPlay();

	// Si no se asignó el mesh del arma en el editor, intentamos resolverlo solo para
	// owners con fuente interna (enemigos). El jugador usa un Actor de arma externo.
	if (!bUseExternalWeaponSource && !WeaponMeshComponent)
	{
		ResolveWeaponMesh();
	}
}

void UWeaponTraceComponent::ResolveWeaponMesh()
{
	// El jugador debe recibir siempre el mesh desde su arma equipada. Este guard evita
	// que un notify tardío o un orden de inicialización inesperado active el fallback.
	if (bUseExternalWeaponSource)
	{
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner) return;

	WeaponMeshComponent = nullptr;

	// Estrategia 1: buscar un componente con el ComponentTag "Weapon".
	// Permite marcar explícitamente cuál componente es el arma en el Blueprint.
	TArray<UPrimitiveComponent*> PrimitiveComponents;
	Owner->GetComponents<UPrimitiveComponent>(PrimitiveComponents);

	for (UPrimitiveComponent* Comp : PrimitiveComponents)
	{
		if (Comp && Comp->ComponentHasTag(FName("Weapon")))
		{
			WeaponMeshComponent = Comp;
			return;
		}
	}

	// Estrategia 2 (fallback): el primer StaticMeshComponent del actor.
	// Útil para armas que son un Static Mesh hijo del mesh del personaje.
	for (UPrimitiveComponent* Comp : PrimitiveComponents)
	{
		if (Comp && Comp->IsA<UStaticMeshComponent>())
		{
			WeaponMeshComponent = Comp;
			return;
		}
	}
}

bool UWeaponTraceComponent::IsWeaponMeshValidForOwner() const
{
	if (!IsValid(WeaponMeshComponent))
	{
		return false;
	}

	const AActor* Owner = GetOwner();
	const AActor* MeshOwner = WeaponMeshComponent->GetOwner();
	if (!Owner || !MeshOwner)
	{
		return false;
	}

	if (MeshOwner == Owner)
	{
		return true;
	}

	return MeshOwner->GetOwner() == Owner;
}

void UWeaponTraceComponent::SetDamageSpec(const FGameplayEffectSpecHandle& InDamageSpecHandle)
{
	DamageSpecHandle = InDamageSpecHandle;

	if (bLogTraceDebug || bDebugMode)
	{
		const UGameplayEffect* DamageEffect = DamageSpecHandle.IsValid() ? DamageSpecHandle.Data->Def : nullptr;
		UE_LOG(LogPanthelia, Log, TEXT("WeaponTrace SetDamageSpec: Owner=%s Spec=%s Effect=%s Level=%.1f"),
			*GetNameSafe(GetOwner()),
			DamageSpecHandle.IsValid() ? TEXT("valid") : TEXT("invalid"),
			*GetNameSafe(DamageEffect),
			DamageSpecHandle.IsValid() ? DamageSpecHandle.Data->GetLevel() : 0.f);
	}
}

void UWeaponTraceComponent::SetActiveMontageTag(const FGameplayTag& InMontageTag)
{
	// Registra qué montage está activo (ej. Montage.Attack.Weapon). El WeaponTraceNotifyState
	// lo llama en NotifyBegin junto a ActivateTrace(). PerformTrace lo incluye en los
	// AggregatedSourceTags del GameplayCue.Melee.Impact para que GC_MeleeImpact sepa qué
	// ImpactSound reproducir (buscando en el array de TaggedMontages del atacante).
	ActiveMontageTag = InMontageTag;
}

void UWeaponTraceComponent::SetActiveImpactSound(USoundBase* InImpactSound)
{
	// Registra el sonido de impacto del ataque en curso. La ability lo lee del montage del
	// arma equipada y lo pasa antes de la ventana de daño. PerformTrace lo reproduce en el
	// punto de hit. Puede ser null (ataque sin sonido asignado).
	ActiveImpactSound = InImpactSound;
}

void UWeaponTraceComponent::SetAutoLockOnFromBasicAttackHitAllowed(bool bInAllowed)
{
	bAutoLockOnFromBasicAttackHitAllowed = bInAllowed;
}

void UWeaponTraceComponent::SetWeaponMeshComponent(UPrimitiveComponent* InWeaponMesh,
	FName InBaseSocketName, FName InTipSocketName)
{
	// Asignar el mesh externo (el del arma equipada del jugador). A partir de aquí
	// el sweep lee los sockets de este mesh y el modo externo prohíbe por contrato
	// cualquier intento posterior de ResolveWeaponMesh.
	WeaponMeshComponent = InWeaponMesh;

	// Un mesh inyectado pertenece al modo externo. Desde este momento, aunque el
	// actor del arma sea destruido, el componente no debe buscar un fallback interno.
	if (IsValid(InWeaponMesh))
	{
		bUseExternalWeaponSource = true;
	}

	// Si se proveen nombres de socket válidos, los adoptamos. Esto permite que cada
	// arma del jugador defina sus propios nombres de socket en su WeaponDefinition.
	// Si vienen como NAME_None, conservamos los nombres ya configurados.
	if (!InBaseSocketName.IsNone())
	{
		WeaponBaseSocketName = InBaseSocketName;
	}

	if (!InTipSocketName.IsNone())
	{
		WeaponTipSocketName = InTipSocketName;
	}
}

void UWeaponTraceComponent::SetUseExternalWeaponSource(bool bInUseExternalWeaponSource)
{
	bUseExternalWeaponSource = bInUseExternalWeaponSource;

	if (bUseExternalWeaponSource)
	{
		// El modo externo empieza sin asumir ningún componente del owner. La ability
		// inyectará el mesh del arma equipada antes de abrir la ventana de daño.
		WeaponMeshComponent = nullptr;
	}
}

void UWeaponTraceComponent::ClearExternalWeaponTraceSource()
{
	// DeactivateTrace cierra la ventana, limpia los ignorados y revoca el permiso
	// de auto lock-on del swing actual.
	DeactivateTrace();

	WeaponMeshComponent = nullptr;
	DamageSpecHandle = FGameplayEffectSpecHandle();
	ActiveMontageTag = FGameplayTag();
	ActiveImpactSound = nullptr;
	bAutoLockOnFromBasicAttackHitAllowed = false;
	IgnoredActors.Empty();

	// Se conserva el modo externo para impedir que un notify tardío use fallback.
	bUseExternalWeaponSource = true;
}

void UWeaponTraceComponent::ActivateTrace()
{
	StartTrace(TraceRadius);
}

void UWeaponTraceComponent::ActivateTraceWithRadius(float OverrideTraceRadius)
{
	StartTrace(OverrideTraceRadius > 0.f ? OverrideTraceRadius : TraceRadius);
}

void UWeaponTraceComponent::StartTrace(float InTraceRadius)
{
	// Una fuente externa incompleta debe fallar cerrada. Esto cubre el caso donde el
	// arma se desequipa y después llega el NotifyBegin de un montage aún reproduciéndose.
	if (bUseExternalWeaponSource && !IsWeaponMeshValidForOwner())
	{
		bIsTracing = false;

		if (bLogTraceDebug || bDebugMode)
		{
			UE_LOG(LogPanthelia, Warning,
				TEXT("WeaponTrace no iniciado: Owner=%s usa fuente externa sin arma válida."),
				*GetNameSafe(GetOwner()));
		}

		return;
	}

	// Sin un spec válido no existe un golpe aplicable. No abrimos una ventana que
	// pueda quedar activa esperando datos que ya fueron limpiados al desequipar.
	if (!DamageSpecHandle.IsValid())
	{
		bIsTracing = false;

		if (bLogTraceDebug || bDebugMode)
		{
			UE_LOG(LogPanthelia, Warning,
				TEXT("WeaponTrace no iniciado: Owner=%s no tiene DamageSpec válido."),
				*GetNameSafe(GetOwner()));
		}

		return;
	}

	bIsTracing = true;
	ActiveTraceRadius = FMath::Max(0.f, InTraceRadius);

	// Limpiamos por seguridad: cada swing empieza sin actores ignorados.
	// (DeactivateTrace también limpia, pero esto protege ante notifies solapados.)
	IgnoredActors.Empty();

	if (bLogTraceDebug || bDebugMode)
	{
		if (!bUseExternalWeaponSource && !IsWeaponMeshValidForOwner())
		{
			ResolveWeaponMesh();
		}

		const bool bHasBaseSocket = WeaponMeshComponent && WeaponMeshComponent->DoesSocketExist(WeaponBaseSocketName);
		const bool bHasTipSocket = WeaponMeshComponent && WeaponMeshComponent->DoesSocketExist(WeaponTipSocketName);
		const UGameplayEffect* DamageEffect = DamageSpecHandle.IsValid() ? DamageSpecHandle.Data->Def : nullptr;
		const AActor* WeaponOwner = WeaponMeshComponent ? WeaponMeshComponent->GetOwner() : nullptr;
		const USceneComponent* AttachParent = WeaponMeshComponent ? WeaponMeshComponent->GetAttachParent() : nullptr;
		const FVector OwnerLocation = GetOwner() ? GetOwner()->GetActorLocation() : FVector::ZeroVector;
		const FVector WeaponLocation = WeaponMeshComponent ? WeaponMeshComponent->GetComponentLocation() : FVector::ZeroVector;
		const FVector StartLocation = WeaponMeshComponent ? WeaponMeshComponent->GetSocketLocation(WeaponBaseSocketName) : FVector::ZeroVector;
		const FVector EndLocation = WeaponMeshComponent ? WeaponMeshComponent->GetSocketLocation(WeaponTipSocketName) : FVector::ZeroVector;

		UE_LOG(LogPanthelia, Log,
			TEXT("WeaponTrace started: Owner=%s MontageTag=%s Radius=%.1f Weapon=%s WeaponOwner=%s AttachParent=%s BaseSocket=%s(%s) TipSocket=%s(%s) OwnerLoc=%s WeaponLoc=%s Start=%s End=%s DamageSpec=%s Effect=%s"),
			*GetNameSafe(GetOwner()),
			*ActiveMontageTag.ToString(),
			ActiveTraceRadius,
			*GetNameSafe(WeaponMeshComponent),
			*GetNameSafe(WeaponOwner),
			*GetNameSafe(AttachParent),
			*WeaponBaseSocketName.ToString(),
			bHasBaseSocket ? TEXT("exists") : TEXT("missing"),
			*WeaponTipSocketName.ToString(),
			bHasTipSocket ? TEXT("exists") : TEXT("missing"),
			*OwnerLocation.ToCompactString(),
			*WeaponLocation.ToCompactString(),
			*StartLocation.ToCompactString(),
			*EndLocation.ToCompactString(),
			DamageSpecHandle.IsValid() ? TEXT("valid") : TEXT("invalid"),
			*GetNameSafe(DamageEffect));
	}
}

void UWeaponTraceComponent::DeactivateTrace()
{
	bIsTracing = false;

	// Limpiamos la lista para que el próximo swing pueda volver a golpear
	// a los mismos actores.
	IgnoredActors.Empty();

	// El permiso de auto lock-on es por swing. La ability lo volverá a configurar
	// en el siguiente ataque si corresponde.
	bAutoLockOnFromBasicAttackHitAllowed = false;
}

void UWeaponTraceComponent::TickComponent(float DeltaTime, ELevelTick TickType,
	FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

	// Solo trabajamos durante la ventana de daño abierta por el notify state.
	if (!bIsTracing) return;

	PerformTrace();
}

void UWeaponTraceComponent::PerformTrace()
{
	if (!bUseExternalWeaponSource && !IsWeaponMeshValidForOwner())
	{
		ResolveWeaponMesh();
	}

	// Sin un mesh válido y perteneciente al owner no podemos leer sockets. Para el
	// jugador no se intenta fallback; para enemigos ya se intentó resolver arriba.
	if (!IsWeaponMeshValidForOwner())
	{
		if (bLogTraceDebug || bDebugMode)
		{
			UE_LOG(LogPanthelia, Warning, TEXT("WeaponTrace rejected: Owner=%s Reason=no valid weapon mesh"), *GetNameSafe(GetOwner()));
		}
		return;
	}

	AActor* Owner = GetOwner();
	if (!Owner)
	{
		if (bLogTraceDebug || bDebugMode)
		{
			UE_LOG(LogPanthelia, Warning, TEXT("WeaponTrace rejected: Reason=no owner"));
		}
		return;
	}

	// Posiciones de inicio (base) y fin (punta) de la hoja en el mundo.
	const FVector StartLocation = WeaponMeshComponent->GetSocketLocation(WeaponBaseSocketName);
	const FVector EndLocation = WeaponMeshComponent->GetSocketLocation(WeaponTipSocketName);

	// Parámetros del sweep: ignoramos siempre al dueño del arma (no autogolpe).
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Owner);

	// Forma de cápsula a lo largo de la hoja. SweepMultiByChannel con una esfera
	// barrida entre dos puntos equivale a una cápsula que cubre toda la hoja.
	const FCollisionShape SweepShape = FCollisionShape::MakeSphere(ActiveTraceRadius);

	TArray<FHitResult> HitResults;
	GetWorld()->SweepMultiByChannel(
		HitResults,
		StartLocation,
		EndLocation,
		FQuat::Identity,
		TraceChannel,
		SweepShape,
		QueryParams
	);

	if (bDebugMode)
	{
		// Dibuja una cápsula entre base y punta para visualizar el área del sweep.
		const FVector Center = (StartLocation + EndLocation) * 0.5f;
		const float HalfHeight = (EndLocation - StartLocation).Size() * 0.5f + ActiveTraceRadius;
		const FVector Direction = (EndLocation - StartLocation).GetSafeNormal();
		const FRotator CapsuleRotation = FRotationMatrix::MakeFromZ(Direction).Rotator();
		const FLinearColor DebugColor = HitResults.Num() > 0 ? FLinearColor::Green : FLinearColor::Red;

		UKismetSystemLibrary::DrawDebugCapsule(
			GetWorld(),
			Center,
			HalfHeight,
			ActiveTraceRadius,
			CapsuleRotation,
			DebugColor,
			0.f,
			1.f
		);
	}

	if (HitResults.Num() == 0) return;

	// El spec debe ser válido para poder aplicar daño. Si la ability no lo
	// configuró (SetDamageSpec), no aplicamos nada — pero el debug ya se dibujó.
	if (!DamageSpecHandle.IsValid())
	{
		if (bLogTraceDebug || bDebugMode)
		{
			UE_LOG(LogPanthelia, Warning, TEXT("WeaponTrace rejected: Owner=%s Reason=invalid damage spec HitCount=%d"),
				*GetNameSafe(Owner),
				HitResults.Num());
		}
		return;
	}

	// Flag para reproducir el sonido de impacto solo una vez por frame de trace, aunque el
	// sweep golpee a varios actores en el mismo frame.
	bool bImpactSoundPlayedThisTrace = false;

	for (const FHitResult& Hit : HitResults)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor)
		{
			if (bLogTraceDebug || bDebugMode)
			{
				UE_LOG(LogPanthelia, Log, TEXT("WeaponTrace hit rejected: Owner=%s Component=%s Reason=no hit actor"),
					*GetNameSafe(Owner),
					*GetNameSafe(Hit.GetComponent()));
			}
			continue;
		}

		if (bLogTraceDebug || bDebugMode)
		{
			UE_LOG(LogPanthelia, Log, TEXT("WeaponTrace hit actor: Owner=%s Actor=%s Component=%s"),
				*GetNameSafe(Owner),
				*GetNameSafe(HitActor),
				*GetNameSafe(Hit.GetComponent()));
		}

		// Un hit por swing por actor: si ya lo golpeamos, lo saltamos.
		if (IgnoredActors.Contains(HitActor))
		{
			if (bLogTraceDebug || bDebugMode)
			{
				UE_LOG(LogPanthelia, Log, TEXT("WeaponTrace hit rejected: Owner=%s Target=%s Reason=already hit"),
					*GetNameSafe(Owner),
					*GetNameSafe(HitActor));
			}
			continue;
		}

		// No dañar a actores del mismo equipo (evita friendly fire entre enemigos).
		if (!UPantheliaAbilitySystemLibrary::IsNotFriend(Owner, HitActor))
		{
			if (bLogTraceDebug || bDebugMode)
			{
				auto TagsToString = [](const TArray<FName>& Tags)
				{
					FString Result;
					for (const FName& Tag : Tags)
					{
						if (!Result.IsEmpty())
						{
							Result += TEXT(",");
						}
						Result += Tag.ToString();
					}
					return Result;
				};

				UE_LOG(LogPanthelia, Log, TEXT("WeaponTrace hit rejected: Owner=%s Target=%s Reason=friendly target OwnerTags=%s TargetTags=%s"),
					*GetNameSafe(Owner),
					*GetNameSafe(HitActor),
					*TagsToString(Owner->Tags),
					*TagsToString(HitActor->Tags));
			}
			continue;
		}

		// Aplicar el daño vía GAS: buscamos el ASC del objetivo y le aplicamos el spec.
		UAbilitySystemComponent* TargetASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);

		if (!TargetASC)
		{
			if (bLogTraceDebug || bDebugMode)
			{
				UE_LOG(LogPanthelia, Warning, TEXT("WeaponTrace hit rejected: Owner=%s Target=%s Component=%s Reason=no ASC"),
					*GetNameSafe(Owner),
					*GetNameSafe(HitActor),
					*GetNameSafe(Hit.GetComponent()));
			}
			continue;
		}

		// === FIX DE CONTAMINACIÓN DE CONTEXTO (auditoría post-315) — leer antes de tocar ===
		//
		// Este bucle aplica el MISMO DamageSpecHandle a VARIOS objetivos en un mismo
		// swing, y todas esas aplicaciones comparten el MISMO objeto de contexto
		// (el handle es ref-counting, no una copia por objetivo). La versión anterior
		// escribía KnockbackForce/LaunchForce/KnockbackIsHeavy en el contexto SOLO
		// cuando el dado salía, y nunca los limpiaba — así que un éxito para el
		// objetivo A quedaba "pegado" y el objetivo B (cuyo dado falló) salía
		// despedido con el vector de A, o entraba en GA_HeavyKnockback sin motivo.
		//
		// La regla del fix es ESCRIBIR SIEMPRE: para CADA objetivo, ANTES de aplicarle
		// el spec, se escribe en el contexto el resultado de SUS tiradas — vector real
		// si ganó, ZeroVector/false si no. Así cada aplicación lee un contexto que
		// describe SU golpe, nunca el del objetivo anterior. (Es el mismo principio
		// que ya seguía el crítico en ExecCalc_Damage, y el mismo fix aplicado allí
		// al parry y al debuff, y en PantheliaProjectile.cpp por consistencia.)

		// --- KNOCKBACK (clase 315) ---
		// Adaptación respecto al proyectil (PantheliaProjectile.cpp): un ataque melee no
		// tiene una "dirección de vuelo" que reutilizar — en su lugar, usamos la dirección
		// del ATACANTE (Owner, quien empuña el arma) hacia el OBJETIVO golpeado (HitActor).
		// Los valores por defecto (ZeroVector / false) son lo que se escribe si el dado
		// no sale — "sin knockback" para ESTE objetivo, sin heredar nada del anterior.
		FVector KnockbackForce = FVector::ZeroVector;
		bool bKnockbackIsHeavyThisTarget = false;

		const float KnockbackForceMagnitude = DamageSpecHandle.Data->GetSetByCallerMagnitude(
			FPantheliaGameplayTags::Get().CombatTricks_KnockbackForceMagnitude, false, 0.f);

		if (KnockbackForceMagnitude > 0.f)
		{
			const float KnockbackChance = DamageSpecHandle.Data->GetSetByCallerMagnitude(
				FPantheliaGameplayTags::Get().CombatTricks_KnockbackChance, false, 0.f);

			const bool bKnockback = FMath::RandRange(1, 100) <= KnockbackChance;

			if (bKnockback)
			{
				// Dirección atacante -> objetivo (normalizada), con el mismo pitch override
				// de 45° que usa el proyectil — consistencia de sensación entre ambos tipos
				// de ataque, aunque el cálculo de la dirección base sea distinto.
				const FVector ToTarget = (HitActor->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal();
				const FVector KnockbackDirection = UPantheliaAbilitySystemLibrary::GetDirectionWithPitchOverride(
					ToTarget, 45.f);

				KnockbackForce = KnockbackDirection * KnockbackForceMagnitude;

				// Nivel 2 (a petición): si la ability marcó este knockback como "pesado",
				// lo anotamos también — HandleIncomingDamage decidirá qué hacer con ello
				// (bloquear HitReact + disparar GA_HeavyKnockback en vez del comportamiento
				// normal). No hace falta un roll de dado aparte: "pesado" no es una
				// probabilidad propia, es una propiedad fija de la ability que se activa
				// junto con el knockback normal cuando este ya tuvo éxito.
				bKnockbackIsHeavyThisTarget = DamageSpecHandle.Data->GetSetByCallerMagnitude(
					FPantheliaGameplayTags::Get().CombatTricks_KnockbackIsHeavy, false, 0.f) > 0.5f;
			}
		}

		// --- LAUNCH / NIVEL 3 (post-315, a petición) ---
		// Mismo mecanismo que el Knockback de arriba, como sistema independiente — ver
		// la explicación completa en PantheliaProjectile.cpp (mismo bloque, adaptado ahí
		// también). Dirección atacante -> objetivo, igual que el Knockback de este archivo.
		FVector LaunchForce = FVector::ZeroVector;

		const float LaunchForceMagnitude = DamageSpecHandle.Data->GetSetByCallerMagnitude(
			FPantheliaGameplayTags::Get().CombatTricks_LaunchForceMagnitude, false, 0.f);

		if (LaunchForceMagnitude > 0.f)
		{
			const float LaunchChance = DamageSpecHandle.Data->GetSetByCallerMagnitude(
				FPantheliaGameplayTags::Get().CombatTricks_LaunchChance, false, 0.f);

			const bool bLaunch = FMath::RandRange(1, 100) <= LaunchChance;

			if (bLaunch)
			{
				const float LaunchPitchOverride = DamageSpecHandle.Data->GetSetByCallerMagnitude(
					FPantheliaGameplayTags::Get().CombatTricks_LaunchPitchOverride, false, 65.f);

				const FVector ToTargetLaunch = (HitActor->GetActorLocation() - Owner->GetActorLocation()).GetSafeNormal();
				const FVector LaunchDirection = UPantheliaAbilitySystemLibrary::GetDirectionWithPitchOverride(
					ToTargetLaunch, LaunchPitchOverride);

				LaunchForce = LaunchDirection * LaunchForceMagnitude;
			}
		}

		// ESCRIBIR SIEMPRE (el corazón del fix): los tres resultados van al contexto en
		// cada iteración, con el valor real de ESTE objetivo — éxito o cero/false.
		FGameplayEffectContextHandle ContextHandle = DamageSpecHandle.Data->GetContext();
		UPantheliaAbilitySystemLibrary::SetKnockbackForce(ContextHandle, KnockbackForce);
		UPantheliaAbilitySystemLibrary::SetLaunchForce(ContextHandle, LaunchForce);
		UPantheliaAbilitySystemLibrary::SetKnockbackIsHeavy(ContextHandle, bKnockbackIsHeavyThisTarget);

		// Si este swing es un ataque básico del jugador, permite que el lock-on se fije
		// automáticamente al enemigo golpeado. La propia función del LockonComponent protege
		// la regla principal: si ya hay lock-on activo, no cambia de target.
		if (bAutoLockOnFromBasicAttackHitAllowed)
		{
			if (ULockonComponent* LockonComponent = Owner->FindComponentByClass<ULockonComponent>())
			{
				LockonComponent->TryAutoLockOnFromBasicAttackHit(HitActor);
			}
		}

		TargetASC->ApplyGameplayEffectSpecToSelf(*DamageSpecHandle.Data.Get());

		if (bLogTraceDebug || bDebugMode)
		{
			const FPantheliaGameplayTags& GameplayTags = FPantheliaGameplayTags::Get();
			const float PhysicalDamage = DamageSpecHandle.Data->GetSetByCallerMagnitude(
				GameplayTags.Damage_Physical, false, 0.f);
			const float PoiseDamage = DamageSpecHandle.Data->GetSetByCallerMagnitude(
				GameplayTags.Damage_Poise, false, 0.f);

			UE_LOG(LogPanthelia, Log, TEXT("WeaponTrace damage applied: Owner=%s Target=%s Effect=%s Level=%.1f Physical=%.1f Poise=%.1f Invulnerable=%s"),
				*GetNameSafe(Owner),
				*GetNameSafe(HitActor),
				*GetNameSafe(DamageSpecHandle.Data->Def),
				DamageSpecHandle.Data->GetLevel(),
				PhysicalDamage,
				PoiseDamage,
				TargetASC->HasMatchingGameplayTag(GameplayTags.State_Invulnerable) ? TEXT("true") : TEXT("false"));
		}

		// Disparar el Gameplay Cue de impacto melee (efectos visuales + sonido).
		// Usamos el ASC del ATACANTE (Owner) para el dispatch, que garantiza replicacion
		// correcta incluso para enemigos IA (cuyo ASC no pertenece a ningun player).
		// Parametros del Cue:
		// Location = punto de impacto real en la superficie del actor golpeado.
		// SourceObject = la victima (HitActor): GC_MeleeImpact llama GetBloodEffect
		// sobre este para obtener las particulas propias del personaje.
		// EffectCauser = el atacante (Owner): GC_MeleeImpact busca en su array de
		// TaggedMontages el ImpactSound del montage activo.
		// AggregatedSourceTags = contiene el ActiveMontageTag para que el GC identifique
		// que sonido usar sin necesidad de un cast.
		UAbilitySystemComponent* OwnerASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Owner);

		if (OwnerASC)
		{
			FGameplayCueParameters CueParams;
			CueParams.Location = Hit.ImpactPoint;
			CueParams.SourceObject = HitActor;
			CueParams.EffectCauser = Owner;

			if (ActiveMontageTag.IsValid())
			{
				CueParams.AggregatedSourceTags.AddTag(ActiveMontageTag);
			}

			OwnerASC->ExecuteGameplayCue(
				FPantheliaGameplayTags::Get().GameplayCue_Melee_Impact,
				CueParams);
		}

		// Reproducir el sonido de impacto del ataque (si la ability asigno uno). Directo con
		// PlaySoundAtLocation porque el juego es single-player (no necesita replicacion por Cue).
		// Solo suena cuando hay hit real; un swing al aire no reproduce nada. Usamos un flag
		// local para reproducir UNA sola vez por frame aunque el sweep golpee a varios actores
		// (dos clangs solapados sonarian mal). El blood/Cue si se dispara por cada victima.
		if (ActiveImpactSound && !bImpactSoundPlayedThisTrace)
		{
			UGameplayStatics::PlaySoundAtLocation(this, ActiveImpactSound, Hit.ImpactPoint);
			bImpactSoundPlayedThisTrace = true;
		}

		// Marcamos al actor como ya golpeado en este swing.
		IgnoredActors.AddUnique(HitActor);
	}
}
