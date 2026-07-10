#include "PantheliaGameplayTags.h"
#include "GameplayTagsManager.h"

FPantheliaGameplayTags FPantheliaGameplayTags::GameplayTags;

void FPantheliaGameplayTags::InitializeNativeGameplayTags()
{
	UGameplayTagsManager& Manager = UGameplayTagsManager::Get();

	// --- ATRIBUTOS PRIMARIOS ---
	GameplayTags.Attributes_Primary_Hardness = Manager.AddNativeGameplayTag(FName("Attributes.Primary.Hardness"), FString("Aumenta el daño físico y la eficacia de los ataques con arma."));
	GameplayTags.Attributes_Primary_Resonance = Manager.AddNativeGameplayTag(FName("Attributes.Primary.Resonance"), FString("Aumenta el daño mágico y la eficacia de los hechizos."));
	GameplayTags.Attributes_Primary_Resilience = Manager.AddNativeGameplayTag(FName("Attributes.Primary.Resilience"), FString("Aumenta la vida máxima y la armadura."));
	GameplayTags.Attributes_Primary_Endurance = Manager.AddNativeGameplayTag(FName("Attributes.Primary.Endurance"), FString("Aumenta la stamina máxima, la postura máxima y la tenacidad."));
	GameplayTags.Attributes_Primary_Spirit = Manager.AddNativeGameplayTag(FName("Attributes.Primary.Spirit"), FString("Aumenta el maná máximo y la resistencia mágica."));

	// --- ATRIBUTOS SECUNDARIOS ---
	GameplayTags.Attributes_Secondary_MaxHealth = Manager.AddNativeGameplayTag(FName("Attributes.Secondary.MaxHealth"), FString("Vida máxima."));
	GameplayTags.Attributes_Secondary_MaxMana = Manager.AddNativeGameplayTag(FName("Attributes.Secondary.MaxMana"), FString("Maná máximo."));
	GameplayTags.Attributes_Secondary_MaxStamina = Manager.AddNativeGameplayTag(FName("Attributes.Secondary.MaxStamina"), FString("Stamina máxima."));
	GameplayTags.Attributes_Secondary_MaxPoise = Manager.AddNativeGameplayTag(FName("Attributes.Secondary.MaxPoise"), FString("Postura máxima."));
	GameplayTags.Attributes_Secondary_Armor = Manager.AddNativeGameplayTag(FName("Attributes.Secondary.Armor"), FString("Reduce el daño físico recibido."));
	GameplayTags.Attributes_Secondary_MagicResistance = Manager.AddNativeGameplayTag(FName("Attributes.Secondary.MagicResistance"), FString("Reduce el daño mágico recibido."));
	GameplayTags.Attributes_Secondary_Tenacity = Manager.AddNativeGameplayTag(FName("Attributes.Secondary.Tenacity"), FString("Reduce la duración de los efectos de control."));
	GameplayTags.Attributes_Secondary_PhysicalDamage = Manager.AddNativeGameplayTag(FName("Attributes.Secondary.PhysicalDamage"), FString("Daño físico base."));
	GameplayTags.Attributes_Secondary_MagicDamage = Manager.AddNativeGameplayTag(FName("Attributes.Secondary.MagicDamage"), FString("Daño mágico base."));
	GameplayTags.Attributes_Secondary_ArmorPenetration = Manager.AddNativeGameplayTag(FName("Attributes.Secondary.ArmorPenetration"), FString("Ignora parte de la armadura del enemigo."));
	GameplayTags.Attributes_Secondary_MagicPenetration = Manager.AddNativeGameplayTag(FName("Attributes.Secondary.MagicPenetration"), FString("Ignora parte de la resistencia mágica del enemigo."));
	GameplayTags.Attributes_Secondary_CritChance = Manager.AddNativeGameplayTag(FName("Attributes.Secondary.CritChance"), FString("Probabilidad de crítico."));
	GameplayTags.Attributes_Secondary_CritDamage = Manager.AddNativeGameplayTag(FName("Attributes.Secondary.CritDamage"), FString("Bonus flat de daño en crítico."));

	// --- RESISTENCIAS ELEMENTALES ---
	// Una resistencia cubre los dos tipos de su elemento (físico y mágico).
	GameplayTags.Attributes_Resistance_Fire = Manager.AddNativeGameplayTag(FName("Attributes.Resistance.Fire"), FString("Resistencia al daño de Fuego (Damage.Magical.Fire)."));
	GameplayTags.Attributes_Resistance_Water = Manager.AddNativeGameplayTag(FName("Attributes.Resistance.Water"), FString("Resistencia al daño de Agua (Damage.Physical.Ice + Damage.Magical.Water)."));
	GameplayTags.Attributes_Resistance_Storm = Manager.AddNativeGameplayTag(FName("Attributes.Resistance.Storm"), FString("Resistencia al daño de Tormenta (Damage.Physical.Air + Damage.Magical.Lightning)."));
	GameplayTags.Attributes_Resistance_Nature = Manager.AddNativeGameplayTag(FName("Attributes.Resistance.Nature"), FString("Resistencia al daño de Naturaleza (Damage.Physical.Earth + Damage.Magical.Poison)."));


	// --- META ATRIBUTOS ---
	// Tag usado como SetByCallerTag en GE_EventBasedEffect para transportar la XP ganada.
	// GA_ListenForXPEvents escucha GameplayEvents con tag padre "Attributes" (no exact match),
	// y cuando recibe uno con este tag específico, aplica el GE con la magnitude del evento.
	// El AttributeSet lo consume en PostGameplayEffectExecute → bloque IncomingXP.
	GameplayTags.Attributes_Meta_IncomingXP = Manager.AddNativeGameplayTag(FName("Attributes.Meta.IncomingXP"), FString("Meta atributo de XP entrante. No se muestra en UI ni se replica."));

	// --- INPUT TAGS ---
	// InputTag (raíz): padre de todos los input tags. Solo para queries jerárquicas
	// (GetInputTagFromSpec) — ninguna ability lo lleva asignado directamente.
	// Nota: los hijos (InputTag.LightAttack, etc.) ya creaban este padre implícitamente
	// en la jerarquía de GAS; registrarlo explícitamente solo lo convierte en un miembro
	// C++ seguro en compilación. No cambia nada en el editor ni en los assets.
	GameplayTags.InputTag = Manager.AddNativeGameplayTag(FName("InputTag"), FString("Raíz de todos los input tags. Solo para queries jerárquicas."));
	GameplayTags.InputTag_LightAttack = Manager.AddNativeGameplayTag(FName("InputTag.LightAttack"), FString());
	GameplayTags.InputTag_HeavyAttack = Manager.AddNativeGameplayTag(FName("InputTag.HeavyAttack"), FString());
	GameplayTags.InputTag_Block_Physical = Manager.AddNativeGameplayTag(FName("InputTag.Block.Physical"), FString());
	GameplayTags.InputTag_Block_Magic = Manager.AddNativeGameplayTag(FName("InputTag.Block.Magic"), FString());
	GameplayTags.InputTag_Dodge = Manager.AddNativeGameplayTag(FName("InputTag.Dodge"), FString());
	GameplayTags.InputTag_Spell_1 = Manager.AddNativeGameplayTag(FName("InputTag.Spell.1"), FString());
	GameplayTags.InputTag_Spell_2 = Manager.AddNativeGameplayTag(FName("InputTag.Spell.2"), FString());
	GameplayTags.InputTag_Spell_3 = Manager.AddNativeGameplayTag(FName("InputTag.Spell.3"), FString());
	GameplayTags.InputTag_Spell_4 = Manager.AddNativeGameplayTag(FName("InputTag.Spell.4"), FString());
	GameplayTags.InputTag_Spell_5 = Manager.AddNativeGameplayTag(FName("InputTag.Spell.5"), FString());
	GameplayTags.InputTag_Spell_Ultimate = Manager.AddNativeGameplayTag(FName("InputTag.Spell.Ultimate"), FString());

	// --- TIPOS DE DAÑO ---
	GameplayTags.DamageParent_Physical = Manager.AddNativeGameplayTag(FName("Damage.Physical"), FString("Tag raíz para todos los daños físicos. Mitigado por Armor."));
	GameplayTags.DamageParent_Magical = Manager.AddNativeGameplayTag(FName("Damage.Magical"), FString("Tag raíz para todos los daños mágicos. Mitigado por MagicResistance."));

	GameplayTags.Damage_Physical = GameplayTags.DamageParent_Physical; // genérico — sin elemento
	GameplayTags.Damage_Physical_Ice = Manager.AddNativeGameplayTag(FName("Damage.Physical.Ice"), FString("Daño físico de Hielo. Elemento: Agua."));
	GameplayTags.Damage_Physical_Air = Manager.AddNativeGameplayTag(FName("Damage.Physical.Air"), FString("Daño físico de Aire. Elemento: Tormenta."));
	GameplayTags.Damage_Physical_Earth = Manager.AddNativeGameplayTag(FName("Damage.Physical.Earth"), FString("Daño físico de Tierra. Elemento: Naturaleza."));
	GameplayTags.Damage_Magical_Fire = Manager.AddNativeGameplayTag(FName("Damage.Magical.Fire"), FString("Daño mágico de Fuego. Elemento: Fuego."));
	GameplayTags.Damage_Magical_Water = Manager.AddNativeGameplayTag(FName("Damage.Magical.Water"), FString("Daño mágico de Agua. Elemento: Agua."));
	GameplayTags.Damage_Magical_Lightning = Manager.AddNativeGameplayTag(FName("Damage.Magical.Lightning"), FString("Daño mágico de Rayo. Elemento: Tormenta."));
	GameplayTags.Damage_Magical_Poison = Manager.AddNativeGameplayTag(FName("Damage.Magical.Poison"), FString("Daño mágico de Veneno. Elemento: Naturaleza."));
	GameplayTags.Damage_Poise = Manager.AddNativeGameplayTag(FName("Damage.Poise"), FString("Daño a la postura. No mitigado por Armor ni MagicResistance."));

	// ============================================================
	// Mapa tipo de daño → tag de resistencia elemental
	// ============================================================
	// Key   = tag del tipo de daño (SetByCaller del ability)
	// Value = tag del atributo de resistencia en el target
	//         FGameplayTag() (vacío) = sin resistencia elemental (solo Armor lo mitiga)
	//
	// NOTA: Damage.Physical (genérico) se mapea a FGameplayTag() vacío.
	// En ExecCalc: si ResistanceTag.IsValid() == false → no se aplica resistencia elemental.

	GameplayTags.DamageTypesToResistances.Add(GameplayTags.Damage_Physical, FGameplayTag());  // sin resist elemental
	GameplayTags.DamageTypesToResistances.Add(GameplayTags.Damage_Physical_Ice, GameplayTags.Attributes_Resistance_Water);
	GameplayTags.DamageTypesToResistances.Add(GameplayTags.Damage_Physical_Air, GameplayTags.Attributes_Resistance_Storm);
	GameplayTags.DamageTypesToResistances.Add(GameplayTags.Damage_Physical_Earth, GameplayTags.Attributes_Resistance_Nature);
	GameplayTags.DamageTypesToResistances.Add(GameplayTags.Damage_Magical_Fire, GameplayTags.Attributes_Resistance_Fire);
	GameplayTags.DamageTypesToResistances.Add(GameplayTags.Damage_Magical_Water, GameplayTags.Attributes_Resistance_Water);
	GameplayTags.DamageTypesToResistances.Add(GameplayTags.Damage_Magical_Lightning, GameplayTags.Attributes_Resistance_Storm);
	GameplayTags.DamageTypesToResistances.Add(GameplayTags.Damage_Magical_Poison, GameplayTags.Attributes_Resistance_Nature);

	// ============================================================
	// Mapa tipo de daño → elemento del atacante (para tabla de afinidades ±15%)
	// ============================================================
	GameplayTags.DamageTypeToElement.Add(GameplayTags.Damage_Physical_Ice, EPantheliaElement::Water);
	GameplayTags.DamageTypeToElement.Add(GameplayTags.Damage_Physical_Air, EPantheliaElement::Storm);
	GameplayTags.DamageTypeToElement.Add(GameplayTags.Damage_Physical_Earth, EPantheliaElement::Nature);
	GameplayTags.DamageTypeToElement.Add(GameplayTags.Damage_Magical_Fire, EPantheliaElement::Fire);
	GameplayTags.DamageTypeToElement.Add(GameplayTags.Damage_Magical_Water, EPantheliaElement::Water);
	GameplayTags.DamageTypeToElement.Add(GameplayTags.Damage_Magical_Lightning, EPantheliaElement::Storm);
	GameplayTags.DamageTypeToElement.Add(GameplayTags.Damage_Magical_Poison, EPantheliaElement::Nature);
	// Damage.Physical genérico no tiene elemento → devuelve nullptr → None → neutro en tabla

	// --- TAGS DE ABILITIES ---
	// Abilities (raíz): padre de todos los ability tags. Solo para las queries
	// jerárquicas de GetAbilityTagFromSpec — ninguna ability lo lleva directamente.
	// Igual que con la raíz InputTag: los hijos ya lo creaban implícitamente en la
	// jerarquía; el registro explícito solo lo expone como miembro C++ seguro.
	GameplayTags.Abilities = Manager.AddNativeGameplayTag(FName("Abilities"), FString("Raíz de todos los ability tags. Solo para queries jerárquicas."));

	// Abilities.Attack: tag padre — útil para queries genéricas.
	// El BT NO lo usa directamente; usa los hijos Melee/Ranged para evitar
	// activar múltiples abilities en un mismo enemigo híbrido.
	GameplayTags.Abilities_Attack = Manager.AddNativeGameplayTag(FName("Abilities.Attack"), FString("Tag padre de todos los ataques de enemigos."));

	// Abilities.Attack.Melee: activa la ability de ataque cuerpo a cuerpo.
	// Asignar en el AbilityTags de GA_MeleeAttack.
	// La rama melee del Behavior Tree usa este tag en BTT_Attack.
	GameplayTags.Abilities_Attack_Melee = Manager.AddNativeGameplayTag(FName("Abilities.Attack.Melee"), FString("Activa la ability de ataque melee del enemigo."));

	// Abilities.Attack.Ranged: activa la ability de ataque a distancia/mágico.
	// Asignar en el AbilityTags de GA_RangedAttack y futuros hechizos de enemigos.
	// La rama ranged del Behavior Tree usa este tag en BTT_Attack.
	// Necesario para enemigos híbridos: tags separados evitan que TryActivateAbilitiesByTag
	// active melee y ranged a la vez cuando el enemigo tiene ambas abilities.
	GameplayTags.Abilities_Attack_Ranged = Manager.AddNativeGameplayTag(FName("Abilities.Attack.Ranged"), FString("Activa la ability de ataque a distancia/mágico del enemigo."));

	// --- TAGS DE ABILITIES: BOSSES ---
	// Hojas específicas para acciones de jefes. BossProfile.ActionDefinitions debe usar
	// estas hojas exactas como AbilityTag cuando una acción necesita ejecutar una
	// GameplayAbility concreta y no el ataque genérico compartido por enemigos normales.
	GameplayTags.Abilities_Boss = Manager.AddNativeGameplayTag(
		FName("Abilities.Boss"),
		FString("Padre de todas las abilities específicas de bosses."));
	GameplayTags.Abilities_Boss_WarriorBoss = Manager.AddNativeGameplayTag(
		FName("Abilities.Boss.WarriorBoss"),
		FString("Padre de abilities específicas del WarriorBoss."));
	GameplayTags.Abilities_Boss_WarriorBoss_ShortSlash = Manager.AddNativeGameplayTag(
		FName("Abilities.Boss.WarriorBoss.ShortSlash"),
		FString("WarriorBoss: tajo corto/rápido."));
	GameplayTags.Abilities_Boss_WarriorBoss_HeavySlash = Manager.AddNativeGameplayTag(
		FName("Abilities.Boss.WarriorBoss.HeavySlash"),
		FString("WarriorBoss: tajo pesado."));
	GameplayTags.Abilities_Boss_WarriorBoss_WideSlash = Manager.AddNativeGameplayTag(
		FName("Abilities.Boss.WarriorBoss.WideSlash"),
		FString("WarriorBoss: barrido amplio para cubrir ángulos laterales."));
	GameplayTags.Abilities_Boss_WarriorBoss_GapCloser = Manager.AddNativeGameplayTag(
		FName("Abilities.Boss.WarriorBoss.GapCloser"),
		FString("WarriorBoss: acción de cierre de distancia."));

	// --- TAGS DE BOSS AI ---
	// Boss.Phase.* identifica fases del boss.
	GameplayTags.Boss_Phase = Manager.AddNativeGameplayTag(
		FName("Boss.Phase"),
		FString("Raiz de tags de fase de boss."));
	GameplayTags.Boss_Phase_1 = Manager.AddNativeGameplayTag(
		FName("Boss.Phase.1"),
		FString("Fase 1 del boss."));
	GameplayTags.Boss_Phase_2 = Manager.AddNativeGameplayTag(
		FName("Boss.Phase.2"),
		FString("Fase 2 del boss."));

	// Boss.Action.* identifica acciones data-driven de UPantheliaBossProfile.
	// GameplayAbilities debe seguir usando Abilities.Attack.* salvo decision explicita futura.
	GameplayTags.Boss_Action = Manager.AddNativeGameplayTag(
		FName("Boss.Action"),
		FString("Raiz de acciones data-driven de BossProfile."));
	GameplayTags.Boss_Action_Melee = Manager.AddNativeGameplayTag(
		FName("Boss.Action.Melee"),
		FString("Acciones melee de boss."));
	GameplayTags.Boss_Action_Melee_Basic = Manager.AddNativeGameplayTag(
		FName("Boss.Action.Melee.Basic"),
		FString("Accion melee basica de boss. Tag genérico conservado para compatibilidad."));
	GameplayTags.Boss_Action_Melee_ShortSlash = Manager.AddNativeGameplayTag(
		FName("Boss.Action.Melee.ShortSlash"),
		FString("Accion melee rápida/corta de boss."));
	GameplayTags.Boss_Action_Melee_Heavy = Manager.AddNativeGameplayTag(
		FName("Boss.Action.Melee.Heavy"),
		FString("Accion melee pesada de boss."));
	GameplayTags.Boss_Action_Melee_WideSlash = Manager.AddNativeGameplayTag(
		FName("Boss.Action.Melee.WideSlash"),
		FString("Accion melee de barrido amplio de boss."));
	GameplayTags.Boss_Action_Ranged = Manager.AddNativeGameplayTag(
		FName("Boss.Action.Ranged"),
		FString("Accion ranged de boss."));
	GameplayTags.Boss_Action_GapCloser = Manager.AddNativeGameplayTag(
		FName("Boss.Action.GapCloser"),
		FString("Accion de acercamiento de boss."));
	GameplayTags.Boss_Action_GapCloser_Leap = Manager.AddNativeGameplayTag(
		FName("Boss.Action.GapCloser.Leap"),
		FString("Accion de salto/cierre de distancia de boss."));
	GameplayTags.Boss_Action_Reposition = Manager.AddNativeGameplayTag(
		FName("Boss.Action.Reposition"),
		FString("Accion de reposicionamiento de boss."));
	GameplayTags.Boss_Action_Retreat = Manager.AddNativeGameplayTag(
		FName("Boss.Action.Retreat"),
		FString("Accion de retirada de boss."));
	GameplayTags.Boss_Action_Punish = Manager.AddNativeGameplayTag(
		FName("Boss.Action.Punish"),
		FString("Accion de castigo de boss."));
	GameplayTags.Boss_Action_Punish_Heal = Manager.AddNativeGameplayTag(
		FName("Boss.Action.Punish.Heal"),
		FString("Accion de castigo contra curacion."));
	GameplayTags.Boss_Action_Combo = Manager.AddNativeGameplayTag(
		FName("Boss.Action.Combo"),
		FString("Acciones de combo de boss."));
	GameplayTags.Boss_Action_Combo_Starter = Manager.AddNativeGameplayTag(
		FName("Boss.Action.Combo.Starter"),
		FString("Accion que inicia combo de boss."));
	GameplayTags.Boss_Action_Combo_Extender = Manager.AddNativeGameplayTag(
		FName("Boss.Action.Combo.Extender"),
		FString("Accion que extiende combo de boss."));

	// Boss.State.* representa estado runtime de BossBrain y futuro StateTree.
	GameplayTags.Boss_State = Manager.AddNativeGameplayTag(
		FName("Boss.State"),
		FString("Raiz de estados runtime de Boss AI."));
	GameplayTags.Boss_State_Neutral = Manager.AddNativeGameplayTag(
		FName("Boss.State.Neutral"),
		FString("Boss en estado neutral."));
	GameplayTags.Boss_State_SelectingAction = Manager.AddNativeGameplayTag(
		FName("Boss.State.SelectingAction"),
		FString("BossBrain seleccionando accion."));
	GameplayTags.Boss_State_StartingAction = Manager.AddNativeGameplayTag(
		FName("Boss.State.StartingAction"),
		FString("Boss iniciando accion seleccionada."));
	GameplayTags.Boss_State_ActionRunning = Manager.AddNativeGameplayTag(
		FName("Boss.State.ActionRunning"),
		FString("Boss ejecutando accion."));
	GameplayTags.Boss_State_Recovering = Manager.AddNativeGameplayTag(
		FName("Boss.State.Recovering"),
		FString("Boss en recuperacion tras accion."));
	GameplayTags.Boss_State_Staggered = Manager.AddNativeGameplayTag(
		FName("Boss.State.Staggered"),
		FString("Boss staggered."));
	GameplayTags.Boss_State_PhaseTransition = Manager.AddNativeGameplayTag(
		FName("Boss.State.PhaseTransition"),
		FString("Boss en transicion de fase."));
	GameplayTags.Boss_State_Dead = Manager.AddNativeGameplayTag(
		FName("Boss.State.Dead"),
		FString("Boss muerto."));

	// --- TAGS DE ABILITIES: HECHIZOS DEL JUGADOR ---
	// Jerarquía coherente con Cooldown.Spell.*: mismos niveles, mismo propósito.
	// ASIGNAR en cada ability Blueprint → Class Defaults → "Ability Tags":
	//   GA_Firebolt → Abilities.Spell.Fire.Firebolt
	// Al tener padres, GetAbilityTagFromSpec lo detecta (filtra hijos de "Abilities") y
	// las queries contra Abilities.Spell o Abilities.Spell.Fire también lo encuentran.
	// Las hojas de Agua/Tormenta/Naturaleza se añadirán con cada Corazón Elemental.
	GameplayTags.Abilities_Spell = Manager.AddNativeGameplayTag(FName("Abilities.Spell"), FString("Padre de todos los hechizos del jugador. Query: '¿tiene algún hechizo activo?'"));
	GameplayTags.Abilities_Spell_Fire = Manager.AddNativeGameplayTag(FName("Abilities.Spell.Fire"), FString("Padre de hechizos de Fuego. Otorgados por el Corazón de Fuego."));
	GameplayTags.Abilities_Spell_Fire_Firebolt = Manager.AddNativeGameplayTag(FName("Abilities.Spell.Fire.Firebolt"), FString("Hechizo Firebolt. Asignar en GA_Firebolt → Ability Tags."));
	GameplayTags.Abilities_Spell_Water = Manager.AddNativeGameplayTag(FName("Abilities.Spell.Water"), FString("Padre de hechizos de Agua. Otorgados por el Corazón de Agua (futuro)."));
	GameplayTags.Abilities_Spell_Storm = Manager.AddNativeGameplayTag(FName("Abilities.Spell.Storm"), FString("Padre de hechizos de Tormenta. Otorgados por el Corazón de Tormenta (futuro)."));
	GameplayTags.Abilities_Spell_Nature = Manager.AddNativeGameplayTag(FName("Abilities.Spell.Nature"), FString("Padre de hechizos de Naturaleza. Otorgados por el Corazón de Naturaleza (futuro)."));

	// --- TAGS DE MONTAGE ATTACK ---
	// Cada montage de ataque envía uno de estos tags via AN_MontageEvent.
	// GetCombatSocketLocation los usa para devolver el socket correcto del hit.
	GameplayTags.Montage_Attack_Weapon = Manager.AddNativeGameplayTag(FName("Montage.Attack.Weapon"), FString("Socket de hit en el arma equipada."));
	GameplayTags.Montage_Attack_RightHand = Manager.AddNativeGameplayTag(FName("Montage.Attack.RightHand"), FString("Socket de hit en la mano derecha."));
	GameplayTags.Montage_Attack_LeftHand = Manager.AddNativeGameplayTag(FName("Montage.Attack.LeftHand"), FString("Socket de hit en la mano izquierda."));
	GameplayTags.Montage_Attack_RightFoot = Manager.AddNativeGameplayTag(FName("Montage.Attack.RightFoot"), FString("Socket de hit en el pie derecho (patadas de bosses)."));
	GameplayTags.Montage_Attack_LeftFoot = Manager.AddNativeGameplayTag(FName("Montage.Attack.LeftFoot"), FString("Socket de hit en el pie izquierdo (patadas de bosses)."));
	GameplayTags.Montage_Attack_Mouth = Manager.AddNativeGameplayTag(FName("Montage.Attack.Mouth"), FString("Socket de hit/spawn en la boca (mordiscos, alientos elementales)."));

	// --- TAGS DE EFECTOS ---
	GameplayTags.Effects_HitReact = Manager.AddNativeGameplayTag(FName("Effects.HitReact"), FString("Otorgado por GE_HitReact. Inmoviliza al enemigo durante el hit react."));
	GameplayTags.Effects_Stagger = Manager.AddNativeGameplayTag(FName("Effects.Stagger"), FString("Otorgado cuando la postura llega a 0. Aturdimiento prolongado."));
	GameplayTags.Effects_GetUp = Manager.AddNativeGameplayTag(FName("Effects.GetUp"), FString("Tag de activacion de GA_GetUp. La dispara Landed() al aterrizar desde State.Airborne."));
	GameplayTags.Effects_HeavyKnockback = Manager.AddNativeGameplayTag(FName("Effects.HeavyKnockback"), FString("Tag de activacion de GA_HeavyKnockback (Nivel 2). La dispara HandleIncomingDamage."));

	// --- TAGS DE DEBUFF (efectos de estado negativos elementales) ---
	// Un debuff por elemento (ver el header para la decisión de diseño completa). La raíz
	// "Debuff" se registra como tag nativo vacío (igual que Cooldown/Abilities/InputTag):
	// existe en la jerarquía para permitir la query "¿tiene algún debuff?" (HasTag con
	// MatchesTag incluye a los hijos), aunque ningún GE la conceda directamente.
	GameplayTags.Debuff = Manager.AddNativeGameplayTag(
		FName("Debuff"),
		FString("Raíz de todos los debuffs. Solo para queries jerárquicas (¿tiene algún debuff?)."));
	GameplayTags.Debuff_Burn = Manager.AddNativeGameplayTag(
		FName("Debuff.Burn"),
		FString("Quemadura (elemento Fuego). Daño de fuego periódico durante una duración."));
	GameplayTags.Debuff_Shock = Manager.AddNativeGameplayTag(
		FName("Debuff.Shock"),
		FString("Electrocución (elemento Tormenta). Efecto de daño/control por rayo (mecánica por balancear)."));
	GameplayTags.Debuff_Saturation = Manager.AddNativeGameplayTag(
		FName("Debuff.Saturation"),
		FString("Saturación (elemento Agua). Empapado — efecto pendiente de decidir/balancear."));
	GameplayTags.Debuff_Poison = Manager.AddNativeGameplayTag(
		FName("Debuff.Poison"),
		FString("Veneno (elemento Naturaleza). Daño por tiempo acumulativo (mecánica por decidir, State_Pending §4)."));

	// ============================================================
	// Mapa elemento → tag de debuff
	// ============================================================
	// 4 entradas canónicas (una por elemento). None NO aparece: el daño físico genérico
	// no inflige debuff elemental. Lo consumirá el sistema de buildup al detonar la barra
	// de un elemento; hoy nadie lo lee todavía (infraestructura adelantada, barata y estable).
	GameplayTags.ElementToDebuff.Add(EPantheliaElement::Fire, GameplayTags.Debuff_Burn);
	GameplayTags.ElementToDebuff.Add(EPantheliaElement::Storm, GameplayTags.Debuff_Shock);
	GameplayTags.ElementToDebuff.Add(EPantheliaElement::Water, GameplayTags.Debuff_Saturation);
	GameplayTags.ElementToDebuff.Add(EPantheliaElement::Nature, GameplayTags.Debuff_Poison);

	// Mapa elemento → tag de resistencia (ver header). Mismas 4 entradas canónicas.
	GameplayTags.ElementToResistance.Add(EPantheliaElement::Fire, GameplayTags.Attributes_Resistance_Fire);
	GameplayTags.ElementToResistance.Add(EPantheliaElement::Storm, GameplayTags.Attributes_Resistance_Storm);
	GameplayTags.ElementToResistance.Add(EPantheliaElement::Water, GameplayTags.Attributes_Resistance_Water);
	GameplayTags.ElementToResistance.Add(EPantheliaElement::Nature, GameplayTags.Attributes_Resistance_Nature);

	// --- TAGS DE PARÁMETROS DE DEBUFF (SetByCaller, clase 304) ---
	// Ver la explicación completa en el header. Se registran como hojas sueltas bajo la
	// raíz Debuff (ya registrada arriba); nunca se conceden a un ASC, solo transportan
	// magnitudes dentro de un FGameplayEffectSpec.
	// NOTA: Debuff.Chance fue ELIMINADO (decisión cerrada del sistema de buildup): el
	// disparador de los estados es el umbral de acumulación, sin azar. Ver el header.
	GameplayTags.Debuff_Damage = Manager.AddNativeGameplayTag(
		FName("Debuff.Damage"),
		FString("SetByCaller: daño que tiquea el debuff cada Debuff.Frequency segundos."));
	GameplayTags.Debuff_Duration = Manager.AddNativeGameplayTag(
		FName("Debuff.Duration"),
		FString("SetByCaller: duración total en segundos del debuff activo."));
	GameplayTags.Debuff_Frequency = Manager.AddNativeGameplayTag(
		FName("Debuff.Frequency"),
		FString("SetByCaller: cada cuántos segundos tiquea Debuff.Damage (el 'Period' del GE periódico)."));

	// --- TAGS DE "COMBAT TRICKS" GENÉRICOS (clase 313) ---
	GameplayTags.CombatTricks_DeathImpulseMagnitude = Manager.AddNativeGameplayTag(
		FName("CombatTricks.DeathImpulseMagnitude"),
		FString("SetByCaller: magnitud del impulso físico a aplicar si esta ability da el golpe que mata."));

	// Knockback (clase 315)
	GameplayTags.CombatTricks_KnockbackForceMagnitude = Manager.AddNativeGameplayTag(
		FName("CombatTricks.KnockbackForceMagnitude"),
		FString("SetByCaller: magnitud de la fuerza de knockback si el golpe (no fatal) lo activa."));
	GameplayTags.CombatTricks_KnockbackChance = Manager.AddNativeGameplayTag(
		FName("CombatTricks.KnockbackChance"),
		FString("SetByCaller: probabilidad (0-100) de que este golpe (no fatal) active el knockback."));
	GameplayTags.CombatTricks_KnockbackIsHeavy = Manager.AddNativeGameplayTag(
		FName("CombatTricks.KnockbackIsHeavy"),
		FString("SetByCaller (1.0/0.0): si es true, este knockback bloquea HitReact y dispara GA_HeavyKnockback en vez del comportamiento normal."));

	// Launch (post-315, Nivel 3 — lanzamiento aéreo, independiente del Knockback)
	GameplayTags.CombatTricks_LaunchChance = Manager.AddNativeGameplayTag(
		FName("CombatTricks.LaunchChance"),
		FString("SetByCaller: probabilidad (0-100) de que este golpe active el lanzamiento aereo."));
	GameplayTags.CombatTricks_LaunchForceMagnitude = Manager.AddNativeGameplayTag(
		FName("CombatTricks.LaunchForceMagnitude"),
		FString("SetByCaller: magnitud de la fuerza del lanzamiento aereo."));
	GameplayTags.CombatTricks_LaunchPitchOverride = Manager.AddNativeGameplayTag(
		FName("CombatTricks.LaunchPitchOverride"),
		FString("SetByCaller: angulo (grados) del pitch override para la direccion del lanzamiento."));

	// --- TAGS DE BUILDUP ELEMENTAL (SetByCaller — sistema de umbral, sin azar) ---
	// Ver la explicación completa en el header. Transporte del buildup por elemento.
	GameplayTags.CombatTricks_Buildup_Fire = Manager.AddNativeGameplayTag(
		FName("CombatTricks.Buildup.Fire"),
		FString("SetByCaller: buildup de Quemadura que deposita este golpe (pre-resistencia/critico)."));
	GameplayTags.CombatTricks_Buildup_Storm = Manager.AddNativeGameplayTag(
		FName("CombatTricks.Buildup.Storm"),
		FString("SetByCaller: buildup de Electrocucion que deposita este golpe (pre-resistencia/critico)."));
	GameplayTags.CombatTricks_Buildup_Water = Manager.AddNativeGameplayTag(
		FName("CombatTricks.Buildup.Water"),
		FString("SetByCaller: buildup de Saturacion que deposita este golpe (pre-resistencia/critico)."));
	GameplayTags.CombatTricks_Buildup_Nature = Manager.AddNativeGameplayTag(
		FName("CombatTricks.Buildup.Nature"),
		FString("SetByCaller: buildup de Veneno que deposita este golpe (pre-resistencia/critico)."));

	// --- TAGS DE ATRIBUTO DE BUILDUP (identidad para TagsToAttributes/UI) ---
	GameplayTags.Attributes_Buildup_Fire = Manager.AddNativeGameplayTag(
		FName("Attributes.Buildup.Fire"),
		FString("Barra de acumulacion de Quemadura (0-100). Al llenarse dispara el estado y se resetea."));
	GameplayTags.Attributes_Buildup_Storm = Manager.AddNativeGameplayTag(
		FName("Attributes.Buildup.Storm"),
		FString("Barra de acumulacion de Electrocucion (0-100). Al llenarse dispara el estado y se resetea."));
	GameplayTags.Attributes_Buildup_Water = Manager.AddNativeGameplayTag(
		FName("Attributes.Buildup.Water"),
		FString("Barra de acumulacion de Saturacion (0-100). Al llenarse dispara el estado y se resetea."));
	GameplayTags.Attributes_Buildup_Nature = Manager.AddNativeGameplayTag(
		FName("Attributes.Buildup.Nature"),
		FString("Barra de acumulacion de Veneno (0-100). Al llenarse dispara el estado y se resetea."));

	// --- PARRY / BLOQUEO ---
	GameplayTags.Abilities_Parry = Manager.AddNativeGameplayTag(FName("Abilities.Parry"), FString("Asignar en AbilityTags de GA_Parry_Physical / GA_Parry_Magic."));
	GameplayTags.State_Parry_Physical = Manager.AddNativeGameplayTag(FName("State.Parry.Physical"), FString("Ventana de parry fisico activa (bloqueo perfecto). La concede GA_Parry_Physical."));
	GameplayTags.State_Parry_Magic = Manager.AddNativeGameplayTag(FName("State.Parry.Magic"), FString("Ventana de parry magico activa (bloqueo perfecto). La concede GA_Parry_Magic."));
	GameplayTags.State_Block_Physical = Manager.AddNativeGameplayTag(FName("State.Block.Physical"), FString("Bloqueo fisico imperfecto sostenido (tras la ventana de parry)."));
	GameplayTags.State_Block_Magic = Manager.AddNativeGameplayTag(FName("State.Block.Magic"), FString("Bloqueo magico imperfecto sostenido (tras la ventana de parry)."));

	// --- i-frames genéricos (post-315, a petición) ---
	GameplayTags.State_Invulnerable = Manager.AddNativeGameplayTag(
		FName("State.Invulnerable"),
		FString("Invulnerabilidad temporal. ExecCalc_Damage anula todo dano mientras el target lo tenga."));

	// --- Lanzamiento aéreo / Nivel 3 de knockback (post-315) ---
	GameplayTags.State_Airborne = Manager.AddNativeGameplayTag(
		FName("State.Airborne"),
		FString("Personaje en el aire por un lanzamiento. Bloquea GA_HitReact; su remocion en Landed() dispara GA_GetUp."));

	// --- Nivel 2 de knockback ("empujón fuerte", a petición) ---
	GameplayTags.State_HeavyKnockback = Manager.AddNativeGameplayTag(
		FName("State.HeavyKnockback"),
		FString("Concedido brevemente por un knockback pesado. Bloquea GA_HitReact mientras dura."));

	// Gameplay Cues del parry/bloqueo. Efectos visuales y sonidos por combinacion.
	// Los tags siguen la convencion GameplayCue.X para que el GameplayCueManager los
	// detecte automaticamente. Crear los assets GC_Parry_* en el editor cuando se quieran
	// anadir efectos; hasta entonces el sistema funciona sin ellos (la llamada es silenciosa).
	GameplayTags.GameplayCue_Parry_Physical_Perfect = Manager.AddNativeGameplayTag(
		FName("GameplayCue.Parry.Physical.Perfect"),
		FString("Cue para parry fisico perfecto. Asignar particulas de impacto fisico + sonido de clang metalico."));
	GameplayTags.GameplayCue_Parry_Physical_Block = Manager.AddNativeGameplayTag(
		FName("GameplayCue.Parry.Physical.Block"),
		FString("Cue para bloqueo fisico imperfecto. Particulas mas sutiles, sonido de golpe absorbido."));
	GameplayTags.GameplayCue_Parry_Magic_Perfect = Manager.AddNativeGameplayTag(
		FName("GameplayCue.Parry.Magic.Perfect"),
		FString("Cue para parry magico perfecto. Particulas de energia magica/destello, sonido cristalino."));
	GameplayTags.GameplayCue_Parry_Magic_Block = Manager.AddNativeGameplayTag(
		FName("GameplayCue.Parry.Magic.Block"),
		FString("Cue para bloqueo magico imperfecto. Efecto de escudo/burbuja parcial, sonido amortiguado."));

	// Gameplay Cue de impacto melee. Se dispara desde el WeaponTraceComponent cuando el
	// sweep de la hoja golpea a un actor. Pasa como parametros: posicion de impacto
	// (Location), la victima (SourceObject, para GetBloodEffect), el atacante
	// (EffectCauser, para buscar ImpactSound en su TaggedMontage), y el MontageTag
	// activo (AggregatedSourceTags, para identificar qué sonido usar).
	// El asset GC_MeleeImpact (GameplayCueNotify_Static) se crea en el editor.
	GameplayTags.GameplayCue_Melee_Impact = Manager.AddNativeGameplayTag(
		FName("GameplayCue.Melee.Impact"),
		FString("Cue de impacto melee: spawna efecto de sangre/particulas y reproduce el sonido del ataque."));

	// --- TAGS DE COOLDOWN ---
	// Registramos la jerarquia completa. Los padres se registran como tags nativos vacios
	// (sin descripcion util) para que existan en la jerarquia de GAS y permitan queries por
	// nivel, aunque ningun GE los conceda directamente. Solo el tag hoja
	// (Cooldown.Spell.Fire.Firebolt) se concede via Granted Tag en GE_Cooldown_Firebolt;
	// como tiene a los padres por encima, las queries contra Cooldown.Spell o
	// Cooldown.Spell.Fire siguen detectandolo automaticamente (MatchesTag incluye hijos).
	GameplayTags.Cooldown = Manager.AddNativeGameplayTag(
		FName("Cooldown"),
		FString("Raiz de todos los cooldowns del juego."));
	GameplayTags.Cooldown_Spell = Manager.AddNativeGameplayTag(
		FName("Cooldown.Spell"),
		FString("Padre de todos los cooldowns de hechizos."));
	GameplayTags.Cooldown_Spell_Fire = Manager.AddNativeGameplayTag(
		FName("Cooldown.Spell.Fire"),
		FString("Cooldowns de hechizos del elemento Fuego."));
	GameplayTags.Cooldown_Spell_Fire_Firebolt = Manager.AddNativeGameplayTag(
		FName("Cooldown.Spell.Fire.Firebolt"),
		FString("Cooldown del hechizo Firebolt. Lo concede GE_Cooldown_Firebolt como Granted Tag."));
	// Padres de los demás elementos (las hojas se añadirán con cada nuevo hechizo).
	GameplayTags.Cooldown_Spell_Water = Manager.AddNativeGameplayTag(
		FName("Cooldown.Spell.Water"),
		FString("Cooldowns de hechizos de Agua (futuro — Corazón de Agua)."));
	GameplayTags.Cooldown_Spell_Storm = Manager.AddNativeGameplayTag(
		FName("Cooldown.Spell.Storm"),
		FString("Cooldowns de hechizos de Tormenta (futuro — Corazón de Tormenta)."));
	GameplayTags.Cooldown_Spell_Nature = Manager.AddNativeGameplayTag(
		FName("Cooldown.Spell.Nature"),
		FString("Cooldowns de hechizos de Naturaleza (futuro — Corazón de Naturaleza)."));
}
