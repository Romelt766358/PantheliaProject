// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "ComboWindowNotifyState.generated.h"

/**
 * UComboWindowNotifyState
 *
 * AnimNotifyState que marca la VENTANA DE COMBO dentro de un montage de ataque.
 * Mientras la ventana está abierta, la ability de ataque del jugador acepta el
 * input de "siguiente golpe" en su buffer (modelo Dark Souls clásico: pulsas
 * durante el swing y el encadenado sale al cerrarse la ventana).
 *
 * Colocación en el montage: cubre el tramo del swing donde tiene sentido encadenar
 * (normalmente desde que la hoja conecta hasta poco antes del final de la animación).
 *
 * Funcionamiento:
 *   - NotifyBegin → abre la ventana en la ability (OpenComboWindow).
 *   - NotifyEnd   → cierra la ventana en la ability (CloseComboWindow). Ahí la
 *                   ability decide: si hubo input buffered, encadena el siguiente
 *                   golpe; si no, resetea el combo y termina.
 *
 * Busca la ability de ataque activa en el ASC del personaje que reproduce el montage.
 */
UCLASS()
class PANTHELIAPROJECT_API UComboWindowNotifyState : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		float TotalDuration, const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
