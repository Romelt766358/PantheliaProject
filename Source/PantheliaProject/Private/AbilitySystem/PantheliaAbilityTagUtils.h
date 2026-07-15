#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"

namespace PantheliaAbilityTagUtils
{
	// Devuelve únicamente las hojas explícitas bajo RootTag. Si el container contiene
	// a la vez un padre y uno de sus hijos, el padre no identifica la ability: la hoja
	// más específica es la clave estable que deben compartir validación y runtime.
	inline TArray<FGameplayTag> FindLeafTagsUnderRoot(
		const FGameplayTagContainer& Tags,
		const FGameplayTag& RootTag)
	{
		TArray<FGameplayTag> MatchingTags;
		for (const FGameplayTag& Tag : Tags)
		{
			if (Tag.IsValid() && Tag != RootTag && Tag.MatchesTag(RootTag))
			{
				MatchingTags.Add(Tag);
			}
		}

		TArray<FGameplayTag> LeafTags;
		for (const FGameplayTag& Candidate : MatchingTags)
		{
			bool bIsParentOfAnotherTag = false;
			for (const FGameplayTag& OtherTag : MatchingTags)
			{
				if (OtherTag != Candidate && OtherTag.MatchesTag(Candidate))
				{
					bIsParentOfAnotherTag = true;
					break;
				}
			}

			if (!bIsParentOfAnotherTag)
			{
				LeafTags.Add(Candidate);
			}
		}

		return LeafTags;
	}
}
