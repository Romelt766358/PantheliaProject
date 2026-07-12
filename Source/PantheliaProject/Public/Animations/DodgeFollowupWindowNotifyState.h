// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "DodgeFollowupWindowNotifyState.generated.h"

/**
 * UDodgeFollowupWindowNotifyState
 *
 * Marca el tramo final de un montage de dodge en el que el jugador puede introducir
 * un ataque ligero o pesado post-dodge. La ventana es estado interno de GA_Dodge:
 * no concede tags ni activa ataques por sí misma.
 *
 * Colocación recomendada: último tercio del montage, después del desplazamiento
 * comprometido, cerrando con margen antes del final para evitar carreras con
 * OnCompleted. El primer input válido dentro de la ventana gana.
 */
UCLASS()
class PANTHELIAPROJECT_API UDodgeFollowupWindowNotifyState : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
