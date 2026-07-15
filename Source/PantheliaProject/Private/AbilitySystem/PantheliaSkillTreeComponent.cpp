// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/PantheliaSkillTreeComponent.h"
#include "AbilitySystem/Data/PantheliaSkillTreeInfo.h"
#include "AbilitySystem/PantheliaAbilitySystemComponent.h"
#include "Player/PantheliaPlayerState.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayEffect.h"
#include "PantheliaGameplayTags.h"
#include "PantheliaLogChannels.h"

UPantheliaSkillTreeComponent::UPantheliaSkillTreeComponent()
{
	// Este componente es puro estado + API bajo demanda: no necesita Tick.
	// Desactivarlo explícitamente es gratis y evita un tick vacío por frame.
	PrimaryComponentTick.bCanEverTick = false;
}

APantheliaPlayerState* UPantheliaSkillTreeComponent::GetPantheliaPlayerState() const
{
	// El owner de este componente ES el PlayerState (se crea en su constructor).
	// Cast normal (no CastChecked) porque las funciones públicas de este componente
	// son llamables desde Blueprint: si alguien añadiera el componente a otro actor
	// por error, preferimos devolver nullptr y loguear a crashear el editor.
	return Cast<APantheliaPlayerState>(GetOwner());
}

UPantheliaAbilitySystemComponent* UPantheliaSkillTreeComponent::GetPantheliaASC() const
{
	if (const APantheliaPlayerState* PS = GetPantheliaPlayerState())
	{
		return Cast<UPantheliaAbilitySystemComponent>(PS->GetAbilitySystemComponent());
	}
	return nullptr;
}

int32 UPantheliaSkillTreeComponent::GetNodeRank(const FGameplayTag& NodeTag) const
{
	// La ausencia del tag en el mapa equivale a rango 0 ("no desbloqueado") —
	// misma convención que GetAbilityLevelFromTag en el ASC y que EnemyKillCounts
	// en el PlayerState: no guardamos entradas para lo que no ha pasado.
	const int32* FoundRank = NodeRanks.Find(NodeTag);
	return FoundRank ? *FoundRank : 0;
}

bool UPantheliaSkillTreeComponent::CanUnlockNode(const FGameplayTag& NodeTag) const
{
	// Sin Data Asset asignado no hay árbol que consultar. Warning (no Error) porque
	// durante el desarrollo es normal que aún no exista DA_SkillTree.
	if (!SkillTreeInfo)
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("[SkillTree] SkillTreeInfo sin asignar en el SkillTreeComponent. "
			     "Asígnalo en Class Defaults de BP_PantheliaPlayerState."));
		return false;
	}

	// bLogNotFound = false a propósito: esta función es de consulta y la UI puede
	// llamarla constantemente (para pintar botones); si el tag no existe, devolver
	// false en silencio es la respuesta correcta. TryUnlockNode sí loguea.
	const FPantheliaSkillNodeInfo* NodeInfo = SkillTreeInfo->FindNodeInfoForTag(NodeTag, false);
	if (!NodeInfo) return false;

	// Un coste negativo o un rango máximo inválido son errores de datos, no nodos
	// gratuitos. Fallamos cerrado para que la UI no ofrezca una compra corrupta.
	if (!NodeInfo->NodeTag.IsValid() || NodeInfo->MaxRank <= 0 ||
		NodeInfo->CostPerRank < 0 || NodeInfo->LevelRequirement < 1)
	{
		return false;
	}

	const APantheliaPlayerState* PS = GetPantheliaPlayerState();
	if (!PS) return false;

	// 1. ¿Ya está al rango máximo? No hay nada más que comprar.
	if (GetNodeRank(NodeTag) >= NodeInfo->MaxRank) return false;

	// 2. Requisito de nivel de personaje.
	if (PS->GetPlayerLevel() < NodeInfo->LevelRequirement) return false;

	// 3. ¿Alcanzan los puntos de habilidad para UN rango más?
	if (PS->GetSkillPoints() < NodeInfo->CostPerRank) return false;

	// 4. Prerequisitos: TODOS los nodos requeridos deben tener rango >= 1.
	// GetGameplayTagArray nos da los tags del container uno a uno para consultarlos
	// contra nuestro mapa de rangos.
	TArray<FGameplayTag> Prerequisites;
	NodeInfo->PrerequisiteNodeTags.GetGameplayTagArray(Prerequisites);
	for (const FGameplayTag& PrereqTag : Prerequisites)
	{
		if (GetNodeRank(PrereqTag) < 1) return false;
	}

	return true;
}

bool UPantheliaSkillTreeComponent::TryUnlockNode(const FGameplayTag& NodeTag)
{
	// Toda la validación de reglas vive en CanUnlockNode para que UI y runtime
	// compartan exactamente el mismo contrato. La aplicación GAS hace además su
	// propio preflight de datos antes de confirmar la compra.
	if (!CanUnlockNode(NodeTag))
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("[SkillTree] TryUnlockNode rechazado para '%s' (rango máximo, nivel, "
			     "puntos o prerequisitos). Consulta CanUnlockNode antes desde la UI."),
			*NodeTag.ToString());
		return false;
	}

	const FPantheliaSkillNodeInfo* NodeInfo = SkillTreeInfo->FindNodeInfoForTag(NodeTag, true);
	APantheliaPlayerState* PS = GetPantheliaPlayerState();
	if (!NodeInfo || !PS)
	{
		return false;
	}

	const int32 SafeCost = FMath::Max(0, NodeInfo->CostPerRank);
	const int32 NewRank = GetNodeRank(NodeTag) + 1;

	// Reservamos el saldo de forma atómica. Si la aplicación de GAS falla, el mismo
	// flujo lo reembolsa y ningún rango se publica.
	if (!PS->TrySpendSkillPoints(SafeCost))
	{
		return false;
	}

	if (!ApplyNodeToGAS(*NodeInfo, NewRank))
	{
		PS->GrantSkillPoints(SafeCost);
		UE_LOG(LogPanthelia, Error,
			TEXT("[SkillTree] Compra revertida para '%s': GAS no pudo aplicar el rango %d."),
			*NodeTag.ToString(), NewRank);
		return false;
	}

	// GAS ya confirmó el nuevo estado. Solo ahora publicamos la verdad persistente.
	NodeRanks.Add(NodeTag, NewRank);
	OnSkillNodeChangedDelegate.Broadcast(NodeTag, NewRank);

	UE_LOG(LogPanthelia, Log, TEXT("[SkillTree] Nodo '%s' comprado: rango %d/%d."),
		*NodeTag.ToString(), NewRank, NodeInfo->MaxRank);
	return true;
}

void UPantheliaSkillTreeComponent::ReapplyAllNodes()
{
	// Reconstrucción completa del estado de GAS desde NodeRanks + Data Asset.
	// Uso previsto: justo después de que el SaveGame cargue NodeRanks desde disco.
	if (!SkillTreeInfo)
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("[SkillTree] ReapplyAllNodes sin SkillTreeInfo asignado: nada que reaplicar."));
		return;
	}

	for (const TPair<FGameplayTag, int32>& Pair : NodeRanks)
	{
		const FPantheliaSkillNodeInfo* NodeInfo = SkillTreeInfo->FindNodeInfoForTag(Pair.Key, false);
		if (!NodeInfo)
		{
			// Caso real que hay que tolerar: un save viejo tiene un nodo que ya no
			// existe en el Data Asset (rediseño del árbol entre versiones del juego).
			// Saltarlo con warning degrada con gracia; crashear castigaría al jugador.
			UE_LOG(LogPanthelia, Warning,
				TEXT("[SkillTree] ReapplyAllNodes: el nodo guardado '%s' ya no existe "
				     "en el Data Asset. Se omite (¿árbol rediseñado entre versiones?)."),
				*Pair.Key.ToString());
			continue;
		}
		const int32 SafeRank = FMath::Clamp(Pair.Value, 1, FMath::Max(1, NodeInfo->MaxRank));
		if (!ApplyNodeToGAS(*NodeInfo, SafeRank))
		{
			UE_LOG(LogPanthelia, Error,
				TEXT("[SkillTree] ReapplyAllNodes no pudo reconstruir '%s' al rango %d."),
				*Pair.Key.ToString(), SafeRank);
		}

	}
}

bool UPantheliaSkillTreeComponent::ApplyNodeToGAS(
	const FPantheliaSkillNodeInfo& NodeInfo,
	int32 Rank)
{
	UPantheliaAbilitySystemComponent* ASC = GetPantheliaASC();
	if (!ASC)
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[SkillTree] ApplyNodeToGAS sin ASC disponible — ¿el componente vive "
			     "fuera de APantheliaPlayerState?"));
		return false;
	}

	if (!NodeInfo.NodeTag.IsValid() || Rank < 1 || Rank > FMath::Max(1, NodeInfo.MaxRank))
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[SkillTree] Datos inválidos para aplicar nodo '%s'. Rank=%d MaxRank=%d."),
			*NodeInfo.NodeTag.ToString(), Rank, NodeInfo.MaxRank);
		return false;
	}

	const bool bHasAbility = NodeInfo.GrantedAbility != nullptr;
	const bool bHasEffects = NodeInfo.GrantedEffects.Num() > 0;
	if (!bHasAbility && !bHasEffects)
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[SkillTree] El nodo '%s' no concede ability ni Gameplay Effects."),
			*NodeInfo.NodeTag.ToString());
		return false;
	}

	// --- PREFLIGHT DE ABILITY ---
	FGameplayTag AbilityTag;
	bool bAbilityAlreadyGranted = false;
	if (bHasAbility)
	{
		AbilityTag = GetAbilityTagFromClass(NodeInfo.GrantedAbility);
		if (!AbilityTag.IsValid())
		{
			UE_LOG(LogPanthelia, Error,
				TEXT("[SkillTree] La ability '%s' del nodo '%s' no tiene un Ability Tag "
				     "válido bajo la raíz 'Abilities'. Compra cancelada."),
				*GetNameSafe(NodeInfo.GrantedAbility), *NodeInfo.NodeTag.ToString());
			return false;
		}

		FGameplayAbilitySpec* ExistingAbilitySpec = ASC->GetSpecFromAbilityTag(AbilityTag);
		if (ExistingAbilitySpec &&
			(!ExistingAbilitySpec->Ability ||
			 ExistingAbilitySpec->Ability->GetClass() != NodeInfo.GrantedAbility.Get()))
		{
			UE_LOG(LogPanthelia, Error,
				TEXT("[SkillTree] Colisión de AbilityTag '%s' en nodo '%s'. La spec existente "
				     "pertenece a otra clase. Compra cancelada."),
				*AbilityTag.ToString(), *NodeInfo.NodeTag.ToString());
			return false;
		}

		bAbilityAlreadyGranted = ExistingAbilitySpec != nullptr;
	}

	// --- PREFLIGHT DE GAMEPLAY EFFECTS ---
	TArray<FGameplayEffectSpecHandle> PreparedEffectSpecs;
	PreparedEffectSpecs.Reserve(NodeInfo.GrantedEffects.Num());

	for (const TSubclassOf<UGameplayEffect>& EffectClass : NodeInfo.GrantedEffects)
	{
		if (!EffectClass)
		{
			UE_LOG(LogPanthelia, Error,
				TEXT("[SkillTree] El nodo '%s' contiene un GrantedEffect nulo."),
				*NodeInfo.NodeTag.ToString());
			return false;
		}

		const UGameplayEffect* EffectCDO = EffectClass.GetDefaultObject();
		if (!EffectCDO || EffectCDO->DurationPolicy != EGameplayEffectDurationType::Infinite)
		{
			UE_LOG(LogPanthelia, Error,
				TEXT("[SkillTree] El GE '%s' del nodo '%s' debe ser Infinite para permitir "
				     "rollback, respec y carga. Compra cancelada."),
				*GetNameSafe(EffectClass), *NodeInfo.NodeTag.ToString());
			return false;
		}

		// La transacción aplica primero el rango nuevo y conserva temporalmente el
		// anterior para poder hacer rollback. Un GE con stacking podría reutilizar el
		// mismo FActiveGameplayEffectHandle; al retirar después el rango anterior se
		// eliminaría también el nuevo. Por eso los GEs del árbol deben crear instancias
		// independientes: Infinite y EGameplayEffectStackingType::None.
		if (EffectCDO->GetStackingType() != EGameplayEffectStackingType::None)
		{
			UE_LOG(LogPanthelia, Error,
				TEXT("[SkillTree] El GE '%s' del nodo '%s' tiene stacking configurado. "
				     "Los GEs del árbol deben usar Stacking Type=None para que la "
				     "transacción y el rollback sean seguros. Compra cancelada."),
				*GetNameSafe(EffectClass), *NodeInfo.NodeTag.ToString());
			return false;
		}

		FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
		ContextHandle.AddSourceObject(GetOwner());

		FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, Rank, ContextHandle);
		if (!SpecHandle.IsValid())
		{
			UE_LOG(LogPanthelia, Error,
				TEXT("[SkillTree] No se pudo construir el spec '%s' del nodo '%s'."),
				*GetNameSafe(EffectClass), *NodeInfo.NodeTag.ToString());
			return false;
		}

		for (const TPair<FGameplayTag, FScalableFloat>& Magnitude : NodeInfo.SetByCallerMagnitudes)
		{
			const float Value = Magnitude.Value.GetValueAtLevel(Rank);
			if (!Magnitude.Key.IsValid() || !FMath::IsFinite(Value))
			{
				UE_LOG(LogPanthelia, Error,
					TEXT("[SkillTree] SetByCaller inválido en nodo '%s'. Tag=%s Value=%f."),
					*NodeInfo.NodeTag.ToString(), *Magnitude.Key.ToString(), Value);
				return false;
			}

			SpecHandle.Data->SetSetByCallerMagnitude(Magnitude.Key, Value);
		}

		PreparedEffectSpecs.Add(SpecHandle);
	}

	// Aplicamos las nuevas instancias sin retirar todavía las anteriores. El preflight
	// ya garantizó Stacking Type=None, así que cada aplicación obtiene una instancia
	// independiente. Si una falla, las nuevas se remueven y el rango confirmado continúa intacto.
	TArray<FActiveGameplayEffectHandle> NewEffectHandles;
	NewEffectHandles.Reserve(PreparedEffectSpecs.Num());

	for (const FGameplayEffectSpecHandle& SpecHandle : PreparedEffectSpecs)
	{
		const FActiveGameplayEffectHandle ActiveHandle =
			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
		if (!ActiveHandle.IsValid())
		{
			for (const FActiveGameplayEffectHandle& NewHandle : NewEffectHandles)
			{
				ASC->RemoveActiveGameplayEffect(NewHandle, -1);
			}
			return false;
		}
		NewEffectHandles.Add(ActiveHandle);
	}

	// La ability se confirma al final, cuando todos los GEs nuevos ya son válidos.
	// Si falla, retiramos las instancias nuevas y dejamos activas las anteriores.
	if (bHasAbility)
	{
		bool bAbilityApplied = false;
		if (bAbilityAlreadyGranted)
		{
			bAbilityApplied = ASC->SetAbilityLevel(AbilityTag, Rank);
		}
		else
		{
			const FGameplayAbilitySpecHandle GrantedHandle =
				ASC->GiveAbility(FGameplayAbilitySpec(NodeInfo.GrantedAbility, Rank));
			bAbilityApplied = GrantedHandle.IsValid();
		}

		if (!bAbilityApplied)
		{
			for (const FActiveGameplayEffectHandle& NewHandle : NewEffectHandles)
			{
				ASC->RemoveActiveGameplayEffect(NewHandle, -1);
			}
			return false;
		}
	}

	// Todo el estado nuevo está aplicado. Retiramos las instancias del rango anterior
	// y publicamos los handles nuevos como única contabilidad activa.
	RemoveNodeEffects(NodeInfo.NodeTag);
	if (NewEffectHandles.Num() > 0)
	{
		ActiveNodeEffectHandles.Add(NodeInfo.NodeTag, MoveTemp(NewEffectHandles));
	}

	return true;
}

void UPantheliaSkillTreeComponent::RemoveNodeEffects(const FGameplayTag& NodeTag)
{
	UPantheliaAbilitySystemComponent* ASC = GetPantheliaASC();
	if (!ASC) return;

	// Find (no FindOrAdd): si el nodo nunca aplicó GEs, no hay nada que limpiar
	// y no queremos crear una entrada vacía en el mapa.
	if (TArray<FActiveGameplayEffectHandle>* Handles = ActiveNodeEffectHandles.Find(NodeTag))
	{
		for (const FActiveGameplayEffectHandle& Handle : *Handles)
		{
			if (Handle.IsValid())
			{
				// -1 elimina por completo la instancia. Los GEs de nodo no pueden usar
				// stacking (el preflight lo rechaza), pero conservamos -1 como limpieza
				// defensiva ante datos legacy o cambios externos.
				ASC->RemoveActiveGameplayEffect(Handle, -1);
			}
		}
		ActiveNodeEffectHandles.Remove(NodeTag);
	}
}

FGameplayTag UPantheliaSkillTreeComponent::GetAbilityTagFromClass(TSubclassOf<UGameplayAbility> AbilityClass)
{
	if (!AbilityClass) return FGameplayTag();

	// El CDO (Class Default Object) es la instancia "plantilla" que Unreal crea de
	// cada clase: podemos leerle los Ability Tags configurados en el Blueprint de la
	// ability SIN necesidad de instanciarla ni de tenerla otorgada en ningún ASC.
	const UGameplayAbility* AbilityCDO = AbilityClass.GetDefaultObject();
	if (!AbilityCDO) return FGameplayTag();

	// Mismo criterio que GetAbilityTagFromSpec en el ASC: el tag identificador de
	// una ability es el que cuelga de la raíz nativa "Abilities" (Etapa 1).
	for (const FGameplayTag& Tag : AbilityCDO->GetAssetTags())
	{
		if (Tag.MatchesTag(FPantheliaGameplayTags::Get().Abilities))
		{
			return Tag;
		}
	}
	return FGameplayTag();
}
