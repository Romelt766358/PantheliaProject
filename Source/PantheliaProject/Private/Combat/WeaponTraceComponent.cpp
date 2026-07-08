// Fill out your copyright notice in the Description page of Project Settings.

#include "Combat/WeaponTraceComponent.h"

#include "AbilitySystem/PantheliaAbilitySystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Combat/LockonComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Kismet/GameplayStatics.h"
#include "PantheliaGameplayTags.h"
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

	// Si no se asignó el mesh del arma en el editor, intentamos resolverlo.
	if (!WeaponMeshComponent)
	{
		ResolveWeaponMesh();
	}
}

void UWeaponTraceComponent::ResolveWeaponMesh()
{
	AActor* Owner = GetOwner();
	if (!Owner) return;

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

void UWeaponTraceComponent::SetDamageSpec(const FGameplayEffectSpecHandle& InDamageSpecHandle)
{
	DamageSpecHandle = InDamageSpecHandle;
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
	// el sweep lee los sockets de este mesh y ResolveWeaponMesh ya no se invoca
	// (PerformTrace solo resuelve si WeaponMeshComponent es null).
	WeaponMeshComponent = InWeaponMesh;

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

void UWeaponTraceComponent::ActivateTrace()
{
	bIsTracing = true;

	// Limpiamos por seguridad: cada swing empieza sin actores ignorados.
	// (DeactivateTrace también limpia, pero esto protege ante notifies solapados.)
	IgnoredActors.Empty();
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
	// Sin mesh de arma no podemos leer los sockets — nada que hacer.
	if (!WeaponMeshComponent) return;

	AActor* Owner = GetOwner();
	if (!Owner) return;

	// Posiciones de inicio (base) y fin (punta) de la hoja en el mundo.
	const FVector StartLocation = WeaponMeshComponent->GetSocketLocation(WeaponBaseSocketName);
	const FVector EndLocation = WeaponMeshComponent->GetSocketLocation(WeaponTipSocketName);

	// Parámetros del sweep: ignoramos siempre al dueño del arma (no autogolpe).
	FCollisionQueryParams QueryParams;
	QueryParams.AddIgnoredActor(Owner);

	// Forma de cápsula a lo largo de la hoja. SweepMultiByChannel con una esfera
	// barrida entre dos puntos equivale a una cápsula que cubre toda la hoja.
	const FCollisionShape SweepShape = FCollisionShape::MakeSphere(TraceRadius);

	TArray<FHitResult> HitResults;
	const bool bHit = GetWorld()->SweepMultiByChannel(
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
		const float HalfHeight = (EndLocation - StartLocation).Size() * 0.5f + TraceRadius;
		const FVector Direction = (EndLocation - StartLocation).GetSafeNormal();
		const FRotator CapsuleRotation = FRotationMatrix::MakeFromZ(Direction).Rotator();
		const FLinearColor DebugColor = bHit ? FLinearColor::Green : FLinearColor::Red;

		UKismetSystemLibrary::DrawDebugCapsule(
			GetWorld(),
			Center,
			HalfHeight,
			TraceRadius,
			CapsuleRotation,
			DebugColor,
			0.f,
			1.f
		);
	}

	if (!bHit) return;

	// El spec debe ser válido para poder aplicar daño. Si la ability no lo
	// configuró (SetDamageSpec), no aplicamos nada — pero el debug ya se dibujó.
	if (!DamageSpecHandle.IsValid()) return;

	// Flag para reproducir el sonido de impacto solo una vez por frame de trace, aunque el
	// sweep golpee a varios actores en el mismo frame.
	bool bImpactSoundPlayedThisTrace = false;

	for (const FHitResult& Hit : HitResults)
	{
		AActor* HitActor = Hit.GetActor();
		if (!HitActor) continue;

		// Un hit por swing por actor: si ya lo golpeamos, lo saltamos.
		if (IgnoredActors.Contains(HitActor)) continue;

		// No dañar a actores del mismo equipo (evita friendly fire entre enemigos).
		if (!UPantheliaAbilitySystemLibrary::IsNotFriend(Owner, HitActor)) continue;

		// Aplicar el daño vía GAS: buscamos el ASC del objetivo y le aplicamos el spec.
		UAbilitySystemComponent* TargetASC =
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(HitActor);

		if (!TargetASC) continue;

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