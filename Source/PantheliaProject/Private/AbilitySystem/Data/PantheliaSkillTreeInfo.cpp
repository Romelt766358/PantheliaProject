// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Data/PantheliaSkillTreeInfo.h"
#include "PantheliaLogChannels.h"

const FPantheliaSkillNodeInfo* UPantheliaSkillTreeInfo::FindNodeInfoForTag(
	const FGameplayTag& NodeTag, bool bLogNotFound) const
{
	// Búsqueda lineal, igual que FindAbilityInfoForTag en UPantheliaAbilityInfoAsset.
	// Con las decenas de nodos que tendrá el árbol es más que suficiente; si algún día
	// hubiera cientos, se puede cachear un TMap<Tag, índice> en PostLoad sin cambiar
	// esta firma (los callers no se enteran — esa es la ventaja de encapsular aquí).
	for (const FPantheliaSkillNodeInfo& NodeInfo : SkillNodes)
	{
		// MatchesTagExact: la clave primaria exige coincidencia exacta, no jerárquica.
		// (Un lookup jerárquico devolvería el primer hijo que encontrara — peligroso
		// para una clave que identifica UN nodo concreto.)
		if (NodeInfo.NodeTag.MatchesTagExact(NodeTag))
		{
			return &NodeInfo;
		}
	}

	if (bLogNotFound)
	{
		UE_LOG(LogPanthelia, Error,
			TEXT("[SkillTree] No existe ningún nodo con el tag '%s' en el Data Asset '%s'."),
			*NodeTag.ToString(), *GetNameSafe(this));
	}
	return nullptr;
}

#if WITH_EDITOR

#include "Abilities/GameplayAbility.h"
#include "AbilitySystem/PantheliaAbilityTagUtils.h"
#include "GameplayEffect.h"
#include "PantheliaGameplayTags.h"
#include "Validation/PantheliaDataValidationUtils.h"

EDataValidationResult UPantheliaSkillTreeInfo::IsDataValid(FDataValidationContext& Context) const
{
	using namespace PantheliaDataValidation;

	EDataValidationResult Result = MakeInitialResult(Super::IsDataValid(Context));

	// Un árbol vacío es válido durante la etapa de infraestructura. En cuanto se añada
	// el primer nodo, todas las invariantes siguientes pasan a ser obligatorias.
	if (SkillNodes.IsEmpty())
	{
		return Result;
	}

	const FPantheliaGameplayTags& Tags = FPantheliaGameplayTags::Get();
	TMap<FGameplayTag, int32> NodeIndexByTag;
	TMap<FGameplayTag, FGameplayTag> AbilityOwnerNodeByTag;

	for (int32 NodeIndex = 0; NodeIndex < SkillNodes.Num(); ++NodeIndex)
	{
		const FPantheliaSkillNodeInfo& Node = SkillNodes[NodeIndex];
		const FString NodeLabel = FString::Printf(TEXT("SkillNodes[%d]"), NodeIndex);

		if (!Node.NodeTag.IsValid())
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s: NodeTag es inválido."), *NodeLabel));
		}
		else if (const int32* ExistingIndex = NodeIndexByTag.Find(Node.NodeTag))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s: NodeTag '%s' está duplicado; ya existe en SkillNodes[%d]."),
				*NodeLabel,
				*Node.NodeTag.ToString(),
				*ExistingIndex));
		}
		else
		{
			NodeIndexByTag.Add(Node.NodeTag, NodeIndex);
		}

		if (Node.MaxRank < 1)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (%s): MaxRank debe ser al menos 1."),
				*NodeLabel, *Node.NodeTag.ToString()));
		}

		if (Node.CostPerRank < 0)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (%s): CostPerRank no puede ser negativo."),
				*NodeLabel, *Node.NodeTag.ToString()));
		}

		if (Node.LevelRequirement < 1)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (%s): LevelRequirement debe ser al menos 1."),
				*NodeLabel, *Node.NodeTag.ToString()));
		}

		const bool bHasAbility = Node.GrantedAbility != nullptr;
		const bool bHasEffects = Node.GrantedEffects.Num() > 0;
		if (!bHasAbility && !bHasEffects)
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (%s): el nodo no concede una ability ni Gameplay Effects."),
				*NodeLabel, *Node.NodeTag.ToString()));
		}

		if (!bHasEffects && !Node.SetByCallerMagnitudes.IsEmpty())
		{
			AddError(Context, Result, FString::Printf(
				TEXT("%s (%s): tiene SetByCallerMagnitudes pero ningún GrantedEffect que las consuma."),
				*NodeLabel, *Node.NodeTag.ToString()));
		}

		if (bHasAbility)
		{
			const UGameplayAbility* AbilityCDO = Node.GrantedAbility.GetDefaultObject();
			if (!AbilityCDO)
			{
				AddError(Context, Result, FString::Printf(
					TEXT("%s (%s): no se pudo leer el CDO de GrantedAbility '%s'."),
					*NodeLabel,
					*Node.NodeTag.ToString(),
					*GetNameSafe(Node.GrantedAbility)));
			}
			else
			{
				const TArray<FGameplayTag> AbilityLeafTags = PantheliaAbilityTagUtils::FindLeafTagsUnderRoot(
					AbilityCDO->GetAssetTags(), Tags.Abilities);

				if (AbilityLeafTags.Num() != 1)
				{
					AddError(Context, Result, FString::Printf(
						TEXT("%s (%s): GrantedAbility '%s' debe tener una sola hoja identificadora bajo 'Abilities'; se encontraron %d."),
						*NodeLabel,
						*Node.NodeTag.ToString(),
						*GetNameSafe(Node.GrantedAbility),
						AbilityLeafTags.Num()));
				}
				else
				{
					const FGameplayTag AbilityTag = AbilityLeafTags[0];
					if (const FGameplayTag* ExistingNodeTag = AbilityOwnerNodeByTag.Find(AbilityTag))
					{
						AddError(Context, Result, FString::Printf(
							TEXT("%s (%s): la AbilityTag '%s' ya pertenece al nodo '%s'. Dos nodos no pueden poseer el mismo nivel de ability."),
							*NodeLabel,
							*Node.NodeTag.ToString(),
							*AbilityTag.ToString(),
							*ExistingNodeTag->ToString()));
					}
					else if (Node.NodeTag.IsValid())
					{
						AbilityOwnerNodeByTag.Add(AbilityTag, Node.NodeTag);
					}
				}
			}
		}

		TSet<const UClass*> SeenEffectClasses;
		for (int32 EffectIndex = 0; EffectIndex < Node.GrantedEffects.Num(); ++EffectIndex)
		{
			const TSubclassOf<UGameplayEffect>& EffectClass = Node.GrantedEffects[EffectIndex];
			if (!EffectClass)
			{
				AddError(Context, Result, FString::Printf(
					TEXT("%s (%s): GrantedEffects[%d] es nulo."),
					*NodeLabel, *Node.NodeTag.ToString(), EffectIndex));
				continue;
			}

			if (SeenEffectClasses.Contains(EffectClass.Get()))
			{
				AddError(Context, Result, FString::Printf(
					TEXT("%s (%s): el GE '%s' está repetido dentro del mismo nodo."),
					*NodeLabel,
					*Node.NodeTag.ToString(),
					*GetNameSafe(EffectClass)));
			}
			SeenEffectClasses.Add(EffectClass.Get());

			const UGameplayEffect* EffectCDO = EffectClass.GetDefaultObject();
			if (!EffectCDO)
			{
				AddError(Context, Result, FString::Printf(
					TEXT("%s (%s): no se pudo leer el CDO del GE '%s'."),
					*NodeLabel,
					*Node.NodeTag.ToString(),
					*GetNameSafe(EffectClass)));
				continue;
			}

			if (EffectCDO->DurationPolicy != EGameplayEffectDurationType::Infinite)
			{
				AddError(Context, Result, FString::Printf(
					TEXT("%s (%s): el GE '%s' debe usar Duration Policy=Infinite."),
					*NodeLabel,
					*Node.NodeTag.ToString(),
					*GetNameSafe(EffectClass)));
			}

			if (EffectCDO->GetStackingType() != EGameplayEffectStackingType::None)
			{
				AddError(Context, Result, FString::Printf(
					TEXT("%s (%s): el GE '%s' debe usar Stacking Type=None para que rollback y cambio de rango sean seguros."),
					*NodeLabel,
					*Node.NodeTag.ToString(),
					*GetNameSafe(EffectClass)));
			}
		}

		for (const TPair<FGameplayTag, FScalableFloat>& Magnitude : Node.SetByCallerMagnitudes)
		{
			if (!Magnitude.Key.IsValid())
			{
				AddError(Context, Result, FString::Printf(
					TEXT("%s (%s): existe una magnitud SetByCaller con tag inválido."),
					*NodeLabel, *Node.NodeTag.ToString()));
				continue;
			}

			// Las magnitudes negativas son válidas para reducciones (por ejemplo, costes
			// planos o porcentuales). La invariante común es que el valor sea finito.
			ValidateScalableFloatFinite(
				Magnitude.Value,
				1,
				FMath::Max(1, Node.MaxRank),
				FString::Printf(TEXT("%s (%s) SetByCaller '%s'"),
					*NodeLabel,
					*Node.NodeTag.ToString(),
					*Magnitude.Key.ToString()),
				Context,
				Result);
		}
	}

	// Los prerequisitos se validan después de construir el índice completo para que
	// el orden de las entradas en el array no importe.
	for (int32 NodeIndex = 0; NodeIndex < SkillNodes.Num(); ++NodeIndex)
	{
		const FPantheliaSkillNodeInfo& Node = SkillNodes[NodeIndex];
		TArray<FGameplayTag> PrerequisiteTags;
		Node.PrerequisiteNodeTags.GetGameplayTagArray(PrerequisiteTags);

		for (const FGameplayTag& PrerequisiteTag : PrerequisiteTags)
		{
			if (!PrerequisiteTag.IsValid())
			{
				AddError(Context, Result, FString::Printf(
					TEXT("SkillNodes[%d] (%s): contiene un prerequisito inválido."),
					NodeIndex, *Node.NodeTag.ToString()));
			}
			else if (PrerequisiteTag == Node.NodeTag)
			{
				AddError(Context, Result, FString::Printf(
					TEXT("SkillNodes[%d] (%s): un nodo no puede ser prerequisito de sí mismo."),
					NodeIndex, *Node.NodeTag.ToString()));
			}
			else if (!NodeIndexByTag.Contains(PrerequisiteTag))
			{
				AddError(Context, Result, FString::Printf(
					TEXT("SkillNodes[%d] (%s): el prerequisito '%s' no existe en este Data Asset."),
					NodeIndex,
					*Node.NodeTag.ToString(),
					*PrerequisiteTag.ToString()));
			}
		}
	}

	// Detección de ciclos por DFS. Un ciclo hace imposible satisfacer los requisitos
	// sin editar estado a mano o cargar un save corrupto. Reservamos el máximo de
	// entradas posible y, además, nunca conservamos referencias a valores del TMap
	// durante la recursión: una inserción podría realojar su almacenamiento interno.
	TMap<FGameplayTag, uint8> VisitState;
	VisitState.Reserve(NodeIndexByTag.Num());
	FGameplayTag CycleFrom;
	FGameplayTag CycleTo;

	TFunction<bool(const FGameplayTag&)> VisitNode = [&](const FGameplayTag& NodeTag)
	{
		const uint8 State = VisitState.FindRef(NodeTag);
		if (State == 1)
		{
			return true;
		}
		if (State == 2)
		{
			return false;
		}

		VisitState.FindOrAdd(NodeTag) = 1;
		const int32* NodeIndex = NodeIndexByTag.Find(NodeTag);
		if (NodeIndex && SkillNodes.IsValidIndex(*NodeIndex))
		{
			TArray<FGameplayTag> PrerequisiteTags;
			SkillNodes[*NodeIndex].PrerequisiteNodeTags.GetGameplayTagArray(PrerequisiteTags);
			for (const FGameplayTag& PrerequisiteTag : PrerequisiteTags)
			{
				if (!NodeIndexByTag.Contains(PrerequisiteTag))
				{
					continue;
				}

				const uint8 PrerequisiteState = VisitState.FindRef(PrerequisiteTag);
				if (PrerequisiteState == 1)
				{
					CycleFrom = NodeTag;
					CycleTo = PrerequisiteTag;
					return true;
				}

				if (PrerequisiteState == 0 && VisitNode(PrerequisiteTag))
				{
					return true;
				}
			}
		}

		VisitState.FindOrAdd(NodeTag) = 2;
		return false;
	};

	for (const TPair<FGameplayTag, int32>& Pair : NodeIndexByTag)
	{
		if (VisitState.FindRef(Pair.Key) == 0 && VisitNode(Pair.Key))
		{
			AddError(Context, Result, FString::Printf(
				TEXT("El árbol contiene un ciclo de prerequisitos detectado entre '%s' y '%s'."),
				*CycleFrom.ToString(),
				*CycleTo.ToString()));
			break;
		}
	}

	return Result;
}

#endif // WITH_EDITOR
