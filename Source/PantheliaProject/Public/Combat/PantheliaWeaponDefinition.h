// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "ScalableFloat.h"
#include "AbilitySystem/Abilities/PantheliaGameplayAbility.h" // FAbilityAttributeScaling
#include "Combat/PantheliaWeaponTypes.h"                       // EWeaponType
#include "PantheliaElementTypes.h"                              // EPantheliaElement
#include "PantheliaWeaponDefinition.generated.h"

class UAnimMontage;
class USoundBase;

/**
 * FPantheliaWeaponAttackModifiers
 *
 * Perfil ofensivo de una CATEGORÍA de ataque del arma. El arma conserva una única
 * identidad base (DamageTypes, PoiseDamage y BuildupAmounts), mientras cada tipo
 * de golpe decide cuánto multiplica cada canal de forma independiente.
 *
 * Esta separación evita acoplar el daño de vida al daño de postura: un pesado puede
 * romper mucha más postura sin tener que inflar en la misma proporción el daño a HP.
 * También deja un punto de extensión estable para que árbol de habilidades, corazones,
 * equipo y buffs apliquen modificadores runtime encima de estos valores base mediante GAS.
 */
USTRUCT(BlueprintType)
struct PANTHELIAPROJECT_API FPantheliaWeaponAttackModifiers
{
	GENERATED_BODY()

	FPantheliaWeaponAttackModifiers() = default;

	FPantheliaWeaponAttackModifiers(
		const float InDamageMultiplier,
		const float InPoiseDamageMultiplier,
		const float InBuildupMultiplier)
		: DamageMultiplier(InDamageMultiplier)
		, PoiseDamageMultiplier(InPoiseDamageMultiplier)
		, BuildupMultiplier(InBuildupMultiplier)
	{
	}

	// Multiplica el paquete ofensivo de la categoría después de calcular:
	// DañoBase + Σ(Ratio × Atributo). Así Light, Heavy y Charged Heavy pueden
	// modificar el resultado completo sin volver a implementar el escalado.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Modifiers", meta = (ClampMin = "0.0"))
	float DamageMultiplier = 1.f;

	// Multiplica únicamente el daño de postura del golpe.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Modifiers", meta = (ClampMin = "0.0"))
	float PoiseDamageMultiplier = 1.f;

	// Multiplica únicamente el buildup elemental depositado por el golpe.
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack Modifiers", meta = (ClampMin = "0.0"))
	float BuildupMultiplier = 1.f;
};

/**
 * UPantheliaWeaponDefinition
 *
 * DataAsset que define TODOS los datos de un arma del jugador (modelo soulslike,
 * arma reemplazable que determina moveset, daño y scaling).
 *
 * Separación de responsabilidades (ver State_Combat.md, sistema de armas):
 *   - UPantheliaWeaponDefinition (este archivo) = DATOS puros del arma.
 *     Crear un asset por cada arma del juego. Sin lógica, sin presencia física.
 *   - ABaseWeapon (Actor) = PRESENCIA física en el mundo. Apunta a un WeaponDefinition
 *     y usa sus datos. Es lo que se attachea a la mano del personaje.
 *   - GA_PlayerLightAttack / HeavyAttack (abilities) = COMPORTAMIENTO. Leen el arma
 *     equipada, toman su moveset y su daño, y ejecutan el ataque vía GAS.
 *
 * Este patrón es el mismo que DA_CharacterClassInfo usa para las clases: data-driven,
 * permite crear decenas de armas rellenando datos sin tocar código ni Blueprints.
 *
 * El daño del ataque del jugador VIENE DEL ARMA (decisión de diseño): cada arma define
 * su daño base y con qué atributos del jugador escala. La ability construye el spec
 * de daño con estos valores usando el pipeline GAS (ApplyDamageScalingToSpec).
 */
UCLASS(BlueprintType)
class PANTHELIAPROJECT_API UPantheliaWeaponDefinition : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:

	// ----------------------------------------------------------------
	// IDENTIDAD Y PRESENTACIÓN
	// ----------------------------------------------------------------

	// Nombre legible del arma (para UI de inventario/equipo a futuro).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Identity")
	FText WeaponName;

	// Familia del arma. Determina el set de animaciones de locomoción/idle y lógica
	// de gameplay por familia. NO define el moveset de ataque (eso son los montages).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Identity")
	EWeaponType WeaponType = EWeaponType::Sword;

	// Elemento del arma (Fire, Water, Storm, Nature, o None/neutral).
	// Usa el MISMO enum que el corazón elemental del personaje, para permitir
	// SINERGIAS: cuando el elemento del arma coincide con el del corazón elemental
	// equipado, se puede aplicar un bonus (a daño, scaling, efectos). La lógica de
	// sinergia se implementará junto al sistema del corazón elemental; aquí el arma
	// solo declara su elemento. None = arma neutral, sin afinidad elemental.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Identity")
	EPantheliaElement WeaponElement = EPantheliaElement::None;

	// NOTA SOBRE EL MESH (decisión de arquitectura escalable):
	// El mesh del arma y su posición NO viven aquí. Cada arma es un Blueprint
	// (hijo de APantheliaWeapon) con su mesh asignado en el componente, ajustable
	// visualmente con gizmo en el viewport del BP. El DataAsset guarda solo DATOS
	// (daño, elemento, stamina, moveset); el BP guarda la PRESENCIA visual.
	// Ver Flujo_Nueva_Arma_Panthelia.md para el workflow completo.

	// ----------------------------------------------------------------
	// MOVESET — las animaciones de ataque (combo expandible)
	// ----------------------------------------------------------------

	// Cadena de ataques ligeros (light attack). El índice avanza con cada golpe
	// encadenado dentro de la ventana de combo; al llegar al final, vuelve a 0.
	// Empezamos con 3 elementos pero el array es expandible sin tocar código.
	// Cada montage debe llevar: WeaponTraceNotifyState (ventana de daño) y un
	// notify de ventana de combo (para encadenar el siguiente golpe).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Moveset")
	TArray<TObjectPtr<UAnimMontage>> LightAttackMontages;

	// Cadena de ataques ESPECIALES (tap del boton de ataque pesado). Modelo Lies of P /
	// Elden Ring: pulsacion corta = especial encadenable. El indice avanza con cada
	// golpe encadenado dentro de la ventana de combo (hasta 2 por diseno). Puede tener
	// distinta longitud que el combo ligero. Cada montage debe llevar WeaponTraceNotifyState
	// y ComboWindowNotifyState (igual que los light).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Moveset")
	TArray<TObjectPtr<UAnimMontage>> HeavyAttackMontages;

	// Golpe de APERTURA ligero al encadenar desde un dodge. No es una cadena separada:
	// tras su ComboWindowNotifyState, el sistema continúa en el índice 1 del combo ligero
	// normal. Si está vacío, el follow-up degrada con gracia al primer golpe normal.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Moveset|Dodge Followup")
	TObjectPtr<UAnimMontage> DodgeLightAttackMontage;

	// Golpe de APERTURA pesado al encadenar desde un dodge. Se reproduce de inmediato,
	// sin fase tap-vs-hold ni carga; después puede continuar en el índice 1 de la cadena
	// Heavy normal. Si está vacío, usa el primer montage pesado normal.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Moveset|Dodge Followup")
	TObjectPtr<UAnimMontage> DodgeHeavyAttackMontage;

	// Ataque PESADO CARGADO (hold del boton de ataque pesado). Modelo Elden Ring:
	// mantener presionado = golpe unico mas potente, sin cadena. Un solo montage.
	// Lleva WeaponTraceNotifyState (ventana de dano) pero NO necesita ComboWindowNotifyState
	// (no encadena). El dano se escala con ChargedHeavyDamageMultiplier.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Moveset")
	TObjectPtr<UAnimMontage> ChargedHeavyMontage;

	// Campo legacy conservado para que los WeaponDefinition existentes puedan cargar
	// el valor serializado anterior sin invalidar el asset. El runtime nuevo NO lo usa:
	// configurar ChargedHeavyAttackModifiers.DamageMultiplier en su lugar.
	UPROPERTY()
	float ChargedHeavyDamageMultiplier = 2.0f;

	// ----------------------------------------------------------------
	// PERFILES OFENSIVOS POR CATEGORÍA DE ATAQUE
	// ----------------------------------------------------------------
	// Estos perfiles multiplican por separado el daño a HP, el daño de postura y el
	// buildup base del arma. Los valores son datos base del moveset; modificadores
	// runtime del árbol/corazones/equipo se aplicarán encima mediante GAS.

	// Ataques ligeros, incluido el golpe ligero de apertura después de un dodge.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Attack Profiles")
	FPantheliaWeaponAttackModifiers LightAttackModifiers =
		FPantheliaWeaponAttackModifiers(1.f, 1.f, 1.f);

	// Ataques pesados de pulsación corta y su apertura después de un dodge.
	// Por defecto conservan el daño a HP base, pero aplican 50% más daño de postura.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Attack Profiles")
	FPantheliaWeaponAttackModifiers HeavyAttackModifiers =
		FPantheliaWeaponAttackModifiers(1.f, 1.5f, 1.f);

	// Ataque pesado cargado. Por defecto duplica el daño a HP y aplica 2.5 veces
	// el daño de postura base. El buildup queda independiente y empieza en 1.0.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Attack Profiles")
	FPantheliaWeaponAttackModifiers ChargedHeavyAttackModifiers =
		FPantheliaWeaponAttackModifiers(2.f, 2.5f, 1.f);

	// ----------------------------------------------------------------
	// SONIDO DE IMPACTO (audio del golpe al conectar)
	// ----------------------------------------------------------------

	// Sonido base de impacto del arma, reproducido cuando un ataque conecta con un objetivo.
	// Representa el "tipo de golpe" del arma (corte de espada, impacto de hacha, etc.).
	// El WeaponTraceComponent lo reproduce con PlaySoundAtLocation en el punto de hit.
	// GANCHO FUTURO (estilo souls): para que el sonido dependa tambien del MATERIAL del
	// objetivo (carne vs metal vs madera), se anadira un mapa SurfaceType->Sound que, si el
	// actor golpeado tiene un Physical Material con surface type, use ese override en vez de
	// este sonido base. El Hit.PhysMaterial ya esta disponible en el sweep del trace.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Impact")
	TObjectPtr<USoundBase> ImpactSound;

	// ----------------------------------------------------------------
	// DAÑO Y ESCALADO (viene del arma — decisión de diseño)
	// ----------------------------------------------------------------

	// Tipo(s) de daño que el arma inflige, con su curva de daño base por nivel de arma.
	// Igual estructura que UPantheliaDamageGameplayAbility::DamageTypes para que la
	// ability pueda construir el spec con el mismo pipeline (ApplyDamageScalingToSpec).
	// Ej: { Damage.Physical.Slash -> curva }.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage")
	TMap<FGameplayTag, FScalableFloat> DamageTypes;

	// Escalado por atributos del jugador (modelo LoL, declarado por cada arma).
	// Fórmula antes del perfil de ataque:
	//   Daño bruto = Daño base + Σ(Ratio × Atributo del portador)
	//
	// Ejemplo para PhysicalDamage:
	//   arma rápida  -> Attributes.Secondary.PhysicalDamage, Ratio 0.7
	//   arma lenta   -> Attributes.Secondary.PhysicalDamage, Ratio 2.1
	//
	// PhysicalDamage NO se añade automáticamente en ExecCalc_Damage. Si un arma
	// debe aprovecharlo, tiene que declararlo aquí. Máximo 2 entradas según las
	// reglas de diseño de FAbilityAttributeScaling.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage",
		meta = (TitleProperty = "AttributeTag"))
	TArray<FAbilityAttributeScaling> AttributeScalings;

	// Daño a la postura (poise) del objetivo por golpe. Armas pesadas rompen más postura.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage")
	FScalableFloat PoiseDamage;

	// ----------------------------------------------------------------
	// BUILDUP ELEMENTAL (sistema de umbral, sin azar)
	// ----------------------------------------------------------------
	// CUÁNTO llena ESTA arma la barra de estado de la víctima, POR TIPO DE DAÑO
	// (misma clave que DamageTypes). Aquí vive parte de la identidad del arma:
	// un espadón de fuego puede hacer mucho daño y poco buildup, mientras que una
	// daga de veneno puede hacer poco daño directo y acumular rápidamente su estado.
	//
	// La escala base es 0-100 porque el estado se dispara al alcanzar 100.
	// Ejemplo: 25 significa aproximadamente cuatro golpes antes de resistencias.
	// Si un tipo de daño no tiene entrada en este mapa, aplica 0 buildup.
	//
	// El arma NO define qué hace el estado ni su potencia. El payload vive en
	// DA_ElementalStatusConfig y la potencia se modifica mediante los atributos
	// XStatusPower del portador, alterados por árbol, equipo, buffs y pasivas.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Buildup")
	TMap<FGameplayTag, FScalableFloat> BuildupAmounts;

	// ----------------------------------------------------------------
	// MEJORA DE ARMA (upgrade con materiales, estilo soulslike — Titanite)
	// ----------------------------------------------------------------
	// Estructura reservada para el sistema de mejora. La LÓGICA de aplicar mejoras
	// (consumir materiales, subir el nivel) se implementará más adelante; estos
	// campos definen los límites y el escalado para que el sistema futuro los use.
	//
	// Diseño previsto: un arma tiene un nivel de mejora (0 = base, hasta MaxUpgradeLevel).
	// El nivel de mejora se usará como el "nivel" al evaluar las curvas FScalableFloat
	// de DamageTypes/PoiseDamage (GetValueAtLevel(UpgradeLevel)), de modo que mejorar
	// el arma sube su daño según la curva. Así una sola curva define todo el escalado
	// de mejora del arma sin código extra.

	// Nivel máximo de mejora que admite esta arma (ej. 10 para +10).
	// 0 significa que el arma no es mejorable.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Upgrade", meta = (ClampMin = "0"))
	int32 MaxUpgradeLevel = 10;

	// Material requerido para mejorar esta arma (ej. un mineral concreto).
	// TSoftClassPtr para no cargar el item en memoria hasta que se necesite.
	// El tipo concreto del item se definirá cuando exista el sistema de inventario;
	// por ahora es un placeholder de clase de Actor genérico.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Upgrade")
	TSoftClassPtr<AActor> RequiredUpgradeMaterial;

	// Cantidad de material requerida por cada nivel de mejora.
	// Índice 0 = coste de pasar de +0 a +1, índice 1 = de +1 a +2, etc.
	// Si está vacío o el índice no existe, el sistema futuro puede usar un coste por defecto.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Upgrade")
	TArray<int32> UpgradeMaterialCostPerLevel;

	// ----------------------------------------------------------------
	// COSTE (stamina, estilo Dark Souls: sin stamina suficiente no se ataca)
	// ----------------------------------------------------------------

	// Stamina consumida por cada ataque ligero. La ability la usa para su GE de costo;
	// si el jugador no tiene esta cantidad, el ataque no se activa.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Cost", meta = (ClampMin = "0.0"))
	float LightAttackStaminaCost = 15.f;

	// Stamina consumida por cada ataque pesado (normalmente mayor que el ligero).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Cost", meta = (ClampMin = "0.0"))
	float HeavyAttackStaminaCost = 30.f;

	// ----------------------------------------------------------------
	// DEFENSA (parry = bloqueo perfecto / bloqueo = bloqueo imperfecto)
	// ----------------------------------------------------------------
	// Terminologia del proyecto: "Parry" = bloqueo en la ventana perfecta (0.2s);
	// "Bloqueo" = bloqueo imperfecto (fuera de la ventana, mientras se mantiene guardia).
	// Los valores elementales y modificadores por arbol/corazon se aplican ENCIMA de
	// estos via ganchos (no implementados aun).

	// Multiplicador de dano FISICO recibido en un BLOQUEO fisico imperfecto.
	// 0.6 = recibes el 60% del dano fisico. El parry fisico perfecto anula el 100%.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Defense", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PhysicalImperfectBlockMultiplier = 0.6f;

	// Multiplicador de dano MAGICO recibido al parar magia con el PARRY FISICO perfecto.
	// El parry fisico contra magia solo mitiga (sin anular, sin beneficios, sin postura).
	// 0.8 = recibes el 80% del dano magico aun con parry fisico perfecto.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Defense", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MagicOnPhysicalParryMultiplier = 0.8f;

	// Multiplicador de dano MAGICO recibido en un BLOQUEO magico imperfecto.
	// El parry magico perfecto anula el 100%; el imperfecto mitiga segun este valor.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Defense", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float MagicImperfectBlockMultiplier = 0.5f;

	// Multiplicador de dano FISICO recibido al parar fisica con el PARRY MAGICO.
	// El parry magico no esta hecho para fisica: solo mitiga parcialmente, sin anular,
	// sin postura, sin beneficios. 0.7 = recibes el 70% del dano fisico.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Defense", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float PhysicalOnMagicParryMultiplier = 0.7f;

	// Dano de POSTURA al enemigo en un PARRY FISICO perfecto contra un ataque fisico.
	// Valor base por arma; los modificadores de arbol/corazon se aplican encima (gancho).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Defense", meta = (ClampMin = "0.0"))
	float PhysicalParryPoiseDamage = 30.f;

	// Dano de POSTURA al enemigo en un PARRY MAGICO perfecto contra un ataque magico.
	// Menor que el fisico (el parry magico no esta orientado a romper postura).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Defense", meta = (ClampMin = "0.0"))
	float MagicParryPoiseDamage = 12.f;

	// Campo legacy conservado para no invalidar assets existentes. El coste runtime
	// del parry perfecto ya no usa un valor fijo: se calcula como BlockStaminaCost ×
	// PerfectParryStaminaCostMultiplier para mantener una relación configurable y
	// compatible con futuros modificadores del árbol/equipamiento.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Defense", meta = (ClampMin = "0.0"))
	float ParryStaminaCost = 12.f;

	// Coste BASE de stamina al bloquear un ataque Normal.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Defense", meta = (ClampMin = "0.0"))
	float BlockStaminaCost = 10.f;

	// Multiplicador del coste base para bloquear un ataque Heavy.
	// Decisión cerrada: un pesado consume cuatro veces el coste normal.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Defense", meta = (ClampMin = "1.0"))
	float HeavyBlockStaminaCostMultiplier = 4.f;

	// Multiplicador fijo del coste base que paga CUALQUIER parry perfecto, sin importar
	// si el golpe era Normal, Heavy o Fury. 0.5 = mitad del coste normal.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Defense", meta = (ClampMin = "0.0"))
	float PerfectParryStaminaCostMultiplier = 0.5f;

	// Campo legacy conservado para no invalidar WeaponDefinitions existentes. La regla
	// definitiva de guardia rota reutiliza directamente Stagger y ya no multiplica el dano
	// de postura; no usar este valor en contenido nuevo.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Defense", meta = (ClampMin = "1.0"))
	float NoStaminaBlockPoiseMultiplier = 3.0f;

	// ----------------------------------------------------------------
	// SOCKETS DE TRACE (hitbox de la hoja)
	// ----------------------------------------------------------------

	// Nombre del socket en la base/empuñadura de la hoja (inicio del sweep de daño).
	// Debe existir en WeaponMesh. El WeaponTraceComponent lo lee para el hitbox.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace")
	FName WeaponBaseSocketName = FName("WeaponBase");

	// Nombre del socket en la punta de la hoja (fin del sweep de daño).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Trace")
	FName WeaponTipSocketName = FName("WeaponTip");
};