// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Widgets/PantheliaUserWidget.h"
#include "PantheliaProgressBar.generated.h"

class UImage;
class UWidgetAnimation;
class USizeBox;
class UMaterialInstanceDynamic;

UCLASS(Abstract)
class PANTHELIAPROJECT_API UPantheliaProgressBar : public UPantheliaUserWidget
{
	GENERATED_BODY()

public:

	// Llamada cuando el valor actual cambia (ej. Health baja por daño).
	// Activa la ghost bar si el valor bajó.
	UFUNCTION(BlueprintCallable, Category = "Progress Bar")
	void SetProgressBarPercent(float Percentage);

	// Llamada cuando el valor máximo cambia (ej. MaxHealth cambia por equipamiento).
	// Actualiza el porcentaje visible sin activar la ghost bar.
	// Úsala desde OnMaxHealthChanged después de actualizar MaxValue.
	UFUNCTION(BlueprintCallable, Category = "Progress Bar")
	void ForceSetPercent(float Percentage);

	UFUNCTION(BlueprintCallable, Category = "Progress Bar")
	void LerpCachedValue(float Progress);

	UFUNCTION(BlueprintCallable, Category = "Progress Bar")
	void UpdateBoxSize();

protected:

	virtual void NativeConstruct() override;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<UImage> HealthBarImage;

	UPROPERTY(BlueprintReadOnly, meta = (BindWidget))
	TObjectPtr<USizeBox> SizeBox_Root;

	UPROPERTY(Transient, BlueprintReadWrite, Category = "Progress Bar")
	TObjectPtr<UWidgetAnimation> GhostAnimation;

	UPROPERTY(EditAnywhere, Category = "Material Parameters")
	FName CurrentHealthParamName = FName("CurrentHealth");

	UPROPERTY(EditAnywhere, Category = "Material Parameters")
	FName PreviousHealthParamName = FName("PreviousHealth");

	UPROPERTY(EditAnywhere, Category = "Interpolation")
	float InterpSpeed = 10.0f;

	UPROPERTY(BlueprintReadWrite, Category = "Progress Bar")
	float MaxValue = 1.0f;

	UPROPERTY(EditAnywhere, Category = "Progress Bar")
	float PixelsPerPoint = 2.0f;

	UPROPERTY(EditAnywhere, Category = "Progress Bar")
	float BarHeight = 25.0f;

private:

	void SetMaterialParameter(const FName& ParameterName, float Value);

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> BarMID;

	float CurrentPercent = 0.0f;
	float CachedPercent = 0.0f;

	// Indica si la barra ya recibió su primer valor real válido.
	// Mientras sea false, todas las llamadas sincronizan Current y Cached
	// sin activar la ghost bar, independientemente de cuántos atributos
	// o GEs se inicialicen. Esto hace el sistema completamente escalable.
	bool bGlobeInitialized = false;
};