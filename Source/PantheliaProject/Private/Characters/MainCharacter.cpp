// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/MainCharacter.h"
#include "Animations/PlayerAnimInstance.h"
#include "Characters/PlayerActionsComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Combat/LockonComponent.h"
#include "Combat/BlockComponent.h"
#include "Combat/PantheliaEquipmentComponent.h"
#include "Combat/WeaponTraceComponent.h"
#include "AbilitySystem/Abilities/PantheliaPlayerAttackAbility.h"
#include "Player/PantheliaPlayerState.h"
#include "AbilitySystem/Data/PantheliaLevelUpInfo.h"
#include "AbilitySystem/PantheliaAbilitySystemComponent.h"
#include "UI/HUD/PantheliaHUD.h"
#include "Engine/World.h"
// Cámara y spring arm — clase 262
#include "Camera/CameraComponent.h"
#include "GameFramework/SpringArmComponent.h"
// Niagara — efecto de partículas al subir de nivel (clase 262)
#include "NiagaraComponent.h"

AMainCharacter::AMainCharacter()
{
    PrimaryActorTick.bCanEverTick = true;

    // === CÁMARA (migrada de Blueprint a C++ en clase 262) ===
    //
    // El spring arm se adjunta a la raíz del personaje. bUsePawnControlRotation = true
    // permite que el jugador rote la cámara con el ratón/stick. Los valores numéricos
    // (TargetArmLength, lags, offsets) se ajustan en Class Defaults del Blueprint para
    // no tener que recompilar C++ cuando se quiera tunear la cámara.
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
    CameraBoom->SetupAttachment(GetRootComponent());
    CameraBoom->TargetArmLength = 400.f;
    CameraBoom->bUsePawnControlRotation = true;   // Rota con el input de cámara
    CameraBoom->bEnableCameraLag = true;           // Suaviza el movimiento lateral
    CameraBoom->bEnableCameraRotationLag = true;  // Suaviza la rotación
    CameraBoom->SetRelativeLocation(FVector(0.f, 0.f, 8.49f)); // Offset vertical del Blueprint original

    // La cámara se adjunta al socket final del spring arm.
    // bUsePawnControlRotation = false porque ya lo gestiona el boom.
    PlayerCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("PlayerCameraComponent"));
    PlayerCameraComponent->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);
    PlayerCameraComponent->bUsePawnControlRotation = false;

    // === NIAGARA DE LEVEL UP (clase 262) ===
    //
    // Se adjunta a la raíz. bAutoActivate = false: se activa manualmente en
    // LevelUp_Implementation cada vez que el jugador sube de nivel.
    // El Niagara System (NS_LevelUp) se asigna en Class Defaults del Blueprint.
    LevelUpNiagaraComponent = CreateDefaultSubobject<UNiagaraComponent>(TEXT("LevelUpNiagaraComponent"));
    LevelUpNiagaraComponent->SetupAttachment(GetRootComponent());
    LevelUpNiagaraComponent->bAutoActivate = false;

    // CombatComponent y TraceComponent (melee legacy NO-GAS) retirados en la Parte 5.
    LockonComponent = CreateDefaultSubobject<ULockonComponent>(TEXT("LockonComponent"));
    BlockComponent = CreateDefaultSubobject<UBlockComponent>(TEXT("BlockComponent"));
    PlayerActionsComponent = CreateDefaultSubobject<UPlayerActionsComponent>(TEXT("PlayerActionsComponent"));

    // Componente de equipo (GAS-ready). Gestiona el arma equipada del jugador
    // (spawnea un APantheliaWeapon data-driven). Sustituye al EquippedWeapon legacy.
    EquipmentComponent = CreateDefaultSubobject<UPantheliaEquipmentComponent>(TEXT("EquipmentComponent"));

    // Trace de arma (GAS-ready). Sweep por sockets de la hoja; aplica el spec de daño.
    // Sustituye al UTraceComponent legacy. La ability de ataque le inyecta mesh y spec.
    WeaponTraceComponent = CreateDefaultSubobject<UWeaponTraceComponent>(TEXT("WeaponTraceComponent"));

    GetCharacterMovement()->bOrientRotationToMovement = true;
    GetCharacterMovement()->RotationRate = FRotator(0.f, 400.f, 0.f);

    bUseControllerRotationYaw = false;
}

void AMainCharacter::PossessedBy(AController* NewController)
{
    Super::PossessedBy(NewController);
    InitAbilityActorInfo();
    // Otorgamos las abilities de startup ahora que el ASC está inicializado.
    // Solo se llama en PossessedBy (servidor), no en OnRep_PlayerState (cliente),
    // porque GAS replica las specs automáticamente.
    AddCharacterAbilities();

    // Otorgamos y activamos de inmediato las abilities pasivas (servidor only).
    // GA_ListenForXPEvents debe estar en StartupPassiveAbilities del Blueprint del jugador.
    if (UPantheliaAbilitySystemComponent* PantheliaASC = Cast<UPantheliaAbilitySystemComponent>(AbilitySystemComponent))
    {
        PantheliaASC->AddCharacterPassiveAbilities(StartupPassiveAbilities);
    }
}

void AMainCharacter::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();
    InitAbilityActorInfo();
}

// GetDamage() (IFighter legacy) retirado en la Parte 5. El daño del jugador ahora
// viene del arma equipada (UPantheliaWeaponDefinition) vía el spec del WeaponTrace.

void AMainCharacter::ResetPlayerCombo()
{
    if (!AbilitySystemComponent) return;

    // Buscar la ability de ataque del jugador entre las concedidas al ASC.
    // Es Instanced Per Actor, por lo que el CDO/spec existe aunque no esté activa.
    for (const FGameplayAbilitySpec& Spec : AbilitySystemComponent->GetActivatableAbilities())
    {
        if (UPantheliaPlayerAttackAbility* AttackAbility =
            Cast<UPantheliaPlayerAttackAbility>(Spec.GetPrimaryInstance()))
        {
            AttackAbility->ResetCombo();
            break; // Solo necesitamos la primera (light o heavy comparten el índice)
        }
    }
}

void AMainCharacter::AddToXP_Implementation(int32 InXP)
{
    // Obtenemos el PlayerState y delegamos la lógica de XP + leveling allí.
    // El PlayerState llama a UpdateLevelFromXP si se cruza un umbral.
    APantheliaPlayerState* PS = GetPlayerState<APantheliaPlayerState>();
    check(PS);
    PS->AddToXP(InXP);
}

void AMainCharacter::LevelUp_Implementation()
{
    // === EFECTO VISUAL DE SUBIDA DE NIVEL (clase 262) ===
    //
    // Orientamos el sistema Niagara hacia la cámara y lo activamos.
    // El cálculo de rotación toma el vector desde el Niagara hasta la cámara,
    // lo convierte en rotator y lo asigna al componente antes de activarlo.
    //
    // Sin Multicast RPC: Panthelia no tiene multijugador, así que la activación
    // es local y directa. Si en el futuro se añade multijugador, aquí iría el
    // Multicast para que todos los clientes vean el efecto de TODOS los jugadores.
    //
    // Blueprint puede extender este comportamiento con Event LevelUp override
    // en BP_ThirdPersonCharacter para añadir sonidos, camera shake, etc.
    if (IsValid(LevelUpNiagaraComponent) && IsValid(PlayerCameraComponent))
    {
        const FVector CameraLocation = PlayerCameraComponent->GetComponentLocation();
        const FVector NiagaraLocation = LevelUpNiagaraComponent->GetComponentLocation();
        // Rotación desde el Niagara hasta la cámara para que el efecto "mire" al jugador
        const FRotator ToCameraRotation = (CameraLocation - NiagaraLocation).Rotation();
        LevelUpNiagaraComponent->SetWorldRotation(ToCameraRotation);
        // Activate(true) resetea el sistema antes de reproducirlo
        LevelUpNiagaraComponent->Activate(true);
    }
}

int32 AMainCharacter::GetXP_Implementation() const
{
    const APantheliaPlayerState* PS = GetPlayerState<APantheliaPlayerState>();
    check(PS);
    return PS->GetXP();
}

int32 AMainCharacter::FindLevelForXP_Implementation(int32 InXP) const
{
    const APantheliaPlayerState* PS = GetPlayerState<APantheliaPlayerState>();
    check(PS);
    const UPantheliaLevelUpInfo* Info = PS->GetLevelUpInfo();
    checkf(Info, TEXT("[PlayerInterface] LevelUpInfo no asignado en BP_PantheliaPlayerState."));
    return Info->FindLevelForXP(InXP);
}

int32 AMainCharacter::GetAttributePointsReward_Implementation(int32 Level) const
{
    const APantheliaPlayerState* PS = GetPlayerState<APantheliaPlayerState>();
    check(PS);
    const UPantheliaLevelUpInfo* Info = PS->GetLevelUpInfo();
    checkf(Info, TEXT("[PlayerInterface] LevelUpInfo no asignado en BP_PantheliaPlayerState."));
    if (!Info->LevelUpInformation.IsValidIndex(Level)) return 0;
    return Info->LevelUpInformation[Level].AttributePointAward;
}

int32 AMainCharacter::GetSkillPointsReward_Implementation(int32 Level) const
{
    const APantheliaPlayerState* PS = GetPlayerState<APantheliaPlayerState>();
    check(PS);
    const UPantheliaLevelUpInfo* Info = PS->GetLevelUpInfo();
    checkf(Info, TEXT("[PlayerInterface] LevelUpInfo no asignado en BP_PantheliaPlayerState."));
    if (!Info->LevelUpInformation.IsValidIndex(Level)) return 0;
    return Info->LevelUpInformation[Level].SkillPointAward;
}

// Getters de SALDO actual (distintos de los "Reward" de arriba, que devuelven el
// premio de un nivel concreto). Añadidos en la clase 267 para que
// UPantheliaAbilitySystemComponent::UpgradeAttribute pueda comprobar si el
// jugador tiene puntos disponibles antes de gastar uno.
int32 AMainCharacter::GetAttributePoints_Implementation() const
{
    const APantheliaPlayerState* PS = GetPlayerState<APantheliaPlayerState>();
    check(PS);
    return PS->GetAttributePoints();
}

int32 AMainCharacter::GetSkillPoints_Implementation() const
{
    const APantheliaPlayerState* PS = GetPlayerState<APantheliaPlayerState>();
    check(PS);
    return PS->GetSkillPoints();
}

void AMainCharacter::AddToPlayerLevel_Implementation(int32 InLevel)
{
    APantheliaPlayerState* PS = GetPlayerState<APantheliaPlayerState>();
    check(PS);
    PS->AddToLevel(InLevel);
}

void AMainCharacter::AddToAttributePoints_Implementation(int32 InAttributePoints)
{
    APantheliaPlayerState* PS = GetPlayerState<APantheliaPlayerState>();
    check(PS);
    PS->AddToAttributePoints(InAttributePoints);
}

void AMainCharacter::AddToSkillPoints_Implementation(int32 InSkillPoints)
{
    APantheliaPlayerState* PS = GetPlayerState<APantheliaPlayerState>();
    check(PS);
    PS->AddToSkillPoints(InSkillPoints);
}

int32 AMainCharacter::GetPlayerLevel_Implementation() const
{
    const APantheliaPlayerState* PS = GetPlayerState<APantheliaPlayerState>();
    check(PS);
    return PS->GetPlayerLevel();
}

void AMainCharacter::BeginPlay()
{
    Super::BeginPlay();

    // El spawn del arma legacy (EquippedWeapon/WeaponClass) se retiró en la Parte 5.
    // Ahora el arma inicial la equipa EquipmentComponent en su propio BeginPlay
    // (vía DefaultWeaponClass + DefaultWeaponDefinition).

    PlayerAnim = Cast<UPlayerAnimInstance>(GetMesh()->GetAnimInstance());
}

void AMainCharacter::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
}

void AMainCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);
}

void AMainCharacter::InitAbilityActorInfo()
{
    // === GUARDA CONTRA DOBLE INICIALIZACIÓN (por-Pawn) ===
    //
    // Esta función se llama desde DOS sitios: PossessedBy() (cuando el PlayerController
    // posee este personaje) y OnRep_PlayerState() (que Unreal ejecuta MANUALMENTE, una
    // sola vez, incluso en single-player sin red real — es una particularidad interna
    // del motor para garantizar que el PlayerState esté sincronizado localmente).
    //
    // Desde la Etapa 4 esta bandera protege solo el cableado de ESTE Pawn (punteros,
    // HUD, delegates); la protección de los DATOS persistentes de GAS (no reaplicar
    // los GEs de atributos por defecto sobre el ASC que sobrevive en el PlayerState)
    // vive en el propio ASC (bAttributesInitialized) y se aplica dentro de
    // InitializeDefaultAttributes. Ver la división de responsabilidades documentada
    // sobre la declaración de la bandera en MainCharacter.h.
    //
    // PENDIENTE CONOCIDO para la futura etapa de muerte/respawn: cuando exista respawn,
    // este método se re-ejecutará completo en el Pawn nuevo (correcto y necesario para
    // fijar el nuevo Avatar), y habrá que revisar que HUD->InitOverlay no cree un
    // segundo overlay al re-llamarse — se abordará al implementar el respawn.
    if (bAbilityActorInfoInitialized) return;

    APantheliaPlayerState* PS = GetPlayerState<APantheliaPlayerState>();
    check(PS);

    PS->GetAbilitySystemComponent()->InitAbilityActorInfo(PS, this);

    AbilitySystemComponent = PS->GetAbilitySystemComponent();
    AttributeSet = PS->GetAttributeSet();

    if (UPantheliaAbilitySystemComponent* PantheliaASC = Cast<UPantheliaAbilitySystemComponent>(AbilitySystemComponent))
    {
        PantheliaASC->AbilityActorInfoSet();
    }

    // Broadcast del delegate de ASC listo (clase 311). Mismo motivo que en
    // APantheliaEnemy::InitAbilityActorInfo — ver el comentario extendido ahí.
    // Protegido por el guard bAbilityActorInfoInitialized de arriba: solo se
    // broadcastea una vez por Pawn, nunca en una re-ejecución.
    OnASCRegistered.Broadcast(AbilitySystemComponent);

    if (APlayerController* PC = Cast<APlayerController>(GetController()))
    {
        if (APantheliaHUD* HUD = Cast<APantheliaHUD>(PC->GetHUD()))
        {
            HUD->InitOverlay(PC, GetPlayerState(), AbilitySystemComponent, AttributeSet);
        }
    }

    InitializeDefaultAttributes();

    // Nos suscribimos al delegate de cambio de nivel del PlayerState para poder refrescar
    // los atributos secundarios cada vez que el jugador sube de nivel (ver explicación
    // extendida en RefreshSecondaryAttributes(), en PantheliaCharacterBase.h/.cpp).
    //
    // Usamos AddUObject (no AddLambda) a propósito: AddUObject desvincula automáticamente
    // este bind si "this" (el Pawn) es destruido, evitando que el delegate intente llamar
    // a una función sobre un Pawn ya destruido. Esto es importante en un soulslike con
    // futura muerte/respawn del jugador, donde el PlayerState puede sobrevivir más tiempo
    // que el Pawn actual. AddLambda capturando "this" no tiene esa protección automática.
    //
    // Como esta suscripción está dentro del bloque protegido por bAbilityActorInfoInitialized,
    // solo se registra una vez por personaje, sin importar cuántas veces se llame a esta función.
    PS->OnLevelChangedDelegate.AddUObject(this, &AMainCharacter::OnPlayerLevelChanged);

    // Marcamos como inicializado AL FINAL, después de que todo el setup haya
    // terminado sin problemas. Así, si algo llama de nuevo a esta función
    // (PossessedBy o OnRep_PlayerState), la guarda del principio lo detiene.
    bAbilityActorInfoInitialized = true;
}

void AMainCharacter::OnPlayerLevelChanged(int32 NewLevel)
{
    // Ver la explicación extendida en PantheliaCharacterBase.h (RefreshSecondaryAttributes)
    // para entender por qué es necesario forzar este refresco manualmente al subir de nivel.
    RefreshSecondaryAttributes();
}