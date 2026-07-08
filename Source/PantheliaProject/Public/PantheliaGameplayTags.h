#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "PantheliaElementTypes.h"

/**
 * FPantheliaGameplayTags
 *
 * Singleton de tags nativos del juego. Inicializado por UPantheliaAssetManager.
 *
 * TIPOS DE DAÑO → RESISTENCIAS:
 * DamageTypesToResistances mapea cada tipo de daño a su tag de resistencia elemental.
 * Una resistencia cubre los dos tipos de su elemento (físico y mágico).
 *
 *   Damage.Physical (genérico) → (vacío — solo lo mitiga Armor, sin resistencia elemental)
 *   Damage.Physical.Ice        → Attributes.Resistance.Water
 *   Damage.Physical.Air        → Attributes.Resistance.Storm
 *   Damage.Physical.Earth      → Attributes.Resistance.Nature
 *   Damage.Magical.Fire        → Attributes.Resistance.Fire
 *   Damage.Magical.Water       → Attributes.Resistance.Water
 *   Damage.Magical.Lightning   → Attributes.Resistance.Storm
 *   Damage.Magical.Poison      → Attributes.Resistance.Nature
 *
 * En ExecCalc, para cada entrada del mapa:
 *   Key   → GetSetByCallerMagnitude (daño base del tipo)
 *   Value → tag del atributo de resistencia en el target (si es tag válido)
 */
struct FPantheliaGameplayTags
{
public:

	static const FPantheliaGameplayTags& Get() { return GameplayTags; }
	static void InitializeNativeGameplayTags();

	// --- ATRIBUTOS PRIMARIOS ---
	FGameplayTag Attributes_Primary_Hardness;
	FGameplayTag Attributes_Primary_Resonance;
	FGameplayTag Attributes_Primary_Resilience;
	FGameplayTag Attributes_Primary_Endurance;
	FGameplayTag Attributes_Primary_Spirit;

	// --- ATRIBUTOS SECUNDARIOS ---
	FGameplayTag Attributes_Secondary_MaxHealth;
	FGameplayTag Attributes_Secondary_MaxMana;
	FGameplayTag Attributes_Secondary_MaxStamina;
	FGameplayTag Attributes_Secondary_MaxPoise;
	FGameplayTag Attributes_Secondary_Armor;
	FGameplayTag Attributes_Secondary_MagicResistance;
	FGameplayTag Attributes_Secondary_Tenacity;
	FGameplayTag Attributes_Secondary_PhysicalDamage;
	FGameplayTag Attributes_Secondary_MagicDamage;
	FGameplayTag Attributes_Secondary_ArmorPenetration;
	FGameplayTag Attributes_Secondary_MagicPenetration;
	FGameplayTag Attributes_Secondary_CritChance;
	FGameplayTag Attributes_Secondary_CritDamage;

	// --- RESISTENCIAS ELEMENTALES ---
	// Atributos secundarios que reducen el daño elemental correspondiente.
	// Una resistencia cubre los dos tipos de su elemento (físico y mágico).
	// Ej: StormResistance = 15 → −15% al daño de Damage.Physical.Air Y Damage.Magical.Lightning.
	// Los aportan equipamiento y el árbol de habilidades (no se derivan de primarios).
	FGameplayTag Attributes_Resistance_Fire;     // Mitiga Damage.Magical.Fire
	FGameplayTag Attributes_Resistance_Water;    // Mitiga Damage.Physical.Ice + Damage.Magical.Water
	FGameplayTag Attributes_Resistance_Storm;    // Mitiga Damage.Physical.Air + Damage.Magical.Lightning
	FGameplayTag Attributes_Resistance_Nature;   // Mitiga Damage.Physical.Earth + Damage.Magical.Poison

	// --- META ATRIBUTOS ---
	// Tags usados como SetByCaller en los GEs que modifican meta atributos del AttributeSet.
	// Estos atributos no se muestran en la UI ni se replican — son temporales en el servidor.
	// El GE_EventBasedEffect usa Attributes.Meta.IncomingXP como SetByCallerTag para
	// transportar la XP ganada desde GA_ListenForXPEvents hasta el AttributeSet.
	FGameplayTag Attributes_Meta_IncomingXP;

	// --- INPUT TAGS ---
	// InputTag (raíz): padre de todos los input tags. NO se asigna a ninguna ability —
	// existe para hacer queries jerárquicas ("¿este tag es un input tag?") sin recurrir
	// a FGameplayTag::RequestGameplayTag con un string, que es frágil (un typo compila
	// y falla silenciosamente en runtime). Lo usa GetInputTagFromSpec en el ASC.
	FGameplayTag InputTag;
	FGameplayTag InputTag_LightAttack;
	FGameplayTag InputTag_HeavyAttack;
	FGameplayTag InputTag_Block_Physical;
	FGameplayTag InputTag_Block_Magic;
	FGameplayTag InputTag_Dodge;
	FGameplayTag InputTag_Spell_1;
	FGameplayTag InputTag_Spell_2;
	FGameplayTag InputTag_Spell_3;
	FGameplayTag InputTag_Spell_4;
	FGameplayTag InputTag_Spell_5;
	FGameplayTag InputTag_Spell_Ultimate;

	// --- TAGS DE DAÑO ---
	FGameplayTag DamageParent_Physical;    // "Damage.Physical" — tag raíz físicos
	FGameplayTag DamageParent_Magical;     // "Damage.Magical"  — tag raíz mágicos

	FGameplayTag Damage_Physical;          // genérico, sin elemento
	FGameplayTag Damage_Physical_Ice;
	FGameplayTag Damage_Physical_Air;
	FGameplayTag Damage_Physical_Earth;
	FGameplayTag Damage_Magical_Fire;
	FGameplayTag Damage_Magical_Water;
	FGameplayTag Damage_Magical_Lightning;
	FGameplayTag Damage_Magical_Poison;

	FGameplayTag Damage_Poise;

	// Mapa tipo de daño → tag de resistencia elemental del target.
	// Key   = SetByCaller tag (el ability lo asigna con este tag)
	// Value = tag del atributo de resistencia en el AttributeSet del target
	//         (vacío para Damage.Physical genérico — solo lo mitiga Armor)
	//
	// Usado en ExecCalc_Damage para:
	//   1. Iterar y leer SetByCaller por tipo (pair.Key)
	//   2. Leer la resistencia del target cuando exista (pair.Value) [clase 160]
	TMap<FGameplayTag, FGameplayTag> DamageTypesToResistances;

	// Mapa tipo de daño → elemento del atacante (para tabla de afinidades ±15%)
	TMap<FGameplayTag, EPantheliaElement> DamageTypeToElement;

	// --- TAGS DE ABILITIES ---
	// Abilities (raíz): padre de TODOS los ability tags del juego (Attack, Spell, Parry...).
	// NO se asigna a ninguna ability directamente — existe para las queries jerárquicas
	// tipo "¿este tag identifica una ability?" que hace GetAbilityTagFromSpec en el ASC.
	// Registrarlo como tag nativo (en vez de RequestGameplayTag por string) da seguridad
	// en compilación: un typo en el miembro no compila; un typo en el string sí.
	// El futuro árbol de habilidades usará esta misma raíz para identificar la ability
	// de cada nodo dentro de las specs del ASC.
	FGameplayTag Abilities;

	// Abilities.Attack: tag padre de todos los ataques de enemigos.
	// Útil para queries genéricas ("¿tiene este enemigo algún ataque?").
	// El BT NO usa este tag directamente — usa los hijos Melee/Ranged para evitar
	// activar múltiples abilities en un mismo enemigo híbrido.
	FGameplayTag Abilities_Attack;

	// Abilities.Attack.Melee: activa la ability de ataque cuerpo a cuerpo.
	// Asignar este tag en el AbilityTags de GA_MeleeAttack.
	// La rama melee del Behavior Tree activa abilities con este tag.
	FGameplayTag Abilities_Attack_Melee;

	// Abilities.Attack.Ranged: activa la ability de ataque a distancia.
	// Asignar este tag en el AbilityTags de GA_RangedAttack (y futuros hechizos enemigos).
	// La rama ranged del Behavior Tree activa abilities con este tag.
	// Necesario para enemigos híbridos: sin tags separados, TryActivateAbilitiesByTag
	// activaría TODAS las abilities de ataque a la vez.
	FGameplayTag Abilities_Attack_Ranged;

	// --- TAGS DE BOSS AI ---
	// Boss.Phase.* identifica fases del boss.
	FGameplayTag Boss_Phase;
	FGameplayTag Boss_Phase_1;
	FGameplayTag Boss_Phase_2;

	// Boss.Action.* identifica acciones data-driven de UPantheliaBossProfile.
	// GameplayAbilities debe seguir usando Abilities.Attack.* salvo decision explicita futura.
	FGameplayTag Boss_Action;
	FGameplayTag Boss_Action_Melee;
	FGameplayTag Boss_Action_Melee_Basic;
	FGameplayTag Boss_Action_Melee_Heavy;
	FGameplayTag Boss_Action_Ranged;
	FGameplayTag Boss_Action_GapCloser;
	FGameplayTag Boss_Action_Reposition;
	FGameplayTag Boss_Action_Retreat;
	FGameplayTag Boss_Action_Punish;
	FGameplayTag Boss_Action_Punish_Heal;
	FGameplayTag Boss_Action_Combo;
	FGameplayTag Boss_Action_Combo_Starter;
	FGameplayTag Boss_Action_Combo_Extender;

	// Boss.State.* representa estado runtime de BossBrain y futuro StateTree.
	FGameplayTag Boss_State;
	FGameplayTag Boss_State_Neutral;
	FGameplayTag Boss_State_SelectingAction;
	FGameplayTag Boss_State_StartingAction;
	FGameplayTag Boss_State_ActionRunning;
	FGameplayTag Boss_State_Recovering;
	FGameplayTag Boss_State_Staggered;
	FGameplayTag Boss_State_PhaseTransition;
	FGameplayTag Boss_State_Dead;

	// --- TAGS DE ABILITIES: HECHIZOS DEL JUGADOR ---
	// Jerarquía coherente con Cooldown.Spell.*: mismos niveles, mismo propósito.
	//   Abilities.Spell              → padre de todos los hechizos del jugador.
	//   Abilities.Spell.Fire         → padre de hechizos de Fuego.
	//   Abilities.Spell.Fire.Firebolt → la habilidad Firebolt concreta.
	//   .Water / .Storm / .Nature    → padres de los demás elementos (hechizos futuros).
	// ASIGNAR en cada ability Blueprint → Class Defaults → "Ability Tags":
	//   GA_Firebolt → Abilities.Spell.Fire.Firebolt
	// Al tener padres, GetAbilityTagFromSpec lo detecta (filtra hijos de "Abilities")
	// y las queries contra Abilities.Spell o Abilities.Spell.Fire también lo encuentran.
	FGameplayTag Abilities_Spell;                        // padre global de hechizos del jugador
	FGameplayTag Abilities_Spell_Fire;                   // hechizos de fuego
	FGameplayTag Abilities_Spell_Fire_Firebolt;          // Firebolt (único hechizo de fuego por ahora)
	FGameplayTag Abilities_Spell_Water;                  // hechizos de agua (futuro — Corazón de Agua)
	FGameplayTag Abilities_Spell_Storm;                  // hechizos de tormenta (futuro — Corazón de Tormenta)
	FGameplayTag Abilities_Spell_Nature;                 // hechizos de naturaleza (futuro — Corazón de Naturaleza)

	// --- TAGS DE MONTAGE ATTACK ---
	// Vinculan cada montage de ataque con su socket de hit detection.
	// GetCombatSocketLocation recibe uno de estos tags para devolver el socket correcto:
	//   Weapon    → socket en el arma equipada (FinalWeaponMesh)
	//   RightHand → socket en la mano derecha del skeletal mesh
	//   LeftHand  → socket en la mano izquierda del skeletal mesh
	// El notify AN_MontageEvent en cada montage envía el tag correspondiente.
	FGameplayTag Montage_Attack_Weapon;
	FGameplayTag Montage_Attack_RightHand;
	FGameplayTag Montage_Attack_LeftHand;
	// Sockets adicionales para ataques de bosses (patadas, mordiscos, hechizos por boca).
	// GetCombatSocketLocation los resuelve a RightFootSocket / LeftFootSocket / MouthSocket.
	FGameplayTag Montage_Attack_RightFoot;
	FGameplayTag Montage_Attack_LeftFoot;
	FGameplayTag Montage_Attack_Mouth;

	// --- TAGS DE EFECTOS ---
	FGameplayTag Effects_HitReact;
	FGameplayTag Effects_Stagger;

	// Tag de activación de GA_GetUp (post-315, Nivel 3 de knockback). Mismo patrón
	// exacto que Effects_HitReact/Effects_Stagger: GA_GetUp debe tener este tag en sus
	// Ability Tags, y APantheliaCharacterBase::Landed() la activa con
	// TryActivateAbilitiesByTag cuando el personaje aterriza tras estar State_Airborne.
	FGameplayTag Effects_GetUp;

	// Tag de activación de GA_HeavyKnockback (Nivel 2, a petición). Mismo patrón exacto
	// que Effects_HitReact/Effects_Stagger/Effects_GetUp: GA_HeavyKnockback debe tener
	// este tag en sus Ability Tags, y HandleIncomingDamage (PantheliaAttributeSet.cpp)
	// la activa con TryActivateAbilitiesByTag cuando un knockback "pesado" se confirma.
	FGameplayTag Effects_HeavyKnockback;

	// --- TAGS DE DEBUFF (efectos de estado negativos elementales) ---
	// Un "debuff" es un estado negativo que se aplica a la víctima y dura un tiempo
	// (a diferencia de un golpe, que es instantáneo). En Panthelia cada elemento tiene
	// su propio debuff, coherente con el diseño de Efectos de Estado (State_Pending §4):
	//   Fuego      → Debuff.Burn        (Quemadura)
	//   Tormenta   → Debuff.Shock       (Electrocución)
	//   Agua       → Debuff.Saturation  (Saturación)
	//   Naturaleza → Debuff.Poison      (Veneno)
	//
	// Debuff (raíz): NO se concede a nadie directamente. Existe para queries jerárquicas:
	// preguntar Container.HasTag(Debuff) responde "¿la víctima tiene ALGÚN debuff activo?"
	// de una sola vez — útil para limpiezas (cleanse), inmunidades y los iconos de estado
	// del HUD. Es el mismo patrón de padre-vacío que ya usan las raíces Cooldown, Abilities
	// e InputTag. El antiheal (Debuff.GrievousWounds, State_Pending §5) colgará de esta
	// misma raíz cuando se implemente su pipeline de curación — NO se registra aún porque
	// depende de un sistema que todavía no existe (no dejamos tags "colgando" sin su lógica).
	//
	// DECISIÓN DE DISEÑO (clase 303, adaptada del curso): el curso de Druid Mechanics usa
	// una relación 1-a-1 "un tipo de daño = un debuff" y colapsa cada ability a un solo
	// tipo de daño. Panthelia NO hace eso: mantiene el TMap DamageTypes en la damage
	// ability, porque sus 8 tipos de daño colapsan en 4 elementos (no es 1-a-1) y un
	// soulslike quiere golpes de daño mixto (arma física + elemento). Por eso el debuff
	// se busca por ELEMENTO (ver ElementToDebuff), no por tipo de daño.
	FGameplayTag Debuff;              // raíz — query "¿tiene algún debuff?" (nunca se concede sola)
	FGameplayTag Debuff_Burn;         // Fuego      → Quemadura
	FGameplayTag Debuff_Shock;        // Tormenta   → Electrocución
	FGameplayTag Debuff_Saturation;   // Agua       → Saturación
	FGameplayTag Debuff_Poison;       // Naturaleza → Veneno

	// Mapa elemento → tag de debuff que ese elemento inflige.
	// Reemplaza el "DamageTypesToDebuffs" del curso (que necesitaría 8 entradas con
	// duplicados) por 4 entradas canónicas keyeadas por elemento. La búsqueda en runtime
	// es de dos saltos, reutilizando lo que ya existe:
	//   tipo de daño --(DamageTypeToElement)--> elemento --(ElementToDebuff)--> debuff
	// Cada mapa queda mínimo y con una sola fuente de verdad. Aún NO lo consume nadie:
	// lo usará el sistema de acumulación (buildup) cuando se implemente (State_Pending §4),
	// para saber qué debuff detonar cuando la barra de un elemento se llene.
	// EPantheliaElement::None no tiene entrada (daño físico genérico = sin efecto de estado).
	TMap<EPantheliaElement, FGameplayTag> ElementToDebuff;

	// --- TAGS DE PARÁMETROS DE DEBUFF (SetByCaller, clase 304) ---
	// Distintos de los tags de arriba (Debuff_Burn, etc.): aquellos son tags de IDENTIDAD
	// (identifican QUÉ debuff es — se conceden al target mientras dura). Estos son tags
	// de TRANSPORTE (SetByCaller): solo existen para que UPantheliaDamageGameplayAbility
	// escriba sus 4 parámetros (Chance, Damage, Frequency, Duration) dentro de un
	// FGameplayEffectSpec y el ExecCalc/GE correspondiente los pueda leer por tag. Nunca
	// se conceden a ningún ASC — son claves de diccionario, igual que Attributes_Meta_IncomingXP
	// ya hace dentro de la raíz "Attributes". No son padre-hijo entre sí: son 4 tags hoja
	// independientes bajo la raíz Debuff (que ya existe arriba).
	//
	// NOTA (pendiente de clases 305+): en Panthelia el disparador del debuff será la barra
	// de acumulación (buildup), no una tirada de azar como en el curso — ver sesión de la
	// clase 303. Es posible que Debuff_Chance cambie de rol o quede sin uso cuando se
	// implemente ese disparador; se decide cuando lleguemos a esa clase, no ahora.
	FGameplayTag Debuff_Chance;      // % de probabilidad de aplicar el debuff (rol pendiente de confirmar)
	FGameplayTag Debuff_Damage;      // daño que tiquea cada Debuff_Frequency segundos
	FGameplayTag Debuff_Duration;    // segundos que dura el debuff activo
	FGameplayTag Debuff_Frequency;   // cada cuántos segundos tiquea Debuff_Damage (= "Period" del GE)

	// --- TAGS DE "COMBAT TRICKS" GENÉRICOS (clase 313) ---
	// Raíz nueva para parámetros SetByCaller de ability que NO son ni un tipo de daño
	// (raíz Damage) ni un parámetro de debuff (raíz Debuff) — valores únicos por ability,
	// sin relación con ningún tipo de daño concreto. DeathImpulseMagnitude es el primero:
	// cuánto impulso físico aplicar si esta ability da el golpe que mata. Futuros
	// parámetros de este estilo (Sección 25: Combat Tricks) irían aquí también.
	FGameplayTag CombatTricks_DeathImpulseMagnitude;

	// Knockback (clase 315): igual naturaleza que DeathImpulseMagnitude — un escalar
	// único por ability, sin relación con el tipo de daño. Se activa cuando el golpe
	// NO es fatal (a diferencia del impulso de muerte, que solo aplica en el golpe que
	// mata). KnockbackChance es análogo a Debuff_Chance: probabilidad de que este golpe
	// concreto lance al objetivo por el aire, en vez de aplicarse siempre.
	FGameplayTag CombatTricks_KnockbackForceMagnitude;
	FGameplayTag CombatTricks_KnockbackChance;

	// Nivel 2 de knockback ("empujón fuerte", a petición). NO es un sistema aparte con
	// su propio chance/magnitud — es un booleano que UPGRADEA un knockback normal (el de
	// arriba) a "pesado": misma tirada de dado, mismo vector, pero además bloquea
	// GA_HitReact y dispara una reacción distinta (GA_HeavyKnockback). Se transporta
	// como SetByCaller igual que los demás (1.0 = true, 0.0 = false — SetByCaller solo
	// mueve floats, no hay un tipo "bool" nativo para esto).
	FGameplayTag CombatTricks_KnockbackIsHeavy;

	// Launch (post-315, Nivel 3 — lanzamiento aéreo). Semánticamente distinto del
	// Knockback de arriba, aunque comparten mecanismo interno (LaunchCharacter): el
	// Knockback es un empujón horizontal que convive con HitReact; el Launch es un
	// lanzamiento vertical deliberado, reservado a fuentes específicas (explosiones,
	// golpes desde abajo), que BLOQUEA HitReact y termina en GA_GetUp al aterrizar. Se
	// mantienen como parámetros COMPLETAMENTE INDEPENDIENTES de Knockback (no
	// comparten tags ni campos de contexto) para que una ability pueda tener uno, el
	// otro, ambos, o ninguno, sin que se pisen entre sí.
	FGameplayTag CombatTricks_LaunchChance;
	FGameplayTag CombatTricks_LaunchForceMagnitude;

	// Ángulo (en grados) del pitch override al calcular la dirección del lanzamiento
	// (ver UPantheliaAbilitySystemLibrary::GetDirectionWithPitchOverride). Configurable
	// por ability, no fijo en código — algunas fuentes de daño querrán un lanzamiento
	// más vertical que otras.
	FGameplayTag CombatTricks_LaunchPitchOverride;

	// --- TAGS DE PARRY / BLOQUEO ---
	// Abilities: se asignan en el AbilityTags de GA_Parry_Physical / GA_Parry_Magic.
	FGameplayTag Abilities_Parry;
	// State: conceden las abilities de parry/bloqueo y los lee el ExecCalc para decidir
	// la mitigacion del dano entrante. Parry = ventana perfecta (0.2s); Block = bloqueo
	// imperfecto sostenido (tras la ventana, mientras se mantiene el boton).
	FGameplayTag State_Parry_Physical;
	FGameplayTag State_Parry_Magic;
	FGameplayTag State_Block_Physical;
	FGameplayTag State_Block_Magic;

	// --- i-frames genéricos (post-315, a petición) ---
	// Tag de invulnerabilidad temporal. NO es específico de ninguna ability — cualquier
	// sistema (GA_GetUp al levantarse tras un lanzamiento aéreo, y en el futuro GA_Dodge)
	// puede concederlo brevemente vía UPantheliaAbilitySystemLibrary::GrantTemporaryInvulnerability()
	// para volverse inmune a todo daño mientras dure. ExecCalc_Damage lo comprueba al
	// principio de Execute_Implementation, antes de calcular nada — si el target lo
	// tiene, el daño es cero, sin excepciones ni casos especiales por tipo de daño.
	FGameplayTag State_Invulnerable;

	// Marca que el personaje está en el aire por un lanzamiento (Nivel 3 de knockback,
	// post-315). Dos usos: (1) tag de bloqueo para GA_HitReact — un HitReact normal no
	// tiene sentido mientras vuelas por el aire; (2) señal para
	// APantheliaCharacterBase::Landed() de que ESTE aterrizaje en particular debe
	// disparar GA_GetUp (a diferencia de un salto/caída normal, que no debe hacerlo).
	// Se concede como TAG SUELTO (loose tag, AddLooseGameplayTag/SetLooseGameplayTagCount
	// — no vía un GameplayEffect) porque no sabemos de antemano cuánto va a durar el
	// vuelo (depende de la física real, no de un timer fijo) — se quita exactamente al
	// aterrizar, no tras un tiempo prefijado.
	FGameplayTag State_Airborne;

	// Nivel 2 de knockback ("empujón fuerte", a petición). Concedido brevemente (GE
	// dinámico con duración, vía GrantTemporaryGameplayTag) cuando un knockback marcado
	// como "pesado" se activa. Dos usos, igual que State.Airborne: (1) tag de bloqueo
	// para GA_HitReact — un HitReact normal no pega con salir empujado varios metros;
	// (2) condición para disparar GA_HeavyKnockback (Effects.HeavyKnockback) desde
	// HandleIncomingDamage. A diferencia de State.Airborne, este SÍ tiene una duración
	// fija (no depende de un evento físico como aterrizar) — dura lo que tarde la
	// reacción en reproducirse, así que se concede vía GE con Duration, no loose tag.
	FGameplayTag State_HeavyKnockback;

	// --- Gameplay Cues: efectos visuales y de sonido del parry/bloqueo ---
	// Cada combinacion (tipo x perfeccion) tiene su propio Cue para que el diseñador pueda
	// asignar particulas y sonidos distintos a cada caso sin tocar codigo. Se disparan desde
	// NotifyParryImpact. Los assets de Cue (GC_Parry_Physical_Perfect, etc.) se crean en
	// el editor cuando se quieran añadir efectos; hasta entonces pueden quedar vacios.
	FGameplayTag GameplayCue_Parry_Physical_Perfect; // Parry fisico perfecto
	FGameplayTag GameplayCue_Parry_Physical_Block;   // Bloqueo fisico imperfecto
	FGameplayTag GameplayCue_Parry_Magic_Perfect;    // Parry magico perfecto
	FGameplayTag GameplayCue_Parry_Magic_Block;      // Bloqueo magico imperfecto

	// Cue de impacto melee. Disparado desde el WeaponTraceComponent al detectar un hit.
	// Asset: GC_MeleeImpact (GameplayCueNotify_Static). Ver State_Combat.md.
	FGameplayTag GameplayCue_Melee_Impact;

	// --- TAGS DE COOLDOWN ---
	// Jerarquia pensada para el arbol de habilidades complejo:
	//   Cooldown                      -> raiz: TODOS los cooldowns del juego.
	//   Cooldown.Spell                -> todos los cooldowns de hechizos.
	//   Cooldown.Spell.Fire           -> hechizos del elemento Fuego.
	//   Cooldown.Spell.Fire.Firebolt  -> el hechizo concreto.
	// La jerarquia permite queries por padre: "cualquier hechizo en cooldown?" se
	// pregunta contra Cooldown.Spell; "algun hechizo de fuego?" contra Cooldown.Spell.Fire.
	// Un nodo del arbol que reduzca cooldowns de fuego apuntaria a Cooldown.Spell.Fire.
	// Cada GE_Cooldown_X concede su tag hoja (el mas especifico) como Granted Tag; al tener
	// padres, las queries de nivel superior siguen funcionando automaticamente.
	//
	// Los padres (Cooldown, Cooldown.Spell, Cooldown.Spell.Fire) se registran como tags
	// nativos vacios para que existan en la jerarquia aunque nunca se concedan directamente.
	FGameplayTag Cooldown;                       // raiz
	FGameplayTag Cooldown_Spell;                 // todos los hechizos
	FGameplayTag Cooldown_Spell_Fire;            // hechizos de fuego
	FGameplayTag Cooldown_Spell_Fire_Firebolt;   // Firebolt concreto
	// Padres de los demás elementos (las hojas se añadirán con cada nuevo hechizo).
	FGameplayTag Cooldown_Spell_Water;           // hechizos de agua (futuro)
	FGameplayTag Cooldown_Spell_Storm;           // hechizos de tormenta (futuro)
	FGameplayTag Cooldown_Spell_Nature;          // hechizos de naturaleza (futuro)

private:
	static FPantheliaGameplayTags GameplayTags;
};