#include "AbilitySystem/MMC_MaxHealth.h"
#include "AbilitySystem/PantheliaAttributeSet.h"
#include "Interfaces/CombatInterface.h"

UMMC_MaxHealth::UMMC_MaxHealth()
{
	// Configuramos la captura de Resilience:
	// - Queremos el atributo Resilience del AttributeSet
	// - Lo capturamos del Target (el personaje que recibe el GE)
	// - No lo capturamos como snapshot (queremos el valor actual, no el del momento de creación)
	ResilienceDef.AttributeToCapture = UPantheliaAttributeSet::GetResilienceAttribute();
	ResilienceDef.AttributeSource = EGameplayEffectAttributeCaptureSource::Target;
	ResilienceDef.bSnapshot = false;

	// Registramos la captura para que GAS sepa que la necesitamos
	RelevantAttributesToCapture.Add(ResilienceDef);
}

float UMMC_MaxHealth::CalculateBaseMagnitude_Implementation(const FGameplayEffectSpec& Spec) const
{
	// Obtenemos las tags de la fuente y del objetivo para consultas adicionales
	const FGameplayTagContainer* SourceTags = Spec.CapturedSourceTags.GetAggregatedTags();
	const FGameplayTagContainer* TargetTags = Spec.CapturedTargetTags.GetAggregatedTags();

	// FAggregatorEvaluateParameters es el struct que GAS necesita para evaluar capturas
	FAggregatorEvaluateParameters EvaluationParams;
	EvaluationParams.SourceTags = SourceTags;
	EvaluationParams.TargetTags = TargetTags;

	// Obtenemos el valor actual de Resilience del objetivo
	float Resilience = 0.f;
	GetCapturedAttributeMagnitude(ResilienceDef, Spec, EvaluationParams, Resilience);
	Resilience = FMath::Max<float>(Resilience, 0.f); // Nunca negativo

	// Obtenemos el nivel del personaje a través de la CombatInterface
	// Spec.GetContext().GetSourceObject() devuelve el objeto fuente del GE
	// que en nuestro caso es el propio personaje (lo asignamos en ApplyEffectToSelf)
	int32 PlayerLevel = 1;
	if (AActor* SourceActor = Cast<AActor>(Spec.GetContext().GetSourceObject()))
	{
		if (SourceActor->Implements<UCombatInterface>())
		{
			PlayerLevel = ICombatInterface::Execute_GetPlayerLevel(SourceActor);
		}
	}

	// Fórmula: MaxHealth = 80 + (Resilience * 10) + (Level * 10)
	// Ajusta estos valores según el balance que quieras para tu juego
	const float BaseHealth = 80.f + (2.5f * Resilience) + (10.f * PlayerLevel);

	// Escalado de salud para enemigos: los enemigos tienen 25% de la salud base.
	// Esto los hace más fáciles de matar durante el desarrollo y el testeo.
	// Ajustar cuando se quiera balancear el juego en serio.
	// Se usa el Actor Tag "Enemy" para distinguirlos del jugador.
	float EnemyHealthScale = 1.f;
	if (const AActor* SourceActor = Cast<AActor>(Spec.GetContext().GetSourceObject()))
	{
		if (SourceActor->ActorHasTag(FName("Enemy")))
		{
			EnemyHealthScale = 0.25f;
		}
	}

	return BaseHealth * EnemyHealthScale;
}