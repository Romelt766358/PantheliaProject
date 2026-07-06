// Fill out your copyright notice in the Description page of Project Settings.

#include "AbilitySystem/Debuff/PantheliaDebuffNiagaraComponent.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AbilitySystemComponent.h"
#include "Interfaces/CombatInterface.h"

UPantheliaDebuffNiagaraComponent::UPantheliaDebuffNiagaraComponent()
{
	// Sin esto, el sistema Niagara se activaría solo al spawnear el personaje (el
	// comportamiento por defecto de cualquier Niagara Component). Queremos control
	// manual total: Activate()/Deactivate() los llama exclusivamente DebuffTagChanged
	// y OnOwnerDeath, nunca el motor por su cuenta.
	bAutoActivate = false;
}

void UPantheliaDebuffNiagaraComponent::BeginPlay()
{
	Super::BeginPlay();

	// Intento directo: si el ASC del propietario YA está listo en este instante,
	// suscribirse ahora mismo — es el camino más simple y común (la mayoría de las
	// veces InitAbilityActorInfo ya corrió antes de que este componente llegue aquí).
	UAbilitySystemComponent* ASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(GetOwner());

	// Guardamos la interfaz una sola vez — la necesitamos tanto para el camino de
	// respaldo de abajo (si el ASC no estaba listo) como, siempre, para suscribirnos
	// al delegate de muerte.
	ICombatInterface* CombatInterface = Cast<ICombatInterface>(GetOwner());

	if (ASC)
	{
		// RegisterGameplayTagEvent devuelve un delegate multicast al que nos suscribimos
		// con AddUObject (versión "segura" de bindear una función miembro: si "this" se
		// destruye, la suscripción se limpia sola — no hace falta AddWeakLambda aquí
		// porque AddUObject ya maneja UObjects de forma segura por sí mismo).
		// EGameplayTagEventType::NewOrRemoved: nos interesa tanto cuando el tag aparece
		// (empieza el debuff) como cuando desaparece (termina) — un solo callback cubre
		// ambos casos gracias al parámetro NewCount (ver DebuffTagChanged).
		ASC->RegisterGameplayTagEvent(DebuffTag, EGameplayTagEventType::NewOrRemoved)
			.AddUObject(this, &UPantheliaDebuffNiagaraComponent::DebuffTagChanged);
	}
	else if (CombatInterface)
	{
		// Camino de respaldo: el ASC todavía no existe. Nos suscribimos al delegate
		// "ASC listo" del propietario — cuando SÍ se dispare (en InitAbilityActorInfo
		// del personaje), la lambda de aquí abajo hace la misma suscripción que
		// hicimos arriba, pero con el ASC que llega como parámetro del propio delegate.
		//
		// AddWeakLambda (no AddLambda a secas): la lambda captura "this" (puntero a
		// ESTE componente). Si el componente se destruyera antes de que el ASC quedara
		// listo (poco probable, pero posible), una lambda normal ejecutaría código
		// sobre un puntero inválido → crash. AddWeakLambda envuelve el primer parámetro
		// (this) en un puntero débil: si el objetivo ya no es válido cuando el delegate
		// se dispara, simplemente no ejecuta la lambda, sin crashear.
		CombatInterface->GetOnASCRegisteredDelegate().AddWeakLambda(this,
			[this](UAbilitySystemComponent* InASC)
			{
				InASC->RegisterGameplayTagEvent(DebuffTag, EGameplayTagEventType::NewOrRemoved)
					.AddUObject(this, &UPantheliaDebuffNiagaraComponent::DebuffTagChanged);
			});
	}

	// Suscripción al delegate de muerte — independiente de si el ASC ya estaba listo o
	// no arriba, por eso vive fuera del if/else. Sin esto, un debuff con duración larga
	// seguiría con su VFX encendido sobre un cadáver hasta que el propio GE del debuff
	// expirase por su cuenta (el chequeo de "muerto" de la clase 310 ya detiene el DAÑO,
	// pero no apaga por sí solo el efecto visual de este componente).
	if (CombatInterface)
	{
		CombatInterface->GetOnDeathDelegate().AddDynamic(this, &UPantheliaDebuffNiagaraComponent::OnOwnerDeath);
	}
}

void UPantheliaDebuffNiagaraComponent::DebuffTagChanged(const FGameplayTag CallbackTag, int32 NewCount)
{
	// --- FIX (clase 314): condición de carrera entre debuff y muerte simultáneos ---
	// Bug real que reproduce la transcripción original: un golpe puede, en la MISMA
	// ejecución, matar al objetivo Y concederle el tag de debuff (ej. un Firebolt que
	// mata Y aplica Quemadura a la vez). Sin este chequeo, DebuffTagChanged recibe
	// NewCount > 0 y activa el fuego — pero el personaje ya está muerto, y el efecto
	// visual se queda encendido sobre el cadáver (el delegate OnDeath, que normalmente
	// lo apagaría, ya se disparó ANTES de que el tag terminara de concederse, o el orden
	// entre ambos no está garantizado).
	//
	// La solución: no basta con "¿el tag se concedió?" — hace falta también "¿el
	// propietario sigue vivo y es válido AHORA MISMO?". Separado en dos booleans con
	// nombre para que quede claro qué comprueba cada uno (evita un if gigante e ilegible):
	//   bOwnerValid — ¿el owner sigue existiendo? (no ha sido destruido / marcado para
	//                 destrucción). IsValid() es más seguro que comprobar solo != nullptr:
	//                 también detecta objetos "pendientes de kill" que técnicamente
	//                 todavía existen en memoria pero van a desaparecer este frame.
	//   bOwnerAlive — ¿el owner implementa ICombatInterface Y no está muerto? Si no
	//                 implementa la interfaz, asumimos que no aplica el concepto de
	//                 "muerto" para él, así que tratamos eso como "no vivo" (más seguro
	//                 no activar VFX que activarlo sin poder verificar el estado).
	const bool bOwnerValid = IsValid(GetOwner());
	const bool bOwnerAlive = bOwnerValid
		&& GetOwner()->Implements<UCombatInterface>()
		&& !ICombatInterface::Execute_IsDead(GetOwner());

	if (NewCount > 0 && bOwnerValid && bOwnerAlive)
	{
		Activate();
	}
	else
	{
		Deactivate();
	}
}

void UPantheliaDebuffNiagaraComponent::OnOwnerDeath(AActor* DeadActor)
{
	Deactivate();
}