// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/PantheliaParryAbility.h"
#include "PantheliaGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "TimerManager.h"
#include "Engine/World.h"
#include "GameFramework/Character.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimInstance.h"
#include "GameplayCueManager.h"

UPantheliaParryAbility::UPantheliaParryAbility()
{
	// Instanced Per Actor: cada jugador tiene su instancia con su estado (bInputHeld, etc.).
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void UPantheliaParryAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Aplicar coste (estamina). Si no hay suficiente, en Fase 1 igual seguimos (el manejo
	// fino de "sin estamina" llega en Fase 3). CommitAbility aplica el CostGE del Blueprint.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		// Nota Fase 1: aun sin estamina permitimos entrar a la guardia para poder probar
		// el flujo. La penalizacion real (guardia rota) se implementa en Fase 3.
	}

	bInputHeld = true;

	UE_LOG(LogTemp, Warning, TEXT("[Parry] ActivateAbility. Tipo=%d Ventana=%.2fs"),
		(int32)ParryType, ParryWindow);

	// Reproducir la pose de guardia sostenida DE INMEDIATO en el slot 'GuardSlot'. Antes
	// esto se hacia al expirar la ventana de parry (0.2s despues), lo que producia un delay
	// perceptible (~200ms) antes de verse la guardia. Arrancarla ya elimina ese lag: la
	// guardia visual aparece al instante; la ventana de parry sigue siendo solo logica
	// interna (decide si el golpe se para perfecto), no afecta la animacion.
	PlayGuardLoopMontage();

	// Entrar en la ventana de parry (concede State.Parry.X y arranca el timer).
	EnterParryWindow();
}

void UPantheliaParryAbility::EnterParryWindow()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;

	// Conceder el tag de ventana de parry. El ExecCalc (Fase 2) lo leera para anular dano.
	ASC->AddLooseGameplayTag(GetParryStateTag());

	// Conceder TAMBIEN el tag de bloqueo desde el inicio. El AnimInstance usa State.Block.X
	// para activar bIsGuarding (y con ello el blend upper-body). Si solo concedieramos
	// State.Parry.X aqui, la guardia visual no apareceria hasta que la ventana expire
	// (~0.2s), produciendo un delay perceptible. Con ambos tags, el blend arranca al
	// instante y la ventana de parry sigue siendo funcional para el ExecCalc.
	ASC->AddLooseGameplayTag(GetBlockStateTag());

	UE_LOG(LogTemp, Warning, TEXT("[Parry] Ventana ABIERTA: %s (+ %s para anim)"),
		*GetParryStateTag().ToString(), *GetBlockStateTag().ToString());

	// Timer: al expirar la ventana, decidir si pasamos a bloqueo sostenido o terminamos.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().SetTimer(
			ParryWindowTimerHandle, this,
			&UPantheliaParryAbility::OnParryWindowExpired,
			ParryWindow, /*bLoop=*/false);
	}
}

void UPantheliaParryAbility::OnParryWindowExpired()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;

	// La ventana de parry termino. Quitar el tag de parry.
	ASC->RemoveLooseGameplayTag(GetParryStateTag());

	// Si el jugador SIGUE manteniendo el boton -> bloqueo imperfecto sostenido.
	// Si ya lo solto -> terminamos la guardia.
	if (bInputHeld)
	{
		// El tag State.Block.X ya se concedio en EnterParryWindow (para que la animacion
		// arrancara sin delay). Aqui solo confirmamos el paso a bloqueo sostenido; no hay
		// que volver a anadirlo. La pose de guardia ya esta sonando desde ActivateAbility.
		UE_LOG(LogTemp, Warning, TEXT("[Parry] Ventana expirada -> BLOQUEO sostenido: %s"), *GetBlockStateTag().ToString());

		// --- SEGUNDO COSTE (transicion a bloqueo) ---
		// Aplicar el GE de coste de bloqueo UNA SOLA VEZ al entrar en el sostenido.
		// Este GE es independiente del Cost Gameplay Effect Class estandar (que ya se
		// cobro al activar la ability). Representa el esfuerzo de aguantar un golpe que
		// no se pudo parry. Si no se asigno BlockTransitionCostEffectClass en el
		// Blueprint, no pasa nada (comportamiento seguro).
		if (BlockTransitionCostEffectClass)
		{
			ApplyGameplayEffectToOwner(
				CurrentSpecHandle,
				CurrentActorInfo,
				CurrentActivationInfo,
				BlockTransitionCostEffectClass->GetDefaultObject<UGameplayEffect>(),
				GetAbilityLevel());
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Parry] Ventana expirada y boton ya soltado -> fin."));
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UPantheliaParryAbility::NotifyBlockInputReleased()
{
	bInputHeld = false;

	UE_LOG(LogTemp, Warning, TEXT("[Parry] Input soltado."));

	// Si soltamos durante la ventana de parry, la dejamos correr (el timer la cerrara y,
	// como bInputHeld ya es false, terminara la ability). Si ya estabamos en bloqueo
	// sostenido, terminamos de inmediato.
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (ASC && ASC->HasMatchingGameplayTag(GetBlockStateTag()))
	{
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UPantheliaParryAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Limpieza: quitar tags de estado y cancelar el timer.
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ParryWindowTimerHandle);
	}

	// Detener el montage de guardia sostenida si estaba sonando (en el slot 'GuardSlot').
	if (GuardLoopMontage)
	{
		if (const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo())
		{
			if (UAnimInstance* AnimInstance = Info->GetAnimInstance())
			{
				// Solo lo paramos si es el que esta activo, para no cortar otro montage.
				if (AnimInstance->Montage_IsPlaying(GuardLoopMontage))
				{
					AnimInstance->Montage_Stop(0.15f, GuardLoopMontage);
				}
			}
		}
	}

	ClearParryBlockTags();
	bInputHeld = false;

	Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

void UPantheliaParryAbility::ClearParryBlockTags()
{
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;

	// RemoveLooseGameplayTag es seguro aunque el tag no este presente.
	ASC->RemoveLooseGameplayTag(GetParryStateTag());
	ASC->RemoveLooseGameplayTag(GetBlockStateTag());
}

void UPantheliaParryAbility::PlayGuardLoopMontage()
{
	if (!GuardLoopMontage)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Parry] GuardLoopMontage no asignado; no hay pose de guardia upper-body."));
		return;
	}

	// Obtener la AnimInstance del jugador para reproducir el montage directamente.
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	if (!Info) return;

	UAnimInstance* AnimInstance = Info->GetAnimInstance();
	if (!AnimInstance) return;

	// Montage_Play arranca el montage en su slot propio ('GuardSlot', definido dentro del
	// montage). El LayeredBoneBlend del AnimBlueprint lo mezcla en el tren superior.
	// El montage debe estar configurado para hacer loop (seccion que apunta a si misma).
	const float Duration = AnimInstance->Montage_Play(GuardLoopMontage, 1.0f);
	if (Duration > 0.f)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Parry] GuardLoopMontage reproduciendo en GuardSlot."));
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Parry] Montage_Play de GuardLoopMontage fallo (Duration=0)."));
	}
}

void UPantheliaParryAbility::NotifyParryImpact(bool bWasPerfectParry)
{
	// El golpe fue parado. Damos feel de impacto con un RETROCESO FISICO (knockback), sin
	// tocar ningun montage. Antes saltabamos a una seccion del GuardMontage, pero eso
	// arrancaba el montage de entrada (full body, en DefaultSlot) por encima del
	// GuardLoopMontage que sostiene la pose (en GuardSlot), lo que rompia la animacion y
	// dejaba la pose deforme. Dejando intacto el GuardLoopMontage, la pose de guardia se
	// mantiene y el knockback aporta el impacto. El retroceso de la pose superior se puede
	// anadir despues con un montage aditivo en GuardSlot si se quiere mas detalle.
	UE_LOG(LogTemp, Warning, TEXT("[Parry] NotifyParryImpact. PerfectParry=%d"), bWasPerfectParry ? 1 : 0);

	ApplyGuardKnockback(bWasPerfectParry);
	FireParryCue(bWasPerfectParry);
}

FGameplayTag UPantheliaParryAbility::GetParryStateTag() const
{
	const FPantheliaGameplayTags& Tags = FPantheliaGameplayTags::Get();
	return ParryType == EParryType::Physical ? Tags.State_Parry_Physical : Tags.State_Parry_Magic;
}

FGameplayTag UPantheliaParryAbility::GetBlockStateTag() const
{
	const FPantheliaGameplayTags& Tags = FPantheliaGameplayTags::Get();
	return ParryType == EParryType::Physical ? Tags.State_Block_Physical : Tags.State_Block_Magic;
}

void UPantheliaParryAbility::ApplyGuardKnockback(bool bWasPerfectParry)
{
	// Obtener el Character del defensor (el que paro el golpe) para empujarlo hacia atras.
	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	if (!Info) return;

	ACharacter* Character = Cast<ACharacter>(Info->AvatarActor.Get());
	if (!Character)
	{
		UE_LOG(LogTemp, Warning, TEXT("[Parry] Knockback: AvatarActor no es ACharacter; no se aplica."));
		return;
	}

	// Direccion del retroceso: opuesta a hacia donde mira el personaje (hacia atras).
	// GetActorForwardVector apunta al frente; negandolo obtenemos el "atras".
	const FVector Backward = -Character->GetActorForwardVector();

	// Fuerza segun el tipo: el parry perfecto repele mas que el bloqueo (que absorbe).
	const float Speed = bWasPerfectParry ? ParryKnockbackSpeed : BlockKnockbackSpeed;

	// LaunchCharacter aplica una velocidad instantanea respetando el sistema de movimiento
	// del Character (no atraviesa paredes ni rompe el lock-on). Lo mantenemos horizontal
	// (sin componente Z) para que sea un paso atras, no un salto. Los bXYOverride en true
	// hacen que esta velocidad reemplace la horizontal actual de forma limpia.
	const FVector LaunchVelocity = Backward * Speed;
	Character->LaunchCharacter(LaunchVelocity, /*bXYOverride=*/true, /*bZOverride=*/false);

	UE_LOG(LogTemp, Warning, TEXT("[Parry] Knockback aplicado: %.0f cm/s hacia atras (perfecto=%d)."),
		Speed, bWasPerfectParry ? 1 : 0);
}

void UPantheliaParryAbility::FireParryCue(bool bWasPerfectParry)
{
	// Determinar el tag del Cue segun tipo de parry (fisico/magico) y perfeccion.
	// EParryType::Physical + perfecto -> GameplayCue.Parry.Physical.Perfect, etc.
	const FPantheliaGameplayTags& Tags = FPantheliaGameplayTags::Get();
	const bool bIsPhysical = (ParryType == EParryType::Physical);

	FGameplayTag CueTag;
	if (bIsPhysical && bWasPerfectParry)       CueTag = Tags.GameplayCue_Parry_Physical_Perfect;
	else if (bIsPhysical && !bWasPerfectParry) CueTag = Tags.GameplayCue_Parry_Physical_Block;
	else if (!bIsPhysical && bWasPerfectParry) CueTag = Tags.GameplayCue_Parry_Magic_Perfect;
	else                                        CueTag = Tags.GameplayCue_Parry_Magic_Block;

	// Obtener el ASC y el Actor para calcular la posicion del Cue. La posicion se pone
	// ligeramente delante del personaje (~60cm) para que las particulas aparezcan en el
	// punto de contacto con el arma/escudo, no en el pivote del actor.
	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC) return;

	const FGameplayAbilityActorInfo* Info = GetCurrentActorInfo();
	if (!Info) return;

	AActor* Avatar = Info->AvatarActor.Get();
	if (!Avatar) return;

	const FVector SpawnLocation = Avatar->GetActorLocation()
		+ Avatar->GetActorForwardVector() * 60.f
		+ FVector(0.f, 0.f, 80.f); // offset en Z para quedar a la altura del torso

	// Parametros del Cue: posicion, rotacion y el actor que origina el efecto.
	FGameplayCueParameters CueParams;
	CueParams.Location = SpawnLocation;
	CueParams.Normal = Avatar->GetActorForwardVector();
	CueParams.Instigator = Avatar;
	CueParams.EffectCauser = Avatar;

	// ExecuteGameplayCue dispara el Cue una sola vez (tipo burst/instantaneo).
	// Si el asset GC_Parry_* no existe aun, la llamada es silenciosa (no da error).
	ASC->ExecuteGameplayCue(CueTag, CueParams);

	UE_LOG(LogTemp, Warning, TEXT("[Parry] FireParryCue: '%s' en %.0f,%.0f,%.0f"),
		*CueTag.ToString(), SpawnLocation.X, SpawnLocation.Y, SpawnLocation.Z);
}