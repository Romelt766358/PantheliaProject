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
	// Toda la validación vive en CanUnlockNode — así la UI y este método usan
	// EXACTAMENTE las mismas reglas y nunca pueden discrepar (si el botón se pintó
	// como comprable, la compra no puede fallar por una regla distinta).
	if (!CanUnlockNode(NodeTag))
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("[SkillTree] TryUnlockNode rechazado para '%s' (rango máximo, nivel, "
			     "puntos o prerequisitos). Consulta CanUnlockNode antes desde la UI."),
			*NodeTag.ToString());
		return false;
	}

	// CanUnlockNode ya garantizó que todo esto existe y es válido.
	const FPantheliaSkillNodeInfo* NodeInfo = SkillTreeInfo->FindNodeInfoForTag(NodeTag, true);
	APantheliaPlayerState* PS = GetPantheliaPlayerState();

	// 1. Gastar los puntos. Usamos el setter del PlayerState (no tocamos el campo
	// directamente): así el broadcast de OnSkillPointsChangedDelegate sale gratis
	// y la UI de puntos se actualiza sola, igual que hace el gasto de puntos de
	// atributo en el ASC (UpgradeAttribute).
	PS->SetSkillPoints(PS->GetSkillPoints() - NodeInfo->CostPerRank);

	// 2. Subir el rango en el estado (la única verdad que guardará el SaveGame).
	const int32 NewRank = GetNodeRank(NodeTag) + 1;
	NodeRanks.Add(NodeTag, NewRank);

	// 3. Traducir el nuevo rango a estado de GAS (abilities + GEs).
	ApplyNodeToGAS(*NodeInfo, NewRank);

	// 4. Avisar a la UI del cambio de ESTE nodo.
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
		ApplyNodeToGAS(*NodeInfo, Pair.Value);
	}
}

void UPantheliaSkillTreeComponent::ApplyNodeToGAS(const FPantheliaSkillNodeInfo& NodeInfo, int32 Rank)
{
	UPantheliaAbilitySystemComponent* ASC = GetPantheliaASC();
	if (!ASC)
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[SkillTree] ApplyNodeToGAS sin ASC disponible — ¿el componente vive "
			     "fuera de APantheliaPlayerState?"));
		return;
	}

	// --- (1) ABILITY DEL NODO (si tiene) ---
	if (NodeInfo.GrantedAbility)
	{
		// Derivamos el tag desde el CDO de la clase (ver GetAbilityTagFromClass):
		// cero duplicación de datos en el editor, cero typos posibles.
		const FGameplayTag AbilityTag = GetAbilityTagFromClass(NodeInfo.GrantedAbility);

		// ¿El ASC ya tiene esta ability? (rango 2+ o re-aplicación tras cargar save)
		// → solo hay que ajustar su nivel. SetAbilityLevel (Etapa 2) ya ignora
		// niveles repetidos, así que ReapplyAllNodes es idempotente aquí.
		if (AbilityTag.IsValid() && ASC->GetSpecFromAbilityTag(AbilityTag))
		{
			ASC->SetAbilityLevel(AbilityTag, Rank);
		}
		else
		{
			// Primera vez: otorgamos la ability con nivel = rango del nodo.
			// NOTA: aquí NO se asigna InputTag — el nodo desbloquea la ability, pero
			// EQUIPARLA a un slot de input es responsabilidad del futuro sistema de
			// equipado de hechizos (sección 24 del curso, clases ~290+). Desbloquear
			// y equipar son acciones distintas por diseño.
			FGameplayAbilitySpec AbilitySpec(NodeInfo.GrantedAbility, Rank);
			ASC->GiveAbility(AbilitySpec);

			if (!AbilityTag.IsValid())
			{
				// La ability no tiene ningún tag bajo la raíz "Abilities": funcionará,
				// pero los rangos siguientes no podrán localizarla para subirle el
				// nivel. Es un error de datos que conviene gritar pronto.
				UE_LOG(LogPanthelia, Error,
					TEXT("[SkillTree] La ability '%s' del nodo '%s' no tiene ningún "
					     "Ability Tag bajo la raíz 'Abilities'. Añádeselo en su Blueprint "
					     "o los rangos 2+ de este nodo no podrán subirle el nivel."),
					*GetNameSafe(NodeInfo.GrantedAbility), *NodeInfo.NodeTag.ToString());
			}
		}
	}

	// --- (2) GAMEPLAY EFFECTS DEL NODO (si tiene) ---
	//
	// Patrón QUITAR + REAPLICAR (el mismo de RefreshSecondaryAttributes, y por la
	// misma razón): la forma determinista de que un GE Infinite refleje las
	// magnitudes del rango nuevo es destruir la instancia del rango viejo y crear
	// una nueva desde cero. Esto también hace idempotente a ReapplyAllNodes.
	RemoveNodeEffects(NodeInfo.NodeTag);

	for (const TSubclassOf<UGameplayEffect>& EffectClass : NodeInfo.GrantedEffects)
	{
		if (!EffectClass) continue;

		// Validación de datos en desarrollo: los GEs de nodo DEBEN ser Infinite
		// (removibles). Un Instant aquí sería irrevocable — imposible de quitar en
		// un respec o al recargar partida. Aplicamos igualmente (quizá es deliberado
		// en algún caso raro), pero avisando fuerte.
		if (EffectClass.GetDefaultObject()->DurationPolicy != EGameplayEffectDurationType::Infinite)
		{
			UE_LOG(LogPanthelia, Warning,
				TEXT("[SkillTree] El GE '%s' del nodo '%s' NO es Infinite. Los efectos "
				     "de nodo deben ser Infinite para poder removerse (respec/carga)."),
				*GetNameSafe(EffectClass), *NodeInfo.NodeTag.ToString());
		}

		// Contexto del efecto: registramos al PlayerState como SourceObject para que
		// cualquier sistema futuro pueda preguntar "¿de dónde salió este efecto?".
		FGameplayEffectContextHandle ContextHandle = ASC->MakeEffectContext();
		ContextHandle.AddSourceObject(GetOwner());

		// El nivel del SPEC es el rango del nodo: si el GE usa curvas por nivel en
		// sus magnitudes (alternativa a SetByCaller), también escalarán con el rango.
		FGameplayEffectSpecHandle SpecHandle = ASC->MakeOutgoingSpec(EffectClass, Rank, ContextHandle);
		if (!SpecHandle.IsValid()) continue;

		// Inyectamos las magnitudes SetByCaller evaluadas AL RANGO ACTUAL.
		// FScalableFloat::GetValueAtLevel(Rank) lee la curva del Data Asset:
		// rango 1 → fila 1 de la curva, rango 2 → fila 2, etc.
		for (const TPair<FGameplayTag, FScalableFloat>& Magnitude : NodeInfo.SetByCallerMagnitudes)
		{
			SpecHandle.Data->SetSetByCallerMagnitude(Magnitude.Key, Magnitude.Value.GetValueAtLevel(Rank));
		}

		// Aplicamos y GUARDAMOS EL HANDLE — la "llave" para poder remover esta
		// instancia exacta en el siguiente rango o en un respec (misma disciplina
		// de contabilidad que SecondaryAttributesEffectHandle en el ASC).
		const FActiveGameplayEffectHandle ActiveHandle =
			ASC->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data);
		ActiveNodeEffectHandles.FindOrAdd(NodeInfo.NodeTag).Add(ActiveHandle);
	}
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
				// -1 = remover todos los stacks de la instancia (los GEs de nodo
				// tienen Stack Limit 1, pero -1 es la forma robusta — mismo criterio
				// que RefreshSecondaryAttributes).
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
