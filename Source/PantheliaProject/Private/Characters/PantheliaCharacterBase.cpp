// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/PantheliaCharacterBase.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "AbilitySystem/PantheliaAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "AbilitySystem/PantheliaAttributeSet.h"
#include "PantheliaGameplayTags.h"
#include "PantheliaProject.h"
#include "NiagaraSystem.h"
#include "Kismet/GameplayStatics.h"
// Necesario para crear BurnDebuffComponent en el constructor (clase 311).
#include "AbilitySystem/Debuff/PantheliaDebuffNiagaraComponent.h"
// Necesario para ResolveDeathWeaponMesh (corrección post-314): encontrar el arma real
// de un enemigo, que vive en un componente separado de FinalWeaponMesh.
#include "Combat/WeaponTraceComponent.h"

APantheliaCharacterBase::APantheliaCharacterBase()
{
	PrimaryActorTick.bCanEverTick = false;

	// BurnDebuffComponent (clase 311): un UPantheliaDebuffNiagaraComponent dedicado a la
	// Quemadura, compartido por TODOS los personajes (jugador y enemigos) al vivir aquí,
	// en la clase base. VisibleAnywhere en vez de EditDefaultsOnly (ver el .h): así cada
	// Blueprint concreto puede reposicionarlo/asignarle su Niagara System sin poder
	// crear uno nuevo por accidente — solo existe UNO por personaje, ya creado aquí.
	//
	// Por qué SetupAttachment a GetRootComponent() y no a GetMesh(): el root no se ve
	// afectado por animaciones de esqueleto, así que la posición configurada en cada
	// Blueprint (Details → BurnDebuffComponent → Transform) se mantiene estable sin
	// bailar con el movimiento de huesos — el propio curso ajusta esta posición a mano
	// por personaje (cabeza, pecho, espalda...) en el editor, no en código.
	BurnDebuffComponent = CreateDefaultSubobject<UPantheliaDebuffNiagaraComponent>(TEXT("BurnDebuffComponent"));
	BurnDebuffComponent->SetupAttachment(GetRootComponent());
	BurnDebuffComponent->DebuffTag = FPantheliaGameplayTags::Get().Debuff_Burn;

	// Los otros 3 elementos — mismo patrón exacto que Burn, añadidos a petición.
	// FPantheliaGameplayTags::Get() ya se llamó arriba, pero lo volvemos a llamar aquí:
	// es un singleton (siempre devuelve la misma instancia ya construida), así que no
	// hay coste real en llamarlo varias veces — resulta más legible que guardar una
	// variable local `Tags` solo para 4 usos consecutivos.
	ShockDebuffComponent = CreateDefaultSubobject<UPantheliaDebuffNiagaraComponent>(TEXT("ShockDebuffComponent"));
	ShockDebuffComponent->SetupAttachment(GetRootComponent());
	ShockDebuffComponent->DebuffTag = FPantheliaGameplayTags::Get().Debuff_Shock;

	SaturationDebuffComponent = CreateDefaultSubobject<UPantheliaDebuffNiagaraComponent>(TEXT("SaturationDebuffComponent"));
	SaturationDebuffComponent->SetupAttachment(GetRootComponent());
	SaturationDebuffComponent->DebuffTag = FPantheliaGameplayTags::Get().Debuff_Saturation;

	PoisonDebuffComponent = CreateDefaultSubobject<UPantheliaDebuffNiagaraComponent>(TEXT("PoisonDebuffComponent"));
	PoisonDebuffComponent->SetupAttachment(GetRootComponent());
	PoisonDebuffComponent->DebuffTag = FPantheliaGameplayTags::Get().Debuff_Poison;

	FinalWeaponMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("FinalWeaponMesh"));
	FinalWeaponMesh->SetupAttachment(GetMesh(), FName("Hand_R_Sword"));
	FinalWeaponMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Camera, ECR_Ignore);
	GetMesh()->SetCollisionResponseToChannel(ECC_Projectile, ECR_Overlap);
	GetMesh()->SetCollisionResponseToChannel(ECC_Fighter, ECR_Overlap);
	GetMesh()->SetGenerateOverlapEvents(true);
	GetCapsuleComponent()->SetGenerateOverlapEvents(false);
}

UAbilitySystemComponent* APantheliaCharacterBase::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

FVector APantheliaCharacterBase::GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag)
{
	const FPantheliaGameplayTags& GameplayTags = FPantheliaGameplayTags::Get();

	// CASO ARMA: usar FinalWeaponMesh (el arma equipada) con el WeaponTipSocketName.
	// WeaponTipSocketName se configura por Blueprint (típicamente "WeaponTip").
	// Si el arma no está disponible, fallback a la malla del personaje.
	if (MontageTag.MatchesTagExact(GameplayTags.Montage_Attack_Weapon))
	{
		if (IsValid(FinalWeaponMesh))
		{
			return FinalWeaponMesh->GetSocketLocation(WeaponTipSocketName);
		}
		if (IsValid(GetMesh()))
		{
			return GetMesh()->GetSocketLocation(WeaponTipSocketName);
		}
		return GetActorLocation();
	}

	// CASOS SIN ARMA: sockets en el mesh del personaje.
	// Elegimos el nombre de socket según el tag y leemos su posición del mesh.
	check(IsValid(GetMesh()));

	FName SocketName = NAME_None;

	if (MontageTag.MatchesTagExact(GameplayTags.Montage_Attack_RightHand))
	{
		SocketName = RightHandSocketName;
	}
	else if (MontageTag.MatchesTagExact(GameplayTags.Montage_Attack_LeftHand))
	{
		SocketName = LeftHandSocketName;
	}
	else if (MontageTag.MatchesTagExact(GameplayTags.Montage_Attack_RightFoot))
	{
		SocketName = RightFootSocketName;
	}
	else if (MontageTag.MatchesTagExact(GameplayTags.Montage_Attack_LeftFoot))
	{
		SocketName = LeftFootSocketName;
	}
	else if (MontageTag.MatchesTagExact(GameplayTags.Montage_Attack_Mouth))
	{
		SocketName = MouthSocketName;
	}

	// Si el tag coincidió con alguno conocido, devolvemos la posición de ese socket.
	if (SocketName != NAME_None)
	{
		return GetMesh()->GetSocketLocation(SocketName);
	}

	// FALLBACK: tag desconocido o inválido. Devolvemos la ubicación del mesh para
	// evitar spawns en el origen del mundo (0,0,0) si algo no está configurado.
	UE_LOG(LogTemp, Warning,
		TEXT("GetCombatSocketLocation: MontageTag '%s' no reconocido en %s — usando ubicación del mesh."),
		*MontageTag.ToString(), *GetName());
	return GetMesh()->GetComponentLocation();
}

bool APantheliaCharacterBase::IsDead_Implementation() const
{
	return bDead;
}

AActor* APantheliaCharacterBase::GetAvatar_Implementation()
{
	return this;
}

TArray<FTaggedMontage> APantheliaCharacterBase::GetAttackMontages_Implementation()
{
	return AttackMontages;
}

UAnimMontage* APantheliaCharacterBase::GetHitReactMontage_Implementation()
{
	return HitReactMontage;
}

UAnimMontage* APantheliaCharacterBase::GetStaggerMontage_Implementation()
{
	return StaggerMontage;
}

UAnimMontage* APantheliaCharacterBase::GetGetUpMontage_Implementation()
{
	return GetUpMontage;
}

UAnimMontage* APantheliaCharacterBase::GetHeavyKnockbackMontage_Implementation()
{
	return HeavyKnockbackMontage;
}

UNiagaraSystem* APantheliaCharacterBase::GetBloodEffect_Implementation()
{
	// Gancho de impacto: devuelve el efecto Niagara asignado en el Blueprint de este
	// personaje (vacío por ahora). El WeaponTraceComponent lo obtendra de la victima al
	// detectar un hit y lo spawneara en el punto de impacto.
	return BloodEffect;
}

EPantheliaCharacterClass APantheliaCharacterBase::GetCharacterClass_Implementation() const
{
	// Devuelve el arquetipo configurado en el Blueprint de este personaje.
	// Enemigos: configuran CharacterClass en Details → Panthelia|Combat.
	// Jugador: AMainCharacter sobreescribe CharacterClass = Elementalist en su constructor.
	return CharacterClass;
}

void APantheliaCharacterBase::Die(const FVector& DeathImpulse)
{
	// Al morir cancelamos el timer de regen de postura y el de decay de buildup —
	// un cadáver no regenera postura ni "se enfría" de sus barras de estado (y el
	// bloqueo global de muertos en PostGameplayEffectExecute ya impide que entre
	// buildup nuevo; esto solo apaga el tick que quedaría girando en vacío).
	GetWorldTimerManager().ClearTimer(PoiseRegenTimerHandle);
	GetWorldTimerManager().ClearTimer(BuildupDecayTimerHandle);

	// Limpieza de estado de combate. Es especialmente importante para el jugador:
	// su ASC vive en PlayerState y sobrevive al Pawn, así que sin esta limpieza una
	// barra parcial, una Quemadura activa o State.Airborne podrían reaparecer tras
	// el futuro respawn. En enemigos también es correcto y evita ticks innecesarios.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponent())
	{
		ASC->SetNumericAttributeBase(UPantheliaAttributeSet::GetFireBuildupAttribute(), 0.f);
		ASC->SetNumericAttributeBase(UPantheliaAttributeSet::GetStormBuildupAttribute(), 0.f);
		ASC->SetNumericAttributeBase(UPantheliaAttributeSet::GetWaterBuildupAttribute(), 0.f);
		ASC->SetNumericAttributeBase(UPantheliaAttributeSet::GetNatureBuildupAttribute(), 0.f);

		FGameplayTagContainer ElementalStatusTags;
		const FPantheliaGameplayTags& Tags = FPantheliaGameplayTags::Get();
		ElementalStatusTags.AddTag(Tags.Debuff_Burn);
		ElementalStatusTags.AddTag(Tags.Debuff_Shock);
		ElementalStatusTags.AddTag(Tags.Debuff_Saturation);
		ElementalStatusTags.AddTag(Tags.Debuff_Poison);
		ASC->RemoveActiveEffectsWithGrantedTags(ElementalStatusTags);

		// Si murió durante un launch, Landed() ya no tiene por qué ejecutarse.
		ASC->SetLooseGameplayTagCount(Tags.State_Airborne, 0);
	}

	// Corrección post-314: antes esto era siempre FinalWeaponMesh->DetachFromComponent(...).
	// Ahora se desprende el arma REAL de este personaje (ver ResolveDeathWeaponMesh en el
	// header para la explicación completa de por qué hace falta esta indirección).
	if (UPrimitiveComponent* WeaponMesh = ResolveDeathWeaponMesh())
	{
		WeaponMesh->DetachFromComponent(
			FDetachmentTransformRules(EDetachmentRule::KeepWorld, true)
		);
	}

	MulticastHandleDeath(DeathImpulse);
}

void APantheliaCharacterBase::MulticastHandleDeath(const FVector& DeathImpulse)
{
	// Marcamos el personaje como muerto para que IsDead_Implementation devuelva true.
	// Esto permite a GetLivePlayersWithinRadius filtrar cadáveres correctamente.
	bDead = true;

	// Reproducir el sonido de muerte si está asignado en el BP del personaje.
	// Se reproduce antes del dissolve para que suene junto al momento del "colapso".
	if (DeathSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, DeathSound, GetActorLocation(), GetActorRotation());
	}

	// Corrección post-314: ANTES estos 3 bloques (física del arma, física del mesh,
	// impulsos) usaban siempre FinalWeaponMesh directamente. Ahora se resuelve UNA vez
	// cuál es el arma real de este personaje (ver ResolveDeathWeaponMesh en el header) y
	// se reutiliza — así funciona igual para un personaje con FinalWeaponMesh real (el
	// modelo original) y para un enemigo cuya arma vive en un componente separado que
	// UWeaponTraceComponent ya encontró en su propio BeginPlay.
	UPrimitiveComponent* WeaponMesh = ResolveDeathWeaponMesh();
	if (WeaponMesh)
	{
		WeaponMesh->SetSimulatePhysics(true);
		WeaponMesh->SetEnableGravity(true);
		WeaponMesh->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	}

	GetMesh()->SetSimulatePhysics(true);
	GetMesh()->SetEnableGravity(true);
	GetMesh()->SetCollisionEnabled(ECollisionEnabled::PhysicsOnly);
	GetMesh()->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);

	// --- IMPULSO DE MUERTE (clase 314) ---
	// AddImpulse SOLO tiene efecto DESPUÉS de habilitar SetSimulatePhysics(true) arriba
	// — sobre un mesh que no simula física, un impulso no hace absolutamente nada (no
	// hay ningún cuerpo físico al que aplicárselo todavía). Por eso este bloque va aquí
	// y no antes.
	//
	// bVelChange = true (segundo parámetro después del bone name) — descubrimiento clave
	// de la transcripción original: sin esto, AddImpulse tiene en cuenta la MASA del
	// cuerpo, lo que obliga a usar magnitudes gigantescas e imprevisibles (miles de
	// unidades) para conseguir un movimiento visible. Con bVelChange=true, el valor que
	// pasamos se interpreta directamente como un CAMBIO DE VELOCIDAD, ignorando la masa
	// — mismo impulso, resultado visual mucho más predecible y fácil de afinar en Details.
	//
	// WeaponDeathImpulseScale (ver .h): el arma es mucho más ligera que el cuerpo y
	// necesita una fracción del impulso para no salir disparada de forma ridícula —
	// ajustable por Blueprint, sin tocar código, para cada personaje/arma.
	GetMesh()->AddImpulse(DeathImpulse, NAME_None, true);
	if (WeaponMesh)
	{
		WeaponMesh->AddImpulse(DeathImpulse * WeaponDeathImpulseScale, NAME_None, true);
	}

	GetCapsuleComponent()->SetCollisionEnabled(ECollisionEnabled::NoCollision);

	Dissolve();

	// Broadcast del delegate de muerte (clase 311) — AL FINAL, después de que bDead ya
	// es true y toda la lógica de muerte (ragdoll, sonido, dissolve) ya corrió. Cualquier
	// listener (como UPantheliaDebuffNiagaraComponent, que se desactiva al recibir esto)
	// reacciona sobre un personaje ya completamente en su estado "muerto" final.
	OnDeath.Broadcast(this);
}

void APantheliaCharacterBase::Dissolve()
{
	if (IsValid(DissolveMaterialInstance))
	{
		UMaterialInstanceDynamic* DynamicMatInst = UMaterialInstanceDynamic::Create(DissolveMaterialInstance, this);
		GetMesh()->SetMaterial(0, DynamicMatInst);
		StartDissolveTimeline(DynamicMatInst);
	}

	if (IsValid(WeaponDissolveMaterialInstance))
	{
		// Corrección post-314: antes esto era siempre FinalWeaponMesh->SetMaterial(...).
		// Mismo motivo que en Die()/MulticastHandleDeath(): sin este cambio, el arma que
		// de verdad se ve en pantalla (para un enemigo normal, un componente distinto de
		// FinalWeaponMesh) nunca recibía el material de disolución y se quedaba visible,
		// sólida, después de que el resto del cuerpo desapareciera.
		if (UPrimitiveComponent* WeaponMesh = ResolveDeathWeaponMesh())
		{
			UMaterialInstanceDynamic* DynamicMatInst = UMaterialInstanceDynamic::Create(WeaponDissolveMaterialInstance, this);
			WeaponMesh->SetMaterial(0, DynamicMatInst);
			StartWeaponDissolveTimeline(DynamicMatInst);
		}
	}
}

UPrimitiveComponent* APantheliaCharacterBase::ResolveDeathWeaponMesh() const
{
	// Estrategia 1: si este personaje tiene un UWeaponTraceComponent (jugador vía C++,
	// o cualquier enemigo que lo añada en su Blueprint — FindComponentByClass encuentra
	// ambos casos por igual, sin importar cómo se creó), su WeaponMeshComponent YA es
	// el arma real que se ve en pantalla — WeaponTraceComponent lo resolvió solo en su
	// propio BeginPlay (ResolveWeaponMesh(): busca un componente con tag "Weapon", o si
	// no lo halla, el primer Static Mesh del actor).
	if (const UWeaponTraceComponent* WeaponTrace = FindComponentByClass<UWeaponTraceComponent>())
	{
		if (IsValid(WeaponTrace->WeaponMeshComponent))
		{
			return WeaponTrace->WeaponMeshComponent;
		}
	}

	// Estrategia 2 (respaldo): no hay WeaponTraceComponent, o no logró resolver nada.
	// Usamos FinalWeaponMesh — el modelo original (curso), donde el arma sí es este
	// componente creado en C++. Mantiene el comportamiento de antes de este arreglo
	// para cualquier personaje que no use WeaponTraceComponent en absoluto.
	return FinalWeaponMesh;
}

void APantheliaCharacterBase::BeginPlay()
{
	Super::BeginPlay();
}

// ============================================================
// Landed (post-315, Nivel 3 de knockback — a petición)
// ============================================================
// ACharacter::Landed() ya existe en el motor y se dispara SOLO — Unreal lo llama
// automáticamente cuando el CharacterMovementComponent detecta la transición de
// "cayendo" a "en el suelo" (no hace falta ningún timer ni sondear IsFalling() a mano).
//
// El problema que resuelve este override: un salto normal, caerse de un borde, o el
// lanzamiento de este Nivel 3 TODOS terminan en un Landed() — pero solo el último debe
// disparar GA_GetUp. La distinción la hace el tag State.Airborne: solo se concede
// cuando HandleIncomingDamage aplica un Launch (ver PantheliaAttributeSet.cpp), nunca
// en un salto/caída normal — así que comprobarlo aquí es exactamente la señal correcta.
void APantheliaCharacterBase::Landed(const FHitResult& Hit)
{
	Super::Landed(Hit);

	UAbilitySystemComponent* ASC = GetAbilitySystemComponent();
	if (!ASC) return;

	if (ASC->HasMatchingGameplayTag(FPantheliaGameplayTags::Get().State_Airborne))
	{
		// SetLooseGameplayTagCount(Tag, 0) en vez de RemoveLooseGameplayTag: los tags
		// sueltos (loose tags) llevan un contador interno — si por lo que sea el tag se
		// concedió más de una vez mientras el personaje estaba en el aire (ej. dos
		// lanzamientos encadenados sin aterrizar entre medias), RemoveLooseGameplayTag
		// solo lo bajaría en 1, dejando el tag todavía activo. Fijar el contador a 0
		// directamente lo apaga sin importar cuántas veces se concedió.
		ASC->SetLooseGameplayTagCount(FPantheliaGameplayTags::Get().State_Airborne, 0);

		// Dispara GA_GetUp — mismo patrón exacto que usa GA_HitReact (activarse desde
		// FUERA de la propia ability, vía TryActivateAbilitiesByTag con su Ability Tag).
		FGameplayTagContainer GetUpTags;
		GetUpTags.AddTag(FPantheliaGameplayTags::Get().Effects_GetUp);
		ASC->TryActivateAbilitiesByTag(GetUpTags);
	}
}

void APantheliaCharacterBase::InitAbilityActorInfo()
{
	// Implementación base vacía.
}

FActiveGameplayEffectHandle APantheliaCharacterBase::ApplyEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level) const
{
	check(IsValid(GetAbilitySystemComponent()));
	check(GameplayEffectClass);

	FGameplayEffectContextHandle ContextHandle = GetAbilitySystemComponent()->MakeEffectContext();
	ContextHandle.AddSourceObject(this);

	const FGameplayEffectSpecHandle SpecHandle = GetAbilitySystemComponent()->MakeOutgoingSpec(
		GameplayEffectClass, Level, ContextHandle);

	// ApplyGameplayEffectSpecToTarget devuelve un FActiveGameplayEffectHandle: la "llave" que
	// identifica esta instancia activa concreta del efecto. La devolvemos al llamador para que
	// pueda guardarla y, si hace falta, quitar este efecto más adelante (ver comentario extendido
	// en el .h sobre por qué esta función ya no es void).
	return GetAbilitySystemComponent()->ApplyGameplayEffectSpecToTarget(
		*SpecHandle.Data.Get(), GetAbilitySystemComponent());
}

void APantheliaCharacterBase::InitializeDefaultAttributes() const
{
	// El estado persistente de inicialización vive en el ASC (ver explicación extendida
	// en PantheliaAbilitySystemComponent.h). CastChecked: en este proyecto el ASC es
	// SIEMPRE UPantheliaAbilitySystemComponent; si no lo fuera, es un error de setup y
	// preferimos el crash explícito en desarrollo a un fallo silencioso.
	// Nota sobre const: esta función es const, pero escribir en los miembros del ASC a
	// través del puntero es legal en C++ (const protege a ESTE personaje, no a los
	// objetos a los que apunta) — por eso ya no hace falta el "mutable" que necesitaba
	// el handle cuando era miembro de esta clase.
	UPantheliaAbilitySystemComponent* PantheliaASC =
		CastChecked<UPantheliaAbilitySystemComponent>(GetAbilitySystemComponent());

	// === GUARDA DE RESPAWN (Etapa 4) ===
	// Si este ASC ya inicializó sus atributos alguna vez, NO volvemos a aplicarlos.
	// El caso que esto protege: el jugador muere y respawnea → Pawn NUEVO, pero el ASC
	// (en el PlayerState) es el MISMO y conserva sus atributos, incluidos los puntos
	// primarios ya gastados. Reaplicar los GEs por defecto aquí los machacaría a sus
	// valores iniciales y apilaría un segundo GE Infinite de secundarios.
	// Para los enemigos esta guarda es transparente: su ASC nace con cada enemigo, así
	// que siempre entra con false (además, APantheliaEnemy sobrescribe esta función
	// con su propia ruta por CharacterClassInfo y ni siquiera pasa por aquí).
	if (PantheliaASC->bAttributesInitialized) return;

	ApplyEffectToSelf(DefaultPrimaryAttributes, 1.f);

	// Guardamos el handle de esta aplicación inicial. RefreshSecondaryAttributes() lo usará
	// más adelante para poder QUITAR esta instancia exacta antes de crear una nueva (ver
	// explicación extendida en el .h, sección RefreshSecondaryAttributes).
	PantheliaASC->SecondaryAttributesEffectHandle = ApplyEffectToSelf(DefaultSecondaryAttributes, 1.f);

	ApplyEffectToSelf(DefaultVitalAttributes, 1.f);

	// Marcamos la guarda AL FINAL, con los tres GEs ya aplicados correctamente.
	PantheliaASC->bAttributesInitialized = true;
}

void APantheliaCharacterBase::RefreshSecondaryAttributes() const
{
	// El handle vive en el ASC desde la Etapa 4 (ver InitializeDefaultAttributes arriba
	// y la explicación extendida en PantheliaAbilitySystemComponent.h).
	UPantheliaAbilitySystemComponent* PantheliaASC =
		CastChecked<UPantheliaAbilitySystemComponent>(GetAbilitySystemComponent());

	// === ENFOQUE: QUITAR + REAPLICAR (no "refrescar") ===
	//
	// Un primer intento configuró Stacking en el asset (Aggregate By Target + Refresh On
	// Successful Application) esperando que reaplicar el mismo GameplayEffect bastara para
	// forzar el recálculo de MaxHealth/MaxMana/MaxStamina. Las pruebas en juego confirmaron
	// que NO es así: ese Stacking solo refresca el temporizador de duración del efecto, pero
	// no vuelve a ejecutar CalculateBaseMagnitude_Implementation de los MMC con el nivel nuevo.
	//
	// La única forma determinista de forzar ese recálculo es destruir la instancia activa
	// existente y crear una completamente nueva desde cero — una instancia nueva SIEMPRE
	// re-ejecuta el cálculo completo de sus modificadores con los valores actuales.
	if (PantheliaASC->SecondaryAttributesEffectHandle.IsValid())
	{
		// -1 en StacksToRemove elimina TODOS los stacks de esta instancia (aunque aquí,
		// con Stack Limit = 1, como mucho hay uno).
		PantheliaASC->RemoveActiveGameplayEffect(PantheliaASC->SecondaryAttributesEffectHandle, -1);
	}

	// Creamos una instancia nueva y guardamos su handle para la SIGUIENTE vez que
	// se llame a esta función (por ejemplo, al subir otro nivel más adelante).
	PantheliaASC->SecondaryAttributesEffectHandle = ApplyEffectToSelf(DefaultSecondaryAttributes, 1.f);
}

void APantheliaCharacterBase::AddCharacterAbilities()
{
	if (!HasAuthority()) return;

	UPantheliaAbilitySystemComponent* PantheliaASC = CastChecked<UPantheliaAbilitySystemComponent>(AbilitySystemComponent);
	PantheliaASC->AddCharacterAbilities(StartupAbilities);
}

// ============================================================
// SISTEMA DE REGENERACIÓN DE POSTURA
// ============================================================

void APantheliaCharacterBase::ResetPoiseRegenTimer()
{
	// Cancelamos cualquier timer activo (delay pendiente o tick de regen en curso).
	GetWorldTimerManager().ClearTimer(PoiseRegenTimerHandle);

	// Iniciamos un nuevo delay. Cada golpe lo reinicia — igual que en Elden Ring.
	GetWorldTimerManager().SetTimer(
		PoiseRegenTimerHandle,
		this,
		&APantheliaCharacterBase::StartPoiseRegen,
		PoiseRegenDelay,
		false // no loop — es solo el delay inicial
	);
}

void APantheliaCharacterBase::StartPoiseRegen()
{
	// El delay terminó. Iniciamos el tick de regeneración cada 0.1s.
	GetWorldTimerManager().SetTimer(
		PoiseRegenTimerHandle,
		this,
		&APantheliaCharacterBase::TickPoiseRegen,
		0.1f,
		true // loop
	);
}

void APantheliaCharacterBase::TickPoiseRegen()
{
	// Usamos el AttributeSet directamente para leer y escribir Poise.
	// SetPoise() del macro ATTRIBUTE_ACCESSORS llama internamente a
	// SetNumericAttributeBase() en el ASC — la forma correcta en UE 5.7.
	// (SetAttributeBaseValue() fue eliminado de UAbilitySystemComponent en UE 5.x)
	UPantheliaAttributeSet* PAS = Cast<UPantheliaAttributeSet>(AttributeSet);
	if (!PAS) return;

	const float CurrentPoise = PAS->GetPoise();
	const float MaxPoiseValue = PAS->GetMaxPoise();

	if (CurrentPoise >= MaxPoiseValue)
	{
		// Postura llena — paramos el timer.
		GetWorldTimerManager().ClearTimer(PoiseRegenTimerHandle);
		return;
	}

	// PoiseRegenRate unidades/segundo × 0.1s por tick = unidades por tick.
	const float NewPoise = FMath::Min(CurrentPoise + PoiseRegenRate * 0.1f, MaxPoiseValue);
	PAS->SetPoise(NewPoise);
}

// ============================================================
// DECAY DE LAS BARRAS DE BUILDUP ELEMENTAL (sistema de umbral)
// ============================================================
// Clon deliberado del patrón de regeneración de postura de arriba (timer de 0.1s +
// escritura directa de la base vía los setters del AttributeSet), con dos diferencias
// razonadas: (1) SIN delay inicial — en Elden Ring las barras de estado empiezan a
// caer inmediatamente; la tensión del sistema es la carrera entre tu ritmo de golpeo
// y el decay constante, no una ventana de gracia; (2) el ritmo escala con la
// RESISTENCIA del propio personaje (decisión cerrada): a más resistencia, más rápido
// se sacude el estado — segunda mitad del doble rol de las resistencias (la primera
// mitad, reducir el intake, vive en el ExecCalc).

void APantheliaCharacterBase::NotifyElementalBuildupReceived()
{
	// Si el timer ya está girando (recibimos buildup con barras aún cargadas), no
	// hay nada que hacer: el tick ya se encarga. Solo lo arrancamos si estaba
	// detenido — a diferencia de ResetPoiseRegenTimer, aquí NO reiniciamos nada
	// (no hay delay que resetear: el decay corre siempre que haya barra).
	if (GetWorldTimerManager().IsTimerActive(BuildupDecayTimerHandle))
	{
		return;
	}

	GetWorldTimerManager().SetTimer(
		BuildupDecayTimerHandle,
		this,
		&APantheliaCharacterBase::TickBuildupDecay,
		0.1f,
		true // loop — se auto-detiene dentro del tick cuando las 4 barras lleguen a 0
	);
}

void APantheliaCharacterBase::TickBuildupDecay()
{
	// Mismo acceso directo al AttributeSet que TickPoiseRegen (y por el mismo motivo:
	// SetXBuildup → SetNumericAttributeBase, la vía correcta en UE 5.x para escribir
	// la base sin pasar por un GE). Escribir la base directamente NO dispara
	// PostGameplayEffectExecute — imprescindible aquí: vaciar la barra jamás debe
	// re-evaluar el umbral de disparo.
	UPantheliaAttributeSet* PAS = Cast<UPantheliaAttributeSet>(AttributeSet);
	if (!PAS)
	{
		GetWorldTimerManager().ClearTimer(BuildupDecayTimerHandle);
		return;
	}

	// Por cada elemento: si su barra tiene contenido, restarle su decay de este tick.
	// DecayPorSegundo = BuildupDecayRate × (1 + Res/100) → ×0.1 por tick de 0.1s.
	// La resistencia se lee del PROPIO personaje (es SU capacidad de sacudirse el
	// estado), no de quien le pegó.
	bool bAnyRemaining = false;

	struct FDecayRoute
	{
		float Current;
		float Resistance;
		void (UPantheliaAttributeSet::*Setter)(float);
	};
	const FDecayRoute Routes[] =
	{
		{ PAS->GetFireBuildup(),   PAS->GetFireResistance(),   &UPantheliaAttributeSet::SetFireBuildup },
		{ PAS->GetStormBuildup(),  PAS->GetStormResistance(),  &UPantheliaAttributeSet::SetStormBuildup },
		{ PAS->GetWaterBuildup(),  PAS->GetWaterResistance(),  &UPantheliaAttributeSet::SetWaterBuildup },
		{ PAS->GetNatureBuildup(), PAS->GetNatureResistance(), &UPantheliaAttributeSet::SetNatureBuildup },
	};

	for (const FDecayRoute& Route : Routes)
	{
		if (Route.Current <= 0.f)
		{
			continue;
		}

		const float DecayThisTick = BuildupDecayRate * (1.f + Route.Resistance / 100.f) * 0.1f;
		const float NewValue = FMath::Max(0.f, Route.Current - DecayThisTick);
		(PAS->*Route.Setter)(NewValue);

		if (NewValue > 0.f)
		{
			bAnyRemaining = true;
		}
	}

	// Las 4 barras vacías → el timer se apaga solo. El siguiente golpe con buildup
	// lo volverá a arrancar vía NotifyElementalBuildupReceived. Coste cero en reposo.
	if (!bAnyRemaining)
	{
		GetWorldTimerManager().ClearTimer(BuildupDecayTimerHandle);
	}
}
