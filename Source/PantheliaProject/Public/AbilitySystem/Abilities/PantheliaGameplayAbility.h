// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Abilities/GameplayAbility.h"
#include "GameplayTagContainer.h"
#include "ScalableFloat.h"
#include "PantheliaGameplayAbility.generated.h"

class UAbilitySystemComponent;
class UGameplayEffect;

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
 * EPantheliaResourceCostType
 *
 * Recurso que consume el coste común de Panthelia. None conserva el comportamiento
 * nativo de UGameplayAbility y permite que una ability enemiga o gratuita no entre
 * en el pipeline dinámico.
 */
UENUM(BlueprintType)
enum class EPantheliaResourceCostType : uint8
{
	None UMETA(DisplayName = "None"),
	Stamina UMETA(DisplayName = "Stamina"),
	Mana UMETA(DisplayName = "Mana")
};

/**
 * FPantheliaAdditionalResourceCost
 *
 * Componente de coste adicional aplicado por el mismo CommitAbility. Permite que una
 * ability consuma varios recursos de forma conjunta sin duplicar lógica en Blueprint.
 *
 * Ejemplo de hechizo del jugador:
 *   - Coste principal: Mana (propiedades heredadas de UGameplayAbility).
 *   - AdditionalResourceCosts[0]: Stamina.
 *
 * REGLAS:
 *   - ResourceType no puede ser None.
 *   - No puede repetir el recurso principal ni otro coste adicional.
 *   - El GameplayEffect debe ser Instant y modificar únicamente su recurso.
 *   - Si falta cualquiera de los recursos, CheckCost falla antes de cobrar nada.
 */
USTRUCT(BlueprintType)
struct FPantheliaAdditionalResourceCost
{
	GENERATED_BODY()

	// Recurso adicional consumido por la ability.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cost")
	EPantheliaResourceCostType ResourceType =
		EPantheliaResourceCostType::None;

	// Coste base por nivel de ability antes de multiplicadores y modificadores planos.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cost",
		meta = (ClampMin = "0.0"))
	FScalableFloat BaseCost;

	// GE Instant del recurso. Debe usar SetByCaller Cost.Stamina o Cost.Mana.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cost")
	TSubclassOf<UGameplayEffect> CostGameplayEffectClass;
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
 * Contiene el input común y el resolvedor compartido de costes de recursos.
 *
 * MODELO DE COSTE:
 *   CosteFinal = max(0, CosteBase × MultiplicadorAcumulado + ModificadorPlanoAcumulado)
 *
 * Los acumuladores viven como atributos GAS en UPantheliaCostAttributeSet. Por tanto,
 * árbol, equipamiento, Corazones y buffs pueden modificarlos con GameplayEffects
 * Infinite/Duration sin que las abilities conozcan el origen de cada modificación.
 *
 * Una ability puede conservar el Cost Gameplay Effect legacy dejando
 * bUsePantheliaResourceCost=false. Al activar el pipeline nuevo, el coste principal y
 * todos los costes adicionales se comprueban primero; solo si todos son pagables se
 * aplican dentro del mismo CommitAbility. El coste base se inyecta por SetByCaller y,
 * como red de compatibilidad, se corrige cualquier diferencia que deje un GE antiguo
 * con magnitud fija. Esto permite migrar assets por etapas.
 *
 * Las abilities que infligen daño heredan de UPantheliaDamageGameplayAbility,
 * que añade DamageTypes (TMap de tipo→curva), PoiseDamage, AttributeScalings y DamageEffectClass.
 */
UCLASS()
class PANTHELIAPROJECT_API UPantheliaGameplayAbility : public UGameplayAbility
{
	GENERATED_BODY()

public:
#if WITH_EDITOR
	bool IsPantheliaResourceCostEnabledForEditor() const
	{
		return bUsePantheliaResourceCost;
	}

	EPantheliaResourceCostType GetResourceCostTypeForEditor() const
	{
		return ResourceCostType;
	}

	const FScalableFloat& GetBaseResourceCostForEditor() const
	{
		return BaseResourceCost;
	}

	const TArray<FPantheliaAdditionalResourceCost>&
	GetAdditionalResourceCostsForEditor() const
	{
		return AdditionalResourceCosts;
	}

	EPantheliaAbilityInputActivationPolicy
	GetInputActivationPolicyForEditor() const
	{
		return InputActivationPolicy;
	}
#endif

	/**
	 * Resuelve un coste base usando los modificadores GAS activos del recurso.
	 *
	 * AdditionalMultiplier y AdditionalFlatModifier son ganchos para una categoría
	 * concreta (arma, perk especial o regla de la subclase). En la infraestructura
	 * actual se dejan en 1 y 0 salvo que un sistema especializado los necesite.
	 */
	static float ResolveResourceCost(
		const UAbilitySystemComponent* AbilitySystemComponent,
		EPantheliaResourceCostType ResourceType,
		float BaseCost,
		float AdditionalMultiplier = 1.f,
		float AdditionalFlatModifier = 0.f);

	// Input tag con el que esta ability empieza el juego asociada.
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	FGameplayTag StartupInputTag;

	// Política de activación asociada al input. Por defecto, una pulsación real genera
	// un único intento de activación y mantener el botón NO repite la ability.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Input")
	EPantheliaAbilityInputActivationPolicy InputActivationPolicy =
		EPantheliaAbilityInputActivationPolicy::OnInputTriggered;

protected:
	/**
	 * Activa el resolvedor común para esta ability.
	 *
	 * False = GAS usa el Cost Gameplay Effect exactamente como antes.
	 * True  = Panthelia calcula el coste final y lo aplica por el mismo GE.
	 *
	 * Se deja false por defecto para no cambiar silenciosamente Blueprints existentes.
	 * Los ataques de arma lo activan en C++; dodge/parry/hechizos se migran desde sus
	 * Class Defaults copiando su coste base actual.
	 */
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cost|Panthelia")
	bool bUsePantheliaResourceCost = false;

	// Recurso consumido cuando bUsePantheliaResourceCost está activo.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cost|Panthelia",
		meta = (EditCondition = "bUsePantheliaResourceCost"))
	EPantheliaResourceCostType ResourceCostType =
		EPantheliaResourceCostType::None;

	// Coste base por nivel de ability. Las subclases data-driven pueden sobrescribir
	// TryGetBaseResourceCost para obtenerlo de un arma u otra fuente externa.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cost|Panthelia",
		meta = (EditCondition = "bUsePantheliaResourceCost", ClampMin = "0.0"))
	FScalableFloat BaseResourceCost;

	// Costes secundarios cobrados por el mismo CommitAbility. El coste principal
	// continúa usando ResourceCostType + BaseResourceCost + CostGameplayEffectClass
	// heredado de UGameplayAbility para no romper dodge, parry ni ataques existentes.
	//
	// Caso de uso actual: hechizo = Mana principal + Stamina adicional.
	// Cada recurso se resuelve con sus propios atributos globales de coste.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cost|Panthelia",
		meta = (EditCondition = "bUsePantheliaResourceCost", TitleProperty = "ResourceType"))
	TArray<FPantheliaAdditionalResourceCost> AdditionalResourceCosts;

	// Log de diagnóstico opt-in. Útil al validar nuevos GEs del árbol/equipamiento,
	// pero apagado por defecto para no llenar el Output Log en cada golpe.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Cost|Panthelia",
		meta = (EditCondition = "bUsePantheliaResourceCost"))
	bool bLogResolvedResourceCost = false;

	virtual bool CheckCost(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		FGameplayTagContainer* OptionalRelevantTags = nullptr) const override;

	virtual void ApplyCost(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		const FGameplayAbilityActivationInfo ActivationInfo) const override;

	// Aplica un spec Instant ya construido y fuerza que el recurso termine exactamente
	// en Before - FinalCost. Centraliza SetByCaller y la red de migración para GEs
	// legacy con magnitud fija. Devuelve false si el spec o el recurso son inválidos.
	static bool ApplyResolvedResourceCostSpec(
		UAbilitySystemComponent* AbilitySystemComponent,
		EPantheliaResourceCostType ResourceType,
		float FinalCost,
		FGameplayEffectSpecHandle& CostSpec,
		float* OutResourceBefore = nullptr,
		float* OutResourceAfter = nullptr);

	// Fuente extensible del coste base. False significa configuración inválida y bloquea
	// la activación; un coste explícito de cero sigue siendo válido.
	virtual bool TryGetBaseResourceCost(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		float& OutBaseCost) const;

	// Gancho por dominio para reglas futuras más específicas (por ejemplo, una
	// reducción que afecte solo al dodge o solo a una familia de hechizos). Recibe el
	// recurso porque una misma ability puede pagar Mana y Stamina simultáneamente. La
	// base devuelve valores neutros y evita duplicar CheckCost/ApplyCost.
	virtual void GetAdditionalResourceCostModifiers(
		EPantheliaResourceCostType ResourceType,
		float& OutMultiplier,
		float& OutFlatModifier) const;

private:
	struct FResolvedResourceCost
	{
		EPantheliaResourceCostType ResourceType =
			EPantheliaResourceCostType::None;
		float BaseCost = 0.f;
		float AccumulatedMultiplier = 1.f;
		float AccumulatedFlatModifier = 0.f;
		float FinalCost = 0.f;
		TSubclassOf<UGameplayEffect> CostGameplayEffectClass;
	};

	// Construye y valida todos los componentes antes de ApplyCost. Así un hechizo no
	// puede gastar Mana si su Stamina adicional es insuficiente (ni viceversa).
	bool BuildResolvedResourceCosts(
		const FGameplayAbilitySpecHandle Handle,
		const FGameplayAbilityActorInfo* ActorInfo,
		TArray<FResolvedResourceCost>& OutResolvedCosts,
		FGameplayTagContainer* OptionalRelevantTags) const;
};
