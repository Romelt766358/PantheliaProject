// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/PantheliaPlayerAttackAbility.h"

#include "Combat/PantheliaEquipmentComponent.h"
#include "Combat/PantheliaWeaponDefinition.h"
#include "Combat/PantheliaWeapon.h"
#include "Combat/WeaponTraceComponent.h"
#include "Combat/LockonComponent.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Components/MeshComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"
#include "Engine/World.h"

bool UPantheliaPlayerAttackAbility::CanActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayTagContainer* SourceTags,
	const FGameplayTagContainer* TargetTags,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	// Si esta ability ya esta activa (reproduciendo el combo), NO permitimos reactivarla.
	// El input Held intenta reactivar cada frame; sin este bloqueo, la reactivacion
	// reseteaba el combo a 0. Las pulsaciones durante el combo se manejan por el buffer.
	if (IsActive())
	{
		return false;
	}

	return Super::CanActivateAbility(Handle, ActorInfo, SourceTags, TargetTags, OptionalRelevantTags);
}

void UPantheliaPlayerAttackAbility::ActivateAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Aplicar el coste de stamina (y cooldown si lo hubiera). Si no hay stamina
	// suficiente, CommitAbility falla y no atacamos (modelo Dark Souls).
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	// Estado limpio al iniciar el combo.
	bComboWindowOpen = false;
	bComboInputBuffered = false;

	// Arrancar el ataque. La base reproduce el golpe directamente; las hijas (pesado)
	// pueden sobrescribir StartComboFromActivation para decidir tap-vs-hold primero.
	StartComboFromActivation();
}

void UPantheliaPlayerAttackAbility::StartComboFromActivation()
{
	// Comportamiento base (ataque ligero): reproducir el golpe actual de inmediato.
	// ComboIndex persiste entre activaciones (Instanced Per Actor), pero como cada
	// activacion arranca un combo nuevo, normalmente empieza en 0 si el reset previo
	// lo dejo asi. El reset ocurre en CloseComboWindow o al terminar sin encadenar.
	PlayCurrentComboMontage();
}

void UPantheliaPlayerAttackAbility::EndAbility(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility,
	bool bWasCancelled)
{
	// Resetear el estado del combo al terminar (sea por completar, romper o cancelar
	// por dash). Asi la proxima activacion empieza limpia en el golpe 0.
	StopReorient();
	ComboIndex = 0;
	bComboWindowOpen = false;
	bComboInputBuffered = false;
	CurrentMontageTask = nullptr;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UPantheliaPlayerAttackAbility::TryBufferComboInput()
{
	// Llamado una vez por pulsacion real (desde el ASC via AbilityInputTagPressed).
	// Solo guardamos el input si la ventana de combo esta abierta. Buffer de 1 solo
	// (un bool no acumula): pulsar varias veces sigue siendo un unico encadenado.
	if (bComboWindowOpen)
	{
		bComboInputBuffered = true;
	}
}

void UPantheliaPlayerAttackAbility::ResetCombo()
{
	// Fallback de seguridad: resetea el estado del combo desde fuera
	// (p.ej. ABP via MainCharacter::ResetPlayerCombo). El flujo normal
	// resetea en EndAbility, pero este método existe como fallback de seguridad para casos donde el notify de ventana no se dispara
	// (p.ej. si se interrumpe el montage antes de que abra la ventana).
	ComboIndex = 0;
	bComboWindowOpen = false;
	bComboInputBuffered = false;
}

void UPantheliaPlayerAttackAbility::OpenComboWindow()
{
	bComboWindowOpen = true;
}

void UPantheliaPlayerAttackAbility::CloseComboWindow()
{
	bComboWindowOpen = false;

	// Al cerrar la ventana decidimos: si el jugador buffereo un input, encadenamos
	// el siguiente golpe; si no, dejamos que el montage termine y la ability acabe.
	if (bComboInputBuffered)
	{
		bComboInputBuffered = false;
		AdvanceAndPlayNext();
	}

	// Si no hay buffer, no hacemos nada: el montage actual seguira hasta su fin y
	// OnMontageCompleted terminara la ability. Eso resetea el combo (en EndAbility).
}

void UPantheliaPlayerAttackAbility::PlayCurrentComboMontage()
{
	UAnimMontage* Montage = GetCurrentComboMontage();
	if (!Montage)
	{
		// Sin montage valido (sin arma, array vacio, indice malo) no hay nada que hacer.
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// Reorientar el personaje hacia la direccion deseada ANTES de arrancar el montage.
	// Esto reorienta cada golpe del combo (modelo Lies of P): el golpe en curso no se
	// toca, pero el siguiente sale en la direccion actual del jugador / lock-on. Se hace
	// aqui (antes de que el montage tome control con su root motion) para no pelear con
	// la animacion. La rotacion es suave (interpolada en unos frames) via timer.
	StartReorientToDesiredDirection();

	// Preparar el dano en el WeaponTrace antes de que el montage abra su ventana de dano.
	SetupWeaponTraceForCurrentAttack();

	// Si habia una task de montage anterior (caso de encadenado), desenganchamos sus
	// callbacks y la terminamos ANTES de crear la nueva. Asi el montage viejo no nos
	// avisa su OnCompleted al ser reemplazado, y no tenemos que adivinar el orden de
	// los callbacks. Este es el mecanismo robusto (en vez de banderas de timing).
	if (CurrentMontageTask)
	{
		CurrentMontageTask->OnCompleted.RemoveAll(this);
		CurrentMontageTask->OnInterrupted.RemoveAll(this);
		CurrentMontageTask->OnCancelled.RemoveAll(this);
		CurrentMontageTask->EndTask();
		CurrentMontageTask = nullptr;
	}

	// Crear y lanzar la task de montage. Esta task mantiene la ability viva mientras
	// el montage se reproduce y nos avisa por sus delegates cuando termina.
	CurrentMontageTask = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
		this,
		NAME_None,
		Montage,
		1.f,
		NAME_None,
		/*bStopWhenAbilityEnds=*/true);

	if (!CurrentMontageTask)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	// Enganchar callbacks. OnCompleted = el montage llego a su fin natural.
	// OnInterrupted/OnCancelled = algo lo corto (p.ej. dash futuro, recibir golpe).
	// NOTA: NO conectamos OnBlendOut a la terminacion. El blend out empieza ANTES de
	// que el montage termine del todo, y conectarlo causaba una carrera donde la
	// ability terminaba (reseteando el buffer) antes de que el ComboWindowNotifyState
	// cerrara su ventana. Solo terminamos en OnCompleted (fin real) o si se interrumpe.
	CurrentMontageTask->OnCompleted.AddDynamic(this, &UPantheliaPlayerAttackAbility::OnMontageCompleted);
	CurrentMontageTask->OnInterrupted.AddDynamic(this, &UPantheliaPlayerAttackAbility::OnMontageInterruptedOrCancelled);
	CurrentMontageTask->OnCancelled.AddDynamic(this, &UPantheliaPlayerAttackAbility::OnMontageInterruptedOrCancelled);
	CurrentMontageTask->ReadyForActivation();
}

void UPantheliaPlayerAttackAbility::AdvanceAndPlayNext()
{
	const UPantheliaWeaponDefinition* WeaponDef = GetEquippedWeaponDefinition();
	if (!WeaponDef)
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
		return;
	}

	const TArray<TObjectPtr<UAnimMontage>>& Montages =
		(AttackType == EPlayerAttackType::Heavy) ? WeaponDef->HeavyAttackMontages : WeaponDef->LightAttackMontages;

	// Avanzar el indice. Si llegamos al final del combo, ciclamos a 0.
	ComboIndex = (Montages.Num() > 0) ? (ComboIndex + 1) % Montages.Num() : 0;

	// Reproducir el siguiente golpe. PlayCurrentComboMontage limpia la task anterior
	// (desengancha sus callbacks) antes de crear la nueva, asi que el montage viejo
	// no terminara la ability por error al ser reemplazado.
	PlayCurrentComboMontage();
}

void UPantheliaPlayerAttackAbility::SetupWeaponTraceForCurrentAttack(float DamageMultiplier)
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor) return;

	UWeaponTraceComponent* TraceComp = AvatarActor->FindComponentByClass<UWeaponTraceComponent>();
	UPantheliaEquipmentComponent* EquipComp = AvatarActor->FindComponentByClass<UPantheliaEquipmentComponent>();

	if (!TraceComp || !EquipComp) return;

	// Pasar al trace el mesh del arma equipada (su componente activo) para que lea
	// los sockets WeaponBase/WeaponTip de la hoja durante el sweep.
	if (APantheliaWeapon* Weapon = EquipComp->GetEquippedWeapon())
	{
		if (UMeshComponent* WeaponMesh = Weapon->GetActiveMeshComponent())
		{
			TraceComp->SetWeaponMeshComponent(WeaponMesh);
		}
	}

	// El auto lock-on por golpe solo se permite para el ataque básico (Light).
	// La opción global sigue viviendo en LockonComponent, de modo que el futuro menú
	// de opciones puede desactivarlo sin tocar esta ability ni el WeaponTrace.
	TraceComp->SetAutoLockOnFromBasicAttackHitAllowed(AttackType == EPlayerAttackType::Light);

	// Pasar el spec de dano del arma. El WeaponTraceNotifyState del montage abrira/
	// cerrara la ventana de dano y aplicara este spec a quien golpee.
	// DamageMultiplier (1.0 por defecto) lo usa el pesado cargado para mas dano.
	TraceComp->SetDamageSpec(MakeWeaponDamageSpec(DamageMultiplier));

	// Pasar el sonido de impacto del arma. El WeaponTraceComponent lo reproduce en el
	// punto de hit (solo si el ataque conecta). Lo leemos de la WeaponDefinition equipada.
	if (const UPantheliaWeaponDefinition* WeaponDef = GetEquippedWeaponDefinition())
	{
		TraceComp->SetActiveImpactSound(WeaponDef->ImpactSound);
	}
}

FGameplayEffectSpecHandle UPantheliaPlayerAttackAbility::MakeWeaponDamageSpec(float DamageMultiplier)
{
	const UPantheliaWeaponDefinition* WeaponDef = GetEquippedWeaponDefinition();
	if (!WeaponDef) return FGameplayEffectSpecHandle();

	UAbilitySystemComponent* SourceASC = GetAbilitySystemComponentFromActorInfo();
	if (!SourceASC || !DamageEffectClass) return FGameplayEffectSpecHandle();

	// Copiar los datos de dano DEL ARMA a las propiedades heredadas de la clase base
	// para reutilizar el pipeline de escalado ya probado (ApplyDamageScalingToSpec).
	// Asi melee de enemigos, proyectiles y melee del jugador comparten el mismo calculo.
	DamageTypes = WeaponDef->DamageTypes;
	AttributeScalings = WeaponDef->AttributeScalings;
	PoiseDamage = WeaponDef->PoiseDamage;

	FGameplayEffectContextHandle ContextHandle = SourceASC->MakeEffectContext();
	ContextHandle.AddSourceObject(GetAvatarActorFromActorInfo());

	FGameplayEffectSpecHandle SpecHandle = SourceASC->MakeOutgoingSpec(
		DamageEffectClass,
		GetAbilityLevel(),
		ContextHandle);

	if (!SpecHandle.IsValid()) return SpecHandle;

	// Escalado completo: dano base + escalado por atributos + poise.
	// DamageMultiplier (1.0 por defecto) escala el dano final; el pesado cargado lo usa >1.
	ApplyDamageScalingToSpec(SpecHandle, SourceASC, DamageMultiplier);
	return SpecHandle;
}

UAnimMontage* UPantheliaPlayerAttackAbility::GetCurrentComboMontage() const
{
	const UPantheliaWeaponDefinition* WeaponDef = GetEquippedWeaponDefinition();
	if (!WeaponDef) return nullptr;

	const TArray<TObjectPtr<UAnimMontage>>& Montages =
		(AttackType == EPlayerAttackType::Heavy) ? WeaponDef->HeavyAttackMontages : WeaponDef->LightAttackMontages;

	if (Montages.Num() == 0) return nullptr;

	// Seguridad: indice fuera de rango (p.ej. cambio de arma con combo mas corto).
	const int32 SafeIndex = Montages.IsValidIndex(ComboIndex) ? ComboIndex : 0;
	return Montages[SafeIndex];
}

UPantheliaWeaponDefinition* UPantheliaPlayerAttackAbility::GetEquippedWeaponDefinition() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor) return nullptr;

	UPantheliaEquipmentComponent* EquipComp = AvatarActor->FindComponentByClass<UPantheliaEquipmentComponent>();
	if (!EquipComp) return nullptr;

	return EquipComp->GetEquippedWeaponDefinition();
}

void UPantheliaPlayerAttackAbility::OnMontageCompleted()
{
	// El montage actual llego a su fin sin que se encadenara (si se hubiera encadenado,
	// PlayCurrentComboMontage habria desenganchado este callback antes de reemplazar la
	// task). Por tanto, llegar aqui significa que el combo no continuo: terminamos.
	// EndAbility resetea el indice del combo y limpia el buffer.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
}

void UPantheliaPlayerAttackAbility::OnMontageInterruptedOrCancelled()
{
	// Algo corto el montage (recibir golpe, o el dash futuro al cancelar la ability).
	// Terminamos la ability; EndAbility resetea el combo y limpia el buffer.
	EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
}

FRotator UPantheliaPlayerAttackAbility::GetDesiredAttackRotation() const
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor) return FRotator::ZeroRotator;

	const FVector ActorLocation = AvatarActor->GetActorLocation();

	// 1) Con lock-on: mirar hacia el objetivo actual. Sin lock-on duro, intentar
	// soft-lock melee: elegir un enemigo cercano/frontal y orientar el golpe hacia él
	// SIN fijar CurrentTargetActor ni mostrar widget. Si el golpe conecta, el auto-lock
	// por hit que ya existe podrá fijarlo después.
	if (ULockonComponent* LockonComp = AvatarActor->FindComponentByClass<ULockonComponent>())
	{
		AActor* AttackTarget = nullptr;
		if (IsValid(LockonComp->CurrentTargetActor))
		{
			AttackTarget = LockonComp->CurrentTargetActor.Get();
		}
		else
		{
			AttackTarget = LockonComp->FindBestSoftLockTarget();
		}

		if (IsValid(AttackTarget))
		{
			FVector ToTarget = LockonComp->GetLockonLocation(AttackTarget) - ActorLocation;
			ToTarget.Z = 0.f; // Solo yaw, no inclinar el personaje.

			if (!ToTarget.IsNearlyZero())
			{
				return ToTarget.Rotation();
			}
		}
	}

	// 2) Sin lock-on ni soft-lock: mirar hacia la direccion del input de movimiento del jugador.
	// GetLastMovementInputVector devuelve la direccion del ultimo AddMovementInput.
	if (const UCharacterMovementComponent* MoveComp = AvatarActor->FindComponentByClass<UCharacterMovementComponent>())
	{
		FVector InputDir = MoveComp->GetLastInputVector();
		InputDir.Z = 0.f;

		if (!InputDir.IsNearlyZero())
		{
			return InputDir.Rotation();
		}
	}

	// 3) Sin lock-on y sin input: mantener la orientacion actual (no girar).
	return AvatarActor->GetActorRotation();
}

void UPantheliaPlayerAttackAbility::StartReorientToDesiredDirection()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor) return;

	// Guardar la rotacion objetivo (solo yaw) hacia donde apunta el jugador/lock-on.
	ReorientTargetRotation = GetDesiredAttackRotation();

	// Arrancar una interpolacion corta por timer. Tickea a ~60fps durante un lapso
	// breve para girar suavemente sin pelear con el root motion del montage (que toma
	// el control de la posicion una vez avanza la animacion). Es rapida (Lies of P).
	if (UWorld* World = GetWorld())
	{
		ReorientElapsed = 0.f;
		World->GetTimerManager().SetTimer(
			ReorientTimerHandle,
			this,
			&UPantheliaPlayerAttackAbility::TickReorient,
			ReorientTickInterval,
			/*bLoop=*/true);
	}
}

void UPantheliaPlayerAttackAbility::TickReorient()
{
	AActor* AvatarActor = GetAvatarActorFromActorInfo();
	if (!AvatarActor)
	{
		StopReorient();
		return;
	}

	ReorientElapsed += ReorientTickInterval;

	// Interpolacion suave del yaw actual hacia el objetivo. InterpSpeed alto = giro rapido.
	const FRotator Current = AvatarActor->GetActorRotation();
	const FRotator NewRot = FMath::RInterpTo(Current, ReorientTargetRotation, ReorientTickInterval, ReorientInterpSpeed);
	AvatarActor->SetActorRotation(FRotator(0.f, NewRot.Yaw, 0.f));

	// Terminar cuando llegamos (o casi) al objetivo, o cuando se agota el tiempo maximo.
	const bool bReached = FMath::Abs(FRotator::NormalizeAxis(NewRot.Yaw - ReorientTargetRotation.Yaw)) < 1.0f;
	if (bReached || ReorientElapsed >= ReorientMaxDuration)
	{
		StopReorient();
	}
}

void UPantheliaPlayerAttackAbility::StopReorient()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ReorientTimerHandle);
	}
}
