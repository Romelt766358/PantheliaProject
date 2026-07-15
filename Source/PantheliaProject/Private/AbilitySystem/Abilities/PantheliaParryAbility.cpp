// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/PantheliaParryAbility.h"
#include "PantheliaGameplayTags.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/PantheliaAbilitySystemComponent.h"
#include "AbilitySystem/PantheliaAttributeSet.h"
#include "GameplayEffect.h"
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

	// GA_Parry_Physical / GA_Parry_Magic pueden migrar su coste de intento desde
	// Class Defaults sin tener que seleccionar de nuevo el tipo de recurso.
	ResourceCostType = EPantheliaResourceCostType::Stamina;
}

void UPantheliaParryAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	const FGameplayEventData* TriggerEventData)
{
	Super::ActivateAbility(Handle, ActorInfo, ActivationInfo, TriggerEventData);

	// Aplicar el coste de entrada. La regla definitiva exige pagar antes de abrir la
	// ventana: si CommitAbility falla, no se conceden tags, no se reproduce la guardia
	// y la ability termina inmediatamente.
	if (!CommitAbility(Handle, ActorInfo, ActivationInfo))
	{
		RuntimeState = EPantheliaParryRuntimeState::Ending;
		EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
		return;
	}

	bInputHeld = true;
	RuntimeState = EPantheliaParryRuntimeState::PerfectWindow;

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
	// Un callback tardío no debe reabrir ni terminar dos veces una ability que ya salió.
	if (RuntimeState != EPantheliaParryRuntimeState::PerfectWindow)
	{
		return;
	}

	UAbilitySystemComponent* ASC = GetAbilitySystemComponentFromActorInfo();
	if (!ASC)
	{
		RuntimeState = EPantheliaParryRuntimeState::Ending;
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// La ventana de parry termino. Quitar el tag de parry.
	ASC->RemoveLooseGameplayTag(GetParryStateTag());

	// Si el jugador SIGUE manteniendo el boton -> bloqueo imperfecto sostenido.
	// Si ya lo solto -> terminamos la guardia.
	if (bInputHeld)
	{
		RuntimeState = EPantheliaParryRuntimeState::SustainedGuard;
		// El tag State.Block.X ya se concedio en EnterParryWindow (para que la animacion
		// arrancara sin delay). Aqui solo confirmamos el paso a bloqueo sostenido; no hay
		// que volver a anadirlo. La pose de guardia ya esta sonando desde ActivateAbility.
		UE_LOG(LogTemp, Warning, TEXT("[Parry] Ventana expirada -> BLOQUEO sostenido: %s"), *GetBlockStateTag().ToString());

		// --- SEGUNDO COSTE (transicion a bloqueo) ---
		// Se valida antes de aplicarlo. Si no puede pagarse, la transición provoca
		// guardia rota inmediata y reutiliza el pipeline/animación de Stagger.
		if (!TryPayBlockTransitionCost())
		{
			return;
		}
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("[Parry] Ventana expirada y boton ya soltado -> fin."));
		RuntimeState = EPantheliaParryRuntimeState::Ending;
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UPantheliaParryAbility::NotifyBlockInputReleased()
{
	bInputHeld = false;

	UE_LOG(LogTemp, Warning, TEXT("[Parry] Input soltado. Estado=%d"), static_cast<int32>(RuntimeState));

	// Soltar durante PerfectWindow NO cancela el parry: la ventana conserva su duración
	// completa y OnParryWindowExpired terminará la ability sin entrar en guardia sostenida.
	if (RuntimeState == EPantheliaParryRuntimeState::PerfectWindow)
	{
		return;
	}

	// Una guardia ya sostenida sí termina inmediatamente al soltar el input.
	if (RuntimeState == EPantheliaParryRuntimeState::SustainedGuard)
	{
		RuntimeState = EPantheliaParryRuntimeState::Ending;
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, false);
	}
}

void UPantheliaParryAbility::EndAbility(const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo,
	bool bReplicateEndAbility, bool bWasCancelled)
{
	// Evitar que callbacks tardíos interpreten el tag visual como una fase todavía activa.
	RuntimeState = EPantheliaParryRuntimeState::Ending;

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
	RuntimeState = EPantheliaParryRuntimeState::Inactive;

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

void UPantheliaParryAbility::NotifyParryImpact(bool bWasPerfectParry, bool bGuardBroken)
{
	// El golpe fue defendido sin interrumpir el GuardLoopMontage. Antes se saltaba a una
	// sección del montage de entrada (full body, DefaultSlot), lo que rompía la pose upper-body
	// del GuardSlot. El bloqueo imperfecto conserva un retroceso físico; el parry perfecto
	// anula la consecuencia ofensiva y usa únicamente su cue/reacción positiva.
	UE_LOG(LogTemp, Warning,
		TEXT("[Parry] NotifyParryImpact. PerfectParry=%d GuardBroken=%d"),
		bWasPerfectParry ? 1 : 0,
		bGuardBroken ? 1 : 0);

	// El golpe que rompe la guardia conserva el cue de bloqueo, pero no aplica el
	// retroceso ordinario: la reacción completa la gobierna Stagger.
	if (bGuardBroken)
	{
		FireParryCue(false);
		RuntimeState = EPantheliaParryRuntimeState::Ending;
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return;
	}

	// Un parry perfecto anula por completo el golpe y no desplaza al jugador. El
	// bloqueo imperfecto sí conserva el retroceso defensivo de la guardia.
	if (!bWasPerfectParry)
	{
		ApplyGuardKnockback();
	}

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

bool UPantheliaParryAbility::TryPayLegacyBlockTransitionCost(
	UPantheliaAbilitySystemComponent* AbilitySystemComponent)
{
	if (!BlockTransitionCostEffectClass)
	{
		return true;
	}

	FGameplayEffectContextHandle CostContext = AbilitySystemComponent->MakeEffectContext();
	CostContext.AddSourceObject(this);

	FGameplayEffectSpecHandle CostSpec = AbilitySystemComponent->MakeOutgoingSpec(
		BlockTransitionCostEffectClass,
		GetAbilityLevel(),
		CostContext);

	if (!CostSpec.IsValid() || !CostSpec.Data.IsValid())
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Parry] No se pudo construir el GE de coste de transición; guardia rota."));
		RuntimeState = EPantheliaParryRuntimeState::Ending;
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		AbilitySystemComponent->TriggerGuardBreak();
		return false;
	}

	CostSpec.Data->CalculateModifierMagnitudes();

	float RequiredStamina = 0.f;
	const FGameplayAttribute StaminaAttribute =
		UPantheliaAttributeSet::GetStaminaAttribute();
	const UGameplayEffect* CostDefinition = CostSpec.Data->Def;

	if (CostDefinition)
	{
		for (int32 ModifierIndex = 0;
			ModifierIndex < CostDefinition->Modifiers.Num() &&
			ModifierIndex < CostSpec.Data->Modifiers.Num();
			++ModifierIndex)
		{
			const FGameplayModifierInfo& ModifierInfo =
				CostDefinition->Modifiers[ModifierIndex];
			if (ModifierInfo.Attribute != StaminaAttribute ||
				ModifierInfo.ModifierOp != EGameplayModOp::Additive)
			{
				continue;
			}

			const float ModifierMagnitude =
				CostSpec.Data->GetModifierMagnitude(ModifierIndex);
			if (ModifierMagnitude < 0.f)
			{
				RequiredStamina += -ModifierMagnitude;
			}
		}
	}

	// Si el GE no contiene un modificador negativo de Stamina, se aplica normalmente.
	// Esto conserva compatibilidad con efectos de transición que solo otorguen tags.
	if (RequiredStamina <= 0.f)
	{
		AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*CostSpec.Data.Get());
		return true;
	}

	const float CurrentStamina =
		AbilitySystemComponent->GetNumericAttribute(StaminaAttribute);
	if (CurrentStamina + KINDA_SMALL_NUMBER < RequiredStamina)
	{
		if (CurrentStamina > 0.f)
		{
			AbilitySystemComponent->ApplyModToAttribute(
				StaminaAttribute,
				EGameplayModOp::Additive,
				-CurrentStamina);
		}

		UE_LOG(LogTemp, Warning,
			TEXT("[Parry] Transición no pagable: actual=%.1f requerida=%.1f -> GUARD BREAK."),
			CurrentStamina,
			RequiredStamina);

		RuntimeState = EPantheliaParryRuntimeState::Ending;
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		AbilitySystemComponent->TriggerGuardBreak();
		return false;
	}

	// Aplicamos el spec construido en vez de llamar ApplyGameplayEffectToOwner para que
	// la misma magnitud evaluada sea la que se cobre. Igualdad exacta está permitida:
	// puede dejar Stamina en 0 sin romper la guardia hasta el siguiente impacto.
	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*CostSpec.Data.Get());
	return true;
}

bool UPantheliaParryAbility::TryPayBlockTransitionCost()
{
	UPantheliaAbilitySystemComponent* ASC =
		Cast<UPantheliaAbilitySystemComponent>(GetAbilitySystemComponentFromActorInfo());
	if (!ASC)
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Parry] No hay PantheliaASC para validar el coste de transición."));
		RuntimeState = EPantheliaParryRuntimeState::Ending;
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		return false;
	}

	// Hasta que cada Blueprint copie su coste anterior a la propiedad base, la ruta
	// legacy permanece intacta. Esto evita una regresión silenciosa durante la migración.
	if (!bUsePantheliaBlockTransitionCost)
	{
		return TryPayLegacyBlockTransitionCost(ASC);
	}

	const float EvaluatedBaseRequiredStamina =
		BlockTransitionBaseStaminaCost.GetValueAtLevel(GetAbilityLevel());

	if (!FMath::IsFinite(EvaluatedBaseRequiredStamina))
	{
		UE_LOG(LogTemp, Error,
			TEXT("[Parry] BlockTransitionBaseStaminaCost no es finito -> GUARD BREAK."));
		RuntimeState = EPantheliaParryRuntimeState::Ending;
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		ASC->TriggerGuardBreak();
		return false;
	}

	const float BaseRequiredStamina =
		FMath::Max(0.f, EvaluatedBaseRequiredStamina);

	// Un cero explícito es una transición gratuita válida. No aplicamos un GE de
	// coste vacío para que el asset no tenga efectos auxiliares ocultos.
	if (BaseRequiredStamina <= 0.f)
	{
		return true;
	}

	const float RequiredStamina = UPantheliaGameplayAbility::ResolveResourceCost(
		ASC,
		EPantheliaResourceCostType::Stamina,
		BaseRequiredStamina);

	if (RequiredStamina <= 0.f)
	{
		return true;
	}

	if (!BlockTransitionCostEffectClass)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[Parry] Coste de transición %.1f sin BlockTransitionCostEffectClass -> GUARD BREAK."),
			RequiredStamina);
		RuntimeState = EPantheliaParryRuntimeState::Ending;
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		ASC->TriggerGuardBreak();
		return false;
	}

	const UGameplayEffect* CostDefinition =
		BlockTransitionCostEffectClass.GetDefaultObject();
	if (!CostDefinition ||
		CostDefinition->DurationPolicy != EGameplayEffectDurationType::Instant)
	{
		UE_LOG(LogTemp, Error,
			TEXT("[Parry] BlockTransitionCostEffectClass debe ser un GE Instant -> GUARD BREAK."));
		RuntimeState = EPantheliaParryRuntimeState::Ending;
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		ASC->TriggerGuardBreak();
		return false;
	}

	const FGameplayAttribute StaminaAttribute =
		UPantheliaAttributeSet::GetStaminaAttribute();
	const float CurrentStamina =
		ASC->GetNumericAttribute(StaminaAttribute);

	if (CurrentStamina + KINDA_SMALL_NUMBER < RequiredStamina)
	{
		if (CurrentStamina > 0.f)
		{
			ASC->ApplyModToAttribute(
				StaminaAttribute,
				EGameplayModOp::Additive,
				-CurrentStamina);
		}

		UE_LOG(LogTemp, Warning,
			TEXT("[Parry] Transición no pagable: actual=%.1f requerida=%.1f -> GUARD BREAK."),
			CurrentStamina,
			RequiredStamina);

		RuntimeState = EPantheliaParryRuntimeState::Ending;
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		ASC->TriggerGuardBreak();
		return false;
	}

	FGameplayEffectContextHandle CostContext = ASC->MakeEffectContext();
	CostContext.AddSourceObject(this);

	FGameplayEffectSpecHandle CostSpec = ASC->MakeOutgoingSpec(
		BlockTransitionCostEffectClass,
		GetAbilityLevel(),
		CostContext);

	if (!UPantheliaGameplayAbility::ApplyResolvedResourceCostSpec(
		ASC,
		EPantheliaResourceCostType::Stamina,
		RequiredStamina,
		CostSpec))
	{
		UE_LOG(LogTemp, Warning,
			TEXT("[Parry] No se pudo aplicar el GE de coste de transición; guardia rota."));
		RuntimeState = EPantheliaParryRuntimeState::Ending;
		EndAbility(CurrentSpecHandle, CurrentActorInfo, CurrentActivationInfo, true, true);
		ASC->TriggerGuardBreak();
		return false;
	}

	// Igualdad exacta está permitida: puede dejar Stamina en 0 sin romper la guardia
	// hasta el siguiente impacto no pagable.
	return true;
}

void UPantheliaParryAbility::ApplyGuardKnockback()
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

	const float Speed = BlockKnockbackSpeed;

	// LaunchCharacter aplica una velocidad instantanea respetando el sistema de movimiento
	// del Character (no atraviesa paredes ni rompe el lock-on). Lo mantenemos horizontal
	// (sin componente Z) para que sea un paso atras, no un salto. Los bXYOverride en true
	// hacen que esta velocidad reemplace la horizontal actual de forma limpia.
	const FVector LaunchVelocity = Backward * Speed;
	Character->LaunchCharacter(LaunchVelocity, /*bXYOverride=*/true, /*bZOverride=*/false);

	UE_LOG(LogTemp, Warning, TEXT("[Parry] Knockback de bloqueo aplicado: %.0f cm/s hacia atras."),
		Speed);
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
