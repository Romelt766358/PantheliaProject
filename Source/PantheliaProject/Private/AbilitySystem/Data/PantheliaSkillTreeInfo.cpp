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
