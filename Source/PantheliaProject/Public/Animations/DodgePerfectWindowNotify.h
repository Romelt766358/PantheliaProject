// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "DodgePerfectWindowNotify.generated.h"

/**
 * UDodgePerfectWindowNotify
 *
 * Solicita el INICIO de la ventana estrecha de Perfect Dodge. Es un notify puntual:
 * la duración real vive en GA_PlayerDodge y puede escalar por nivel, árbol o
 * Corazones. La ability garantiza que esta ventana nunca exista fuera de los
 * i-frames, incluso si comparte timestamp con Dodge I-Frames Start.
 */
UCLASS(meta = (DisplayName = "Dodge Perfect Window Start"))
class PANTHELIAPROJECT_API UDodgePerfectWindowNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
