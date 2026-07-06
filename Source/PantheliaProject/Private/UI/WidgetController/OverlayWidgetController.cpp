// Fill out your copyright notice in the Description page of Project Settings.

#include "UI/WidgetController/OverlayWidgetController.h"
#include "AbilitySystem/PantheliaAttributeSet.h"
#include "AbilitySystem/PantheliaAbilitySystemComponent.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Data/PantheliaAbilityInfo.h"
#include "AbilitySystem/Data/PantheliaLevelUpInfo.h"
#include "Player/PantheliaPlayerState.h"
#include "PantheliaLogChannels.h"

void UOverlayWidgetController::BroadcastInitialValues()
{
	const UPantheliaAttributeSet* PantheliaAS = CastChecked<UPantheliaAttributeSet>(AttributeSet);

	OnMaxHealthChanged.Broadcast(PantheliaAS->GetMaxHealth());
	OnMaxManaChanged.Broadcast(PantheliaAS->GetMaxMana());
	OnMaxStaminaChanged.Broadcast(PantheliaAS->GetMaxStamina());
	OnMaxPoiseChanged.Broadcast(PantheliaAS->GetMaxPoise());

	OnHealthChanged.Broadcast(PantheliaAS->GetHealth());
	OnManaChanged.Broadcast(PantheliaAS->GetMana());
	OnStaminaChanged.Broadcast(PantheliaAS->GetStamina());
	OnPoiseChanged.Broadcast(PantheliaAS->GetPoise());

	// Valor inicial de la barra de XP y del nivel.
	const APantheliaPlayerState* PS = CastChecked<APantheliaPlayerState>(PlayerState);

	// Inicializar el caché de XP ANTES de llamar OnXPChanged para que el delta
	// al arrancar sea 0 (no queremos mostrar "+N" al iniciar el juego).
	CachedXP = PS->GetXP();
	OnXPChanged(PS->GetXP());

	// Broadcastear el nivel inicial para que el widget muestre "Nv. 1" desde el primer frame.
	OnLevelChanged.Broadcast(PS->GetPlayerLevel());
}

void UOverlayWidgetController::BindCallbacksToDependencies()
{
	// --- PlayerState: XP ---
	// Casteamos una vez y bindeamos el callback para que la barra de XP
	// se actualice cada vez que el jugador gane experiencia.
	APantheliaPlayerState* PS = CastChecked<APantheliaPlayerState>(PlayerState);
	PS->OnXPChangedDelegate.AddUObject(this, &UOverlayWidgetController::OnXPChanged);
	PS->OnLevelChangedDelegate.AddUObject(this, &UOverlayWidgetController::OnLevelChangedCallback);

	// --- AttributeSet: atributos vitales y de combate ---
	const UPantheliaAttributeSet* PantheliaAS = CastChecked<UPantheliaAttributeSet>(AttributeSet);

	// Reemplazamos todos los callbacks por lambdas.
	// Capturamos [this] para poder acceder a los delegates del widget controller.
	// La firma del lambda debe coincidir con la del delegate de GAS: const FOnAttributeChangeData&

	// SALUD
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		PantheliaAS->GetHealthAttribute()).AddLambda([this](const FOnAttributeChangeData& Data)
			{
				OnHealthChanged.Broadcast(Data.NewValue);
			});

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		PantheliaAS->GetMaxHealthAttribute()).AddLambda([this](const FOnAttributeChangeData& Data)
			{
				OnMaxHealthChanged.Broadcast(Data.NewValue);
			});

	// ESTAMINA
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		PantheliaAS->GetStaminaAttribute()).AddLambda([this](const FOnAttributeChangeData& Data)
			{
				OnStaminaChanged.Broadcast(Data.NewValue);
			});

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		PantheliaAS->GetMaxStaminaAttribute()).AddLambda([this](const FOnAttributeChangeData& Data)
			{
				OnMaxStaminaChanged.Broadcast(Data.NewValue);
			});

	// MANÁ
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		PantheliaAS->GetManaAttribute()).AddLambda([this](const FOnAttributeChangeData& Data)
			{
				OnManaChanged.Broadcast(Data.NewValue);
			});

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		PantheliaAS->GetMaxManaAttribute()).AddLambda([this](const FOnAttributeChangeData& Data)
			{
				OnMaxManaChanged.Broadcast(Data.NewValue);
			});

	// POSTURA
	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		PantheliaAS->GetPoiseAttribute()).AddLambda([this](const FOnAttributeChangeData& Data)
			{
				OnPoiseChanged.Broadcast(Data.NewValue);
			});

	AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(
		PantheliaAS->GetMaxPoiseAttribute()).AddLambda([this](const FOnAttributeChangeData& Data)
			{
				OnMaxPoiseChanged.Broadcast(Data.NewValue);
			});

	// Suscripción a delegates del PantheliaAbilitySystemComponent.
	// Casteamos una sola vez y reutilizamos el puntero para todos los binds de este bloque.
	if (UPantheliaAbilitySystemComponent* PantheliaASC = Cast<UPantheliaAbilitySystemComponent>(AbilitySystemComponent))
	{
		// --- Mensajes de pickup (efectos GE con asset tags de categoría "Message") ---
		PantheliaASC->EffectAssetTags.AddLambda([this](const FGameplayTagContainer& AssetTags)
			{
				for (const FGameplayTag& Tag : AssetTags)
				{
					const FGameplayTag MessageTag = FGameplayTag::RequestGameplayTag(FName("Message"));
					if (!Tag.MatchesTag(MessageTag)) { continue; }

					const FUIWidgetRow* Row = GetDataTableRowByTag<FUIWidgetRow>(MessageWidgetDataTable, Tag);
					if (Row)
					{
						MessageWidgetRowDelegate.Broadcast(*Row);
					}
				}
			});

		// --- Inicialización de la UI de hechizos ---
		// Resolvemos la carrera de inicialización entre el ASC (que da abilities en BeginPlay)
		// y este widget controller (que bindea callbacks también en BeginPlay).
		// El orden de BeginPlay de distintos actores no está garantizado, así que:
		//
		//   Si bStartupAbilitiesGiven == true: el ASC ya terminó, llamamos directamente.
		//   Si bStartupAbilitiesGiven == false: aún no terminó, bindeamos para cuando ocurra.
		//
		// En ambos casos, OnInitializeStartupAbilities se llama exactamente una vez.
		if (PantheliaASC->bStartupAbilitiesGiven)
		{
			// Las abilities ya fueron dadas antes de que llegáramos a bindear.
			// No tiene sentido bindear al delegate — ya se broadcasteó. Llamamos directo.
			OnInitializeStartupAbilities(PantheliaASC);
		}
		else
		{
			// Las abilities todavía no han sido dadas. Bindeamos nuestro callback para
			// que se llame en cuanto AddCharacterAbilities termine y broadcastee.
			PantheliaASC->AbilitiesGivenDelegate.AddUObject(
				this,
				&UOverlayWidgetController::OnInitializeStartupAbilities);
		}
	}
}

void UOverlayWidgetController::OnInitializeStartupAbilities(UPantheliaAbilitySystemComponent* PantheliaASC)
{
	// Guarda de seguridad: si por algún motivo llegamos aquí sin que las abilities estén dadas,
	// salimos sin hacer nada. No debería ocurrir con el patrón if/else de arriba, pero
	// es buena práctica defensa en profundidad.
	if (!PantheliaASC || !PantheliaASC->bStartupAbilitiesGiven)
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("[AbilityInfo] OnInitializeStartupAbilities llamado antes de que se dieran las abilities. Ignorado."));
		return;
	}

	// Si no hay DA_AbilityInfo asignado en BP_OverlayWidgetController no podemos hacer lookups.
	if (AbilityInfo == nullptr)
	{
		UE_LOG(LogPanthelia, Warning,
			TEXT("[AbilityInfo] AbilityInfo (DA_AbilityInfo) no asignado en BP_OverlayWidgetController. No se inicializa la UI de hechizos."));
		return;
	}

	// Creamos un delegate de tipo FForEachAbility y le bindeamos un lambda. Ese lambda es la
	// operación que queremos aplicar a CADA ability: buscar su info y broadcastearla a la UI.
	// Capturamos [this, PantheliaASC] por valor:
	//   - this: para acceder a AbilityInfo y a AbilityInfoDelegate.
	//   - PantheliaASC: para llamar a las utilidades estáticas (aunque son static, pasamos
	//     el puntero por claridad y para futuras necesidades de instancia).
	FForEachAbility BroadcastDelegate;
	BroadcastDelegate.BindLambda([this, PantheliaASC](const FGameplayAbilitySpec& AbilitySpec)
		{
			// 1. Obtenemos el AbilityTag de esta ability (el que empieza por "Abilities").
			const FGameplayTag AbilityTag = UPantheliaAbilitySystemComponent::GetAbilityTagFromSpec(AbilitySpec);

			// Solo procesamos hechizos del jugador (Abilities.Spell.*). Las abilities de combate
			// (Abilities.Attack, Abilities.Parry, etc.) no tienen slots en el HUD soulslike:
			// son botones del gamepad, no iconos equipables. Saltarlas silenciosamente evita
			// los errores "No se encontro entrada" en el log para abilities que correctamente
			// no estan en DA_AbilityInfo.
			const FGameplayTag SpellParentTag = FGameplayTag::RequestGameplayTag(FName("Abilities.Spell"));
			if (!AbilityTag.MatchesTag(SpellParentTag))
			{
				return;
			}

			// 2. Buscamos su entrada en el DA_AbilityInfo. Si no existe, FindAbilityInfoForTag
			//    devuelve un struct vacío (lo logueamos pasando bLogNotFound = true).
			FPantheliaAbilityInfo Info = AbilityInfo->FindAbilityInfoForTag(AbilityTag, true);

			// 3. Rellenamos el InputTag en runtime: NO está en el DA (puede cambiar al
			//    reasignar hechizos), sino en los DynamicSpecSourceTags de la spec.
			Info.InputTag = UPantheliaAbilitySystemComponent::GetInputTagFromSpec(AbilitySpec);

			// 4. Broadcasteamos el struct completo. Los slots del HUD que escuchen este delegate
			//    compararán Info.InputTag con el suyo y se actualizarán si coincide.
			AbilityInfoDelegate.Broadcast(Info);
		});

	// Le pasamos el delegate al ASC, que lo ejecutará sobre cada ability activable
	// de forma segura (con el FScopedAbilityListLock interno).
	PantheliaASC->ForEachAbility(BroadcastDelegate);
}

void UOverlayWidgetController::OnXPChanged(int32 NewXP)
{
	// === Cálculo del porcentaje de la barra de XP ===
	//
	// DA_LevelUpInfo almacena costes INCREMENTALES (cuánta XP adicional hace falta para
	// pasar del nivel anterior a este). Para saber en qué punto absoluto de XP arranca
	// el nivel actual, acumulamos los costes incrementales de los niveles anteriores.
	//
	// Convención de índices del array LevelUpInformation:
	//   [0] = placeholder (ignorar)
	//   [1] = datos nivel 1 (LevelUpRequirement = 0, se empieza aquí sin XP)
	//   [2] = coste incremental para pasar de nivel 1 a nivel 2
	//   [N] = coste incremental para pasar de nivel N-1 a nivel N
	//
	// Ejemplo con nivel 2 = 300 incrementales y nivel 3 = 600 incrementales:
	//   Jugador con 450 XP está en nivel 2.
	//   XPAtLevelStart = 300 (coste acumulado para llegar a nivel 2)
	//   DeltaToNextLevel = 600 (coste incremental para pasar a nivel 3)
	//   XPForThisLevel = 450 - 300 = 150
	//   Percent = 150 / 600 = 0.25  →  barra al 25%

	const APantheliaPlayerState* PS = CastChecked<APantheliaPlayerState>(PlayerState);
	const UPantheliaLevelUpInfo* LevelUpInfo = PS->GetLevelUpInfo();
	checkf(LevelUpInfo,
		TEXT("[XP] LevelUpInfo es null. Asigna DA_LevelUpInfo en BP_PantheliaPlayerState → Class Defaults."));

	const int32 Level = LevelUpInfo->FindLevelForXP(NewXP);
	const int32 MaxLevel = LevelUpInfo->LevelUpInformation.Num() - 1;

	if (Level <= 0 || Level > MaxLevel)
	{
		// Fuera de rango: no broadcasteamos para no enviar valores sin sentido a la UI.
		UE_LOG(LogPanthelia, Warning,
			TEXT("[XP] Nivel calculado (%d) fuera de rango [1, %d]. NewXP = %d"),
			Level, MaxLevel, NewXP);
		return;
	}

	// Acumulamos los costes incrementales para saber dónde arranca el nivel actual.
	// El bucle va de índice 2 hasta Level (inclusive) porque:
	//   - El índice 1 (nivel 1) tiene coste 0 (se empieza sin XP).
	//   - El índice 2 es el coste para entrar en nivel 2, etc.
	int32 XPAtLevelStart = 0;
	for (int32 i = 2; i <= Level; i++)
	{
		if (LevelUpInfo->LevelUpInformation.IsValidIndex(i))
		{
			XPAtLevelStart += LevelUpInfo->LevelUpInformation[i].LevelUpRequirement;
		}
	}

	// Coste incremental para pasar al siguiente nivel (es el rango total de la barra).
	// Si no hay siguiente nivel (nivel máximo alcanzado), la barra se muestra llena.
	if (!LevelUpInfo->LevelUpInformation.IsValidIndex(Level + 1))
	{
		OnXPPercentChanged.Broadcast(1.f);
		return;
	}

	const int32 DeltaToNextLevel = LevelUpInfo->LevelUpInformation[Level + 1].LevelUpRequirement;
	if (DeltaToNextLevel <= 0)
	{
		// Coste cero o negativo en el DA: evitamos división por cero y mostramos barra llena.
		OnXPPercentChanged.Broadcast(1.f);
		return;
	}

	const int32 XPForThisLevel = NewXP - XPAtLevelStart;
	const float XPBarPercent = static_cast<float>(XPForThisLevel) / static_cast<float>(DeltaToNextLevel);
	// Calcular y broadcastear la XP ganada en este evento (delta para el texto "+N").
	// Solo se broadcastea si hay un incremento real (no en la inicialización, donde
	// CachedXP == NewXP porque lo inicializamos antes de llamar OnXPChanged).
	const int32 XPDelta = NewXP - CachedXP;
	if (XPDelta > 0)
	{
		OnXPGained.Broadcast(XPDelta);
	}
	CachedXP = NewXP;

	OnXPPercentChanged.Broadcast(FMath::Clamp(XPBarPercent, 0.f, 1.f));
}

void UOverlayWidgetController::OnLevelChangedCallback(int32 NewLevel)
{
	// Recibe el nuevo nivel desde APantheliaPlayerState::OnLevelChangedDelegate
	// y lo rebroadcastea para que el widget de XP actualice el texto de nivel ("Nv. X").
	// El widget también puede usar esto para disparar su animación de level-up.
	OnLevelChanged.Broadcast(NewLevel);
}