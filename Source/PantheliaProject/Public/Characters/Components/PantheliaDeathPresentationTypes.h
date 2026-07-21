#pragma once

#include "CoreMinimal.h"
#include "PantheliaDeathPresentationTypes.generated.h"

class AActor;
class UPrimitiveComponent;

UENUM(BlueprintType)
enum class EPantheliaDeathPresentationState : uint8
{
	Alive,
	DeathRequested,
	GameplayShutdown,
	PresentationStarted,
	PresentationFinished,
	Aborted
};

UENUM(BlueprintType)
enum class EPantheliaDeathVisualPartRole : uint8
{
	AuthoritativeBody,
	PoseFollower,
	Groom,
	DissolveMesh,
	HideOnly,
	WeaponPart,
	Ignore
};

UENUM(BlueprintType)
enum class EPantheliaDeathRagdollPolicy : uint8
{
	None,
	LegacyAuthoritativeBody,
	FutureMultipart
};

UENUM(BlueprintType)
enum class EPantheliaDeathDissolvePolicy : uint8
{
	None,
	LegacyPrimaryMaterial,
	FutureMultipart
};

UENUM(BlueprintType)
enum class EPantheliaDeathVisibilityPolicy : uint8
{
	Preserve,
	HideOnPresentationStart
};

UENUM(BlueprintType)
enum class EPantheliaDeathPoseFollowerTickPolicy : uint8
{
	PreserveOriginal,
	PostPhysics
};

USTRUCT(BlueprintType)
struct PANTHELIAPROJECT_API FDeathPresentationFinalizationSettings
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Presentation|Finalization")
	bool bAutoFinishPresentation = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Presentation|Finalization",
		meta = (ClampMin = "0.0", UIMin = "0.0"))
	float PresentationDuration = 5.0f;
};

/**
 * Referencia explicita a una parte visual participante en la presentacion de muerte.
 * El Body autoritativo y las partes del arma se resuelven por rol; los seguidores de
 * pose y grooms conservan su binding y no simulan fisica independiente.
 */
USTRUCT(BlueprintType)
struct PANTHELIAPROJECT_API FPantheliaDeathVisualPart
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Presentation")
	TObjectPtr<UPrimitiveComponent> Component = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Presentation")
	EPantheliaDeathVisualPartRole Role = EPantheliaDeathVisualPartRole::Ignore;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Presentation")
	EPantheliaDeathRagdollPolicy RagdollPolicy = EPantheliaDeathRagdollPolicy::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Presentation")
	EPantheliaDeathDissolvePolicy DissolvePolicy = EPantheliaDeathDissolvePolicy::None;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Presentation")
	EPantheliaDeathVisibilityPolicy VisibilityPolicy = EPantheliaDeathVisibilityPolicy::Preserve;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Death Presentation")
	bool bRequired = false;
};

/**
 * Handoff C++ del arma equipada. Equipment conserva ownership y sigue siendo el
 * unico sistema autorizado para destruir WeaponActor; presentacion solo recibe
 * referencias debiles a sus partes visuales.
 */
struct PANTHELIAPROJECT_API FPantheliaEquippedWeaponDeathHandoff
{
	TWeakObjectPtr<AActor> WeaponActor;
	TArray<TWeakObjectPtr<UPrimitiveComponent>> VisualParts;
	bool bWasDetached = false;

	bool HasValidWeapon() const
	{
		return WeaponActor.IsValid();
	}

	void Reset()
	{
		WeaponActor.Reset();
		VisualParts.Reset();
		bWasDetached = false;
	}
};
