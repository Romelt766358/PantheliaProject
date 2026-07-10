// Fill out your copyright notice in the Description page of Project Settings.

#include "Animations/WeaponTraceNotifyState.h"
#include "Combat/WeaponTraceComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"

void UWeaponTraceNotifyState::NotifyBegin(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	float TotalDuration,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyBegin(MeshComp, Animation, TotalDuration, EventReference);

	if (!MeshComp) return;

	// El actor dueño del mesh es el enemigo (o quien sea que ataca).
	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;

	// Buscamos el componente de trace y abrimos la ventana de daño.
	if (UWeaponTraceComponent* TraceComp = Owner->FindComponentByClass<UWeaponTraceComponent>())
	{
		if (bUseTraceRadiusOverride)
		{
			TraceComp->ActivateTraceWithRadius(OverrideTraceRadius);
		}
		else
		{
			TraceComp->ActivateTrace();
		}
	}
}

void UWeaponTraceNotifyState::NotifyEnd(
	USkeletalMeshComponent* MeshComp,
	UAnimSequenceBase* Animation,
	const FAnimNotifyEventReference& EventReference)
{
	Super::NotifyEnd(MeshComp, Animation, EventReference);

	if (!MeshComp) return;

	AActor* Owner = MeshComp->GetOwner();
	if (!Owner) return;

	// Cerramos la ventana de daño y limpiamos la lista de ignorados del swing.
	if (UWeaponTraceComponent* TraceComp = Owner->FindComponentByClass<UWeaponTraceComponent>())
	{
		TraceComp->DeactivateTrace();
	}
}
