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

	// Ataque PESADO CARGADO (hold del boton de ataque pesado). Modelo Elden Ring:
	// mantener presionado = golpe unico mas potente, sin cadena. Un solo montage.
	// Lleva WeaponTraceNotifyState (ventana de dano) pero NO necesita ComboWindowNotifyState
	// (no encadena). El dano se escala con ChargedHeavyDamageMultiplier.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Moveset")
	TObjectPtr<UAnimMontage> ChargedHeavyMontage;

	// Multiplicador de dano del pesado cargado respecto al dano base del arma.
	// Ej. 2.0 = el cargado hace el doble del dano base. El especial (tap) usa el dano
	// base sin multiplicar (o podriamos darle su propio multiplicador en el futuro).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Moveset")
	float ChargedHeavyDamageMultiplier = 2.0f;

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

	// Escalado por atributos del jugador (modelo de stats que ya usan las abilities).
	// Máximo 2 entradas (regla de diseño de FAbilityAttributeScaling).
	// Ej: espada que escala con un atributo de fuerza al 60%.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage",
		meta = (TitleProperty = "AttributeTag"))
	TArray<FAbilityAttributeScaling> AttributeScalings;

	// Daño a la postura (poise) del objetivo por golpe. Armas pesadas rompen más postura.
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Damage")
	FScalableFloat PoiseDamage;

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

	// Stamina consumida al ejecutar un Parry (bloqueo perfecto).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Defense", meta = (ClampMin = "0.0"))
	float ParryStaminaCost = 12.f;

	// Stamina consumida al mantener un Bloqueo (imperfecto).
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Weapon|Defense", meta = (ClampMin = "0.0"))
	float BlockStaminaCost = 10.f;

	// Multiplicador del dano de POSTURA que RECIBE el jugador si bloquea SIN estamina
	// suficiente (guardia rota). 3.0 = triplica el dano de postura recibido. Ajustable por
	// corazon/tipo de dano via ganchos.
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