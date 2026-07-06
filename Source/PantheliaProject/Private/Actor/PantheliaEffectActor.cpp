#include "Actor/PantheliaEffectActor.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "GameplayEffect.h"

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
    Sphere->OnComponentBeginOverlap.AddDynamic(this, &APantheliaEffectActor::OnOverlap);
    Sphere->OnComponentEndOverlap.AddDynamic(this, &APantheliaEffectActor::EndOverlap);
}

void APantheliaEffectActor::ApplyEffectToTarget(AActor* TargetActor, TSubclassOf<UGameplayEffect> GameplayEffectClass)
{
    if (!GameplayEffectClass) { return; }

    // Si el target es un enemigo y este effect actor no está configurado para afectar enemigos, ignorar.
    // Esto evita que pociones y cristales sean recogidos por enemigos, mientras que
    // áreas de fuego con bApplyEffectsToEnemies=true sí pueden dañarlos.
    if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) { return; }

    // Usamos la Blueprint Library para obtener el ASC, compatible con cualquier actor
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
    if (!TargetASC) { return; }

    FGameplayEffectContextHandle EffectContextHandle = TargetASC->MakeEffectContext();
    EffectContextHandle.AddSourceObject(this);

    const FGameplayEffectSpecHandle EffectSpecHandle = TargetASC->MakeOutgoingSpec(
        GameplayEffectClass,
        ActorLevel,
        EffectContextHandle
    );
    if (!EffectSpecHandle.IsValid()) { return; }

    const FActiveGameplayEffectHandle ActiveHandle = TargetASC->ApplyGameplayEffectSpecToSelf(
        *EffectSpecHandle.Data.Get()
    );

    // Solo guardamos el handle si el efecto es Infinite y está configurado para eliminarse al salir
    const bool bIsInfinite = EffectSpecHandle.Data.Get()->Def.Get()->DurationPolicy
        == EGameplayEffectDurationType::Infinite;

    if (bIsInfinite && InfiniteEffectRemovalPolicy == EEffectRemovalPolicy::RemoveOnOverlapEnd)
    {
        ActiveEffectHandles.Add(ActiveHandle, TargetASC);
    }

    // Solo destruimos el actor al aplicar si NO es un efecto infinito (no destruir áreas de fuego)
    if (bDestroyOnEffectApplication && !bIsInfinite)
    {
        Destroy();
    }
}

void APantheliaEffectActor::HandleOverlap(AActor* TargetActor)
{
    // El chequeo de tag Enemy está dentro de ApplyEffectToTarget.
    // Sin embargo, también lo comprobamos aquí para evitar entrar en lógica innecesaria.
    if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) { return; }

    if (InstantEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlapBegin)
    {
        ApplyEffectToTarget(TargetActor, InstantGameplayEffectClass);
    }
    if (DurationEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlapBegin)
    {
        ApplyEffectToTarget(TargetActor, DurationGameplayEffectClass);
    }
    if (InfiniteEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlapBegin)
    {
        ApplyEffectToTarget(TargetActor, InfiniteGameplayEffectClass);
    }
}

void APantheliaEffectActor::HandleEndOverlap(AActor* TargetActor)
{
    // Mismo chequeo: si es enemigo y no debemos afectarle, ignorar.
    if (TargetActor->ActorHasTag(FName("Enemy")) && !bApplyEffectsToEnemies) { return; }

    if (InstantEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlapEnd)
    {
        ApplyEffectToTarget(TargetActor, InstantGameplayEffectClass);
    }
    if (DurationEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlapEnd)
    {
        ApplyEffectToTarget(TargetActor, DurationGameplayEffectClass);
    }
    if (InfiniteEffectApplicationPolicy == EEffectApplicationPolicy::ApplyOnOverlapEnd)
    {
        ApplyEffectToTarget(TargetActor, InfiniteGameplayEffectClass);
    }

    // Lógica de eliminación para efectos infinitos
    if (InfiniteEffectRemovalPolicy == EEffectRemovalPolicy::RemoveOnOverlapEnd)
    {
        UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
        if (!IsValid(TargetASC)) { return; }

        // Acumulamos los handles a eliminar para no modificar el mapa mientras lo iteramos
        TArray<FActiveGameplayEffectHandle> HandlesToRemove;
        for (auto& HandlePair : ActiveEffectHandles)
        {
            if (TargetASC == HandlePair.Value)
            {
                // El 1 indica que eliminamos solo un stack, no todos
                TargetASC->RemoveActiveGameplayEffect(HandlePair.Key, 1);
                HandlesToRemove.Add(HandlePair.Key);
            }
        }

        // Ahora sí eliminamos del mapa de forma segura
        for (auto& Handle : HandlesToRemove)
        {
            ActiveEffectHandles.FindAndRemoveChecked(Handle);
        }

        if (bDestroyOnEffectRemoval)
        {
            Destroy();
        }
    }
}

// Los callbacks de la Sphere en C++ delegan a las funciones Handle
// para que la lógica sea consistente tanto si se llama desde C++ como desde Blueprint
void APantheliaEffectActor::OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult)
{
    HandleOverlap(OtherActor);
}

void APantheliaEffectActor::EndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
    UPrimitiveComponent* OtherComp, int32 OtherBodyIndex)
{
    HandleEndOverlap(OtherActor);
}