#include "PDSPantheliaMontageEventInspector.h"

#include "Animation/AnimMontage.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "UObject/UnrealType.h"

namespace
{
    bool ObjectContainsExactGameplayTag(
        const UObject* Object,
        const FGameplayTag& RequiredTag)
    {
        if (!IsValid(Object) || !RequiredTag.IsValid())
        {
            return false;
        }

        for (TFieldIterator<FProperty> It(Object->GetClass()); It; ++It)
        {
            const FStructProperty* StructProperty =
                CastField<FStructProperty>(*It);
            if (!StructProperty
                || StructProperty->Struct != FGameplayTag::StaticStruct())
            {
                continue;
            }

            const void* ValuePtr =
                StructProperty->ContainerPtrToValuePtr<void>(Object);
            const FGameplayTag* TagValue =
                static_cast<const FGameplayTag*>(ValuePtr);

            if (TagValue && TagValue->MatchesTagExact(RequiredTag))
            {
                return true;
            }
        }

        return false;
    }
}

bool PDSPantheliaMontageEventInspector::ContainsGameplayTagEvent(
    const UAnimMontage* Montage,
    const FGameplayTag& RequiredTag)
{
    if (!IsValid(Montage) || !RequiredTag.IsValid())
    {
        return false;
    }

    for (const FAnimNotifyEvent& Event : Montage->Notifies)
    {
        if (ObjectContainsExactGameplayTag(Event.Notify, RequiredTag)
            || ObjectContainsExactGameplayTag(
                Event.NotifyStateClass.Get(),
                RequiredTag))
        {
            return true;
        }
    }

    return false;
}
