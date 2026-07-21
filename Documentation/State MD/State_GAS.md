# State_GAS — Sistema Gameplay Ability System

> **Propósito:** Estado actual del backbone GAS del proyecto. Cubre AttributeSet, ASC custom, MMCs, Gameplay Tags Singleton, AssetManager y EffectActors. Lee `State_Overview.md` primero para contexto general.

> **Herramienta disponible:** el plugin **PDS** puede exportar snapshots y comparar Gameplay Tags, assets y records semánticos de hechizos entre sesiones, además de ejecutar la validación por perfiles. Útil para evidenciar qué cambió en GAS tras una fase sin inspección manual. Ver `State_DeveloperTools.md`.

---

## 1. Sistema de Atributos — ✅ Funcional

**Archivos clave:** `PantheliaAttributeSet.h/.cpp`, `MMC_MaxHealth/MaxMana/MaxStamina`

### Atributos primarios (configurables por jugador/equipamiento)

- `Hardness` — afecta daño físico
- `Resonance` — afecta daño mágico
- `Resilience` — afecta MaxHealth y Armor
- `Endurance` — afecta MaxStamina y MaxPoise
- `Spirit` — afecta MaxMana y MagicResistance

### Atributos secundarios (calculados por MMCs desde primarios)

- `MaxHealth = 80 + (2.5 × Resilience) + (10 × Level)`
- `MaxMana = 50 + (2.5 × Spirit) + (5 × Level)`
- `MaxStamina = 80 + (2.5 × Endurance) + (8 × Level)`
- `Armor`, `MagicResistance`, `MaxPoise`, `Tenacity` — pendientes de MMC, valores placeholder
- `PhysicalDamage`, `MagicDamage` — pendientes de MMC
- `ArmorPenetration`, `MagicPenetration`, `CritChance`, `CritDamage` — independientes (items/árbol)
- `AttackSpeed` — 🔨 **planificado, aún no en el AttributeSet.** Multiplicador del play rate de las animaciones de ataque (1.0 = normal). Modificable por buffs/pasivas/árbol vía GE. Ver pendiente 7.7.
- `FireResistance`, `WaterResistance`, `StormResistance`, `NatureResistance` — ✅ **ya en el AttributeSet** con todo el boilerplate (OnRep, DOREPLIFETIME, TagsToAttributes). Porcentuales, una por elemento, reducen el daño de los dos tipos de su elemento. ⚠️ **PLACEHOLDER:** hoy se derivan de `Resilience` vía un modifier en `GE_SecondaryAttributes` (Override, Backing=Resilience, PostMultAdd=3, Coeff=0.5). Cuando existan equipamiento y árbol, ese modifier se elimina y esos sistemas las modifican directamente. Marcado con `// PLACEHOLDER` en `ExecCalc_Damage.cpp` y `PantheliaAttributeSet.h`.

### Atributos vitales

`Health`, `Mana`, `Stamina`, `Poise`

### Meta atributos (no replicados)

`IncomingDamage` — atributo temporal que solo existe en el servidor. Los GEs de daño **añaden** a este atributo (valor positivo), no restan directamente de Health.

**Flujo:** `PostGameplayEffectExecute` detecta el cambio en `IncomingDamage`, lo consume (lo resetea a 0), aplica los cálculos finales y modifica `Health`. En este bloque también se llama a `HandleParryReaction(Props)`: si el golpe fue parado/bloqueado dispara el feedback en el defensor (`NotifyParryImpact`) y, si fue parry perfecto, aplica daño de postura al atacante (y `Effects.Stagger` si su postura llega a 0). Ver `State_Combat.md` sección 11.5.

**Por qué este patrón:** desacopla el GE del resultado final. El GE de daño no sabe cuánto daño real causa — solo declara su intención. El cálculo (resistencias, crítico, etc.) vive en un sitio centralizado. Es el patrón estándar de GAS para daño.

`IncomingPoiseDamage` — meta atributo del mismo patrón, para el daño a postura. No replicado. Se procesa en su propia rama de `PostGameplayEffectExecute`: al consumirse, activa HitReact (si el golpe supera el `FlinchThreshold%`) o Stagger (si Poise llega a 0), y llama a `ResetPoiseRegenTimer()` en el target. Ver `State_Combat.md` (sistema de Postura).

`IncomingXP` — meta atributo del mismo patrón, para conceder experiencia. Su bloque en `PostGameplayEffectExecute` concede la XP al jugador (es la ruta de la tecla L de prueba y el gancho para **fuentes de XP futuras**: áreas, consumibles, eventos, árbol). **Ya NO es la ruta del XP de muerte de enemigos**, que se concede directamente en C++ en `SendXPEvent` (ver `State_Progression.md` sección 3). Se conserva intacto y funcional.

`IncomingHealing` — meta atributo no replicado, **ruta oficial de toda curación** (`Attributes.Meta.IncomingHealing`). Un GE de curación **debe** modificar `IncomingHealing`, no `Health` directamente. `HandleIncomingHealing` consulta `GetActiveGrievousWoundsPercent` y aplica `Curación final = Entrante × (1 − HeridasGraves%/100)`, luego clampea sobre `Health`. Una curación legacy que escriba `Health` directo se clampea igual pero **se salta las Heridas Graves** — por eso frascos, regeneraciones y hechizos futuros deben usar esta ruta. Ver el sistema de Heridas Graves abajo.

### Clamps de seguridad — prerequisito silencioso del árbol (Etapa 1)

El árbol de habilidades moverá los atributos `Max` arriba y abajo (nodos que dan +MaxHealth y se remueven al reespec). Para que eso no corrompa los vitales, se añadieron dos protecciones:

- **`PostAttributeChange` (override nuevo):** cuando cambia un atributo `Max` (`MaxHealth`/`MaxMana`/`MaxStamina`/`MaxPoise`), clampea el vital asociado si quedó por encima del nuevo máximo. Es necesario porque `PreAttributeChange` solo se dispara cuando cambia *ese* atributo, **no** cuando cambia su `Max` — sin este hook, remover un nodo de +MaxHealth dejaría `Health` por encima del máximo, corrupto en el BaseValue.
  - **Decisión soulslike (estilo Elden Ring):** al **bajar** el `Max` se clampea el vital; al **subir** el `Max` **no** se regala vida actual.
  - **Detalles:** guard `NewValue > 0.f` (evita "matar" al personaje durante la construcción, cuando los `Max` pasan por 0); solo escribe si el vital excede el nuevo máximo (evita deltas de UI innecesarios en cada `RefreshSecondaryAttributes`).
- **Clamp de `Health` en `PostGameplayEffectExecute`:** se añadió la rama `else if (Attribute == GetHealthAttribute())` al clamp genérico (antes solo Mana/Stamina/Poise se clampeaban directo; Health solo dentro de la rama de `IncomingDamage`). Cubre las **curaciones directas** futuras (pociones, hoguera, hechizos de vida) que apliquen un GE `Add` sobre Health. **Importante:** esta rama **NO llama a `Die()`** — la muerte sigue siendo responsabilidad exclusiva del pipeline de `IncomingDamage` (documentado en el comentario para evitar una segunda ruta de muerte accidental).

### Combat Tricks: reacciones a golpe (infraestructura GAS) — Sección 25

Piezas de GAS que sostienen las **reacciones a golpe** de `State_Combat.md` sección 12.3-12.4 (i-frames, knockback, launch, impulso de muerte). Auditado. *(El sistema de **estados elementales** se rediseñó a buildup determinista — ver la subsección siguiente.)*

- **Refactor de `PostGameplayEffectExecute`:** se extrajeron `HandleIncomingDamage()`, `HandleIncomingXP()` y `HandleIncomingHealing()` del monolito. En `HandleIncomingDamage` viven las reacciones de knockback/launch.
- **`GrantTemporaryGameplayTag(ASC, Tag, Duration)`** (y su envoltorio `GrantTemporaryInvulnerability`): GE dinámico que concede un tag por un tiempo. Usa **`MakeUniqueObjectName`** — aquí SÍ se quiere que varios coexistan (el contador de tags del ASC los agrega). Base de los i-frames y del heavy knockback.
- **i-frames:** chequeo de `State.Invulnerable` al inicio de `ExecCalc_Damage::Execute_Implementation` (`HasTagExact`). Bloquea solo el daño directo, no los ticks de DoT (decisión Elden Ring, comentada en el código).

### i-frames jerárquicos y esquivabilidad por golpe (sistema de dodge)

El sistema de invulnerabilidad evolucionó de un tag plano a una **jerarquía**, para que un golpe pueda declarar *cómo* de esquivable es:

- **Tags de invulnerabilidad:** `State.Invulnerable` (padre = **invulnerabilidad absoluta**) y sus hijos específicos `State.Invulnerable.Dodge` / `.Jump`.
- **Esquivabilidad del golpe (`DodgeResponse`):**
  - **`Dodgeable`** — evitable por i-frames **y concede Perfect Dodge** si cae en la ventana perfecta.
  - **`AvoidableNoReward`** — los i-frames lo anulan, pero **no** da Perfect Dodge.
  - **`Unavoidable`** — **atraviesa los hijos** (`.Dodge`/`.Jump`), pero **no** la invulnerabilidad absoluta del **tag padre exacto**. Es la vía para ataques de boss que no se pueden esquivar rodando pero que sí respetan una invulnerabilidad cinemática/scriptada.
- **Eventos:** `Event.Dodge.HitAvoided` (emitido al evitar un golpe; lo escucha `GA_Dodge`) y `Event.Dodge.Perfect` (emitido al confirmar un Perfect; payload: Instigator = atacante, Target = jugador, ContextHandle del golpe, Magnitude 1.0). Los efectos futuros del árbol/corazones escuchan **este** evento, no el interior de `GA_Dodge`. Cue: `GameplayCue.Dodge.Perfect`.

### Arquitectura común de costes de recursos (`UPantheliaCostAttributeSet`) — ✅ cerrada

> **Actualización (campaña de hardening 2026-07-16):** el sistema de coste dejó de ser "solo Light/Heavy por arma". Ahora hay una **arquitectura común de costes** con un resolvedor único, migrada a ataques, dodge, parry y hechizos. Lo que sigue reemplaza la nota anterior de "el patrón `Cost.Stamina` solo cubre Light/Heavy".

- **`UPantheliaCostAttributeSet`** (segundo AttributeSet del jugador, en el `PlayerState` junto al principal) — mantiene **multiplicadores y flats** de Mana y Stamina, para que el árbol/equipo/buffs modifiquen los costes por GAS sin ramas por perk.
- **Fórmula canónica de coste:** `Final = max(0, Base × max(0, Multiplier) + Flat)`. La `Base` sale de la **fuente propia de cada dominio**:
  - ataques → arma (`WeaponDefinition`); dodge → ability; parry → ability; **transición a guardia** → propiedad independiente; **impacto bloqueado** → arma **del atacante**; hechizos → ability.
- **GEs dinámicos separados por recurso:** `GE_Cost_Stamina_Dynamic` (`Cost.Stamina`) y `GE_Cost_Mana_Dynamic` (`Cost.Mana`), ambos Instant + SetByCaller (nunca magnitud fija).
- **Pago atómico multi-recurso:** una ability declara `AdditionalResourceCosts` y puede pagar **Mana + Stamina** a la vez. Se **validan todos antes de aplicar**; si falta uno, **no se cobra ninguno**. Hay **rollback defensivo** si un apply falla a mitad. `CheckCost`/`ApplyCost` custom + `CommitAbilityCost` para las continuaciones de combo (cada golpe paga el suyo, **solo tras validar** que existe un golpe válido).
- Fallo por coste añade el failure tag estándar (`ActivateFailCostTag`). Ausencia de arma/definición **no** es coste cero: es config inválida y bloquea; un `0` explícito sí es válido.
- **Bloqueo global de muertos:** `PostGameplayEffectExecute` comprueba primero el `TargetCharacter` para que los estados/buildup no sigan procesándose tras la muerte, y limpia meta atributos/buildup que lleguen después de un output fatal del mismo `ExecCalc`.

### Muerte, `State.Dead` y DeathPresentation (migración MetaHuman)

Separación clave: **el gameplay de la muerte vive en las clases comunes C++ y en el ASC; la presentación de la muerte vive en un componente aparte.** DeathPresentation **no** reemplaza al ASC ni al pipeline de daño — solo presenta la muerte.

- **Tag `State.Dead`** (nativo, `State.Dead`) — se concede al morir. Cancela/bloquea el gameplay del actor muerto. **El ASC del jugador permanece válido después de la muerte** (no se destruye; PlayerState y ASC se conservan), preparado para el respawn futuro.
- **`Die()` idempotente** — un daño fatal repetido no reinicia la secuencia de muerte; el shutdown de gameplay se separa del inicio de la presentación.
- **`UPantheliaDeathPresentationComponent`** (`Characters/Components/`) — capa reusable, no específica del jugador (la base común de personajes, `Enemy`, `EquipmentComponent`, `WeaponTraceComponent` y el `PlayerController` participan). Máquina de estados: `Alive → DeathRequested → GameplayShutdown → PresentationStarted → RagdollActive → PresentationFinished`. API: `SchedulePresentationFinish`/`FinishDeathPresentation`/`AbortDeathPresentation`/`NotifyGameplayShutdownComplete`, delegate `PresentationFinished` **emitido una sola vez**, registro de partes visuales para el ragdoll multipart. Config: `bAutoFinishPresentation`, `PresentationDuration` (5 s).
  - **Ragdoll multipart MetaHuman:** `CharacterMesh0` es el único cuerpo físico; Face lo sigue por Copy Pose; grooms visibles (simulación de Hair off en ragdoll por estabilidad); cápsula a `NoCollision`, movimiento a `MOVE_None`; el arma moderna se desacopla **una vez** y el WeaponTrace se cierra (sin daño póstumo). 10 partes + 6 grooms registrados.
  - **Por tipo de actor:** el **jugador** no se destruye (no hay respawn aún); el **enemigo** arranca su `Lifespan` al **finalizar** la presentación, no al iniciar la muerte.
- **Fallback monomesh** conservado para actores no-MetaHuman.

> **Pendiente de la muerte** (ver `State_Pending.md`): dissolve multipart (Body opaco, Face multi-material, grooms sin política uniforme — **no** reutilizar a ciegas el dissolve monomesh del slot 0), respawn del jugador, y el Physics Asset definitivo del arma placeholder (que podía atravesar el suelo — la solución va en el arma, **no** en lógica especial del jugador).

> **Validación cocinada (Fases 4G-4I):** la build Development Win64 confirmó ASC activo en el jugador productivo, PlayerState válido, abilities de combate funcionando, `State.Dead`, cancelación/bloqueo post-muerte conforme a la arquitectura existente, y **PlayerState + ASC preservados tras finalizar la presentación de muerte**. **Respawn NO existe.** Pendiente para cuando se implemente: **limpieza/restauración de estados y atributos al respawn** (limpiar `State.Dead`, buildup, debuffs, tags temporales) y **reactivación controlada del avatar tras el checkpoint**.

### Estados elementales por buildup determinista + Heridas Graves — rediseño (Sección 25 ampliada)

> **Reemplaza el sistema aleatorio anterior.** Se **eliminó** la ruta `DetermineDebuff` con tirada `DebuffChance` en el `ExecCalc`. Ya no hay probabilidad por golpe ni el "último golpe" define el DoT. Ver el comportamiento en `State_Combat.md` sección 12.2 y la ability en `State_Abilities.md`.

- **Barras de buildup:** 4 atributos replicados `FireBuildup`/`StormBuildup`/`WaterBuildup`/`NatureBuildup` (`Attributes.Buildup.*`). El `ExecCalc` emite outputs en orden `IncomingDamage → IncomingPoiseDamage → Buildup` (resolver muerte antes que el estado). Al llegar a 100: clampe, reset a 0, `TriggerElementalStatus()`. Decay 6/s × `(1+Resist/100)`, timer 0.1s solo mientras haya carga.
- **`DA_ElementalStatusConfig`** (`UPantheliaElementalStatusConfig` + `FPantheliaElementalStatusDefinition` + `EPantheliaElementalStatusPayload{DamageOverTime,BurstDamage,AttributeDebuff}`), referenciado desde `DA_CharacterClassInfo`. Una entrada por elemento; el payload y su escalado (por `StatusPower`, por `MagicDamage`, ramas de % de vida) viven aquí, **no** en la ability. El build modifica **atributos del atacante**, nunca esta config en runtime.
- **`Debuff()` cacheado (se conserva como mecanismo de aplicación):** crea el GE del DoT en runtime, concede el tag con **`UTargetTagsGameplayEffectComponent`** (el `InheritableOwnedTagsContainer` del curso está deprecado desde 5.3), **cachea por AttributeSet+tag+frecuencia** (`Outer` = el propio AttributeSet, no `GetTransientPackage`, para no colisionar entre personajes) y pasa daño/duración por SetByCaller. La unicidad **no** usa `UGameplayEffect::StackingType` (deprecado para esto en 5.8): se **elimina + reaplica** por tag (un solo estado por elemento/objetivo; refresca duración y recalcula magnitud).
- **Heridas Graves (antiheal) — universal, "gana la más fuerte":** meta pipeline `IncomingHealing` → `HandleIncomingHealing` → `GetActiveGrievousWoundsPercent`. **No acumula** varias fuentes hacia un cap (esto reemplaza la spec vieja de `State_Pending`): usa la **reducción activa más intensa** (Direct 30% + Poison 40% → 40%, no 70%). Tags `Effects.GrievousWounds{.Direct,.Poison}` (Direct y Poison coexisten, se refrescan por separado; Poison comparte duración con el Veneno y también concede `Debuff.Poison`). Duración mínima 4 s. Es **inmediata y reutilizable**: armas, hechizos, ticks, parry, armaduras reactivas y bosses pueden aplicarla sin barra de buildup. El atributo legacy `GrievousWounds` se conserva solo por compatibilidad (no está en el `TagsToAttributes` moderno).
- **`MagicResistance` / `MagicPenetration` ahora sí mitigan el daño mágico directo** en el `ExecCalc` (`EffectiveMagicResistance = MagicResistance × (100 − MagicPen × coef)/100`; el daño mágico se reduce por ella). **Standby de balance:** faltan las filas `MagicPenetration` y `EffectiveMagicResistance` en `CT_DamageCalculationCoefficients`; mientras tanto el código **reutiliza las curvas físicas** `ArmorPenetration`/`EffectiveArmor` (fallback funcional). La separación físico/mágico de balance queda pospuesta.
- **Defense Shred:** Quemadura y Veneno pueden reducir Armor/MagicResistance del objetivo (`Effects.DefenseShred.Burn/.Poison`) si el **atacante** tiene los atributos ofensivos; dos GEs `Add` que suman.
- **Limpieza al morir** (`PantheliaCharacterBase`): para el timer de postura y el de decay, pone a 0 las 4 barras e `IncomingHealing`, limpia el `GrievousWounds` legacy, y **elimina los efectos con tags** `Debuff.Burn/Shock/Saturation/Poison` y `Effects.GrievousWounds`, más el loose tag **`State.Airborne`**. Esto **cierra el pendiente de respawn** que estaba anotado para estos tags (ver Etapa 4).

> **Familias de atributos nuevas** (todas en `TagsToAttributes`, salvo el `GrievousWounds` legacy): `Attributes.Buildup.*` (4), `Attributes.StatusPower.*` (4), `Attributes.StatusDamage.<Elem>.<MaxHealth/CurrentHealth/MissingHealth>Percent` (daño porcentual desbloqueable, en **puntos porcentuales**: 0.5 = 0.5%), reducción defensiva ofensiva (Fire/Nature × Armor/MagicResistance), y los ofensivos de Heridas Graves (`GrievousWoundsIntensityBonus`/`GrievousWoundsDurationBonus`). Se añadieron 8 filas a `DA_AttributeInfo` (4 porcentuales + 4 Defense Shred) — ver `State_UI.md`.

> **Tags de la Sección 25** (nativos): `Debuff.Burn/Shock/Saturation/Poison`; `CombatTricks.Buildup.*` (SetByCaller) y `CombatTricks.*` de knockback/launch/impulso; `Effects.DefenseShred.Burn/.Poison`; `Effects.GrievousWounds{.Direct,.Poison}`; `State.Invulnerable/Airborne/HeavyKnockback`; `Effects.GetUp/HeavyKnockback`; y tags/eventos **reservados** para los futuros corazones (sin lógica aún).

> **Fricción 3 CERRADA:** el Override placeholder de resistencias en `GE_SecondaryAttributes` se **eliminó** — las resistencias elementales ya no se derivan de `Resilience` por Override, así que el build (árbol/equipo) puede modificarlas con `Add`/`Multiply`. Ver `Code_Review.md`.

### Inicialización

3 Gameplay Effects en orden (`DefaultPrimaryAttributes`, `DefaultSecondaryAttributes`, `DefaultVitalAttributes`) aplicados en `PantheliaCharacterBase::InitializeDefaultAttributes()`. El orden importa — ver decisión 2.3 en `State_Overview.md`.

### Replicación

Todos los atributos usan `REPNOTIFY_Always` con `OnRep_` correspondiente.

### Mapa Tag → Atributo (clave para escalabilidad)

`PantheliaAttributeSet` contiene un `TMap<FGameplayTag, TStaticFuncPtr<FGameplayAttribute()>> TagsToAttributes` que asocia cada Gameplay Tag (`Attributes.Primary.Hardness`, `Attributes.Secondary.MaxHealth`, etc.) con un puntero a la función estática del accessor del atributo (`GetHardnessAttribute()`).

**Por qué importa:** Permite **iterar todos los atributos sin conocerlos individualmente**. El `AttributeMenuWidgetController` aprovecha esto para broadcastear todos los valores en un loop, sin escribir un bind por cada atributo. Cuando añadas un atributo nuevo, solo tienes que registrarlo en este `TMap` y la UI lo reflejará automáticamente.

`TStaticFuncPtr` es un alias de UE para punteros a función estática — más limpio que `FGameplayAttribute(*)()` y compatible con TMap.

---

## 2. MMCs (Modifier Magnitude Calculations) — ✅ Funcional

Clases que heredan de `UGameplayModMagnitudeCalculation` y proporcionan valores dinámicos a los GameplayEffects. Reciben el `FGameplayEffectSpec` y devuelven un float.

**Implementados:**
- `UMMC_MaxHealth` — captura `Resilience` del AttributeSet y `Level` vía `ICombatInterface::GetPlayerLevel()`. Fórmula real: `80 + (2.5 × Resilience) + (10 × Level)`. Además aplica un **escalado de salud para enemigos**: si el actor tiene el Actor Tag `"Enemy"`, el resultado se multiplica por `EnemyHealthScale` (default **0.25**). El jugador (tag `"Player"`) no se ve afectado. Permite enemigos más frágiles sin tocar las curvas de atributos.
- `UMMC_MaxMana` — captura `Spirit` + nivel
- `UMMC_MaxStamina` — captura `Endurance` + nivel

**Patrón:** Cada MMC declara un `FGameplayEffectAttributeCaptureDefinition` para el atributo primario que necesita, lo captura con `GetCapturedAttributeMagnitude()`, obtiene el nivel via interface en `GetSourceActorFromSpec`, y devuelve la fórmula.

**Pendientes:** MMCs para `Armor`, `MagicResistance`, `MaxPoise`, `Tenacity`, `PhysicalDamage`, `MagicDamage`.

### Recálculo de secundarios cuando cambia el nivel — patrón `RefreshSecondaryAttributes`

> **El problema (importante entenderlo).** Los MMC leen el nivel del personaje con `ICombatInterface::Execute_GetPlayerLevel()` **dentro** de `CalculateBaseMagnitude_Implementation`. Esa lectura **no pasa por el sistema de capturas de GAS** (`RelevantAttributesToCapture` solo vigila atributos como `Resilience`/`Endurance`/`Spirit`). Para GAS, el **nivel es una variable invisible**: puede cambiar sin que el `GameplayEffect` `Infinite` de secundarios ya aplicado se entere, y su magnitud queda "congelada" con el nivel que tenía al aplicarse. Síntoma que provocó: al subir de nivel, `MaxHealth`/`MaxStamina`/`MaxMana` no se actualizaban hasta que algo, de rebote, forzaba una reevaluación (p. ej. gastar el primer punto de atributo aplicaba `GE_EventBasedEffect` y GAS revisaba todos los efectos activos → salto "una sola vez").

> **Lo que NO funcionó** (descartado con pruebas, no por suposición): (a) prevenir la doble llamada a `InitAbilityActorInfo` no era la causa (aunque la guarda se mantiene, ver abajo); (b) configurar `Stacking = Aggregate By Target / Stack Limit 1 / Refresh On Successful Application` y reaplicar el mismo GE **no** fuerza un recálculo de la `CustomCalculationClass` — ese Stacking solo refresca el temporizador de duración, no las magnitudes (no se encontró confirmación en la documentación oficial de que hiciera lo contrario).

**Solución — "quitar + reaplicar" (determinista):**
- `APantheliaCharacterBase::ApplyEffectToSelf` cambió su firma de `void` a **`FActiveGameplayEffectHandle`** (devuelve el handle de la instancia creada). Cambio no disruptivo: los llamadores que ignoraban el retorno siguen igual.
- El handle de la instancia activa de `DefaultSecondaryAttributes` se guarda en **`SecondaryAttributesEffectHandle`**. **[Actualizado en la Etapa 4]** este miembro **se mudó del Pawn al ASC** (`UPantheliaAbilitySystemComponent`), donde es público y **ya no necesita `mutable`** (ver la subsección de la Etapa 4 abajo).
- Nueva función `RefreshSecondaryAttributes()`: si el handle es válido, **quita** la instancia (`RemoveActiveGameplayEffect`, `StacksToRemove = -1`) y crea una **completamente nueva** (`ApplyEffectToSelf`), guardando el nuevo handle. Una instancia nueva **siempre** re-ejecuta los MMC desde cero, capturando el nivel actual — 100% determinista, sin depender de comportamientos de Stacking no confirmados.
- `AMainCharacter`: handler `OnPlayerLevelChanged(int32 NewLevel)`, enganchado con **`AddUObject`** (no `AddLambda`, para desvinculación automática si el Pawn se destruye — relevante para muerte/respawn) al `OnLevelChangedDelegate` del `PlayerState`, dentro de `InitAbilityActorInfo()`.
- **Include necesario:** `FActiveGameplayEffectHandle` usado **por valor** (retorno y miembro) requiere `#include "GameplayEffectTypes.h"`; un forward declaration no basta (el compilador necesita el tamaño completo del struct).

> **Nota de diseño reutilizable (para el árbol/buffs):** guardar el handle que devuelve `ApplyEffectToSelf` para poder **quitar ese efecto exacto después** es la base de cualquier sistema de buffs/hechizos/auras temporales. No es un parche puntual — es infraestructura que el árbol de habilidades y los efectos de estado reaprovecharán. Y la regla general: **cualquier stat futuro que dependa de una fuente externa a GAS (el nivel, u otra) necesitará este mismo refresco explícito** (`Remove` + `Apply`), porque GAS no lo recalcula solo.

### Estado de inicialización en el ASC — preparado para respawn (Etapa 4)

> **El problema (para entender por qué importa).** Al principio, `bAbilityActorInfoInitialized` y `SecondaryAttributesEffectHandle` vivían como miembros del **Pawn** (`APantheliaCharacterBase`). En un futuro respawn del jugador, el Pawn se destruye y se crea uno nuevo con esas variables **reseteadas** — pero el ASC vive en el **PlayerState**, que **sobrevive**. Consecuencia: se reaplicarían los atributos por defecto (perdiendo los puntos ya gastados) y quedaría una **segunda** instancia `Infinite` de secundarios imposible de remover (el handle que la controlaba murió con el Pawn viejo).

**La solución — mudar el estado persistente al ASC** (no al PlayerState). Razón de arquitectura: *el estado debe vivir junto a lo que describe* (los datos de GAS). Así el ASC tiene la vida correcta en ambos mundos **sin código especial**:
- **Jugador:** ASC en el PlayerState → sobrevive al respawn → flags y handle sobreviven → no re-inicializa.
- **Enemigos:** ASC en el propio Character → muere con él → cada enemigo nuevo inicializa con normalidad.

**Estado nuevo en `UPantheliaAbilitySystemComponent`:**
- `bAttributesInitialized` (público) — guarda de respawn, comprobada en `InitializeDefaultAttributes`.
- `SecondaryAttributesEffectHandle` (público) — mudado desde `CharacterBase`.
- `bEffectAppliedDelegateBound` (protected) — guarda contra doble bind.
- `bPassiveAbilitiesGiven` (protected) — guarda de pasivas.
- `bStartupAbilitiesGiven` (ya existía) — ahora también actúa como guarda en `AddCharacterAbilities`.

**Cuatro guardas activas** (un respawn duplicaría CUATRO cosas, no dos):
1. **Atributos por defecto** → `InitializeDefaultAttributes` retorna si `bAttributesInitialized`.
2. **Bind de `EffectApplied`** → `AbilityActorInfoSet` retorna si `bEffectAppliedDelegateBound`. **Hallazgo sutil:** ese bind usa `AddUObject(this)` donde `this` es el **ASC persistente** (no el Pawn), así que **no se autolimpia** al morir el Pawn. Sin la guarda, cada respawn añadiría un bind y la UI recibiría cada efecto N veces.
3. **Startup abilities** → `AddCharacterAbilities` retorna si `bStartupAbilitiesGiven`.
4. **Pasivas** → `AddCharacterPassiveAbilities` retorna si `bPassiveAbilitiesGiven` (crítico: se otorgan **y** activan; duplicarlas = dos `GA_ListenForXPEvents` procesando cada evento dos veces).

**División de responsabilidades** (documentada en los comentarios del código):
- `bAbilityActorInfoInitialized` (**sigue en el Pawn**): "¿este CUERPO ya hizo su cableado?" — protege el doble `PossessedBy` + `OnRep_PlayerState` sobre el mismo Pawn.
- `bAttributesInitialized` (**en el ASC**): "¿los DATOS de GAS ya se inicializaron en esta partida?" — protege el respawn.

> **Detalle de C++ (para principiante):** `InitializeDefaultAttributes`/`RefreshSecondaryAttributes` siguen siendo funciones `const`, y aun así escriben en los miembros del ASC. No hay contradicción: `const` protege **a este objeto** (el personaje), no a los objetos a los que apunta. Escribir en el ASC *a través de su puntero* es legal. Por eso el `mutable` que antes necesitaba el handle ya no hace falta.

> **Pendiente conocido (anotado en `MainCharacter.cpp`):** cuando exista el respawn, `InitAbilityActorInfo` se re-ejecutará completo en el Pawn nuevo (correcto, para fijar el nuevo Avatar), y habrá que revisar que `HUD->InitOverlay` **no cree un segundo overlay**. **Ya resuelto (rediseño de estados):** la limpieza al morir de `PantheliaCharacterBase` **ya** pone a cero las 4 barras de buildup e `IncomingHealing`, y elimina los debuffs (`Debuff.*`), `Effects.GrievousWounds` y el loose tag **`State.Airborne`** del ASC — así que estos ya no sobrevivirán al futuro respawn. Lo que queda para cuando exista el respawn es el doble overlay del HUD. Estas protecciones son **preventivas**: hoy no existe flujo de respawn.

> **A vigilar (no confirmado como problema):** entre el `Remove` y el `Apply` del refresco podría haber un instante en que la vida **actual** (no solo la máxima) caiga, si `PreAttributeChange`/`PostGameplayEffectExecute` clampea `Health` contra un `MaxHealth` momentáneamente bajo. No se ha observado como fallo real; vigilar en pruebas futuras.

---

## 3. PantheliaAbilitySystemComponent — ✅ Funcional

**`UPantheliaAbilitySystemComponent`** hereda de `UAbilitySystemComponent` y añade lógica específica del juego sobre el ASC base de Unreal.

**Responsabilidad principal:** servir de **puente entre GAS y la UI** para mensajes contextuales (ej. "Recogiste una poción de vida").

### Cómo funciona

1. `AbilityActorInfoSet()` se llama desde `MainCharacter::InitAbilityActorInfo()` y `PantheliaEnemy::InitAbilityActorInfo()` justo después de que GAS termine de inicializarse.
2. Dentro, se suscribe a `OnGameplayEffectAppliedDelegateToSelf` (delegate nativo de UAbilitySystemComponent).
3. Cada vez que se aplica **cualquier** GameplayEffect al ASC (Instant, Duration o Infinite), se dispara `EffectApplied()`.
4. `EffectApplied()` extrae los **Asset Tags** del `FGameplayEffectSpec` y los broadcastea por el delegate público `EffectAssetTags`.

### Quién lo consume

`UOverlayWidgetController::BindCallbacksToDependencies()` se suscribe a `EffectAssetTags` y filtra los tags que empiezan con `Message.*`. Por cada tag de mensaje, busca su fila en `MessageWidgetDataTable` (estructura `FUIWidgetRow`) y broadcastea `MessageWidgetRowDelegate` para que el widget de Blueprint muestre el toast/mensaje. Ver `State_UI.md`.

### Por qué es importante para escalabilidad

Cualquier nuevo pickup, buff, debuff o efecto del árbol de habilidades que quiera mostrar un mensaje en pantalla solo necesita:
1. Tener un Asset Tag `Message.X` en su GameplayEffect.
2. Una fila en el DataTable con ese tag, mensaje, icono y widget a mostrar.

**Cero código nuevo** para añadir mensajes. Exactamente el patrón data-driven que necesitamos para el árbol de habilidades.

### Nota técnica

`FEffectAssetTags` usa `DECLARE_MULTICAST_DELEGATE_OneParam` (no `DYNAMIC`), por eso el WidgetController usa `AddLambda` en lugar de `AddDynamic`. Más eficiente, pero el delegate **no es visible en Blueprints** — solo C++ puede suscribirse.

---

## 4. Gameplay Tags Singleton + AssetManager — ✅ Funcional

**Archivos:** `PantheliaGameplayTags.h/.cpp`, `PantheliaAssetManager.h/.cpp`

### FPantheliaGameplayTags

Struct singleton en C++ que almacena todos los Gameplay Tags nativos del proyecto como variables `FGameplayTag`. Patrón clásico de singleton con `static FPantheliaGameplayTags& Get()` y `static void InitializeNativeGameplayTags()`.

**Por qué existe:** En lugar de usar `FGameplayTag::RequestGameplayTag(FName("Attributes.Secondary.Armor"))` disperso por el código (propenso a typos, no refactorizable), accedemos con `FPantheliaGameplayTags::Get().Attributes_Secondary_Armor`. Si un tag se renombra, el compilador avisa.

### Tags registrados actualmente

*Primarios:*
`Attributes.Primary.Hardness`, `Resonance`, `Resilience`, `Endurance`, `Spirit`

*Secundarios:*
`Attributes.Secondary.MaxHealth`, `MaxMana`, `MaxStamina`, `MaxPoise`, `Armor`, `MagicResistance`, `Tenacity`, `PhysicalDamage`, `MagicDamage`, `ArmorPenetration`, `MagicPenetration`, `CritChance`, `CritDamage`

*Resistencias elementales:*
`Attributes.Resistance.Fire`, `Attributes.Resistance.Water`, `Attributes.Resistance.Storm`, `Attributes.Resistance.Nature`

*Daño (8 tipos + postura):* — reemplazan al antiguo tag genérico `Damage` (eliminado)
```
Damage.Physical            genérico, sin elemento, solo mitigado por Armor
Damage.Physical.Ice        elemento Agua,       mitigado por Armor
Damage.Physical.Air        elemento Tormenta,   mitigado por Armor
Damage.Physical.Earth      elemento Naturaleza, mitigado por Armor
Damage.Magical.Fire        elemento Fuego,      mitigado por MagicResistance
Damage.Magical.Water       elemento Agua,       mitigado por MagicResistance
Damage.Magical.Lightning   elemento Tormenta,   mitigado por MagicResistance
Damage.Magical.Poison      elemento Naturaleza, mitigado por MagicResistance
Damage.Poise               daño a postura, sin mitigación
```

*Efectos:*
- `Effects.HitReact` — otorgado por `GE_HitReact` mientras el personaje reproduce su animación de golpe recibido. Inmoviliza al enemigo (MaxWalkSpeed = 0). `APantheliaEnemy` se suscribe con `RegisterGameplayTagEvent` para reaccionar.
- `Effects.Stagger` — otorgado por `GE_Stagger` cuando Poise llega a 0. Aturdimiento prolongado. `APantheliaEnemy` también se suscribe vía `RegisterGameplayTagEvent`. Ver `State_Combat.md` (sistema de Postura).

*Raíces nativas para búsquedas seguras (Etapa 1.3):*
- `Abilities` e `InputTag` — las **raíces** de esas jerarquías ahora son miembros nativos del singleton. Antes, `GetAbilityTagFromSpec`/`GetInputTagFromSpec` del ASC las pedían con `FGameplayTag::RequestGameplayTag(FName("Abilities"/"InputTag"))` — un string mal escrito **compila** y falla en runtime; un miembro nativo **no compila** si tiene typo. **No añade tags nuevos al editor** (los hijos ya creaban estas raíces implícitamente); solo las expone como miembros C++ seguros. Ningún asset ni Blueprint se ve afectado.

*IA del Boss (StateTree):*
- `Boss.Phase.*`, `Boss.Action.*`, `Boss.State.*` — para el sistema de IA del boss (fases, acciones y estado runtime). El `BossProfile` usa además tags de la jerarquía `BossAction.*` (identidad de acción) y `Abilities.Attack.*` (la ability real). Ver `State_AI.md`.

### Mapas data-driven en FPantheliaGameplayTags

Inicializados también en `InitializeNativeGameplayTags()`, sirven al `ExecCalc_Damage` para no hardcodear con `if`s:
- `DamageTypesToResistances` (`TMap<FGameplayTag, FGameplayTag>`) — tipo de daño → tag de resistencia del target. `Damage.Physical` genérico mapea a tag vacío (sin resistencia elemental).
- `DamageTypeToElement` (`TMap<FGameplayTag, EPantheliaElement>`) — tipo de daño → elemento del atacante, para la tabla de afinidades.

(La tabla de afinidades en sí vive como función estática en el ExecCalc — ver `State_Abilities.md` sección 7.)

### UPantheliaAssetManager

Hereda de `UAssetManager` y sobreescribe `StartInitialLoading()`, el punto más temprano del ciclo de vida del juego (antes incluso de `GameInstance::Init`). Desde ahí llama a `FPantheliaGameplayTags::InitializeNativeGameplayTags()`, garantizando que cualquier sistema que pida tags después los encuentre ya registrados.

**Registro en el proyecto:** En `Config/DefaultEngine.ini`, bajo `[/Script/Engine.Engine]`:

```ini
AssetManagerClassName=/Script/PantheliaProject.PantheliaAssetManager
```

Sin esta línea, Unreal usa el `UAssetManager` por defecto y nuestro `StartInitialLoading` nunca se ejecuta.

---

## 5. Effect Actors — ✅ Funcional

**`APantheliaEffectActor`** — Actor genérico que aplica GameplayEffects por overlap:
- Soporta efectos Instant, Duration e Infinite con políticas separadas
- `EEffectApplicationPolicy` — aplicar al begin, end, o nunca
- `EEffectRemovalPolicy` — para efectos infinitos, remover al salir del overlap
- `bDestroyOnEffectApplication` / `bDestroyOnEffectRemoval` para pociones vs áreas persistentes

Usado típicamente para pociones (Instant + destroy on apply), aceleradores de stamina (Duration), y zonas de efecto persistente (Infinite + remove on overlap end).

---

## 6. Sistema de Gameplay Abilities — ✅ Funcional (base)

Ver `State_Abilities.md` para documentación completa. Resumen de lo que afecta al ASC:

### Nuevas funciones en UPantheliaAbilitySystemComponent

**`AddCharacterAbilities(TArray<TSubclassOf<UGameplayAbility>>)`**
- Itera el array, crea un `FGameplayAbilitySpec` por ability
- Castea a `UPantheliaGameplayAbility` para leer `StartupInputTag`
- Agrega `StartupInputTag` a `DynamicAbilityTags` de la spec (editable en runtime → remapeo de teclas posible)
- Llama `GiveAbility()` — NO `GiveAbilityAndActivateOnce` (soulslike: activación por input del jugador)

**`AbilityInputTagHeld(const FGameplayTag&)`**
- Valida tag, itera `GetActivatableAbilities()`
- Por cada spec con ese tag en `DynamicAbilityTags`: `AbilitySpecInputPressed()` + `TryActivateAbility()` si `!IsActive()`
- Usa `HasTagExact` (no acepta tags padre)

**`AbilityInputTagReleased(const FGameplayTag&)`**
- Itera specs con el tag y llama `AbilitySpecInputReleased()`
- NO cancela la ability — la ability decide qué hacer al soltar el input

**`NotifyComboInputPressed(const FGameplayTag&)`** — para el buffer del combo melee del jugador. Busca la ability de ataque del jugador activa y le marca el buffer vía `TryBufferComboInput()`. Lo llama el `PlayerController` desde `AbilityInputTagPressed` (edge-triggered, una pulsación = una llamada, `ETriggerEvent::Started`), **no** desde el input Held. Ver `State_Combat.md` sección 10.3 (por qué el routing edge-triggered fue necesario).

**`NotifyBlockInputReleased(const FGameplayTag&)`** — para el sistema de parry/bloqueo. Busca la ability de parry con ese InputTag y le notifica el release (termina la guardia). Necesario porque el input custom no alimenta de forma fiable el `InputReleased` interno de GAS; se conecta desde `PlayerController::AbilityInputTagReleased`. Ver `State_Combat.md` sección 11.3.

**Contexto efímero de entrada al ataque (sistema de dodge):** `EPantheliaAttackEntryContext {Normal, DodgeFollowup, PerfectBlockCounter, FutureSpecial}` (los dos últimos **reservados**). Vive en `PendingAttackEntryContext` (miembro del ASC). Flujo: el dodge llama `SetPendingAttackEntryContext(DodgeFollowup)` → activa la ability por InputTag → la ability, en su `ActivateAbility`, llama **`ConsumeAttackEntryContext()`**, que **devuelve el contexto y lo resetea a `Normal` en la misma operación** (evita contaminar ataques posteriores). Permite que el ataque sepa que viene de un dodge y elija el montage opener del arma. También se **resetea en `AbilityActorInfoSet()`**, porque el ASC del jugador persiste entre Pawns.

> **Verificación por consumo (patrón importante):** que `TryActivateAbilityByInputTag()` devuelva `true` **no garantiza** que la activación completara. Por eso, tras intentar activar el follow-up, se comprueba **si la ability consumió realmente el contexto**; si sigue pendiente, se considera activación fallida, se resetea `PendingAttackEntryContext` y se loguea un warning (no queda un "contexto fantasma" que contamine el siguiente ataque normal).

**`NotifyDodgeFollowupInputPressed(const FGameplayTag&)`** — ofrece una pulsación al dodge activo (`TryBufferFollowupInput`). Devuelve si el dodge **consumió** el input; si lo consumió, esa pulsación **no** continúa hacia la activación normal (una pulsación, un solo destino). Ver el orden en `State_Input.md`.

**`NotifyParryImpact(bool bWasPerfectParry)`** — busca **cualquier** ability de parry activa (el impacto lo dispara el golpe recibido, no un input; solo una puede estar activa) y le pasa la notificación, que desencadena knockback y Gameplay Cue. Lo llama `HandleParryReaction` desde el AttributeSet. Ver `State_Combat.md` secciones 11.5 y 11.7.

**Nivel de abilities independiente (Etapa 2)** — permite que el árbol suba el nivel de un hechizo sin tocar el nivel del personaje. Tres funciones nuevas:
- `GetSpecFromAbilityTag(FGameplayTag) → FGameplayAbilitySpec*` (**solo C++**, no UFUNCTION) — recorre las abilities otorgadas con `FScopedAbilityListLock` (obligatorio: sin el lock, conceder/quitar una ability a mitad de iteración corrompe el array), compara contra `GetAssetTags()` (reemplazo del deprecado `AbilityTags`) y devuelve el puntero al spec. **Advertencia:** usar y soltar en el mismo scope, **nunca guardar** — el array del ASC puede realojarse y dejar el puntero colgando. Por eso no es Blueprint-accesible.
- `GetAbilityLevelFromTag(FGameplayTag) → int32` (BlueprintCallable) — devuelve el nivel de la ability, o **0 si el ASC no la tiene**. Convención deliberada: **`0 = no desbloqueada`** (toda ability otorgada tiene nivel ≥ 1). Una sola llamada responde "¿la tiene?" + "¿a qué nivel?".
- `SetAbilityLevel(FGameplayTag, int32) → bool` (BlueprintCallable) — cambia `Spec.Level` y llama a `MarkAbilitySpecDirty` (la función oficial de GAS para notificar que una spec cambió). Clampea a mínimo 1 (degradación con gracia ante datos malos del árbol). Si el nivel pedido == actual, retorna **sin** `MarkAbilitySpecDirty` (importante para el `ReapplyAllNodes` del futuro SaveGame: evita una tormenta de notificaciones al recargar). Loguea warning si la ability no existe (error de flujo: desbloquear va antes que subir nivel).

> **Cómo se conecta con el escalado ya existente:** tras `MarkAbilitySpecDirty`, la **siguiente activación** de la ability lee el nivel nuevo en `GetAbilityLevel()`, y con él escalan `DamageTypes` y `PoiseDamage` (que ya usaban `GetValueAtLevel(GetAbilityLevel())`). Por eso esta etapa **no tocó ninguna ability existente** — la tubería de escalado ya estaba lista esperando esto. Una activación en curso no cambia retroactivamente (un Firebolt ya lanzado no sube de daño en el aire). El nivel de abilities es **completamente independiente** del nivel del personaje: solo el árbol lo sube, nodo a nodo. Ver `State_Progression.md` (árbol) y `Code_Review.md` (Fricción 1, resuelta).

**`UpgradeAttribute(const FGameplayTag& AttributeTag)`** — gasto de un punto de atributo (single-player, **sin RPC**): comprueba `GetAttributePoints() > 0` vía la interfaz, envía un `FGameplayEventData` a sí mismo (`EventTag = AttributeTag`, `EventMagnitude = 1.f`) con `SendGameplayEventToActor`, y resta 1 al saldo (`AddToAttributePoints(-1)`). El evento lo recibe `GA_ListenForXPEvents`, que enruta la magnitud al `SetByCaller` del `GE_EventBasedEffect`. Ver `State_Progression.md` sección 4.

**Soporte de Ability Info (clases 240-241):**
- `FAbilitiesGiven AbilitiesGivenDelegate` (multicast, OneParam con el ASC) — se broadcasta al final de `AddCharacterAbilities`. Junto al flag `bStartupAbilitiesGiven` (=false) resuelve la carrera de inicialización ASC↔UI (la UI puede llegar antes o después de que se concedan las abilities).
- `FForEachAbility` (delegate OneParam, **no** multicast) + `ForEachAbility(Delegate)` — itera las abilities activables de forma segura con `FScopedAbilityListLock` y ejecuta el delegate por cada una. Lo usa el `OverlayWidgetController` para poblar el HUD (ver `State_UI.md`).
- Utilidades estáticas: `GetAbilityTagFromSpec` (busca el tag hijo de `Abilities` en los AssetTags) y `GetInputTagFromSpec` (busca el tag hijo de `InputTag` en los DynamicSpecSourceTags).

> **Actualizaciones de API para UE 5.8** (frente a la transcripción del curso): usar `Ability->GetAssetTags()` (no `->AbilityTags`, deprecado UE5.4+) y `Spec.GetDynamicSpecSourceTags()` (no `DynamicAbilityTags`, deprecado UE5.3+ — esto **resuelve de paso** la advertencia de deprecación anotada antes en este documento, allí donde el código nuevo ya usa la API nueva). Para loguear `__FUNCTION__` usar `%hs` (char ANSI), no `%s`.

> **⚠️ Advertencia de deprecación (preexistente, no rompe hoy):** `FGameplayAbilitySpec::DynamicAbilityTags` está deprecado en UE; la API nueva es `GetDynamicSpecSourceTags()`. Aparece en **4 sitios** de `PantheliaAbilitySystemComponent.cpp` (incluidos `AddCharacterAbilities`, `AbilityInputTagHeld/Released`). No rompe ahora, pero sí en una versión futura de UE. Pendiente de migrar.

### Tags de Abilities (activación)

`Abilities.Attack` — tag genérico de ataque, padre de los siguientes. Se conserva para queries (p. ej. `GA_HitReact` cancela las abilities con este tag), pero el Behavior Tree **no** lo usa directamente para activar.

- `Abilities.Attack.Melee` — ability de ataque cuerpo a cuerpo (`GA_MeleeAttack`)
- `Abilities.Attack.Ranged` — ability de ataque a distancia (`GA_RangedAttack`)

**Motivo de la separación:** el BT activa abilities por tag con `TryActivateAbilitiesByTag`. Con un único `Abilities.Attack`, un enemigo híbrido (melee + ranged) activaría ambas a la vez. Los tags hijos permiten que cada rama del BT active solo su ability. Ver `State_Abilities.md` sección 8 (sistema de ataque enemigo).

> El daño base de las abilities ya **no** vive en una variable `Damage` única — se reemplazó por el `TMap` `DamageTypes` en `UPantheliaDamageGameplayAbility`. Ver `State_Abilities.md` secciones 2 y 7.

### Tags de Montage (socket de origen del ataque)

Indican desde qué socket se origina un ataque; los lee `GetCombatSocketLocation(MontageTag)` (centralizado en C++, ver `State_Abilities.md` sección 5):
- `Montage.Attack.Weapon` → arma (`FinalWeaponMesh` + `WeaponTipSocketName`)
- `Montage.Attack.RightHand` / `Montage.Attack.LeftHand` → manos del mesh
- `Montage.Attack.RightFoot` / `Montage.Attack.LeftFoot` / `Montage.Attack.Mouth` → pies y boca (para ataques de bosses futuros: patadas, mordiscos, alientos elementales)

### Tags de Input en FPantheliaGameplayTags

Nuevos tags registrados:
`InputTag.LightAttack`, `InputTag.HeavyAttack`, `InputTag.Block.Physical`, `InputTag.Block.Magic`, `InputTag.Dodge`, `InputTag.Spell.1` a `InputTag.Spell.5`, `InputTag.Spell.Ultimate`

### Tags de estado de parry/bloqueo

Concedidos durante la ventana de defensa por `UPantheliaParryAbility` (ver `State_Combat.md` sección 11):
- `State.Parry.Physical` / `State.Parry.Magic` — ventana de parry perfecto activa
- `State.Block.Physical` / `State.Block.Magic` — guardia sostenida activa (concedidos **a la vez** que los de parry, desde `EnterParryWindow`)
- `State.PerfectBlock` — habilita el contraataque tras un parry perfecto (ver `State_Input.md`)

El `ExecCalc_Damage` evalúa **Parry antes que Block** (`if/else if` en `ApplyParryBlockMitigation`), por eso es seguro tener ambos tags durante la ventana.

### Tags de GameplayCue (parry) y patrón `ExecuteGameplayCue`

4 tags nativos para el feedback de parry/bloqueo, disparados por `FireParryCue()` según `EParryType` y si fue parry perfecto:
- `GameplayCue.Parry.Physical.Perfect` / `GameplayCue.Parry.Physical.Block`
- `GameplayCue.Parry.Magic.Perfect` / `GameplayCue.Parry.Magic.Block`

Y un tag para el impacto de melee:
- `GameplayCue.Melee.Impact` — disparado por el `WeaponTraceComponent` al conectar un golpe (`ExecuteGameplayCue` con la víctima en `SourceObject`, el atacante en `EffectCauser` y el tag del montage en `AggregatedSourceTags`); lo consume `GC_MeleeImpact` (sangre + sonido). Ver `State_Combat.md` sección 9.

**Patrón:** se disparan con `ASC->ExecuteGameplayCue(CueTag, CueParams)` (one-shot, no persistente), rellenando `FGameplayCueParameters` (Location, Normal, Instigator, EffectCauser). Si el asset `GC_*` correspondiente no existe, la llamada es **silenciosa** (no crashea), lo que permite tener los ganchos en código antes que los assets. Requiere `#include "GameplayCueManager.h"`. Los assets `GC_Parry_*` están pendientes (ver `State_Combat.md` sección 11.8).

### Canales de colisión custom — ✅ corregido

Configuración real confirmada en `Config/DefaultEngine.ini`:
- `ECC_GameTraceChannel1` = **"Fighter"** (Trace Channel) — usado por el Weapon Trace (`State_Combat.md` sección 9), por el `UTraceComponent` legacy y por el lock-on (`State_Combat.md` sección 3)
- `ECC_GameTraceChannel2` = **"Projectile"** (Object Channel) — el canal real de proyectiles

El header `PantheliaProject.h` define un alias, ya **corregido**:

```cpp
#define ECC_Projectile ECC_GameTraceChannel2   // corregido (antes apuntaba, por error, a GameTraceChannel1)
```

> **Histórico (resuelto):** este `#define` apuntaba por error a `ECC_GameTraceChannel1` (el canal *Fighter*) en lugar de `ECC_GameTraceChannel2`. El bug estaba oculto porque los proyectiles detectan vía canal `Pawn`, no vía el canal Projectile, así que nunca rompió nada visible. Corregido al canal correcto.

Usado por `APantheliaProjectile` (ver `State_Abilities.md`) como Object Type de su esfera de colisión. Evita que el proyectil impacte con pickups, decorados u otros actores que no deben recibir daño — solo reacciona a lo que explícitamente responde a este canal.

### Custom Effect Context (flag de crítico) — ✅ Funcional

Para transmitir información extra a través del pipeline de daño (empezando por si un golpe fue crítico), se extiende el `FGameplayEffectContext` nativo.

**`FPantheliaGameplayEffectContext`** (`AbilitySystem/PantheliaAbilityTypes.h/.cpp`):
- Subclase de `FGameplayEffectContext` con campos custom (protected). `NetSerialize` usa **14 bits (0–13)**:
  - **0–4:** Instigator / EffectCauser / AbilityCDO(+Level) / SourceObject (heredados de GAS) + `bIsCriticalHit`
  - **5–9 (debuff, Combat Tricks):** `bIsSuccessfulDebuff`, `DebuffDamage`, `DebuffDuration`, `DebuffFrequency`, `DamageType` (`TSharedPtr<FGameplayTag>`, mismo patrón que `HitResult` de la clase base)
  - **10–12 (vectores):** `DeathImpulse`, `KnockbackForce`, `LaunchForce`
  - **13:** `bKnockbackIsHeavy`
  - Además, campos de parry preexistentes: `bWasParried`, `bWasBlocked`, `ParryPoiseDamageToAttacker` (nota conocida: **no** están en `NetSerialize`; irrelevante en single-player).
- Métodos: `IsCriticalHit()`/`SetIsCriticalHit()`; `WasParried()`/`WasBlocked()`/`SetParryResult()`/`SetWasBlocked()`/`GetParryPoiseDamageToAttacker()`; getters/setters de debuff (`IsSuccessfulDebuff`, `GetDebuffDamage/Duration/Frequency`, `GetDamageType`/`SetDamageType` — el `MakeShared` del tag lo hace el llamador, en la librería), y de los vectores/`bKnockbackIsHeavy`.
- Overrides obligatorios: `GetScriptStruct()`, `Duplicate()` (retorna `FPantheliaGameplayEffectContext*`), `NetSerialize()`
- `TStructOpsTypeTraits` con `WithNetSerializer=true, WithCopy=true`

> **Patrón "escribir siempre" (fix de auditoría, crítico).** Un swing melee aplica el **mismo** spec a varios objetivos, y esas aplicaciones **comparten el mismo objeto de contexto** (el handle es ref-counting, no copia por objetivo). Por eso **todo productor de resultado debe escribir su valor en AMBOS casos (éxito y fracaso)**, antes de cada aplicación — si solo escribe en éxito, el resultado del objetivo anterior se "filtra" al siguiente. `SetIsCriticalHit` ya lo hacía bien y es el patrón de referencia. Se corrigieron: los productores de resultado de estado (reset de su flag al entrar), los vectores de knockback/launch (variables locales inicializadas a `ZeroVector`/`false`, escritas siempre antes de aplicar — convención **`ZeroVector` = "esta tirada no salió"**, leída con `IsNearlyZero`), y el bloque de parry (`SetParryResult`/`SetWasBlocked` siempre). *(El productor de debuff aleatorio original, `DetermineDebuff`, fue después eliminado por el rediseño de buildup; el patrón sigue aplicando a los productores vigentes.)* Ver `Code_Review.md`.

**`UPantheliaAbilitySystemGlobals`** (`AbilitySystem/PantheliaAbilitySystemGlobals.h/.cpp`):
- Override de `AllocGameplayEffectContext()` que devuelve `new FPantheliaGameplayEffectContext()`
- Registrado en `Config/DefaultGame.ini`:
  ```ini
  [/Script/GameplayAbilities.AbilitySystemGlobals]
  AbilitySystemGlobalsClassName="/Script/PantheliaProject.PantheliaAbilitySystemGlobals"
  ```
  Sin esta línea, GAS usa el context nativo y el flag se pierde.

**Acceso desde `UPantheliaAbilitySystemLibrary`** (funciones estáticas):
- `IsCriticalHit(const FGameplayEffectContextHandle&)` → bool (BlueprintPure)
- `SetIsCriticalHit(UPARAM(ref) FGameplayEffectContextHandle&, bool)` (BlueprintCallable)

**Uso:** el `ExecCalc_Damage` escribe `bIsCriticalHit` con `SetIsCriticalHit()` cuando un golpe es crítico; el AttributeSet lo lee con `IsCriticalHit()` en `PostGameplayEffectExecute`. **TODO:** disparar VFX/SFX de crítico cuando ese sistema esté listo.

---

## 7. Pendientes del sistema GAS

### 7.1 Daño físico vía GAS — ⚠️ Parcial

El daño de **hechizos** ya pasa por GAS mediante `UExecCalc_Damage` (ver `State_Abilities.md` sección 7). Sin embargo, `TraceComponent` (ver `State_Combat.md`) todavía usa `TakeDamage()` legacy para los **ataques físicos** con arma. Falta migrarlo para que aplique `GE_Damage` con SetByCaller como hacen los hechizos.

### 7.2 Postura / Stagger — ✅ Implementado

`Poise`/`MaxPoise`, el meta atributo `IncomingPoiseDamage`, el flinch (HitReact por umbral), el stagger (al llegar Poise a 0) y la regeneración de postura están implementados. Ver `State_Combat.md` (sistema de Postura). Queda pendiente solo crear `GA_Stagger`/`GE_Stagger` en Blueprint y asignar montages.

### 7.3 MMCs pendientes

`Armor`, `MagicResistance`, `MaxPoise`, `Tenacity`. Por ahora son placeholders en el GE de secundarios.

### 7.4 MMCs para PhysicalDamage y MagicDamage — ⚠️ Pendiente

`PhysicalDamage` y `MagicDamage` están en el AttributeSet pero actualmente valen 0. Cada uno tiene un uso distinto en el diseño de daño (ver `State_Abilities.md` sección 7):

- `MMC_PhysicalDamage` — calculado desde `Hardness`. El ExecCalc lo captura del source y lo **suma al daño físico recibido** (modelo AD de LoL). También puede usarse como ratio en `AttributeScalings` (auto-doble-conteo intencional).
- `MMC_MagicDamage` — calculado desde `Resonance`. El ExecCalc **NO lo captura** (asimetría intencional, modelo AP de LoL). Su único uso es como ratio en `AttributeScalings` de habilidades y armas.

Sin estos MMCs, ambos atributos valen 0 y todos los ratios/sumandos basados en ellos dan 0.

### 7.5 Sistema de Daño Elemental + Escalado por Atributos — ✅ Implementado

Las 8 ramas de daño, las 4 resistencias elementales, la tabla de afinidades ±15%, el escalado por atributos (`FAbilityAttributeScaling`) y `GetDefensiveElement()` están implementados. Documentado en detalle en `State_Abilities.md` sección 7. Pendientes residuales: MagicResistance/MagicPenetration en el ExecCalc (7.4 arriba y `State_Abilities.md` sección 11.2), y el elemento defensivo del jugador depende del Sistema Elemental aún no implementado (`State_Pending.md`).

### 7.6 Bug del `#define ECC_Projectile` — ✅ Resuelto

El alias `#define ECC_Projectile` en `PantheliaProject.h` apuntaba por error a `ECC_GameTraceChannel1` (canal *Fighter*); corregido a `ECC_GameTraceChannel2` (canal *Projectile* real). No rompía nada porque los proyectiles detectan vía canal `Pawn`. Detalle en la sección 6.

### 7.7 Velocidad de ataque (`AttackSpeed`) — 🔨 Diseñado, listo para implementar

Atributo secundario nuevo para acelerar/ralentizar las animaciones de ataque mediante buffs, pasivas y nodos del árbol de habilidades. **Spec cerrada, aún no implementado.**

**Modelo:** `AttackSpeed` es un **multiplicador del play rate** del montage de ataque. `1.0` = velocidad normal; `1.5` = 50% más rápido. No es un porcentaje (no 100 base) — el valor se pasa tal cual como play rate.

**Cap de diseño:** **máximo 2.5** (clamp duro en código). El balance de game design buscará que el jugador no pase de ~2.1–2.2; el 2.5 es el techo absoluto que el código nunca debe superar. Mínimo razonable 0.1 (evita animaciones congeladas).

**Implementación prevista (patrón estándar GAS):**
1. Añadir el atributo `AttackSpeed` al `UPantheliaAttributeSet` con el boilerplate habitual (OnRep, `DOREPLIFETIME`, `ATTRIBUTE_ACCESSORS`, entrada en `TagsToAttributes`). Valor base 1.0.
2. Registrar el tag `Attributes.Secondary.AttackSpeed` en `FPantheliaGameplayTags` y añadirlo a la lista de la sección 4.
3. Clampear en `PreAttributeChange` al rango `[0.1, 2.5]`.
4. En las abilities de ataque (`GA_MeleeAttack`, `GA_Firebolt`, etc.), leer `AttackSpeed` del ASC del caster y pasarlo como **play rate** del `PlayMontageAndWait` (hoy ese pin está fijo en 1.0).
5. Buffs/pasivas/nodos del árbol = GEs que modifican `AttackSpeed` (Duration con Multiply para buffs temporales; Infinite para pasivas/árbol). GAS combina los multiplicadores de forma nativa.

**Interacción con el Weapon Trace** (ver `State_Combat.md` sección 9): el `WeaponTraceNotifyState` está anclado a frames del montage, así que al acelerar el play rate la ventana de daño se acelera con él automáticamente (mismo punto de la animación, menos tiempo real) — no requiere ajustes. El cap de 2.5 también protege contra "tunneling" del sweep a play rates extremos.

**Pendiente de migración:** el combo melee legacy del jugador (`CombatComponent::ComboAttack`, `PlayAnimMontage` directo) no leería `AttackSpeed` hasta que el jugador se migre al Weapon Trace + GAS.

**Decisión de diseño abierta:** si `AttackSpeed` acelera solo el swing o también la recuperación posterior — depende de cómo estén troceados los montages. El play rate afecta a toda la animación que reproduce el `PlayMontageAndWait`.
