// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Abilities/PantheliaGameplayAbility.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "AbilitySystem/PantheliaAttributeSet.h"
#include "AbilitySystem/PantheliaCostAttributeSet.h"
#include "GameplayEffect.h"
#include "PantheliaGameplayTags.h"
#include "PantheliaLogChannels.h"

namespace PantheliaResourceCost
{
	static FGameplayAttribute GetResourceAttribute(
		const EPantheliaResourceCostType ResourceType)
	{
		switch (ResourceType)
		{
		case EPantheliaResourceCostType::Stamina:
			return UPantheliaAttributeSet::GetStaminaAttribute();

		case EPantheliaResourceCostType::Mana:
			return UPantheliaAttributeSet::GetManaAttribute();

		default:
			return FGameplayAttribute();
		}
	}

	static FGameplayTag GetSetByCallerTag(
		const EPantheliaResourceCostType ResourceType)
	{
		const FPantheliaGameplayTags& Tags = FPantheliaGameplayTags::Get();

		switch (ResourceType)
		{
		case EPantheliaResourceCostType::Stamina:
			return Tags.Cost_Stamina;

		case EPantheliaResourceCostType::Mana:
			return Tags.Cost_Mana;

		default:
			return FGameplayTag();
		}
	}

	static const TCHAR* GetResourceName(
		const EPantheliaResourceCostType ResourceType)
	{
		switch (ResourceType)
		{
		case EPantheliaResourceCostType::Stamina:
			return TEXT("Stamina");

		case EPantheliaResourceCostType::Mana:
			return TEXT("Mana");

		default:
			return TEXT("None");
		}
	}

	static void GetGlobalCostModifiers(
		const UAbilitySystemComponent* AbilitySystemComponent,
		const EPantheliaResourceCostType ResourceType,
		float& OutMultiplier,
		float& OutFlatModifier)
	{
		OutMultiplier = 1.f;
		OutFlatModifier = 0.f;

		if (!AbilitySystemComponent)
		{
			return;
		}

		const UPantheliaCostAttributeSet* CostAttributeSet =
			AbilitySystemComponent->GetSet<UPantheliaCostAttributeSet>();
		if (!CostAttributeSet)
		{
			return;
		}

		switch (ResourceType)
		{
		case EPantheliaResourceCostType::Stamina:
			OutMultiplier = CostAttributeSet->GetStaminaCostMultiplier();
			OutFlatModifier = CostAttributeSet->GetStaminaCostFlat();
			break;

		case EPantheliaResourceCostType::Mana:
			OutMultiplier = CostAttributeSet->GetManaCostMultiplier();
			OutFlatModifier = CostAttributeSet->GetManaCostFlat();
			break;

		default:
			break;
		}
	}

	static void AddCostFailureTag(FGameplayTagContainer* OptionalRelevantTags)
	{
		if (OptionalRelevantTags)
		{
			OptionalRelevantTags->AddTag(
				UAbilitySystemGlobals::Get().ActivateFailCostTag);
		}
	}
}

float UPantheliaGameplayAbility::ResolveResourceCost(
	const UAbilitySystemComponent* AbilitySystemComponent,
	const EPantheliaResourceCostType ResourceType,
	const float BaseCost,
	const float AdditionalMultiplier,
	const float AdditionalFlatModifier)
{
	const float SafeBaseCost =
		FMath::IsFinite(BaseCost) ? FMath::Max(0.f, BaseCost) : 0.f;
	const float SafeAdditionalMultiplier =
		FMath::IsFinite(AdditionalMultiplier)
			? FMath::Max(0.f, AdditionalMultiplier)
			: 1.f;
	const float SafeAdditionalFlat =
		FMath::IsFinite(AdditionalFlatModifier)
			? AdditionalFlatModifier
			: 0.f;

	float GlobalMultiplier = 1.f;
	float GlobalFlatModifier = 0.f;
	PantheliaResourceCost::GetGlobalCostModifiers(
		AbilitySystemComponent,
		ResourceType,
		GlobalMultiplier,
		GlobalFlatModifier);

	const float SafeGlobalMultiplier =
		FMath::IsFinite(GlobalMultiplier)
			? FMath::Max(0.f, GlobalMultiplier)
			: 1.f;
	const float SafeGlobalFlat =
		FMath::IsFinite(GlobalFlatModifier)
			? GlobalFlatModifier
			: 0.f;

	const float AccumulatedMultiplier =
		FMath::Max(0.f, SafeGlobalMultiplier * SafeAdditionalMultiplier);
	const float AccumulatedFlatModifier =
		SafeGlobalFlat + SafeAdditionalFlat;

	const float ResolvedCost =
		SafeBaseCost * AccumulatedMultiplier + AccumulatedFlatModifier;

	return FMath::IsFinite(ResolvedCost)
		? FMath::Max(0.f, ResolvedCost)
		: SafeBaseCost;
}

bool UPantheliaGameplayAbility::ApplyResolvedResourceCostSpec(
	UAbilitySystemComponent* AbilitySystemComponent,
	const EPantheliaResourceCostType ResourceType,
	const float FinalCost,
	FGameplayEffectSpecHandle& CostSpec,
	float* OutResourceBefore,
	float* OutResourceAfter)
{
	const FGameplayAttribute ResourceAttribute =
		PantheliaResourceCost::GetResourceAttribute(ResourceType);
	const FGameplayTag SetByCallerTag =
		PantheliaResourceCost::GetSetByCallerTag(ResourceType);

	if (!AbilitySystemComponent ||
		!ResourceAttribute.IsValid() ||
		!SetByCallerTag.IsValid() ||
		!CostSpec.IsValid() ||
		!CostSpec.Data.IsValid() ||
		!CostSpec.Data->Def ||
		CostSpec.Data->Def->DurationPolicy != EGameplayEffectDurationType::Instant)
	{
		return false;
	}

	const float SafeFinalCost =
		FMath::IsFinite(FinalCost) ? FMath::Max(0.f, FinalCost) : 0.f;
	const float ResourceBefore =
		AbilitySystemComponent->GetNumericAttribute(ResourceAttribute);

	if (OutResourceBefore)
	{
		*OutResourceBefore = ResourceBefore;
	}

	if (SafeFinalCost <= 0.f)
	{
		if (OutResourceAfter)
		{
			*OutResourceAfter = ResourceBefore;
		}
		return true;
	}

	// Ruta recomendada para todos los GEs nuevos. La magnitud viaja negativa
	// porque el GE usa una operación Add sobre Stamina o Mana.
	CostSpec.Data->SetSetByCallerMagnitude(
		SetByCallerTag,
		-SafeFinalCost);

	AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(
		*CostSpec.Data.Get());

	const float ResourceAfterEffect =
		AbilitySystemComponent->GetNumericAttribute(ResourceAttribute);
	const float DesiredResourceAfter = ResourceBefore - SafeFinalCost;
	const float ResourceCorrection =
		DesiredResourceAfter - ResourceAfterEffect;

	// Compatibilidad de migración:
	//   - GE SetByCaller correcto -> ResourceCorrection ~= 0.
	//   - GE fijo demasiado caro -> ResourceCorrection > 0 y se reembolsa.
	//   - GE fijo barato/sin coste -> ResourceCorrection < 0 y se cobra.
	//   - GE que aumentaba el recurso por error también se corrige exactamente.
	//
	// El GE debe ser Instant y exclusivo de coste. Sus tags auxiliares se conservan,
	// pero no debe modificar otros recursos durante esta migración.
	if (!FMath::IsNearlyZero(ResourceCorrection, KINDA_SMALL_NUMBER))
	{
		AbilitySystemComponent->ApplyModToAttribute(
			ResourceAttribute,
			EGameplayModOp::Additive,
			ResourceCorrection);
	}

	if (OutResourceAfter)
	{
		*OutResourceAfter =
			AbilitySystemComponent->GetNumericAttribute(ResourceAttribute);
	}

	return true;
}

bool UPantheliaGameplayAbility::BuildResolvedResourceCosts(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	TArray<FResolvedResourceCost>& OutResolvedCosts,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	OutResolvedCosts.Reset();

	const UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!AbilitySystemComponent)
	{
		PantheliaResourceCost::AddCostFailureTag(OptionalRelevantTags);
		return false;
	}

	bool bHasStaminaCost = false;
	bool bHasManaCost = false;

	auto TryRegisterResource = [this, &bHasStaminaCost, &bHasManaCost](
		const EPantheliaResourceCostType ResourceType,
		const FString& SourceName) -> bool
	{
		bool* bAlreadyConfigured = nullptr;
		switch (ResourceType)
		{
		case EPantheliaResourceCostType::Stamina:
			bAlreadyConfigured = &bHasStaminaCost;
			break;

		case EPantheliaResourceCostType::Mana:
			bAlreadyConfigured = &bHasManaCost;
			break;

		default:
			UE_LOG(LogPanthelia, Error,
				TEXT("[RESOURCE COST] Ability='%s' tiene Resource=None en %s."),
				*GetName(),
				*SourceName);
			return false;
		}

		if (*bAlreadyConfigured)
		{
			UE_LOG(LogPanthelia, Error,
				TEXT("[RESOURCE COST] Ability='%s' repite el recurso %s en %s. Cada recurso debe configurarse una sola vez."),
				*GetName(),
				PantheliaResourceCost::GetResourceName(ResourceType),
				*SourceName);
			return false;
		}

		*bAlreadyConfigured = true;
		return true;
	};

	auto TryAddResolvedCost = [
		this,
		AbilitySystemComponent,
		&OutResolvedCosts,
		&TryRegisterResource,
		OptionalRelevantTags](
			const EPantheliaResourceCostType ResourceType,
			const float BaseCost,
			const TSubclassOf<UGameplayEffect> InCostGameplayEffectClass,
			const FString& SourceName) -> bool
	{
		if (!TryRegisterResource(ResourceType, SourceName))
		{
			PantheliaResourceCost::AddCostFailureTag(OptionalRelevantTags);
			return false;
		}

		const FGameplayAttribute ResourceAttribute =
			PantheliaResourceCost::GetResourceAttribute(ResourceType);
		if (!ResourceAttribute.IsValid())
		{
			UE_LOG(LogPanthelia, Error,
				TEXT("[RESOURCE COST] Ability='%s' no pudo resolver el atributo para %s."),
				*GetName(),
				PantheliaResourceCost::GetResourceName(ResourceType));

			PantheliaResourceCost::AddCostFailureTag(OptionalRelevantTags);
			return false;
		}

		if (!FMath::IsFinite(BaseCost))
		{
			UE_LOG(LogPanthelia, Error,
				TEXT("[RESOURCE COST] Ability='%s' tiene un coste base no finito en %s."),
				*GetName(),
				*SourceName);

			PantheliaResourceCost::AddCostFailureTag(OptionalRelevantTags);
			return false;
		}

		float AdditionalMultiplier = 1.f;
		float AdditionalFlatModifier = 0.f;
		GetAdditionalResourceCostModifiers(
			ResourceType,
			AdditionalMultiplier,
			AdditionalFlatModifier);

		float GlobalMultiplier = 1.f;
		float GlobalFlatModifier = 0.f;
		PantheliaResourceCost::GetGlobalCostModifiers(
			AbilitySystemComponent,
			ResourceType,
			GlobalMultiplier,
			GlobalFlatModifier);

		const float SafeGlobalMultiplier =
			FMath::IsFinite(GlobalMultiplier)
				? FMath::Max(0.f, GlobalMultiplier)
				: 1.f;
		const float SafeAdditionalMultiplier =
			FMath::IsFinite(AdditionalMultiplier)
				? FMath::Max(0.f, AdditionalMultiplier)
				: 1.f;
		const float SafeGlobalFlat =
			FMath::IsFinite(GlobalFlatModifier)
				? GlobalFlatModifier
				: 0.f;
		const float SafeAdditionalFlat =
			FMath::IsFinite(AdditionalFlatModifier)
				? AdditionalFlatModifier
				: 0.f;

		FResolvedResourceCost ResolvedCost;
		ResolvedCost.ResourceType = ResourceType;
		ResolvedCost.BaseCost = FMath::Max(0.f, BaseCost);
		ResolvedCost.AccumulatedMultiplier = FMath::Max(
			0.f,
			SafeGlobalMultiplier * SafeAdditionalMultiplier);
		ResolvedCost.AccumulatedFlatModifier =
			SafeGlobalFlat + SafeAdditionalFlat;
		ResolvedCost.FinalCost = ResolveResourceCost(
			AbilitySystemComponent,
			ResourceType,
			ResolvedCost.BaseCost,
			SafeAdditionalMultiplier,
			SafeAdditionalFlat);
		ResolvedCost.CostGameplayEffectClass = InCostGameplayEffectClass;

		if (ResolvedCost.FinalCost > 0.f)
		{
			if (!ResolvedCost.CostGameplayEffectClass)
			{
				UE_LOG(LogPanthelia, Error,
					TEXT("[RESOURCE COST] Ability='%s' usa coste dinámico de %s en %s pero no tiene Cost Gameplay Effect."),
					*GetName(),
					PantheliaResourceCost::GetResourceName(ResourceType),
					*SourceName);

				PantheliaResourceCost::AddCostFailureTag(OptionalRelevantTags);
				return false;
			}

			const UGameplayEffect* CostGameplayEffect =
				ResolvedCost.CostGameplayEffectClass->GetDefaultObject<UGameplayEffect>();
			if (!CostGameplayEffect ||
				CostGameplayEffect->DurationPolicy != EGameplayEffectDurationType::Instant)
			{
				UE_LOG(LogPanthelia, Error,
					TEXT("[RESOURCE COST] Ability='%s' requiere un Cost Gameplay Effect Instant para %s en %s."),
					*GetName(),
					PantheliaResourceCost::GetResourceName(ResourceType),
					*SourceName);

				PantheliaResourceCost::AddCostFailureTag(OptionalRelevantTags);
				return false;
			}
		}

		const float CurrentResource =
			AbilitySystemComponent->GetNumericAttribute(ResourceAttribute);
		if (CurrentResource + KINDA_SMALL_NUMBER < ResolvedCost.FinalCost)
		{
			PantheliaResourceCost::AddCostFailureTag(OptionalRelevantTags);
			return false;
		}

		OutResolvedCosts.Add(ResolvedCost);
		return true;
	};

	float PrimaryBaseCost = 0.f;
	if (!TryGetBaseResourceCost(Handle, ActorInfo, PrimaryBaseCost))
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("[RESOURCE COST] Ability='%s' no pudo resolver su coste base principal."),
			*GetName());

		PantheliaResourceCost::AddCostFailureTag(OptionalRelevantTags);
		return false;
	}

	UGameplayEffect* PrimaryCostGameplayEffect = GetCostGameplayEffect();
	const TSubclassOf<UGameplayEffect> PrimaryCostGameplayEffectClass =
		PrimaryCostGameplayEffect ? PrimaryCostGameplayEffect->GetClass() : nullptr;

	if (!TryAddResolvedCost(
		ResourceCostType,
		PrimaryBaseCost,
		PrimaryCostGameplayEffectClass,
		TEXT("PrimaryResourceCost")))
	{
		return false;
	}

	const float AbilityLevel = GetAbilityLevel(Handle, ActorInfo);
	for (int32 CostIndex = 0; CostIndex < AdditionalResourceCosts.Num(); ++CostIndex)
	{
		const FPantheliaAdditionalResourceCost& AdditionalCost =
			AdditionalResourceCosts[CostIndex];
		const float EvaluatedBaseCost =
			AdditionalCost.BaseCost.GetValueAtLevel(AbilityLevel);
		const FString SourceName = FString::Printf(
			TEXT("AdditionalResourceCosts[%d]"),
			CostIndex);

		if (!TryAddResolvedCost(
			AdditionalCost.ResourceType,
			EvaluatedBaseCost,
			AdditionalCost.CostGameplayEffectClass,
			SourceName))
		{
			return false;
		}
	}

	return true;
}

bool UPantheliaGameplayAbility::CheckCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	FGameplayTagContainer* OptionalRelevantTags) const
{
	if (!bUsePantheliaResourceCost)
	{
		return Super::CheckCost(
			Handle,
			ActorInfo,
			OptionalRelevantTags);
	}

	TArray<FResolvedResourceCost> ResolvedCosts;
	return BuildResolvedResourceCosts(
		Handle,
		ActorInfo,
		ResolvedCosts,
		OptionalRelevantTags);
}

void UPantheliaGameplayAbility::ApplyCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	const FGameplayAbilityActivationInfo ActivationInfo) const
{
	if (!bUsePantheliaResourceCost)
	{
		Super::ApplyCost(Handle, ActorInfo, ActivationInfo);
		return;
	}

	UAbilitySystemComponent* AbilitySystemComponent =
		ActorInfo ? ActorInfo->AbilitySystemComponent.Get() : nullptr;
	if (!AbilitySystemComponent)
	{
		return;
	}

	// Revalidamos todos los recursos antes de construir o aplicar ningún spec. En el
	// proyecto single-player esto vuelve atómico el CommitAbility desde la perspectiva
	// del jugador: si falta Mana o Stamina, no se cobra ninguno de los dos.
	TArray<FResolvedResourceCost> ResolvedCosts;
	if (!BuildResolvedResourceCosts(
		Handle,
		ActorInfo,
		ResolvedCosts,
		nullptr))
	{
		return;
	}

	struct FPreparedResourceCost
	{
		FResolvedResourceCost ResolvedCost;
		FGameplayEffectSpecHandle CostSpec;
		float ResourceBefore = 0.f;
		float ResourceAfter = 0.f;
	};

	TArray<FPreparedResourceCost> PreparedCosts;
	PreparedCosts.Reserve(ResolvedCosts.Num());

	// Todos los specs se construyen antes del primer cobro. Una clase inválida no puede
	// dejar el hechizo a medio pagar.
	for (const FResolvedResourceCost& ResolvedCost : ResolvedCosts)
	{
		if (ResolvedCost.FinalCost <= 0.f)
		{
			if (bLogResolvedResourceCost)
			{
				UE_LOG(LogPanthelia, Log,
					TEXT("[RESOURCE COST] Ability='%s' Resource=%s Base=%.2f Mult=%.3f Flat=%.2f Final=0.00"),
					*GetName(),
					PantheliaResourceCost::GetResourceName(ResolvedCost.ResourceType),
					ResolvedCost.BaseCost,
					ResolvedCost.AccumulatedMultiplier,
					ResolvedCost.AccumulatedFlatModifier);
			}
			continue;
		}

		FPreparedResourceCost& PreparedCost =
			PreparedCosts.AddDefaulted_GetRef();
		PreparedCost.ResolvedCost = ResolvedCost;
		PreparedCost.CostSpec = MakeOutgoingGameplayEffectSpec(
			Handle,
			ActorInfo,
			ActivationInfo,
			ResolvedCost.CostGameplayEffectClass,
			GetAbilityLevel(Handle, ActorInfo));

		if (!PreparedCost.CostSpec.IsValid() ||
			!PreparedCost.CostSpec.Data.IsValid())
		{
			UE_LOG(LogPanthelia, Error,
				TEXT("[RESOURCE COST] No se pudo construir el spec de %s para '%s'. No se aplicó ningún coste."),
				PantheliaResourceCost::GetResourceName(ResolvedCost.ResourceType),
				*GetName());
			return;
		}
	}

	int32 AppliedCostCount = 0;
	for (FPreparedResourceCost& PreparedCost : PreparedCosts)
	{
		if (!ApplyResolvedResourceCostSpec(
			AbilitySystemComponent,
			PreparedCost.ResolvedCost.ResourceType,
			PreparedCost.ResolvedCost.FinalCost,
			PreparedCost.CostSpec,
			&PreparedCost.ResourceBefore,
			&PreparedCost.ResourceAfter))
		{
			// Ruta defensiva para una anomalía inesperada durante Apply. Los specs ya fueron
			// validados, pero restauramos cualquier recurso cobrado antes del fallo para que
			// nunca quede un pago parcial de Mana o Stamina.
			for (int32 RollbackIndex = AppliedCostCount - 1;
				RollbackIndex >= 0;
				--RollbackIndex)
			{
				const FPreparedResourceCost& AppliedCost =
					PreparedCosts[RollbackIndex];
				const FGameplayAttribute ResourceAttribute =
					PantheliaResourceCost::GetResourceAttribute(
						AppliedCost.ResolvedCost.ResourceType);
				if (!ResourceAttribute.IsValid())
				{
					continue;
				}

				const float CurrentResource =
					AbilitySystemComponent->GetNumericAttribute(ResourceAttribute);
				const float RollbackAmount =
					AppliedCost.ResourceBefore - CurrentResource;
				if (!FMath::IsNearlyZero(RollbackAmount, KINDA_SMALL_NUMBER))
				{
					AbilitySystemComponent->ApplyModToAttribute(
						ResourceAttribute,
						EGameplayModOp::Additive,
						RollbackAmount);
				}
			}

			UE_LOG(LogPanthelia, Error,
				TEXT("[RESOURCE COST] Falló la aplicación de %s para '%s'. Se revirtieron los costes anteriores."),
				PantheliaResourceCost::GetResourceName(
					PreparedCost.ResolvedCost.ResourceType),
				*GetName());
			return;
		}

		++AppliedCostCount;

		if (bLogResolvedResourceCost)
		{
			UE_LOG(LogPanthelia, Log,
				TEXT("[RESOURCE COST] Ability='%s' Resource=%s Base=%.2f Mult=%.3f Flat=%.2f Final=%.2f Before=%.2f After=%.2f"),
				*GetName(),
				PantheliaResourceCost::GetResourceName(
					PreparedCost.ResolvedCost.ResourceType),
				PreparedCost.ResolvedCost.BaseCost,
				PreparedCost.ResolvedCost.AccumulatedMultiplier,
				PreparedCost.ResolvedCost.AccumulatedFlatModifier,
				PreparedCost.ResolvedCost.FinalCost,
				PreparedCost.ResourceBefore,
				PreparedCost.ResourceAfter);
		}
	}
}

bool UPantheliaGameplayAbility::TryGetBaseResourceCost(
	const FGameplayAbilitySpecHandle Handle,
	const FGameplayAbilityActorInfo* ActorInfo,
	float& OutBaseCost) const
{
	const float EvaluatedBaseCost = BaseResourceCost.GetValueAtLevel(
		GetAbilityLevel(Handle, ActorInfo));

	if (!FMath::IsFinite(EvaluatedBaseCost))
	{
		OutBaseCost = 0.f;
		return false;
	}

	OutBaseCost = FMath::Max(0.f, EvaluatedBaseCost);
	return true;
}

void UPantheliaGameplayAbility::GetAdditionalResourceCostModifiers(
	const EPantheliaResourceCostType ResourceType,
	float& OutMultiplier,
	float& OutFlatModifier) const
{
	// La clase base no aplica reglas de categoría. ResourceType se expone al override
	// para que una subclase futura pueda modificar Mana y Stamina de forma diferente.
	(void)ResourceType;
	OutMultiplier = 1.f;
	OutFlatModifier = 0.f;
}
