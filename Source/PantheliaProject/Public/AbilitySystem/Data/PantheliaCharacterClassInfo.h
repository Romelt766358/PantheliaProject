// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "PantheliaCharacterClassInfo.generated.h"

class UGameplayEffect;
class UGameplayAbility;
class UCurveTable;

UENUM(BlueprintType)
enum class EPantheliaCharacterClass : uint8
{
    Elementalist,
    Warrior,
    Ranger
};

// Configuración de atributos primarios por arquetipo de enemigo.
// Solo los primarios varían por clase — secundarios y vitales son compartidos.
USTRUCT(BlueprintType)
struct FCharacterClassDefaultInfo
{
    GENERATED_BODY()

    // GE Instant que inicializa los atributos primarios de este arquetipo.
    // Usa Scalable Float con Curve Table para escalar por nivel.
    UPROPERTY(EditDefaultsOnly, Category = "Class Defaults")
    TSubclassOf<UGameplayEffect> DefaultPrimaryAttributes;

    // Abilities otorgadas SOLO a esta clase al inicializarse.
    // Ej: Warrior recibe GA_MeleeAttack, Elementalist recibe GA_FireboltAttack, etc.
    // A diferencia de CommonAbilities, estas son exclusivas del arquetipo.
    // Se otorgan al nivel del personaje (no hardcodeado a 1) para escalar con él.
    UPROPERTY(EditDefaultsOnly, Category = "Class Defaults")
    TArray<TSubclassOf<UGameplayAbility>> StartupAbilities;
};

// Data Asset central de configuración de personajes y combate.
// Se crea UNA instancia (DA_CharacterClassInfo) y se asigna en el GameMode.
UCLASS()
class PANTHELIAPROJECT_API UPantheliaCharacterClassInfo : public UDataAsset
{
    GENERATED_BODY()

public:

    // Mapa de arquetipo → GE de atributos primarios.
    UPROPERTY(EditDefaultsOnly, Category = "Character Class Defaults")
    TMap<EPantheliaCharacterClass, FCharacterClassDefaultInfo> CharacterClassInformation;

    // GE de secundarios — compartido, calculado por MMC desde primarios.
    UPROPERTY(EditDefaultsOnly, Category = "Common Class Defaults")
    TSubclassOf<UGameplayEffect> DefaultSecondaryAttributes;

    // GE de vitales — compartido, inicializa Health/Mana/Stamina/Poise a sus máximos.
    UPROPERTY(EditDefaultsOnly, Category = "Common Class Defaults")
    TSubclassOf<UGameplayEffect> DefaultVitalAttributes;

    // Abilities otorgadas a TODOS los enemigos al inicializarse (GA_HitReact, etc.)
    UPROPERTY(EditDefaultsOnly, Category = "Common Class Defaults")
    TArray<TSubclassOf<UGameplayAbility>> CommonAbilities;

    // Curve Table con los coeficientes escalables por nivel usados en ExecCalc_Damage.
    // Curvas incluidas:
    //   - ArmorPenetration: escala cuánto % de Armor ignora cada punto de ArmorPen
    //   - EffectiveArmor: escala cuánto % de daño ignora cada punto de Armor efectiva
    // Interpolación: Constant (valores constantes entre niveles — escalón a escalón).
    // Asignar CT_DamageCalculationCoefficients desde el editor.
    UPROPERTY(EditDefaultsOnly, Category = "Damage")
    TObjectPtr<UCurveTable> DamageCalculationCoefficients;

    // Devuelve la configuración para el arquetipo pedido.
    // Usa FindChecked: crashea con mensaje claro si el arquetipo no está registrado.
    FCharacterClassDefaultInfo GetClassDefaultInfo(EPantheliaCharacterClass CharacterClass);
};