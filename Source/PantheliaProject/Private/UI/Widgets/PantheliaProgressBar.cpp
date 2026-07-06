// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/Widgets/PantheliaProgressBar.h"
#include "Components/Image.h"
#include "Components/SizeBox.h"
#include "Materials/MaterialInstanceDynamic.h"

void UPantheliaProgressBar::NativeConstruct()
{
	Super::NativeConstruct();

	if (!HealthBarImage) { return; }

	BarMID = HealthBarImage->GetDynamicMaterial();

	SetMaterialParameter(CurrentHealthParamName, 0.0f);
	SetMaterialParameter(PreviousHealthParamName, 0.0f);
}

void UPantheliaProgressBar::SetProgressBarPercent(float Percentage)
{
	// Si el valor no cambió, ignoramos la llamada completamente.
	// Esto evita que llamadas duplicadas del mismo tick destruyan el CachedPercent.
	if (FMath::IsNearlyEqual(Percentage, CurrentPercent)) { return; }

	// Durante la inicialización, GAS dispara múltiples delegates mientras
	// aplica los GEs primarios y secundarios. En ese período, sincronizamos
	// Current y Cached al mismo valor para evitar ghost bars espurias.
	// Consideramos la barra inicializada cuando recibimos un valor positivo
	// DESPUÉS de que ya teníamos algún valor, es decir, cuando el sistema
	// ya terminó de configurarse y empieza a recibir cambios reales.
	if (!bGlobeInitialized)
	{
		CurrentPercent = Percentage;
		CachedPercent = Percentage;
		SetMaterialParameter(CurrentHealthParamName, Percentage);
		SetMaterialParameter(PreviousHealthParamName, Percentage);

		if (Percentage > 0.0f)
		{
			bGlobeInitialized = true;
		}
		return;
	}

	const bool bValueDecreased = Percentage < CurrentPercent;

	CachedPercent = CurrentPercent;
	CurrentPercent = Percentage;

	// La barra de color salta inmediatamente al nuevo valor
	SetMaterialParameter(CurrentHealthParamName, Percentage);

	if (bValueDecreased)
	{
		// El valor bajó: mostramos la ghost bar desde donde estaba.
		// PreviousHealth se irá interpolando hacia CurrentHealth via LerpCachedValue.
		SetMaterialParameter(PreviousHealthParamName, CachedPercent);
		if (GhostAnimation)
		{
			PlayAnimation(GhostAnimation);
		}
	}
	else
	{
		// El valor subió (curación): sincronizamos PreviousHealth con el nuevo valor
		// para que la máscara del material cubra la barra completa hasta CurrentHealth.
		// Sin esto, PreviousHealth queda en el valor anterior y la máscara clipea
		// la barra roja al valor antiguo aunque CurrentHealth sea mayor.
		CachedPercent = Percentage;
		SetMaterialParameter(PreviousHealthParamName, Percentage);
	}
}

void UPantheliaProgressBar::ForceSetPercent(float Percentage)
{
	// Actualiza el porcentaje sin activar la ghost bar y sin ser
	// bloqueada por el check de igualdad. Úsala cuando el MÁXIMO cambia
	// (ej. MaxHealth sube por equipamiento) para que la barra refleje
	// el nuevo ratio correctamente.
	CurrentPercent = Percentage;
	CachedPercent = Percentage;
	SetMaterialParameter(CurrentHealthParamName, Percentage);
	SetMaterialParameter(PreviousHealthParamName, Percentage);

	if (Percentage > 0.0f)
	{
		bGlobeInitialized = true;
	}
}

void UPantheliaProgressBar::LerpCachedValue(float Progress)
{
	const UWorld* World = GetWorld();
	if (!World) { return; }

	const float DeltaTime = World->GetDeltaSeconds();

	CachedPercent = FMath::FInterpTo(
		CachedPercent,
		CurrentPercent,
		DeltaTime,
		InterpSpeed
	);

	SetMaterialParameter(PreviousHealthParamName, CachedPercent);
}

void UPantheliaProgressBar::UpdateBoxSize()
{
	if (!SizeBox_Root) { return; }
	SizeBox_Root->SetWidthOverride(MaxValue * PixelsPerPoint);
	SizeBox_Root->SetHeightOverride(BarHeight);
}

void UPantheliaProgressBar::SetMaterialParameter(const FName& ParameterName, float Value)
{
	if (!BarMID) { return; }
	BarMID->SetScalarParameterValue(ParameterName, Value);
}