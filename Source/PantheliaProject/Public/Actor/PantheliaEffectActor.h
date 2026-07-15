#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "GameplayEffectTypes.h"
#include "PantheliaEffectActor.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UAbilitySystemComponent;
class UGameplayEffect;

UENUM(BlueprintType)
enum class EEffectApplicationPolicy : uint8
{
    ApplyOnOverlapBegin,
    ApplyOnOverlapEnd,
    DoNotApply
};

UENUM(BlueprintType)
enum class EEffectRemovalPolicy : uint8
{
    RemoveOnOverlapEnd,
    DoNotRemove
};

/**
 * Define qué Ability System Component construye el Gameplay Effect Spec.
 *
 * TargetSelf conserva el comportamiento de pickups/consumibles existentes: el
 * objetivo fabrica y recibe su propio spec. Owner e Instigator son las opciones
 * correctas para zonas ofensivas creadas por un jugador o enemigo, porque el
 * source conserva sus atributos, nivel, penetración, crítico y atribución.
 */
UENUM(BlueprintType)
enum class EPantheliaEffectSourcePolicy : uint8
{
    TargetSelf UMETA(DisplayName = "Target Self (Pickup / Self Effect)"),
    EffectActorOwner UMETA(DisplayName = "Effect Actor Owner"),
    EffectActorInstigator UMETA(DisplayName = "Effect Actor Instigator"),
    EffectActorASC UMETA(DisplayName = "Effect Actor ASC")
};

UCLASS()
class PANTHELIAPROJECT_API APantheliaEffectActor : public AActor
{
    GENERATED_BODY()

public:
    APantheliaEffectActor();

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    UFUNCTION(BlueprintCallable)
    void ApplyEffectToTarget(AActor* TargetActor, TSubclassOf<UGameplayEffect> GameplayEffectClass);

    // Callbacks internos de la Sphere (C++)
    UFUNCTION()
    virtual void OnOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex,
        bool bFromSweep, const FHitResult& SweepResult);

    UFUNCTION()
    virtual void EndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComp, int32 OtherBodyIndex);

    // Funciones BlueprintCallable para llamar desde Blueprints con volúmenes custom
    UFUNCTION(BlueprintCallable)
    void HandleOverlap(AActor* TargetActor);

    UFUNCTION(BlueprintCallable)
    void HandleEndOverlap(AActor* TargetActor);

    // --- EFECTOS INSTANT ---
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
    TSubclassOf<UGameplayEffect> InstantGameplayEffectClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
    EEffectApplicationPolicy InstantEffectApplicationPolicy = EEffectApplicationPolicy::DoNotApply;

    // --- EFECTOS DURATION ---
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
    TSubclassOf<UGameplayEffect> DurationGameplayEffectClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
    EEffectApplicationPolicy DurationEffectApplicationPolicy = EEffectApplicationPolicy::DoNotApply;

    // --- EFECTOS INFINITE ---
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
    TSubclassOf<UGameplayEffect> InfiniteGameplayEffectClass;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
    EEffectApplicationPolicy InfiniteEffectApplicationPolicy = EEffectApplicationPolicy::DoNotApply;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
    EEffectRemovalPolicy InfiniteEffectRemovalPolicy = EEffectRemovalPolicy::RemoveOnOverlapEnd;

    // --- CONFIGURACIÓN GENERAL ---
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
    float ActorLevel = 1.0f;

    // Define quién fabrica el spec. TargetSelf conserva los pickups actuales; las
    // zonas ofensivas deben usar Owner o Instigator para atribuir correctamente el daño.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
    EPantheliaEffectSourcePolicy EffectSourcePolicy = EPantheliaEffectSourcePolicy::TargetSelf;

    // Destruye el actor al APLICAR el efecto (pociones/consumibles)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
    bool bDestroyOnEffectApplication = false;

    // Destruye el actor al ELIMINAR el efecto (cuando el área de fuego desaparece, etc.)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
    bool bDestroyOnEffectRemoval = false;

    // false: excluye enemigos. true: permite afectar tanto al jugador como a enemigos.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
    bool bApplyEffectsToEnemies = false;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USphereComponent> Sphere;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Mesh;

    // Mapa que asocia cada handle de efecto activo con el ASC del objetivo.
    // Se usan referencias débiles porque el objetivo puede destruirse antes que el volumen.
    TMap<FActiveGameplayEffectHandle, TWeakObjectPtr<UAbilitySystemComponent>> ActiveEffectHandles;

    // Cuenta cuántos pares de overlap siguen activos por actor. Un mismo objetivo puede
    // solapar varios volúmenes del EffectActor (por ejemplo Sphere + Box) o recibir
    // callbacks duplicados desde C++ y Blueprint. Solo el cambio 0 -> 1 aplica efectos;
    // solo el cambio 1 -> 0 ejecuta políticas de salida y retira Infinite.
    TMap<TWeakObjectPtr<AActor>, int32> ActiveOverlapCounts;

    bool ShouldAffectTarget(const AActor* TargetActor) const;
    bool ApplyEffectToTargetInternal(AActor* TargetActor,
        TSubclassOf<UGameplayEffect> GameplayEffectClass, bool& bOutWasInfinite);
    UAbilitySystemComponent* ResolveSourceAbilitySystemComponent(
        UAbilitySystemComponent* TargetASC) const;
    bool RemoveTrackedEffectsFromTarget(UAbilitySystemComponent* TargetASC);
    void RemoveAllTrackedInfiniteEffects();
};
