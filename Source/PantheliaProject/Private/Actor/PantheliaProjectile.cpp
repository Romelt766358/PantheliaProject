// Fill out your copyright notice in the Description page of Project Settings.

#include "Actor/PantheliaProjectile.h"
#include "Components/SphereComponent.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraFunctionLibrary.h"
#include "Components/AudioComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "PantheliaProject.h"
// Necesarios para leer DeathImpulseMagnitude (SetByCaller) y escribir el vector final
// en el contexto del spec antes de aplicarlo (clase 313).
#include "PantheliaGameplayTags.h"
#include "AbilitySystem/PantheliaAbilitySystemLibrary.h"

namespace
{
	const FName PlayerTeamTag(TEXT("Player"));
	const FName EnemyTeamTag(TEXT("Enemy"));

	bool HasCombatTeamTag(const AActor* Actor)
	{
		return IsValid(Actor)
			&& (Actor->ActorHasTag(PlayerTeamTag) || Actor->ActorHasTag(EnemyTeamTag));
	}

	AActor* ResolveCombatTeamActor(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return nullptr;
		}

		if (HasCombatTeamTag(Actor))
		{
			return Actor;
		}

		AActor* InstigatorActor = Actor->GetInstigator();
		if (HasCombatTeamTag(InstigatorActor))
		{
			return InstigatorActor;
		}

		AActor* OwnerActor = Actor->GetOwner();
		if (HasCombatTeamTag(OwnerActor))
		{
			return OwnerActor;
		}

		return nullptr;
	}
}

APantheliaProjectile::APantheliaProjectile()
{
	PrimaryActorTick.bCanEverTick = false;
	bReplicates = true;

	Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
	SetRootComponent(Sphere);

	Sphere->SetCollisionObjectType(ECC_Projectile);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	ProjectileMovement = CreateDefaultSubobject<UProjectileMovementComponent>(TEXT("ProjectileMovement"));
	ProjectileMovement->InitialSpeed = 550.f;
	ProjectileMovement->MaxSpeed = 550.f;
	ProjectileMovement->ProjectileGravityScale = 0.f;
}

void APantheliaProjectile::BeginPlay()
{
	Super::BeginPlay();

	SetLifeSpan(Lifespan);

	// Reaplicamos la política de colisión en runtime porque los defaults de un
	// Blueprint hijo pueden conservar respuestas serializadas de una versión anterior.
	// Pawn usa overlap para permitir atravesar aliados e i-frames; la geometría usa
	// blocking hit para no depender de GenerateOverlapEvents en cada pared del mapa.
	Sphere->SetCollisionObjectType(ECC_Projectile);
	Sphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
	Sphere->SetGenerateOverlapEvents(true);
	Sphere->SetCollisionResponseToAllChannels(ECR_Ignore);
	Sphere->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
	Sphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);

	Sphere->OnComponentBeginOverlap.AddDynamic(this, &APantheliaProjectile::OnSphereOverlap);

	if (ProjectileMovement)
	{
		ProjectileMovement->OnProjectileStop.AddDynamic(
			this, &APantheliaProjectile::OnProjectileStopped);
	}

	if (IsValid(LoopingSound))
	{
		LoopingSoundComponent = UGameplayStatics::SpawnSoundAttached(LoopingSound, GetRootComponent());
	}
}

void APantheliaProjectile::OnSphereOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
	UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
	bool bFromSweep, const FHitResult& SweepResult)
{
	// FIX 1: Ignorar si el spec no está inicializado o el overlap no aporta actor.
	// En cliente (sin autoridad) el spec nunca se setea — evita crash al hacer .Get().
	if (!DamageEffectSpecHandle.Data.IsValid() || !IsValid(OtherActor)) return;
	if (bHit) return;

	const TWeakObjectPtr<AActor> OtherActorKey(OtherActor);
	if (IgnoredActors.Contains(OtherActorKey)) return;

	const FGameplayEffectContextHandle EffectContext =
		DamageEffectSpecHandle.Data.Get()->GetContext();

	// FIX 2: Ignorar si el actor golpeado es el propio lanzador del proyectil.
	// El EffectCauser en el context es normalmente el AvatarActor (el personaje), no
	// el proyectil. Instigator queda como fallback por si una fuente construye el spec
	// con un context distinto. También protegemos el caso teórico de overlap propio.
	AActor* SourceActor = EffectContext.GetEffectCauser();
	if (!IsValid(SourceActor))
	{
		SourceActor = GetInstigator();
	}

	if (OtherActor == this || OtherActor == SourceActor || OtherActor == GetInstigator()) return;

	// FIX COMBAT-07: los aliados no reciben GE, no generan feedback ofensivo y no
	// consumen el proyectil. Solo aplicamos la comparación de equipos cuando ambos
	// actores declaran un equipo de combate; la geometría y props sin tags siguen
	// consumiendo el proyectil normalmente.
	AActor* SourceTeamActor = ResolveCombatTeamActor(SourceActor);
	AActor* TargetTeamActor = ResolveCombatTeamActor(OtherActor);
	if (SourceTeamActor && TargetTeamActor
		&& !UPantheliaAbilitySystemLibrary::IsNotFriend(SourceTeamActor, TargetTeamActor))
	{
		IgnoredActors.Add(OtherActorKey);
		return;
	}

	// FIX 3: La geometría y los objetivos hostiles consumen el proyectil. Los aliados
	// y los objetivos que niegan el golpe mediante i-frames se registran en IgnoredActors
	// y el proyectil continúa. bHit solo se activa al final, cuando el consumo es real.
	bool bShouldConsumeProjectile = true;

	// La geometría sin ASC conserva feedback de impacto. Contra un objetivo con ASC,
	// el ExecCalc decide si el golpe fue realmente aceptado o si lo negó una defensa.
	bool bShouldPlayImpactFeedback = true;

	// Aplicamos el daño solo en el servidor/singleplayer.
	// El atributo Health está replicado — el cliente verá el cambio automáticamente.
	if (HasAuthority())
	{
		// --- IMPULSO DE MUERTE (clase 313) ---
		// Aquí, y solo aquí, conocemos la DIRECCIÓN del impacto (hacia donde viajaba el
		// proyectil) — la ability que creó este spec (allá en ApplyDamageScalingToSpec)
		// solo conocía la MAGNITUD (DeathImpulseMagnitude), porque en ese punto del
		// pipeline el proyectil ni siquiera había volado todavía. Por eso: leemos la
		// magnitud de vuelta desde el SetByCaller que la ability ya dejó en el spec,
		// la combinamos con la dirección (GetActorForwardVector — válido incluso para
		// un proyectil con trayectoria curva, porque ForwardVector siempre apunta hacia
		// donde se está moviendo AHORA, en el instante del impacto, no hacia donde
		// apuntaba al spawnear), y escribimos el vector resultante DIRECTO en el context
		// del spec — DEBE ser antes de ApplyGameplayEffectSpecToSelf más abajo, o
		// HandleIncomingDamage (que lee esto en la clase siguiente) llegaría demasiado
		// tarde y encontraría el vector todavía en cero.
		const FPantheliaGameplayTags& GameplayTags = FPantheliaGameplayTags::Get();
		const float DeathImpulseMagnitude = DamageEffectSpecHandle.Data->GetSetByCallerMagnitude(
			GameplayTags.CombatTricks_DeathImpulseMagnitude, false, 0.f);

		if (DeathImpulseMagnitude > 0.f)
		{
			const FVector DeathImpulse = GetActorForwardVector() * DeathImpulseMagnitude;

			// GetContext() devuelve el handle POR VALOR, pero internamente comparte el
			// mismo objeto de contexto subyacente (ref-counting) que ya vive dentro del
			// spec — escribir en esta copia local del handle SÍ modifica el context real
			// que luego lee HandleIncomingDamage (mismo patrón ya usado en
			// ExecCalc_Damage::DetermineDebuff, clase 309).
			FGameplayEffectContextHandle ContextHandle = DamageEffectSpecHandle.Data->GetContext();
			UPantheliaAbilitySystemLibrary::SetDeathImpulse(ContextHandle, DeathImpulse);
		}

		// --- KNOCKBACK (clase 315) + LAUNCH (Nivel 3) — patrón "escribir siempre" ---
		//
		// FIX DE CONTAMINACIÓN DE CONTEXTO (auditoría post-315): la versión anterior
		// escribía KnockbackForce/LaunchForce/KnockbackIsHeavy en el contexto SOLO en
		// caso de éxito del dado. Hoy este proyectil impacta a un único objetivo y se
		// destruye, así que aquí el bug no se manifestaba — pero el mismo patrón en
		// WeaponTraceComponent (multi-objetivo, contexto compartido) sí contaminaba a
		// objetivos posteriores, y el fix se aplica en AMBOS sitios por consistencia y
		// para que un futuro proyectil perforante no reintroduzca el bug: cada golpe
		// escribe SIEMPRE sus tres resultados (vector real si ganó la tirada,
		// ZeroVector/false si no), igual que ya hacía el crítico en ExecCalc_Damage.
		//
		// El knockback tiene una PROBABILIDAD de activarse (KnockbackChance), no se
		// aplica siempre que la magnitud sea > 0. Por eso primero comprobamos que haya
		// magnitud configurada (evita tirar el dado para abilities que ni siquiera
		// tienen knockback), y luego tiramos el dado de 100 caras — mismo patrón que
		// DetermineDebuff en ExecCalc_Damage.
		FVector KnockbackForce = FVector::ZeroVector;
		bool bKnockbackIsHeavyThisHit = false;

		const float KnockbackForceMagnitude = DamageEffectSpecHandle.Data->GetSetByCallerMagnitude(
			GameplayTags.CombatTricks_KnockbackForceMagnitude, false, 0.f);

		if (KnockbackForceMagnitude > 0.f)
		{
			const float KnockbackChance = DamageEffectSpecHandle.Data->GetSetByCallerMagnitude(
				GameplayTags.CombatTricks_KnockbackChance, false, 0.f);

			const bool bKnockback = FMath::RandRange(1, 100) <= KnockbackChance;
			if (bKnockback)
			{
				// Pitch override de 45°: un knockback perfectamente horizontal se ve raro
				// (el objetivo "resbala" en vez de "salir volando"). Lanzarlo con un ángulo
				// hacia arriba consistente, sin importar hacia dónde viajaba el proyectil,
				// es lo que de verdad vende la sensación de "salió disparado por el aire".
				const FVector KnockbackDirection = UPantheliaAbilitySystemLibrary::GetDirectionWithPitchOverride(
					GetActorForwardVector(), 45.f);
				KnockbackForce = KnockbackDirection * KnockbackForceMagnitude;

				// Nivel 2 (a petición) — ver la explicación completa en
				// WeaponTraceComponent.cpp (mismo bloque, adaptado ahí también).
				bKnockbackIsHeavyThisHit = DamageEffectSpecHandle.Data->GetSetByCallerMagnitude(
					GameplayTags.CombatTricks_KnockbackIsHeavy, false, 0.f) > 0.5f;
			}
		}

		// --- LAUNCH / NIVEL 3 (post-315, a petición) ---
		// Mismo mecanismo que el Knockback de arriba (leer magnitud, tirar el dado si
		// hay magnitud configurada, calcular vector con pitch override), pero como
		// sistema COMPLETAMENTE INDEPENDIENTE — su propio tag de magnitud/chance, su
		// propio campo de contexto (LaunchForce, no KnockbackForce), y su propio pitch
		// override CONFIGURABLE por ability (no un 45° fijo como Knockback) — algunas
		// fuentes de daño querrán un lanzamiento más o menos vertical.
		FVector LaunchForce = FVector::ZeroVector;

		const float LaunchForceMagnitude = DamageEffectSpecHandle.Data->GetSetByCallerMagnitude(
			GameplayTags.CombatTricks_LaunchForceMagnitude, false, 0.f);

		if (LaunchForceMagnitude > 0.f)
		{
			const float LaunchChance = DamageEffectSpecHandle.Data->GetSetByCallerMagnitude(
				GameplayTags.CombatTricks_LaunchChance, false, 0.f);

			const bool bLaunch = FMath::RandRange(1, 100) <= LaunchChance;
			if (bLaunch)
			{
				const float LaunchPitchOverride = DamageEffectSpecHandle.Data->GetSetByCallerMagnitude(
					GameplayTags.CombatTricks_LaunchPitchOverride, false, 65.f);

				const FVector LaunchDirection = UPantheliaAbilitySystemLibrary::GetDirectionWithPitchOverride(
					GetActorForwardVector(), LaunchPitchOverride);
				LaunchForce = LaunchDirection * LaunchForceMagnitude;
			}
		}

		// ESCRIBIR SIEMPRE (el corazón del fix): los tres resultados van al contexto
		// con el valor real de ESTE impacto — éxito o cero/false.
		{
			FGameplayEffectContextHandle ContextHandle = DamageEffectSpecHandle.Data->GetContext();
			UPantheliaAbilitySystemLibrary::SetKnockbackForce(ContextHandle, KnockbackForce);
			UPantheliaAbilitySystemLibrary::SetLaunchForce(ContextHandle, LaunchForce);
			UPantheliaAbilitySystemLibrary::SetKnockbackIsHeavy(ContextHandle, bKnockbackIsHeavyThisHit);
		}

		if (UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(OtherActor))
		{
			// El spec puede reutilizar su context durante toda la vida del proyectil.
			// Reiniciamos explícitamente el resultado antes de aplicar para no depender
			// de ningún valor previo escrito por otra ruta.
			FGameplayEffectContextHandle ContextHandle =
				DamageEffectSpecHandle.Data->GetContext();
			UPantheliaAbilitySystemLibrary::SetHitOutcome(
				ContextHandle, EPantheliaHitOutcome::Unresolved);

			TargetASC->ApplyGameplayEffectSpecToSelf(*DamageEffectSpecHandle.Data.Get());

			// ApplyGameplayEffectSpecToSelf ejecuta el ExecCalc de forma síncrona.
			// Solo Accepted reproduce el impacto ofensivo normal. I-frames, parry
			// perfecto y bloqueo conservan exclusivamente su feedback defensivo.
			const EPantheliaHitOutcome HitOutcome =
				UPantheliaAbilitySystemLibrary::GetHitOutcome(ContextHandle);
			bShouldPlayImpactFeedback =
				HitOutcome == EPantheliaHitOutcome::Accepted;

			// Los i-frames representan que el proyectil no conectó físicamente con el
			// personaje. No se destruye: atraviesa al objetivo y continúa su trayectoria.
			// Parry perfecto y bloqueo sí consumen el proyectil porque hubo contacto
			// defensivo, aunque su feedback normal quede suprimido.
			if (HitOutcome == EPantheliaHitOutcome::NegatedInvulnerability)
			{
				IgnoredActors.Add(OtherActorKey);
				bShouldConsumeProjectile = false;
			}
		}
	}

	if (!bShouldConsumeProjectile)
	{
		return;
	}

	// A partir de aquí el overlap consume el proyectil. Cualquier overlap adicional
	// que llegue antes de que Destroy() se procese queda bloqueado por bHit.
	ConsumeProjectile(bShouldPlayImpactFeedback, GetActorLocation());
}

void APantheliaProjectile::OnProjectileStopped(const FHitResult& ImpactResult)
{
	// Los personajes nunca deberían llegar por esta ruta: sus cápsulas ignoran el
	// canal Projectile y sus meshes generan overlap. Esta callback queda reservada
	// para paredes, suelo y props que bloquean el canal.
	if (bHit)
	{
		return;
	}

	FVector ImpactLocation = GetActorLocation();
	if (ImpactResult.bBlockingHit)
	{
		ImpactLocation = FVector(ImpactResult.ImpactPoint);
	}

	ConsumeProjectile(/*bPlayImpactFeedback=*/true, ImpactLocation);
}

void APantheliaProjectile::ConsumeProjectile(
	const bool bPlayImpactFeedback,
	const FVector& ImpactLocation)
{
	if (bHit)
	{
		return;
	}

	bHit = true;

	if (IsValid(LoopingSoundComponent))
	{
		LoopingSoundComponent->Stop();
	}

	if (bPlayImpactFeedback)
	{
		if (IsValid(ImpactSound))
		{
			UGameplayStatics::PlaySoundAtLocation(
				this, ImpactSound, ImpactLocation, FRotator::ZeroRotator);
		}

		if (IsValid(ImpactEffect))
		{
			UNiagaraFunctionLibrary::SpawnSystemAtLocation(
				this, ImpactEffect, ImpactLocation);
		}
	}

	Destroy();
}