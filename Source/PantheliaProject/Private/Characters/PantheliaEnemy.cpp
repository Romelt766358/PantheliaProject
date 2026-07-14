// Fill out your copyright notice in the Description page of Project Settings.

#include "Characters/PantheliaEnemy.h"

#include "AbilitySystem/PantheliaAbilitySystemComponent.h"
#include "AbilitySystem/PantheliaAttributeSet.h"
#include "AbilitySystem/PantheliaAbilitySystemLibrary.h"
#include "PantheliaGameplayTags.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Combat/LockonComponent.h"
#include "Kismet/GameplayStatics.h"
#include "AI/PantheliaAIController.h"
#include "AIController.h"
#include "BrainComponent.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardComponent.h"
#include "Components/SceneComponent.h"

APantheliaEnemy::APantheliaEnemy()
{
	// --- Fix de rotación ---
	// Desactivamos el control de rotación por controller (que causaba snapping brusco).
	// En su lugar, usamos bUseControllerDesiredRotation en el CharacterMovement,
	// que rota suavemente hacia la dirección del controller usando RotationRate.
	// La RotationRate (Z = Yaw) se ajusta desde el Blueprint del enemigo (~360°/s).
	bUseControllerRotationPitch = false;
	bUseControllerRotationRoll = false;
	bUseControllerRotationYaw = false;

	GetCharacterMovement()->bUseControllerDesiredRotation = true;

	LockonTargetPoint = CreateDefaultSubobject<USceneComponent>("LockonTargetPoint");
	LockonTargetPoint->SetupAttachment(GetRootComponent());
	LockonTargetPoint->SetRelativeLocation(FVector(0.0f, 0.0f, 80.0f));

	AbilitySystemComponent = CreateDefaultSubobject<UPantheliaAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetIsReplicated(true);

	AttributeSet = CreateDefaultSubobject<UPantheliaAttributeSet>("AttributeSet");

	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
}

void APantheliaEnemy::HighlightActor_Implementation()
{
}

void APantheliaEnemy::UnHighlightActor_Implementation()
{
}

bool APantheliaEnemy::IsLockonTargetable_Implementation() const
{
	// Mientras el actor sigue vivo físicamente por Lifespan/dissolve, ya no debe
	// poder ser seleccionado por lock-on ni por auto-retarget.
	return !bDead;
}

FVector APantheliaEnemy::GetLockonLocation_Implementation() const
{
	// El componente permite ajustar el punto de lock-on por Blueprint sin que
	// ULockonComponent tenga que conocer sockets, meshes ni clases concretas.
	// Si por alguna razón no existe, caemos a GetActorLocation como fallback seguro.
	return LockonTargetPoint ? LockonTargetPoint->GetComponentLocation() : GetActorLocation();
}

int32 APantheliaEnemy::GetPlayerLevel_Implementation() const
{
	return Level;
}

void APantheliaEnemy::Die(const FVector& DeathImpulse)
{
	// Detener primero toda lógica de navegación/decisión. Los enemigos normales usan
	// el BrainComponent del AIController; un boss también puede tener un componente de
	// IA basado en UBrainComponent directamente sobre el pawn. Cubrimos ambos caminos
	// sin acoplar esta clase a un Behavior Tree o StateTree concreto.
	if (AAIController* AIController = Cast<AAIController>(GetController()))
	{
		AIController->StopMovement();

		if (UBrainComponent* ControllerBrain = AIController->GetBrainComponent())
		{
			ControllerBrain->StopLogic(TEXT("Owner died"));
		}
	}

	if (UBrainComponent* PawnBrain = FindComponentByClass<UBrainComponent>())
	{
		PawnBrain->StopLogic(TEXT("Owner died"));
	}

	if (UCharacterMovementComponent* MovementComponent = GetCharacterMovement())
	{
		MovementComponent->StopMovementImmediately();
		MovementComponent->DisableMovement();
	}

	// Notificar también al Blackboard legacy. El decorator "Am I Alive?" comprueba
	// Dead=false; conservar este dato permite que un Behavior Tree reiniciado por
	// error siga rechazando su rama de combate.
	if (PantheliaAIController)
	{
		if (UBlackboardComponent* BlackboardComp = PantheliaAIController->GetBlackboardComponent())
		{
			BlackboardComp->SetValueAsBool(FName("Dead"), true);
		}
	}

	// Retarget automático si este enemigo era el target del lockon.
	// Si hay otro enemigo cercano y razonable, el LockonComponent saltará a él.
	// Si no hay ninguno, limpiará el lock-on como antes.
	if (ACharacter* PlayerCharacter = Cast<ACharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)))
	{
		if (ULockonComponent* LockonComp = PlayerCharacter->FindComponentByClass<ULockonComponent>())
		{
			if (LockonComp->CurrentTargetActor == this)
			{
				LockonComp->HandleCurrentTargetLost(this);
			}
		}
	}

	SetLifeSpan(Lifespan);
	Super::Die(DeathImpulse);
}

void APantheliaEnemy::BeginPlay()
{
	Super::BeginPlay();

	GetCharacterMovement()->MaxWalkSpeed = BaseWalkSpeed;
	InitAbilityActorInfo();

	// Abilities universales (HitReact) + abilities de clase (GA_MeleeAttack para Warrior, etc.)
	// Ambas vienen de DA_CharacterClassInfo según el CharacterClass de este enemigo.
	UPantheliaAbilitySystemLibrary::GiveStartupAbilities(this, AbilitySystemComponent, CharacterClass);

	// Abilities de combate extra, específicas de este enemigo individual.
	// Configuradas en el Blueprint del enemigo (Details → Panthelia|Abilities → CombatAbilities).
	// Se conceden al nivel del personaje para que escalen igual que las de clase.
	// Ejemplo: un Warrior con bCanRangedAttack=true tiene GA_RangedAttack aquí.
	for (TSubclassOf<UGameplayAbility> AbilityClass : CombatAbilities)
	{
		if (!AbilityClass) continue;

		FGameplayAbilitySpec AbilitySpec(AbilityClass, Level);
		AbilitySystemComponent->GiveAbility(AbilitySpec);
	}

	// Suscribirse al tag Effects.HitReact
	AbilitySystemComponent->RegisterGameplayTagEvent(
		FPantheliaGameplayTags::Get().Effects_HitReact,
		EGameplayTagEventType::NewOrRemoved
	).AddUObject(this, &APantheliaEnemy::HitReactTagChanged);

	// Suscribirse al tag Effects.Stagger
	AbilitySystemComponent->RegisterGameplayTagEvent(
		FPantheliaGameplayTags::Get().Effects_Stagger,
		EGameplayTagEventType::NewOrRemoved
	).AddUObject(this, &APantheliaEnemy::StaggerTagChanged);
}

void APantheliaEnemy::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	// Sin multiplayer, HasAuthority() siempre será true en Panthelia.
	// Lo mantenemos como código defensivo y buena práctica de arquitectura GAS/AI.
	// Si en el futuro se añadiese multiplayer, esto protegería la lógica de IA
	// para que solo corra en el servidor.
	if (!HasAuthority()) return;

	// Guardamos referencia al AIController. Si el cast falla (por ejemplo,
	// si asignamos un AIController equivocado en el Blueprint del enemigo),
	// simplemente no se arranca la IA en lugar de crashear.
	PantheliaAIController = Cast<APantheliaAIController>(NewController);
	if (!PantheliaAIController) return;

	// Solo arrancamos la IA si este enemigo tiene un Behavior Tree asignado.
	// Esto permite tener enemigos "mudos" sin IA simplemente dejando el campo vacío.
	if (!BehaviorTree) return;

	// Arrancamos el Behavior Tree. A partir de aquí el enemigo toma decisiones
	// autónomas basándose en las reglas definidas en el árbol.
	PantheliaAIController->RunBehaviorTree(BehaviorTree);

	// Inicializamos el Blackboard con los datos del Behavior Tree.
	// El Blackboard es la "pizarra" donde el árbol lee y escribe variables
	// (ej: "¿Dónde está el jugador?", "¿Estoy en combate?").
	// BehaviorTree->BlackboardAsset es el asset BB_ asociado al BT en el editor.
	if (UBlackboardComponent* BlackboardComp = PantheliaAIController->GetBlackboardComponent())
	{
		BlackboardComp->InitializeBlackboard(*BehaviorTree->BlackboardAsset);

		// Inicializamos las keys de estado explícitamente para que el árbol
		// tenga valores definidos desde el primer tick, sin esperar al primer evento.
		BlackboardComp->SetValueAsBool(FName("HitReacting"), false);

		// RangedAttacker: true si este enemigo puede usar la rama de ataque a distancia.
		// Antes se derivaba de CharacterClass (!= Warrior). Ahora es una propiedad
		// configurable por Blueprint (bCanRangedAttack), permitiendo enemigos híbridos
		// (melee + ranged) independientemente de su clase de atributos.
		BlackboardComp->SetValueAsBool(FName("RangedAttacker"), bCanRangedAttack);
	}
}

void APantheliaEnemy::InitAbilityActorInfo()
{
	AbilitySystemComponent->InitAbilityActorInfo(this, this);

	if (UPantheliaAbilitySystemComponent* PantheliaASC = Cast<UPantheliaAbilitySystemComponent>(AbilitySystemComponent))
	{
		PantheliaASC->AbilityActorInfoSet();
	}

	// Broadcast del delegate de ASC listo (clase 311). Cualquier componente que haya
	// hecho BeginPlay() ANTES de este punto (orden no garantizado entre componentes y
	// el ASC) y no haya podido suscribirse todavía a los cambios de tag, recibe aquí su
	// oportunidad de hacerlo — ver UPantheliaDebuffNiagaraComponent::BeginPlay.
	OnASCRegistered.Broadcast(AbilitySystemComponent);

	InitializeDefaultAttributes();
}

void APantheliaEnemy::InitializeDefaultAttributes() const
{
	UPantheliaAbilitySystemLibrary::InitializeDefaultAttributes(this, CharacterClass, Level, AbilitySystemComponent);
}

void APantheliaEnemy::HitReactTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	bHitReacting = NewCount > 0;

	// Durante hit react: inmovilizar. Al terminar: restaurar velocidad base
	// (pero solo si no estamos staggered también).
	if (!bStaggered)
	{
		GetCharacterMovement()->MaxWalkSpeed = bHitReacting ? 0.f : BaseWalkSpeed;
	}

	// Actualizamos el Blackboard para que el Behavior Tree sepa que estamos
	// en hit react y pueda abortar la rama de movimiento/ataque (decorator abort self).
	if (PantheliaAIController)
	{
		if (UBlackboardComponent* BlackboardComp = PantheliaAIController->GetBlackboardComponent())
		{
			BlackboardComp->SetValueAsBool(FName("HitReacting"), bHitReacting);
		}
	}
}

void APantheliaEnemy::StaggerTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	bStaggered = NewCount > 0;

	// Durante stagger: inmovilizar completamente.
	// Al terminar: restaurar velocidad (el regen de postura ya se gestiona via timer).
	GetCharacterMovement()->MaxWalkSpeed = bStaggered ? 0.f : BaseWalkSpeed;
}

void APantheliaEnemy::SetCombatTarget_Implementation(AActor* InCombatTarget)
{
	// Guardamos el actor al que vamos a atacar.
	// BTT_Attack lo llama justo antes de activar la ability de melee,
	// así GA_MeleeAttack puede leerlo para orientar el Motion Warping.
	CombatTarget = InCombatTarget;
}

AActor* APantheliaEnemy::GetCombatTarget_Implementation() const
{
	return CombatTarget;
}
