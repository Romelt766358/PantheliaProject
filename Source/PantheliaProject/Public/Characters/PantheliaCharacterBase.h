// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "AbilitySystemInterface.h"
#include "GameplayTagContainer.h"
// GameplayEffectTypes.h define FActiveGameplayEffectHandle, que usamos POR VALOR
// (retorno de ApplyEffectToSelf y miembro SecondaryAttributesEffectHandle).
// Un forward declaration no basta para tipos usados por valor: el compilador
// necesita conocer el tamaño completo del struct — por eso el include completo.
#include "GameplayEffectTypes.h"
#include "Interfaces/CombatInterface.h"
#include "PantheliaCharacterBase.generated.h"

class USkeletalMeshComponent;
class UAbilitySystemComponent;
class UAttributeSet;
class UGameplayEffect;
class UGameplayAbility;
class UAnimMontage;
class UMaterialInstance;
class UMaterialInstanceDynamic;
class UNiagaraSystem;
class USoundBase;
class UPantheliaDebuffNiagaraComponent;
class UWeaponTraceComponent;
class UPrimitiveComponent;

UCLASS(ABSTRACT)
class PANTHELIAPROJECT_API APantheliaCharacterBase : public ACharacter, public IAbilitySystemInterface, public ICombatInterface
{
	GENERATED_BODY()

public:
	APantheliaCharacterBase();

	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	UAttributeSet* GetAttributeSet() const { return AttributeSet; }

	// ICombatInterface
	// GetCombatSocketLocation_Implementation: override del BlueprintNativeEvent.
	// Devuelve la posición del socket apropiado según el MontageTag recibido:
	//   Montage.Attack.Weapon    → WeaponTipSocketName en FinalWeaponMesh (arma equipada)
	//   Montage.Attack.RightHand → RightHandSocketName en el mesh del personaje
	//   Montage.Attack.LeftHand  → LeftHandSocketName  en el mesh del personaje
	//   Montage.Attack.RightFoot → RightFootSocketName en el mesh
	//   Montage.Attack.LeftFoot  → LeftFootSocketName  en el mesh
	//   Montage.Attack.Mouth     → MouthSocketName     en el mesh
	//   (tag desconocido)        → ubicación del mesh (fallback)
	// Centralizado aquí — todos los enemigos lo heredan sin reimplementarlo en Blueprint.
	// Para spawn de proyectiles y detección de hits de melee.
	virtual FVector GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag) override;

	// IsDead_Implementation: devuelve bDead.
	// bDead se pone a true en MulticastHandleDeath().
	virtual bool IsDead_Implementation() const override;

	// GetAvatar_Implementation: devuelve 'this'.
	// Permite obtener el actor sin cast desde código que solo tiene ICombatInterface*.
	virtual AActor* GetAvatar_Implementation() override;

	// GetAttackMontages_Implementation: devuelve AttackMontages.
	// GA_MeleeAttack lo llama para obtener qué montage + socket usar en el ataque.
	virtual TArray<FTaggedMontage> GetAttackMontages_Implementation() override;
	virtual UAnimMontage* GetHitReactMontage_Implementation() override;
	virtual UAnimMontage* GetStaggerMontage_Implementation() override;

	// Devuelve GetUpMontage (declarado más abajo) — mismo patrón que los dos de arriba.
	virtual UAnimMontage* GetGetUpMontage_Implementation() override;
	// GetBloodEffect_Implementation: devuelve BloodEffect (gancho de impacto, vacío por ahora).
	virtual UNiagaraSystem* GetBloodEffect_Implementation() override;
	virtual void Die(const FVector& DeathImpulse) override;

	// Overrides de los getters puros virtuales de ICombatInterface (clase 311).
	// Devuelven una referencia a los miembros protegidos declarados más abajo
	// (OnASCRegistered / OnDeath) — ver la explicación completa en CombatInterface.h.
	virtual FOnASCRegistered& GetOnASCRegisteredDelegate() override { return OnASCRegistered; }
	virtual FOnDeath& GetOnDeathDelegate() override { return OnDeath; }

	// GetCharacterClass_Implementation: devuelve el arquetipo de este personaje.
	// La clase base devuelve el miembro CharacterClass (protected).
	// Enemigos: lo heredan y simplemente configuran CharacterClass en su Blueprint.
	// Jugador: AMainCharacter sobreescribe CharacterClass = Elementalist en su constructor.
	virtual EPantheliaCharacterClass GetCharacterClass_Implementation() const override;

	// Devuelve el % de MaxPoise que un golpe debe infligir para activar HitReact.
	// Enemigos normales: 10%, Bosses: 15% (configurable por Blueprint).
	virtual float GetFlinchThreshold() const override { return FlinchThreshold; }

	// Reinicia el timer de regeneración de postura.
	// Llamado por el AttributeSet cada vez que se recibe daño de postura.
	virtual void ResetPoiseRegenTimer() override;

protected:
	virtual void BeginPlay() override;

	// Override de ACharacter::Landed (post-315, Nivel 3 de knockback) — se dispara
	// automáticamente cuando el CharacterMovementComponent detecta que el personaje
	// pasa de "cayendo" a "en el suelo". Aquí comprobamos si ESE aterrizaje en concreto
	// viene de un lanzamiento (State.Airborne activo) para disparar GA_GetUp — un
	// salto o caída normal NO tiene ese tag, así que Landed() no hace nada especial en
	// esos casos. Ver la implementación en el .cpp para la explicación completa.
	virtual void Landed(const FHitResult& Hit) override;
	virtual void InitAbilityActorInfo();
	virtual void InitializeDefaultAttributes() const;

	// Reaplica ÚNICAMENTE el GameplayEffect de atributos secundarios (DefaultSecondaryAttributes).
	//
	// POR QUÉ EXISTE ESTA FUNCIÓN (explicación extendida para quien no conozca este detalle de GAS):
	// Los MMC (Modifier Magnitude Calculation) de MaxHealth/MaxMana/MaxStamina leen el nivel del
	// personaje mediante ICombatInterface::Execute_GetPlayerLevel() DENTRO de su función de cálculo
	// C++ (CalculateBaseMagnitude_Implementation). Esa lectura NO pasa por el sistema de "capturas"
	// de GAS (RelevantAttributesToCapture) — GAS solo sabe recalcular automáticamente un GameplayEffect
	// cuando cambia un ATRIBUTO que él mismo está vigilando (p. ej. Resilience para MaxHealth). El nivel
	// del personaje es, para GAS, una variable "invisible": puede cambiar sin que GAS se entere, y el
	// GameplayEffect de atributos secundarios se queda con un valor de nivel "congelado" (el que tenía
	// en el momento en que se aplicó) hasta que algo fuerza una reevaluación.
	//
	// CÓMO LO RESOLVEMOS (enfoque QUITAR + REAPLICAR, no "refrescar"): un primer intento usó
	// Stacking (Aggregate By Target + Refresh On Successful Application) esperando que reaplicar
	// el mismo GameplayEffect forzara el recálculo. Las pruebas confirmaron que NO es así: ese
	// Stacking solo refresca el temporizador de duración, no vuelve a ejecutar CalculateBaseMagnitude.
	// La única forma 100% determinista de forzar un recálculo real es DESTRUIR la instancia activa
	// existente (RemoveActiveGameplayEffect) y crear una completamente NUEVA (ApplyEffectToSelf),
	// lo cual sí re-ejecuta CalculateBaseMagnitude_Implementation desde cero con el nivel actual.
	// Por eso guardamos el handle de la instancia activa en SecondaryAttributesEffectHandle —
	// que desde la Etapa 4 vive en UPantheliaAbilitySystemComponent, no aquí, para que
	// sobreviva al respawn del Pawn del jugador (el ASC del jugador vive en el PlayerState).
	//
	// CUÁNDO SE LLAMA: cada vez que el nivel del personaje cambia (en AMainCharacter, enganchada al
	// delegate OnLevelChangedDelegate del PlayerState). Así MaxHealth/MaxMana/MaxStamina se actualizan
	// de inmediato al subir de nivel.
	virtual void RefreshSecondaryAttributes() const;

	// Controla si el personaje ha muerto.
	// Se pone a true en MulticastHandleDeath() para que IsDead_Implementation lo devuelva.
	// No necesita replicación: MulticastHandleDeath() se llama en todos los contextos.
	bool bDead = false;

	// --- Delegates de ICombatInterface (clase 311) ---
	// Backing storage de los overrides GetOnASCRegisteredDelegate/GetOnDeathDelegate
	// (declarados arriba, en la sección pública). Ver CombatInterface.h para la
	// explicación completa de qué son y por qué existen.
	//
	// OnASCRegistered se broadcastea en InitAbilityActorInfo — pero OJO: la clase BASE
	// (APantheliaCharacterBase::InitAbilityActorInfo, ver .cpp) está vacía a propósito.
	// El broadcast real vive en CADA subclase (APantheliaEnemy::InitAbilityActorInfo y
	// AMainCharacter::InitAbilityActorInfo), justo después de que cada una asigna su
	// propio AbilitySystemComponent — porque ninguna de las dos llama a Super() aquí
	// (mismo motivo que documenta el comentario de MulticastHandleDeath: es más simple y
	// menos propenso a errores que forzar una llamada a Super() en el orden correcto).
	FOnASCRegistered OnASCRegistered;

	// OnDeath se broadcastea en MulticastHandleDeath() (ver .cpp) — al final, después de
	// que bDead ya es true y toda la lógica de muerte (ragdoll, dissolve, sonido) corrió.
	FOnDeath OnDeath;

	// Aplica el GameplayEffect indicado sobre este mismo personaje (self-application).
	// Devuelve el FActiveGameplayEffectHandle de la instancia creada.
	//
	// POR QUÉ DEVUELVE UN HANDLE (antes era void): un handle es la "llave" que identifica una
	// instancia activa concreta de un GameplayEffect. Sin guardarlo, no hay forma de quitar ese
	// efecto después (por ejemplo, para forzar un recálculo real, como hace RefreshSecondaryAttributes,
	// o para remover un buff/hechizo temporal cuando termine). Devolver el handle aquí es la base
	// para que CUALQUIER sistema futuro (árbol de habilidades, buffs de pociones, auras de aliados,
	// efectos de armas) pueda rastrear y quitar sus propios efectos activos según haga falta —
	// es un cambio pequeño ahora que evita tener que rehacer esta función más adelante.
	// Los llamadores que no necesiten el handle (como antes) simplemente pueden ignorar el valor
	// de retorno; no rompe ningún uso existente.
	FActiveGameplayEffectHandle ApplyEffectToSelf(TSubclassOf<UGameplayEffect> GameplayEffectClass, float Level) const;
	virtual void MulticastHandleDeath(const FVector& DeathImpulse);
	void Dissolve();

	// --- Resolución del arma real para la secuencia de muerte (corrección post-314) ---
	// PROBLEMA QUE RESUELVE: FinalWeaponMesh (arriba) es un USkeletalMeshComponent creado
	// en C++ en esta misma clase, pensado como "el arma" en el modelo original del curso.
	// Pero en Panthelia, los enemigos normales (Warrior, Shaman, y cualquiera basado en
	// APantheliaEnemy) tienen su arma real en un componente APARTE — normalmente un
	// UStaticMeshComponent con el tag "Weapon" añadido a mano en el Blueprint — que
	// UWeaponTraceComponent encuentra solo, en su propio BeginPlay, vía ResolveWeaponMesh()
	// (busca ese tag, o si no lo halla, el primer Static Mesh del actor). FinalWeaponMesh
	// se queda vacío/sin usar para esos personajes: desprenderlo, activarle física o
	// aplicarle un impulso no tiene ningún efecto visible, porque no es lo que se ve en
	// pantalla. Solo BP_Boss funcionaba porque alguien conectó ambos a mano en su
	// Construction Script — un parche por Blueprint, no una solución que escale.
	//
	// SOLUCIÓN: esta función centraliza la pregunta "¿cuál es el arma de verdad, ahora
	// mismo, para ESTE personaje?" en un solo sitio de C++, para que Die(), 
	// MulticastHandleDeath() y Dissolve() no tengan que decidirlo cada uno por su cuenta
	// (evita 3 copias de la misma lógica, y que se desincronicen si un día cambia).
	// Devuelve UPrimitiveComponent* (no USkeletalMeshComponent*) porque es la clase base
	// común entre USkeletalMeshComponent (FinalWeaponMesh) y UStaticMeshComponent (el
	// arma típica de un enemigo) — DetachFromComponent, SetSimulatePhysics,
	// SetEnableGravity, SetCollisionEnabled y AddImpulse existen en esa clase base común,
	// así que el resto del código no necesita saber cuál de los dos tipos es en realidad.
	//
	// FindComponentByClass<UWeaponTraceComponent>() funciona igual si el componente se
	// añadió en C++ (como en AMainCharacter) o solo en Blueprint (como en los enemigos):
	// busca por TIPO en todos los componentes del actor, sin importar cómo se creó.
	//
	// Si el personaje no tiene UWeaponTraceComponent en absoluto, o su WeaponMeshComponent
	// nunca se resolvió (nullptr), cae de vuelta a FinalWeaponMesh — así no se rompe nada
	// para personajes que sigan el modelo original (ej. si BP_Boss no tuviera
	// WeaponTraceComponent, seguiría funcionando igual que antes de este arreglo).
	UPrimitiveComponent* ResolveDeathWeaponMesh() const;

	UFUNCTION(BlueprintImplementableEvent)
	void StartDissolveTimeline(UMaterialInstanceDynamic* DynamicMaterialInstance);

	UFUNCTION(BlueprintImplementableEvent)
	void StartWeaponDissolveTimeline(UMaterialInstanceDynamic* DynamicMaterialInstance);

	// --- POSTURA / FLINCH ---

	// % de MaxPoise que un golpe debe infligir en un solo hit para activar HitReact.
	// Default: 10% (enemigos normales). Bosses: 15%.
	// Editable por Blueprint en Details → Panthelia|Combat|Poise.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panthelia|Combat|Poise")
	float FlinchThreshold = 10.f;

	// Postura regenerada por segundo después del delay post-golpe.
	// Editable por Blueprint en Details → Panthelia|Combat|Poise.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panthelia|Combat|Poise")
	float PoiseRegenRate = 8.f;

	// Segundos a esperar después del último golpe antes de comenzar a regenerar postura.
	// Editable por Blueprint en Details → Panthelia|Combat|Poise.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panthelia|Combat|Poise")
	float PoiseRegenDelay = 2.5f;

	// Arquetipo de este personaje (Warrior, Ranger, Elementalist).
	// Movido desde PantheliaEnemy a CharacterBase para que el jugador también
	// tenga clase (AMainCharacter lo sobreescribe a Elementalist en su constructor).
	// Enemigos configuran el valor en Details → Panthelia|Combat de su Blueprint.
	//
	// GANCHO DE AUTOLEVEL: el sistema de escalado de zonas leerá CharacterClass + Level
	// para calcular la XP reescalada cuando ese sistema exista. Por ahora, CharacterClass
	// se usa únicamente para lookup en DA_CharacterClassInfo (atributos/abilities de clase).
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Panthelia|Combat")
	EPantheliaCharacterClass CharacterClass = EPantheliaCharacterClass::Warrior;

private:

	// Timer usado para la regeneración de postura.
	// ResetPoiseRegenTimer() lo reinicia con cada golpe.
	// StartPoiseRegen() comienza el tick de regen después del delay.
	FTimerHandle PoiseRegenTimerHandle;

	// Inicia el loop de regeneración de postura (llamado tras PoiseRegenDelay).
	void StartPoiseRegen();

	// Se llama cada 0.1s mientras regenera — añade PoiseRegenRate * 0.1 a Poise.
	void TickPoiseRegen();

protected:

	// GEs de atributos
	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Attributes")
	TSubclassOf<UGameplayEffect> DefaultPrimaryAttributes;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Attributes")
	TSubclassOf<UGameplayEffect> DefaultSecondaryAttributes;

	UPROPERTY(BlueprintReadOnly, EditAnywhere, Category = "Attributes")
	TSubclassOf<UGameplayEffect> DefaultVitalAttributes;

	UPROPERTY(EditAnywhere, Category = "Abilities")
	TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;

	void AddCharacterAbilities();

	// Array de montages de ataque disponibles para este personaje.
	// Cada entrada vincula un montage con el tag del socket de hit detection.
	// GA_MeleeAttack selecciona uno al azar y escucha su MontageTag.
	// Configurar en el Blueprint de cada personaje enemigo.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TArray<FTaggedMontage> AttackMontages;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	TObjectPtr<USkeletalMeshComponent> FinalWeaponMesh;

	UPROPERTY(EditAnywhere, Category = "Combat")
	FName WeaponTipSocketName;

	// Factor de escala del impulso de muerte aplicado al ARMA (clase 314). El curso
	// original descubre jugando que la malla del personaje y la del arma necesitan
	// magnitudes muy distintas para verse bien (el arma es mucho más ligera y sale
	// disparada con demasiada violencia si recibe el mismo impulso que el cuerpo) — por
	// eso este factor existe como UPROPERTY ajustable en cada Blueprint de personaje
	// en vez de un número fijo en C++: cada arma/personaje puede necesitar un valor
	// distinto, y así se ajusta jugando, sin tocar código. 1.0 = mismo impulso que el
	// cuerpo; 0.15 (el valor por defecto) es un punto de partida razonable — AJUSTA
	// ESTE VALOR EN EL EDITOR observando cómo se ve la reacción del arma al morir.
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Combat")
	float WeaponDeathImpulseScale = 0.15f;

	// Nombres de socket en el mesh del personaje para ataques sin arma.
	// GetCombatSocketLocation los resuelve según el MontageTag del ataque.
	// Estandarizados con valores por defecto — todos los enemigos comparten estos nombres,
	// así la implementación centralizada funciona sin configurar nada por enemigo.
	// Si un mesh concreto usa otro nombre, se sobrescribe en su Blueprint.
	UPROPERTY(EditAnywhere, Category = "Combat|Sockets")
	FName RightHandSocketName = FName("RightHandSocket");

	UPROPERTY(EditAnywhere, Category = "Combat|Sockets")
	FName LeftHandSocketName = FName("LeftHandSocket");

	UPROPERTY(EditAnywhere, Category = "Combat|Sockets")
	FName RightFootSocketName = FName("RightFootSocket");

	UPROPERTY(EditAnywhere, Category = "Combat|Sockets")
	FName LeftFootSocketName = FName("LeftFootSocket");

	UPROPERTY(EditAnywhere, Category = "Combat|Sockets")
	FName MouthSocketName = FName("MouthSocket");

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UAnimMontage> HitReactMontage;

	// Montage de stagger (aturdimiento cuando la postura llega a 0).
	// Más largo que HitReact. Asignar en el Blueprint del personaje.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UAnimMontage> StaggerMontage;

	// Montage de "levantarse" tras un lanzamiento aéreo (post-315, Nivel 3 de knockback).
	// Lo reproduce GA_GetUp — asignar en el Blueprint del personaje, igual que
	// HitReactMontage/StaggerMontage arriba.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat")
	TObjectPtr<UAnimMontage> GetUpMontage;

	// GANCHO (pendiente de assets): efecto de impacto (sangre/chispas/etc.) que se spawnea
	// en el punto de golpe cuando ESTE personaje es alcanzado por un ataque melee. Se obtiene
	// del personaje GOLPEADO (víctima) vía GetBloodEffect, para que cada tipo de personaje
	// pueda tener su propio efecto (sangre roja, verde, chispas de armadura, etc.). Vacío por
	// ahora; cuando se asigne, el WeaponTraceComponent lo disparará vía Gameplay Cue.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Impact")
	TObjectPtr<UNiagaraSystem> BloodEffect;

	// Sonido que se reproduce cuando este personaje muere, en su posición del mundo.
	// Cada BP de personaje (enemigo/jugador) asigna su propio sonido de muerte.
	// Se reproduce en MulticastHandleDeath, antes del dissolve.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat|Impact")
	TObjectPtr<USoundBase> DeathSound;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dissolve Effects")
	TObjectPtr<UMaterialInstance> DissolveMaterialInstance;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Dissolve Effects")
	TObjectPtr<UMaterialInstance> WeaponDissolveMaterialInstance;

	UPROPERTY()
	TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UAttributeSet> AttributeSet;

	// NOTA (Etapa 4): el handle SecondaryAttributesEffectHandle ya no vive aquí.
	// Se mudó a UPantheliaAbilitySystemComponent para que su vida coincida con la del
	// ASC que lo usa: en el jugador, el ASC vive en el PlayerState y sobrevive al
	// respawn del Pawn (si el handle viviera aquí, moriría con el Pawn y la instancia
	// Infinite de secundarios quedaría huérfana e imposible de remover). Ver la
	// explicación extendida en PantheliaAbilitySystemComponent.h.

	// Componente Niagara de la Quemadura (clase 311) — compartido por jugador y
	// enemigos al vivir en esta clase base. VisibleAnywhere (no EditDefaultsOnly): cada
	// Blueprint puede reposicionarlo y asignarle su Niagara System, pero no puede crear
	// uno nuevo — solo existe UNO, ya creado en el constructor (.cpp). El DebuffTag se
	// asigna también ahí (Debuff.Burn) — no falta configurar nada en Blueprint salvo la
	// posición y el asset visual. Ver UPantheliaDebuffNiagaraComponent para el porqué de
	// todo el mecanismo de activación/desactivación automática.
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Panthelia|Debuff")
	TObjectPtr<UPantheliaDebuffNiagaraComponent> BurnDebuffComponent;

	// Los otros 3 elementos (clase 311, añadidos a petición — mismo patrón exacto que
	// BurnDebuffComponent arriba, solo cambia el tag). Cada uno reacciona ÚNICAMENTE a
	// su propio DebuffTag — un personaje puede tener Quemadura y Electrocución activas
	// a la vez sin que se interfieran entre sí, porque cada componente escucha un tag
	// distinto de forma independiente.
	//
	// Como con Burn: cada Blueprint concreto debe asignarles su Niagara System y ajustar
	// su posición en Details → Shock/Saturation/PoisonDebuffComponent → Transform. Sin
	// un Niagara System asignado, Activate() no crashea — simplemente no se ve nada (un
	// componente Niagara sin asset activo no dibuja ni hace nada).
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Panthelia|Debuff")
	TObjectPtr<UPantheliaDebuffNiagaraComponent> ShockDebuffComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Panthelia|Debuff")
	TObjectPtr<UPantheliaDebuffNiagaraComponent> SaturationDebuffComponent;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Panthelia|Debuff")
	TObjectPtr<UPantheliaDebuffNiagaraComponent> PoisonDebuffComponent;
};