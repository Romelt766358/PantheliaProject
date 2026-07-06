// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotifyState.h"
#include "WeaponTraceNotifyState.generated.h"

/**
 * UWeaponTraceNotifyState
 *
 * Notify State que marca la ventana de daño de un ataque melee en un montage.
 * Colócalo en la pista de animación justo sobre los frames en los que la hoja
 * del arma debería poder golpear (el arco del tajo).
 *
 * - NotifyBegin: busca el UWeaponTraceComponent del actor dueño del mesh y
 *   llama ActivateTrace() — abre la ventana de sweep.
 * - NotifyEnd: llama DeactivateTrace() — cierra la ventana y limpia la lista
 *   de actores golpeados de este swing.
 *
 * Reemplaza al AN_MontageEvent puntual del curso para el melee de enemigos.
 * Ver State_Combat.md §9.
 */
UCLASS()
class PANTHELIAPROJECT_API UWeaponTraceNotifyState : public UAnimNotifyState
{
	GENERATED_BODY()

public:
	virtual void NotifyBegin(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		float TotalDuration,
		const FAnimNotifyEventReference& EventReference
	) override;

	virtual void NotifyEnd(
		USkeletalMeshComponent* MeshComp,
		UAnimSequenceBase* Animation,
		const FAnimNotifyEventReference& EventReference
	) override;
};