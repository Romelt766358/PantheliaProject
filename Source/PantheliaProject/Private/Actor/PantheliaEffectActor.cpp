#include "Actor/PantheliaEffectActor.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameplayEffect.h"
#include "PantheliaLogChannels.h"

APantheliaEffectActor::APantheliaEffectActor()
{
    PrimaryActorTick.bCanEverTick = false;

    Mesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Mesh"));
    SetRootComponent(Mesh);

    Sphere = CreateDefaultSubobject<USphereComponent>(TEXT("Sphere"));
    Sphere->SetupAttachment(GetRootComponent());
}

void APantheliaEffectActor::BeginPlay()
{
    Super::BeginPlay();

    if (ensureMsgf(IsValid(Sphere), TEXT("%s no tiene Sphere válida."), *GetNameSafe(this)))
    {
        Sphere->OnComponentBeginOverlap.AddDynamic(this, &APantheliaEffectActor::OnOverlap);
        Sphere->OnComponentEndOverlap.AddDynamic(this, &APantheliaEffectActor::EndOverlap);
    }
}

void APantheliaEffectActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // EndOverlap no está garantizado cuando el volumen o el objetivo se destruyen.
    // Retirar aquí todos los handles evita dejar Gameplay Effects Infinite huérfanos.
    RemoveAllTrackedInfiniteEffects();

    Super::EndPlay(EndPlayReason);
}

bool APantheliaEffectActor::ShouldAffectTarget(const AActor* TargetActor) const
{
    if (!IsValid(TargetActor) || TargetActor == this)
    {
        return false;
    }

    // Si el target es un enemigo y este effect actor no está configurado para afectar enemigos, ignorar.
    // Esto evita que pociones y cristales sean recogidos por enemigos, mientras que
    // áreas de fuego con bApplyEffectsToEnemies=true sí pueden dañarlos.
    return bApplyEffectsToEnemies || !TargetActor->ActorHasTag(FName("Enemy"));
}

UAbilitySystemComponent* APantheliaEffectActor::ResolveSourceAbilitySystemComponent(
    UAbilitySystemComponent* TargetASC) const
{
    switch (EffectSourcePolicy)
    {
        case EPantheliaEffectSourcePolicy::TargetSelf:
            return TargetASC;

        case EPantheliaEffectSourcePolicy::EffectActorOwner:
            return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());

        case EPantheliaEffectSourcePolicy::EffectActorInstigator:
            return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetInstigator());

        case EPantheliaEffectSourcePolicy::EffectActorASC:
            return UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(
                const_cast<APantheliaEffectActor*>(this));

        default:
            return nullptr;
    }
}

void APantheliaEffectActor::ApplyEffectToTarget(
    AActor* TargetActor,
    TSubclassOf<UGameplayEffect> GameplayEffectClass)
{
    bool bWasInfinite = false;
    const bool bApplied = ApplyEffectToTargetInternal(
        TargetActor, GameplayEffectClass, bWasInfinite);

    // Esta ruta pública representa una aplicación aislada. Los flujos de overlap
    // usan el helper interno para poder aplicar sus tres políticas antes de destruir.
    if (bApplied && bDestroyOnEffectApplication && !bWasInfinite)
    {
        Destroy();
    }
}

bool APantheliaEffectActor::ApplyEffectToTargetInternal(
    AActor* TargetActor,
    TSubclassOf<UGameplayEffect> GameplayEffectClass,
    bool& bOutWasInfinite)
{
    bOutWasInfinite = false;

    if (!ShouldAffectTarget(TargetActor) || !GameplayEffectClass)
    {
        return false;
    }

    // Usamos la Blueprint Library para obtener el ASC, compatible con cualquier actor.
    UAbilitySystemComponent* TargetASC =
        UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
    if (!IsValid(TargetASC))
    {
        return false;
    }

    UAbilitySystemComponent* SourceASC = ResolveSourceAbilitySystemComponent(TargetASC);
    if (!IsValid(SourceASC))
    {
        UE_LOG(LogPanthelia, Error,
            TEXT("EffectActor '%s' no aplicó '%s' a '%s': SourcePolicy=%d no resolvió un ASC válido."),
            *GetNameSafe(this),
            *GetNameSafe(GameplayEffectClass.Get()),
            *GetNameSafe(TargetActor),
            static_cast<int32>(EffectSourcePolicy));
        return false;
    }

    FGameplayEffectContextHandle EffectContextHandle = SourceASC->MakeEffectContext();
    EffectContextHandle.AddSourceObject(this);

    const float SafeActorLevel = FMath::IsFinite(ActorLevel)
        ? FMath::Max(ActorLevel, 0.0f)
        : 1.0f;

    const FGameplayEffectSpecHandle EffectSpecHandle = SourceASC->MakeOutgoingSpec(
        GameplayEffectClass,
        SafeActorLevel,
        EffectContextHandle);

    if (!EffectSpecHandle.IsValid() || !EffectSpecHandle.Data.IsValid() ||
        !IsValid(EffectSpecHandle.Data->Def))
    {
        UE_LOG(LogPanthelia, Error,
            TEXT("EffectActor '%s' no pudo construir el spec '%s'."),
            *GetNameSafe(this),
            *GetNameSafe(GameplayEffectClass.Get()));
        return false;
    }

    bOutWasInfinite =
        EffectSpecHandle.Data->Def->DurationPolicy == EGameplayEffectDurationType::Infinite;

    const FActiveGameplayEffectHandle ActiveHandle =
        TargetASC->ApplyGameplayEffectSpecToSelf(*EffectSpecHandle.Data.Get());

    // Los GE Instant no generan un handle activo aunque se ejecuten correctamente.
    // Duration/Infinite deben devolver handle válido para considerarse aplicados.
    const bool bApplicationSucceeded =
        EffectSpecHandle.Data->Def->DurationPolicy == EGameplayEffectDurationType::Instant ||
        ActiveHandle.IsValid();

    // Solo guardamos el handle si el efecto es Infinite, realmente se aplicó y está
    // configurado para eliminarse al salir. RemoveActiveGameplayEffect(..., -1)
    // retirará todos los stacks asociados a ese handle.
    if (bOutWasInfinite && ActiveHandle.IsValid() &&
        InfiniteEffectRemovalPolicy == EEffectRemovalPolicy::RemoveOnOverlapEnd)
    {
        ActiveEffectHandles.Add(ActiveHandle, TargetASC);
    }

    return bApplicationSucceeded;
}

void APantheliaEffectActor::HandleOverlap(AActor* TargetActor)
{
    // El chequeo de tag Enemy está dentro del helper interno.
    // Sin embargo, también lo comprobamos aquí para evitar entrar en lógica innecesaria.
    if (!ShouldAffectTarget(TargetActor))
    {
        return;
    }

    bool bAppliedAnyEffect = false;
    bool bAppliedAnyInfiniteEffect = false;

    auto ApplyConfiguredEffect =
        [this, TargetActor, &bAppliedAnyEffect, &bAppliedAnyInfiniteEffect]
        (TSubclassOf<UGameplayEffect> EffectClass)
        {
            bool bWasInfinite = false;
            const bool bApplied = ApplyEffectToTargetInternal(
                TargetActor, EffectClass, bWasInfinite);
            bAppliedAnyEffect |= bApplied;
            bAppliedAnyInfiniteEffect |= bApplied && bWasInfinite;
        };

    if (InstantEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlapBegin)
    {
        ApplyConfiguredEffect(InstantGameplayEffectClass);
    }
    if (DurationEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlapBegin)
    {
        ApplyConfiguredEffect(DurationGameplayEffectClass);
    }
    if (InfiniteEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlapBegin)
    {
        ApplyConfiguredEffect(InfiniteGameplayEffectClass);
    }

    // El destroy se decide una sola vez, después de procesar todas las políticas.
    // Si se aplicó un Infinite, el actor debe seguir vivo para retirar su handle.
    if (bDestroyOnEffectApplication && bAppliedAnyEffect && !bAppliedAnyInfiniteEffect)
    {
        Destroy();
    }
}

void APantheliaEffectActor::HandleEndOverlap(AActor* TargetActor)
{
    // Mismo chequeo: si es enemigo y no debemos afectarle, ignorar.
    if (!ShouldAffectTarget(TargetActor))
    {
        return;
    }

    bool bAppliedAnyEffect = false;
    bool bAppliedAnyInfiniteEffect = false;

    auto ApplyConfiguredEffect =
        [this, TargetActor, &bAppliedAnyEffect, &bAppliedAnyInfiniteEffect]
        (TSubclassOf<UGameplayEffect> EffectClass)
        {
            bool bWasInfinite = false;
            const bool bApplied = ApplyEffectToTargetInternal(
                TargetActor, EffectClass, bWasInfinite);
            bAppliedAnyEffect |= bApplied;
            bAppliedAnyInfiniteEffect |= bApplied && bWasInfinite;
        };

    if (InstantEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlapEnd)
    {
        ApplyConfiguredEffect(InstantGameplayEffectClass);
    }
    if (DurationEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlapEnd)
    {
        ApplyConfiguredEffect(DurationGameplayEffectClass);
    }
    if (InfiniteEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlapEnd)
    {
        ApplyConfiguredEffect(InfiniteGameplayEffectClass);
    }

    bool bRemovedTrackedInfiniteEffect = false;

    // Lógica de eliminación para efectos infinitos.
    if (InfiniteEffectRemovalPolicy == EEffectRemovalPolicy::RemoveOnOverlapEnd)
    {
        UAbilitySystemComponent* TargetASC =
            UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
        if (IsValid(TargetASC))
        {
            bRemovedTrackedInfiniteEffect = RemoveTrackedEffectsFromTarget(TargetASC);
        }
    }

    if ((bDestroyOnEffectApplication && bAppliedAnyEffect && !bAppliedAnyInfiniteEffect) ||
        (bDestroyOnEffectRemoval && bRemovedTrackedInfiniteEffect))
    {
        Destroy();
    }
}

bool APantheliaEffectActor::RemoveTrackedEffectsFromTarget(UAbilitySystemComponent* TargetASC)
{
    if (!IsValid(TargetASC))
    {
        return false;
    }

    // Acumulamos los handles a eliminar para no modificar el mapa mientras lo iteramos.
    TArray<FActiveGameplayEffectHandle> HandlesToRemove;
    for (const TPair<FActiveGameplayEffectHandle, TWeakObjectPtr<UAbilitySystemComponent>>& HandlePair :
        ActiveEffectHandles)
    {
        if (HandlePair.Value.Get() == TargetASC)
        {
            // -1 elimina todos los stacks asociados al handle. El código anterior retiraba
            // solo uno y después perdía la referencia de los stacks restantes.
            TargetASC->RemoveActiveGameplayEffect(HandlePair.Key, -1);
            HandlesToRemove.Add(HandlePair.Key);
        }
    }

    for (const FActiveGameplayEffectHandle& Handle : HandlesToRemove)
    {
        ActiveEffectHandles.Remove(Handle);
    }

    return !HandlesToRemove.IsEmpty();
}

void APantheliaEffectActor::RemoveAllTrackedInfiniteEffects()
{
    for (const TPair<FActiveGameplayEffectHandle, TWeakObjectPtr<UAbilitySystemComponent>>& HandlePair :
        ActiveEffectHandles)
    {
        if (UAbilitySystemComponent* TargetASC = HandlePair.Value.Get())
        {
            TargetASC->RemoveActiveGameplayEffect(HandlePair.Key, -1);
        }
    }

    ActiveEffectHandles.Empty();
}

// Los callbacks de la Sphere en C++ delegan a las funciones Handle
// para que la lógica sea consistente tanto si se llama desde C++ como desde Blueprint.
void APantheliaEffectActor::OnOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex,
    bool bFromSweep,
    const FHitResult& SweepResult)
{
    HandleOverlap(OtherActor);
}

void APantheliaEffectActor::EndOverlap(
    UPrimitiveComponent* OverlappedComponent,
    AActor* OtherActor,
    UPrimitiveComponent* OtherComp,
    int32 OtherBodyIndex)
{
    HandleEndOverlap(OtherActor);
}
