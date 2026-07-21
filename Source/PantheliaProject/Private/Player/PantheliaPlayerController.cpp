// Fill out your copyright notice in the Description page of Project Settings.

#include "Player/PantheliaPlayerController.h"

#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystem/PantheliaAbilitySystemComponent.h"
#include "Blueprint/UserWidget.h"
#include "Combat/LockonComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Input/PantheliaInputComponent.h"
#include "Interfaces/Enemy.h"
#include "Interfaces/CombatInterface.h"
#include "PantheliaGameplayTags.h"
#include "UI/Widgets/PantheliaUserWidget.h"

APantheliaPlayerController::APantheliaPlayerController()
{
	bReplicates = false;
}

void APantheliaPlayerController::BeginPlay()
{
	Super::BeginPlay();

	check(PantheliaContext);

	UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(GetLocalPlayer());
	if (Subsystem)
	{
		Subsystem->AddMappingContext(PantheliaContext, 0);
	}

	bShowMouseCursor = false;
	DefaultMouseCursor = EMouseCursor::Default;

	FInputModeGameOnly InputModeData;
	SetInputMode(InputModeData);
}

void APantheliaPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ClearInteractionTarget();

	if (bAttributeMenuOpen)
	{
		CloseAttributeMenu();
	}

	Super::EndPlay(EndPlayReason);
}

void APantheliaPlayerController::SetupInputComponent()
{
	APlayerController::SetupInputComponent();

	// Casteamos al componente custom. Si falla, verificar:
	// Project Settings → Input → Default Input Component Class = PantheliaInputComponent.
	UPantheliaInputComponent* PantheliaInputComponent = CastChecked<UPantheliaInputComponent>(InputComponent);

	// Bindings de movimiento y cámara.
	PantheliaInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &APantheliaPlayerController::Move);
	PantheliaInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &APantheliaPlayerController::Look);
	PantheliaInputComponent->BindAction(OpenAttributeMenuAction, ETriggerEvent::Started, this, &APantheliaPlayerController::ToggleAttributeMenu);

	// Bindings de lock-on. No pasan por GAS porque son control/cámara, no habilidades.
	if (ToggleLockonAction)
	{
		PantheliaInputComponent->BindAction(ToggleLockonAction, ETriggerEvent::Started, this, &APantheliaPlayerController::ToggleLockonInput);
	}

	if (SwitchLockonTargetAction)
	{
		PantheliaInputComponent->BindAction(SwitchLockonTargetAction, ETriggerEvent::Triggered, this, &APantheliaPlayerController::SwitchLockonTargetInput);
	}

	// Bindings de habilidades. BindAbilityActions itera el InputConfig y bindea
	// los tres callbacks a cada InputAction con su tag correspondiente.
	PantheliaInputComponent->BindAbilityActions(InputConfig, this,
		&APantheliaPlayerController::AbilityInputTagPressed,
		&APantheliaPlayerController::AbilityInputTagReleased,
		&APantheliaPlayerController::AbilityInputTagHeld
	);
}

UPantheliaAbilitySystemComponent* APantheliaPlayerController::GetASC()
{
	// Lazy initialization: casteamos solo la primera vez que se llama.
	// AbilityInputTagHeld se llama cada frame, así que castear dentro de él sería costoso.
	// Después del primer cast exitoso, simplemente retornamos el puntero cacheado.
	if (PantheliaASC == nullptr)
	{
		// UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent busca en el Pawn
		// y llama a IAbilitySystemInterface::GetAbilitySystemComponent(), que en
		// AMainCharacter devuelve el ASC del PlayerState. Transparente para nosotros.
		PantheliaASC = Cast<UPantheliaAbilitySystemComponent>(
			UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetPawn<APawn>())
		);
	}

	return PantheliaASC;
}

bool APantheliaPlayerController::IsGameplayInputBlockedByDeath()
{
	APawn* ControlledPawn = GetPawn();
	if (!IsValid(ControlledPawn))
	{
		return true;
	}

	if (ControlledPawn->Implements<UCombatInterface>() &&
		ICombatInterface::Execute_IsDead(ControlledPawn))
	{
		return true;
	}

	const UPantheliaAbilitySystemComponent* ASC = GetASC();
	return ASC && ASC->HasMatchingGameplayTag(FPantheliaGameplayTags::Get().State_Dead);
}

void APantheliaPlayerController::AbilityInputTagPressed(FGameplayTag InputTag)
{
	// Edge-triggered: se llama UNA vez por pulsación real (no cada frame como Held).
	if (GetASC() == nullptr || IsGameplayInputBlockedByDeath()) return;

	// El buffer se procesa ANTES del intento de activación. Este orden es crítico:
	// si activáramos primero el ataque inicial, NotifyComboInputPressed lo encontraría
	// ya activo y esa misma primera pulsación podría dejar bufferizado el golpe siguiente.
	GetASC()->NotifyComboInputPressed(InputTag);

	// Si hay un dodge activo, ofrecerle el input antes de la activación normal. Durante
	// State.Dodge.Active los ataques están bloqueados, pero el dodge puede aceptar el
	// primer input válido dentro de su ventana de follow-up y ejecutarlo al encadenar.
	const bool bConsumedByDodgeFollowup =
		GetASC()->NotifyDodgeFollowupInputPressed(InputTag);

	// Si el dodge aceptó Light/Heavy dentro de su ventana, esa pulsación ya tiene un
	// destino único y no debe pasar también por la ruta normal de activación. El resto
	// de inputs conserva el flujo OnInputTriggered existente.
	if (!bConsumedByDodgeFollowup)
	{
		// Activación normal de abilities OnInputTriggered: una pulsación = un intento.
		// Mantener el botón no las repetirá al terminar porque Held solo procesa la política
		// WhileInputActive, reservada para canalizaciones futuras.
		GetASC()->AbilityInputTagPressed(InputTag);
	}
}

void APantheliaPlayerController::AbilityInputTagReleased(FGameplayTag InputTag)
{
	// Si el ASC aún no está disponible (muy temprano en el juego), salimos sin hacer nada.
	if (GetASC() == nullptr) return;

	// Release nunca activa una ability: se permite para limpiar el estado InputPressed
	// de specs persistentes incluso si el botón se suelta después de morir.
	GetASC()->AbilityInputTagReleased(InputTag);
	if (IsGameplayInputBlockedByDeath()) return;

	// Ademas, notificar a la ability de ataque pesado para la deteccion tap-vs-hold.
	// El cargado decide segun cuanto se mantuvo el boton antes de soltar.
	GetASC()->NotifyHeavyInputReleased(InputTag);

	// Y a la ability de parry/bloqueo, para terminar la guardia sostenida al soltar.
	GetASC()->NotifyBlockInputReleased(InputTag);
}

void APantheliaPlayerController::AbilityInputTagHeld(FGameplayTag InputTag)
{
	// Si el ASC aún no está disponible (muy temprano en el juego), salimos sin hacer nada.
	// No usamos check() porque es normal que al primer frame aún no esté listo.
	if (GetASC() == nullptr || IsGameplayInputBlockedByDeath()) return;

	GetASC()->AbilityInputTagHeld(InputTag);
}

void APantheliaPlayerController::Move(const FInputActionValue& InputActionValue)
{
	if (IsGameplayInputBlockedByDeath()) return;

	const FVector2D InputAxisVector = InputActionValue.Get<FVector2D>();

	const FRotator Rotation = GetControlRotation();
	const FRotator YawRotation(0.f, Rotation.Yaw, 0.f);

	const FVector ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
	const FVector RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);

	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->AddMovementInput(ForwardDirection, InputAxisVector.Y);
		ControlledPawn->AddMovementInput(RightDirection, InputAxisVector.X);
	}
}

void APantheliaPlayerController::Look(const FInputActionValue& InputActionValue)
{
	const FVector2D LookAxisVector = InputActionValue.Get<FVector2D>();

	if (APawn* ControlledPawn = GetPawn())
	{
		ControlledPawn->AddControllerYawInput(LookAxisVector.X);
		ControlledPawn->AddControllerPitchInput(LookAxisVector.Y);
	}
}

void APantheliaPlayerController::ToggleAttributeMenu()
{
	if (bAttributeMenuOpen)
	{
		CloseAttributeMenu();
	}
	else
	{
		OpenAttributeMenu();
	}
}

void APantheliaPlayerController::ToggleLockonInput()
{
	if (IsGameplayInputBlockedByDeath()) return;

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	ULockonComponent* LockonComponent = ControlledPawn->FindComponentByClass<ULockonComponent>();
	if (!LockonComponent)
	{
		return;
	}

	LockonComponent->ToggleLockon();
}

void APantheliaPlayerController::SwitchLockonTargetInput(const FInputActionValue& InputActionValue)
{
	if (IsGameplayInputBlockedByDeath()) return;

	const float Direction = InputActionValue.Get<float>();
	if (FMath::IsNearlyZero(Direction))
	{
		return;
	}

	APawn* ControlledPawn = GetPawn();
	if (!ControlledPawn)
	{
		return;
	}

	ULockonComponent* LockonComponent = ControlledPawn->FindComponentByClass<ULockonComponent>();
	if (!LockonComponent)
	{
		return;
	}

	LockonComponent->SwitchTarget(Direction);
}

void APantheliaPlayerController::OpenAttributeMenu()
{
	if (!AttributeMenuClass) { return; }

	// Creamos el widget y lo añadimos a la pantalla
	AttributeMenuWidget = CreateWidget<UPantheliaUserWidget>(this, AttributeMenuClass);

	if (!AttributeMenuWidget) { return; }

	AttributeMenuWidget->AddToViewport();

	// Pausamos el juego
	SetPause(true);

	// Mostramos el cursor para poder interactuar con el menú
	bShowMouseCursor = true;

	// Cambiamos el modo de input a GameAndUI (no UIOnly).
	// UIOnly enruta TODO el input a Slate/UI, así que la tecla O (una Input Action
	// del PlayerController) nunca llegaba a ToggleAttributeMenu y el menú no se
	// podía cerrar con la misma tecla. GameAndUI deja que el ratón interactúe con
	// el menú Y mantiene vivas las Input Actions del juego (incluida O).
	// El bTriggerWhenPaused del IA_OpenAttributeMenu asegura que O siga disparando
	// aunque el juego esté pausado.
	FInputModeGameAndUI InputModeData;
	InputModeData.SetWidgetToFocus(AttributeMenuWidget->TakeWidget());
	InputModeData.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

	// Evita que un clic fuera del menú devuelva el control solo al juego y
	// desincronice el estado del cursor/pausa; el cierre se gestiona con O.
	InputModeData.SetHideCursorDuringCapture(false);

	SetInputMode(InputModeData);

	bAttributeMenuOpen = true;
}

void APantheliaPlayerController::CloseAttributeMenu()
{
	if (AttributeMenuWidget)
	{
		AttributeMenuWidget->RemoveFromParent();
		AttributeMenuWidget = nullptr;
	}

	// Reanudamos el juego
	SetPause(false);

	// Ocultamos el cursor
	bShowMouseCursor = false;

	// Volvemos al modo de input de juego
	FInputModeGameOnly InputModeData;
	SetInputMode(InputModeData);

	bAttributeMenuOpen = false;
}

void APantheliaPlayerController::PlayerTick(float DeltaTime)
{
	APlayerController::PlayerTick(DeltaTime);
	InteractionTrace();
}

void APantheliaPlayerController::InteractionTrace()
{
	if (!IsValid(PlayerCameraManager) || !IsValid(GetWorld()))
	{
		ClearInteractionTarget();
		return;
	}

	FHitResult InteractionHit;

	const FVector Start = PlayerCameraManager->GetCameraLocation();
	const FVector End = Start + (PlayerCameraManager->GetCameraRotation().Vector() * 500.f);

	GetWorld()->LineTraceSingleByChannel(InteractionHit, Start, End, ECC_Visibility);

	AActor* HitActor = InteractionHit.bBlockingHit ? InteractionHit.GetActor() : nullptr;
	if (!IsValid(HitActor) || !HitActor->Implements<UEnemy>())
	{
		ClearInteractionTarget();
		return;
	}

	SetInteractionTarget(HitActor);
}

void APantheliaPlayerController::SetInteractionTarget(AActor* NewTarget)
{
	if (!IsValid(NewTarget) || !NewTarget->Implements<UEnemy>())
	{
		ClearInteractionTarget();
		return;
	}

	if (ThisActor.GetObject() == NewTarget)
	{
		return;
	}

	if (UObject* PreviousObject = ThisActor.GetObject())
	{
		if (IsValid(PreviousObject) && PreviousObject->Implements<UEnemy>())
		{
			IEnemy::Execute_UnHighlightActor(PreviousObject);
		}
	}

	LastActor = ThisActor;
	ThisActor = NewTarget;
	IEnemy::Execute_HighlightActor(NewTarget);
}

void APantheliaPlayerController::ClearInteractionTarget()
{
	if (UObject* CurrentObject = ThisActor.GetObject())
	{
		if (IsValid(CurrentObject) && CurrentObject->Implements<UEnemy>())
		{
			IEnemy::Execute_UnHighlightActor(CurrentObject);
		}
	}

	LastActor = nullptr;
	ThisActor = nullptr;
}
