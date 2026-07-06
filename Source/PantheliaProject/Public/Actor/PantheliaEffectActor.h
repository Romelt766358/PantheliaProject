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

UCLASS()
class PANTHELIAPROJECT_API APantheliaEffectActor : public AActor
{
    GENERATED_BODY()

public:
    APantheliaEffectActor();

protected:
    virtual void BeginPlay() override;

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

    // Destruye el actor al APLICAR el efecto (pociones/consumibles)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
    bool bDestroyOnEffectApplication = false;

    // Destruye el actor al ELIMINAR el efecto (cuando el área de fuego desaparece, etc.)
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
    bool bDestroyOnEffectRemoval = false;

    // Si es true, solo afecta al jugador. Si es false, también afecta a enemigos.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Applied Effects")
    bool bApplyEffectsToEnemies = false;

private:
    UPROPERTY(VisibleAnywhere)
    TObjectPtr<USphereComponent> Sphere;

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Mesh;

    // Mapa que asocia cada handle de efecto activo con el ASC del objetivo.
    // Necesario para poder eliminar efectos infinitos correctamente.
    TMap<FActiveGameplayEffectHandle, UAbilitySystemComponent*> ActiveEffectHandles;
};