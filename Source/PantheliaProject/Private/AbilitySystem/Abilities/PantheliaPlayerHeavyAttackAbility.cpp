// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/PantheliaPlayerHeavyAttackAbility.h"
#include "Combat/PantheliaWeaponDefinition.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AbilitySystemComponent.h"
#include "Animation/AnimInstance.h"
#include "TimerManager.h"
#include "Engine/World.h"

UPantheliaPlayerHeavyAttackAbility::UPantheliaPlayerHeavyAttackAbility()
{
	// Esta ability usa la cadena de montages "Heavy" del arma para el especial.
	AttackType = EPlayerAttackType::Heavy;
}

bool UPantheliaPlayerHeavyAttackAbility::HasValidActivationMontage() const
{
	// El heavy post-dodge es una entrada ya resuelta y no puede convertirse en
	// cargado; debe tener montage dedicado o fallback normal válido.
	if (IsDodgeFollowupEntry())
	{
		return Super::HasValidActivationMontage();
	}

	const UPantheliaWeaponDefinition* WeaponDef = GetEquippedWeaponDefinition();
	return Super::HasValidActivationMontage() &&
		WeaponDef && IsValid(WeaponDef->ChargedHeavyMontage.Get());
}

void UPantheliaPlayerHeavyAttackAbility::StartComboFromActivation()
{
	// Un heavy que viene del dodge es una entrada especial ya decidida: reproduce el
	// montage post-dodge de inmediato y NO entra en tap-vs-hold ni en la carga. El
	// contexto fue consumido por la clase base antes de llegar aquí.
	if (IsDodgeFollowupEntry())
	{
		bDecisionMade = true;
		bInputHeld = false;
		bIsCharging = false;
		ExecuteSpecialAttack();
		return;
	}

	// NO atacamos de inmediato. Detectamos tap vs hold: si el jugador suelta antes del
	// umbral -> tap (especial); si mantiene -> hold (cargado). El release lo notifica el
	// ASC vía NotifyHeavyInputReleased (no usamos WaitInputRelease porque el sistema de
	// input custom del proyecto no alimenta el estado interno de input que esa task
	// espera, y nunca detectaba el release).
	bDecisionMade = false;
	bInputHeld = true;

	// Temporizador del umbral. Si se cumple sin soltar -> hold (pesado cargado).
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			HoldTimerHandle, this,
			&UPantheliaPlayerHeavyAttackAbility::OnHoldThresholdReached,
			HoldThreshold, /*bLoop=*/false);
	}
}

void UPantheliaPlayerHeavyAttackAbility::NotifyHeavyInputReleased()
{
	// Llamado por el ASC cuando se suelta el boton de ataque pesado.
	bInputHeld = false;

	// CASO 1: ya estamos cargando (hold confirmado, en el loop de carga sostenida).
	// Soltar AHORA significa lanzar el golpe cargado (saltar a la seccion Release).
	if (bIsCharging)
	{
		ReleaseCharge();
		return;
	}

	// CASO 2: aun decidiendo tap-vs-hold. Si ya decidimos, ignoramos.
	if (bDecisionMade) return;
	bDecisionMade = true;

	// Cancelar el temporizador del umbral (solto antes de cumplirse).
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HoldTimerHandle);
	}

	// Solto antes del umbral -> TAP -> ataque especial encadenable.
	ExecuteSpecialAttack();
}

void UPantheliaPlayerHeavyAttackAbility::OnHoldThresholdReached()
{
	// Se cumplio el umbral. Si el jugador ya solto (o ya decidimos), ignoramos.
	if (bDecisionMade || !bInputHeld) return;
	bDecisionMade = true;

	// Mantuvo mas del umbral -> HOLD -> pesado cargado.
	ExecuteChargedAttack();
}

void UPantheliaPlayerHeavyAttackAbility::ExecuteSpecialAttack()
{
	// El ataque especial reutiliza el motor de combo de la clase base: reproduce el
	// golpe actual de la cadena Heavy (AttackType=Heavy ya esta seteado en el ctor).
	// A partir de aqui, el encadenado (tap-tap) lo maneja el sistema de buffer/ventana
	// heredado, igual que el combo ligero.
	Super::StartComboFromActivation();
}

void UPantheliaPlayerHeavyAttackAbility::ExecuteChargedAttack()
{
	const UPantheliaWeaponDefinition* WeaponDef = GetEquippedWeaponDefinition();
	if (!WeaponDef || !WeaponDef->ChargedHeavyMontage)
	{
		// Sin montage de cargado, no hay nada que reproducir: terminamos.
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	bIsCharging = true;

	// Preparar el daño del cargado con su perfil ofensivo independiente. Así el
	// multiplicador de postura no queda forzado a ser igual al multiplicador de HP.
	SetupWeaponTraceForCurrentAttack(WeaponDef->ChargedHeavyAttackModifiers);

	// Reproducir el montage del cargado. El montage tiene 3 secciones:
	//   "Start"   (AS1: ponerse en posicion) -> fluye a "Loop"
	//   "Loop"    (AS2: mantener la pose)     -> se repite a si misma (NextSection=Loop)
	//   "Release" (AS3: el golpe)             -> al soltar saltamos aqui
	// Mientras el loop se sostiene, rotamos hacia el lock-on/input (carga sostenible).
	UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this, NAME_None, WeaponDef->ChargedHeavyMontage, 1.f, ChargeStartSectionName, /*bStopWhenAbilityEnds=*/true);

	if (!Task)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	ChargedMontageTask = Task;

	// OnCompleted/Interrupted/Cancelled: al terminar (tras el golpe Release o si se corta),
	// acabamos la ability. El cargado no encadena.
	Task->OnCompleted.AddDynamic(this, &UPantheliaPlayerHeavyAttackAbility::OnChargedMontageEnded);
	Task->OnInterrupted.AddDynamic(this, &UPantheliaPlayerHeavyAttackAbility::OnChargedMontageEnded);
	Task->OnCancelled.AddDynamic(this, &UPantheliaPlayerHeavyAttackAbility::OnChargedMontageEnded);
	Task->ReadyForActivation();

	// Arrancar la rotacion continua hacia el lock-on/input durante la carga. Un timer
	// periodico re-apunta hacia la direccion deseada mientras bIsCharging siga activo.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ChargeRotateTimerHandle, this,
			&UPantheliaPlayerHeavyAttackAbility::TickChargeRotation,
			ChargeRotateInterval, /*bLoop=*/true);
	}
}

void UPantheliaPlayerHeavyAttackAbility::TickChargeRotation()
{
	// Mientras cargamos, reorientar continuamente hacia el lock-on / direccion de input.
	// Reutiliza el sistema de reorientacion de la clase base (mismo giro suave).
	if (!bIsCharging)
	{
		if (UWorld* World = GetWorld())
		{
			World->GetTimerManager().ClearTimer(ChargeRotateTimerHandle);
		}
		return;
	}
	StartReorientToDesiredDirection();
}

void UPantheliaPlayerHeavyAttackAbility::ReleaseCharge()
{
	// El jugador solto el boton durante la carga: lanzar el golpe (seccion "Release").
	if (!bIsCharging) return;
	bIsCharging = false;

	// Detener la rotacion de carga.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ChargeRotateTimerHandle);
	}

	// Saltar a la seccion del golpe en el montage en curso.
	if (UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo())
	{
		if (UAnimInstance* AnimInstance = ASC->AbilityActorInfo->GetAnimInstance())
		{
			const UPantheliaWeaponDefinition* WeaponDef = GetEquippedWeaponDefinition();
			if (WeaponDef && WeaponDef->ChargedHeavyMontage &&
				AnimInstance->Montage_IsPlaying(WeaponDef->ChargedHeavyMontage))
			{
				AnimInstance->Montage_JumpToSection(ChargeReleaseSectionName, WeaponDef->ChargedHeavyMontage);
			}
		}
	}
}

void UPantheliaPlayerHeavyAttackAbility::OnChargedMontageEnded()
{
	// El pesado cargado es un golpe unico: al terminar el montage, acaba la ability.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UPantheliaPlayerHeavyAttackAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Limpieza propia del tap-vs-hold y de la carga sostenida.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(HoldTimerHandle);
		World->GetTimerManager().ClearTimer(ChargeRotateTimerHandle);
	}
	bDecisionMade = false;
	bInputHeld = false;
	bIsCharging = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}
