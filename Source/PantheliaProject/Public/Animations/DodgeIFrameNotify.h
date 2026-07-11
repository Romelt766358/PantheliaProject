// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "DodgeIFrameNotify.generated.h"

/**
 * UDodgeIFrameNotify
 *
 * Marca el INICIO de los i-frames dentro del montage de dash. Es un notify puntual,
 * no un NotifyState: la duración real viene de GA_PlayerDodge y puede escalar por
 * nivel/árbol/Corazones sin tener que redimensionar la ventana en cada montage.
 */
UCLASS(meta = (DisplayName = "Dodge I-Frames Start"))
class PANTHELIAPROJECT_API UDodgeIFrameNotify : public UAnimNotify
{
	GENERATED_BODY()

public:
	virtual void Notify(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference) override;
};
