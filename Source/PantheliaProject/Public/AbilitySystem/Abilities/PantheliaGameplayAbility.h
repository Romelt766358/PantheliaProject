// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "PantheliaGameplayAbility.generated.h"

/**
 * EPantheliaAbilityInputActivationPolicy
 *
 * Define cómo responde una ability al InputTag que tiene asignado:
 *   - OnInputTriggered: una pulsación real produce un único intento de activación.
 *   - WhileInputActive: se vuelve a intentar mientras el input permanezca presionado.
 *
 * Todas las abilities actuales de Panthelia usan OnInputTriggered. WhileInputActive
 * queda reservado para canalizaciones o acciones continuas futuras. El tap-vs-hold
 * del ataque pesado sigue siendo lógica interna de esa ability, no una política de
 * activación repetida.
 */
UENUM(BlueprintType)
enum class EPantheliaAbilityInputActivationPolicy : uint8
{
	OnInputTriggered,
	WhileInputActive
};

/**
 * FAbilityAttributeScaling
 *
 * Define un escalado opcional de daño basado en un atributo secundario o vital del caster.
 * Modelo LoL: cada habilidad puede escalar con 1-2 atributos del propio personaje.
 *
 * Ejemplo: Firebolt con +50% MagicDamage
 *   AttributeTag = Attributes.Secondary.MagicDamage
 *   Ratio        = 0.5
 *
 * REGLAS (diseño, no técnicas):
 *   - Máximo 2 entradas por habilidad.
 *   - Solo atributos SECUNDARIOS y VITALES del caster (no primarios).
 *   - Los vitales (Health, Mana, Stamina) habilitan habilidades cuyo daño depende
 *     del estado actual del personaje.
 *   - Los ratios son fijos (no escalan por nivel — la curva de daño base ya lo hace).
 *
 * MODELO DE ESCALADO (estilo LoL):
 *   - Ningún atributo ofensivo se suma automáticamente dentro de ExecCalc_Damage.
 *   - Cada fuente de daño declara explícitamente sus ratios en AttributeScalings.
 *   - Fórmula bruta: DañoBase + Σ(Ratio × AtributoDelCaster).
 *   - Ejemplo de arma rápida: PhysicalDamage con Ratio=0.7.
 *   - Ejemplo de arma lenta: PhysicalDamage con Ratio=2.1.
 *
 * Esta política evita dobles conteos y permite que dos armas con el mismo daño base
 * aprovechen PhysicalDamage de forma completamente distinta sin tocar el ExecCalc.
 */
USTRUCT(BlueprintType)
struct FAbilityAttributeScaling
{
	GENERATED_BODY()

	// Tag del atributo del caster que escala el daño.
	// Ej: Attributes.Secondary.MagicDamage, Attributes.Vital.Health
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scaling")
	FGameplayTag AttributeTag;

	// Fracción del atributo que se añade como daño extra.
	// 0.5 = 50% del atributo. 1.0 = 100%.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Scaling", meta = (ClampMin = "0.0"))
	float Ratio = 0.f;
};

/**
 * UPantheliaGameplayAbility
 *
 * Clase base de todas las abilities del proyecto.
 * Solo contiene lo común a TODAS las abilities: el tag de input.
 *
 * Las abilities que infligen daño heredan de UPantheliaDamageGameplayAbility,
 * que añade DamageTypes (TMap de tipo→curva), PoiseDamage, AttributeScalings y DamageEffectClass.
 */
UCLASS()
class PANTHELIAPROJECT_API UPantheliaGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:

	// Input tag con el que esta ability empieza el juego asociada.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	FGameplayTag StartupInputTag;

	// Política de activación asociada al input. Por defecto, una pulsación real genera
	// un único intento de activación y mantener el botón NO repite la ability.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	EPantheliaAbilityInputActivationPolicy InputActivationPolicy =
		EPantheliaAbilityInputActivationPolicy::OnInputTriggered;
};
