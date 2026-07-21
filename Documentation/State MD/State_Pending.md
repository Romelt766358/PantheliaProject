# State_Pending — Sistemas grandes sin implementar

> **Propósito:** Sistemas mayores que están diseñados pero aún no implementados. Cuando alguno se implemente, **mover** su contenido a un archivo dedicado (`State_Parry.md`, etc.) y **eliminarlo de aquí**.

> Pendientes menores que ya tienen sistema base implementado viven en el archivo de su sistema (`State_GAS.md` sección 7, `State_Combat.md` sección 8, `State_UI.md` sección 8, etc.).

> **Leyenda de estado:** ❌ No implementado (conceptual) · 🔨 Diseñado y listo para implementar (spec cerrada).

---

## 1. Sistema de Daño Elemental + Escalado por Atributos — ✅ Implementado

El sistema está **implementado**. La documentación de referencia vive en `State_Abilities.md` sección 7 (ExecCalc, tabla de afinidades, pipeline, `FAbilityAttributeScaling`) y `State_GAS.md` (tags de daño, resistencias elementales, mapas data-driven, meta atributos). Incluye los 8 tipos de daño, la tabla de afinidades ±15%, las 4 resistencias elementales, el escalado por atributos secundarios/vitales del caster, y `GetDefensiveElement()` en `ICombatInterface`.

**Pendientes residuales dentro de este sistema:**
- **MagicResistance + MagicPenetration en el ExecCalc** — pendiente del chat de matemáticas. Marcado con TODO en `ExecCalc_Damage.cpp`. Ver `State_Abilities.md` sección 11.2.
- **MMCs de `PhysicalDamage` y `MagicDamage`** — sin implementar, ambos valen 0 hasta entonces. Ver `State_GAS.md` sección 7.4.
- **`GetDefensiveElement()` del jugador** — devuelve `None` hasta que exista el Sistema Elemental (corazones, sección 3 de este documento). Los enemigos ya tienen su `DefensiveElement` editable.
- **`AttributeScalings` de las habilidades** — configurables por Blueprint en cada `GA_*`. Las habilidades actuales aún no tienen escalado asignado (llega con el playtesting/balance).

> **⚠️ PLACEHOLDER — Resistencias Elementales:**
> `FireResistance`, `WaterResistance`, `StormResistance`, `NatureResistance` se derivan actualmente de `Resilience` mediante un modifier en `GE_SecondaryAttributes` (Override, Backing=Resilience, PostMultAdd=3, Coeff=0.5). Es temporal. Cuando existan el sistema de equipamiento y el árbol de habilidades, ese modifier se elimina del GE y son esos sistemas los que modifican las resistencias directamente. Marcado con `// PLACEHOLDER` en `ExecCalc_Damage.cpp` y `PantheliaAttributeSet.h`.

---

## 2. Árbol de Habilidades — 🔨 Infraestructura lista, faltan los nodos

**Pilar central del juego.** La **infraestructura completa ya está implementada y documentada en `State_Progression.md` sección 5** (Data Asset `UPantheliaSkillTreeInfo`, componente `UPantheliaSkillTreeComponent` en el PlayerState, integración con GAS, principio de persistencia `NodeRanks`). Las decisiones de gameplay (modelo Dragon Age, costes, puntos por nivel) están en `State_Progression.md` sección 1.

**Lo que queda pendiente:**
- **Poblar `DA_SkillTree`** con los nodos reales (sección 24 del curso + diseño soulslike). La infraestructura los espera; es trabajo de editor + diseño, no de código.
- **Equipado de hechizos** (asignar `InputTag` a las abilities desbloqueadas) — sección 24, clases ~290+. **Desbloquear (árbol) y equipar (input) son acciones distintas por diseño.**
- **SaveGame** que serialice `NodeRanks` y llame a `ReapplyAllNodes()` al cargar (el principio "GAS se reconstruye desde datos" ya está fijado).
- **UI del árbol** (`SpellMenuWidgetController`): solo lee y pide `TryUnlockNode`; nunca decide. Se bindeará a `OnSkillPointsChangedDelegate` y `OnSkillNodeChangedDelegate`. Los widgets base de la vitrina (`WBP_SpellGlobeButton`, árboles ofensivo/pasivo) ya existen pero están **pausados** — ver `State_UI.md` sección 10.

> Los `SkillPoints` (moneda del árbol) ya existen en el `PlayerState` desde la Fase 1 de progresión, y el nivel de abilities ya es subible por el árbol (Etapa 2, resuelve la Fricción 1 — ver `Code_Review.md`).

---

## 3. Corazones y equipamiento elemental — ❌ No implementado

> El **daño elemental base y los estados** ya existen (secciones 1 y 4). Lo que sigue pendiente es el sistema de **corazones equipables** que define el elemento activo del jugador y cuelga pasivas por evento. **La arquitectura de destino (cómo debe encajar con el árbol, el loadout de dos armas y la hoguera) está en `State_Architecture_Future.md` — ese documento es la referencia; aquí queda solo el resumen de gameplay y el estado.**

4 elementos: **Fuego**, **Agua**, **Tormenta**, **Naturaleza**. Cada elemento agrupa un tipo de daño físico y uno mágico (salvo Fuego, solo mágico) — ver la tabla en la sección 1.2.

Cada elemento tiene:
- Corazón elemental equipable (define el elemento activo del brazo izquierdo, y el `GetDefensiveElement()` del jugador — ver sección 1.3)
- Hechizos disponibles
- Habilidad definitiva
- Pasivas elementales (modificables por el árbol), **incluidos traits que reducen cooldowns** (ej. corazón de agua → un parry exitoso reduce cooldowns; corazón de fuego → un ataque ligero a un enemigo quemándose los reduce). Patrón acordado de dos mitades: el evento se emite con `SendGameplayEventToActor` (tag tipo `Event.Parry.Success`) desde el código que ya ocurre, y el corazón equipado concede una **ability pasiva** que lo escucha (`WaitGameplayEvent`); si el corazón no está equipado, la ability no existe y el evento no hace nada. La reducción remueve y reaplica un GE de cooldown más corto (el HUD de cooldown radial se reajusta solo — ver `State_UI.md` sección 8.4). Cada corazón = una pasiva distinta escuchando un evento distinto (escalable sin tocar cooldown ni HUD).

**Decisiones pendientes:**
- ✅ **Resuelto (arquitectura futura): un solo corazón activo**, cambiable únicamente en hoguera. Ver `State_Architecture_Future.md` sección 1.
- ¿Cómo se cambia en runtime — desde la hoguera (confirmado el "dónde"); falta el "cómo" exacto de la UI?
- Cómo se representa en GAS — probablemente cada corazón otorga un set de Abilities y un GE Infinite con las pasivas (encaja con `UPantheliaHeartDefinition` del documento de arquitectura).
- Cómo el corazón equipado determina el elemento defensivo y ofensivo del jugador para el sistema de daño (sección 1).

> **Gancho ya disponible:** las armas del jugador ya declaran su `WeaponElement` (mismo enum `EPantheliaElement` que el corazón) en `UPantheliaWeaponDefinition` — ver `State_Combat.md` sección 10.1. Cuando exista el corazón, la sinergia arma-elemento (bonus cuando el elemento del arma coincide con el del corazón) se construye sobre ese campo, que ya está en su sitio.

---

## 4. Efectos de Estado Elementales — ✅ Implementado (buildup determinista)

El sistema se implementó y **ya no es un pendiente grande**. Diseño y detalle en `State_Combat.md` sección 12.2, `State_GAS.md` (estados por buildup) y `State_Abilities.md` (`BuildupAmounts`). Resumen: barras de buildup deterministas a 100 (sin `DebuffChance` aleatoria), `DA_ElementalStatusConfig` global, escalado por `StatusPower`/`MagicDamage`, Quemadura/Veneno (DoT + Defense Shred), Electrocución (burst + postura) y Saturación (parcial).

**Residuales pendientes:**
- **Payload principal de Saturación** (`AttributeDebuff`) — aún no implementado; hoy solo aplica tag + daño de postura y emite un warning.
- **Barras de buildup e iconos de estado en el HUD** (ver `State_UI.md`).
- **Curvas mágicas independientes** (`MagicPenetration`/`EffectiveMagicResistance`) en standby — fallback a curvas físicas mientras tanto.
- **Contenido real:** GEs de perks/equipo que otorguen `StatusPower`/daño porcentual/Defense Shred; Niagara de los estados; pruebas runtime completas del sistema ampliado.

---

## 5. Sistema de Curación + Heridas Graves (antiheal) — ✅ Implementado

Implementado. Detalle en `State_GAS.md` (`IncomingHealing`, `HandleIncomingHealing`, Heridas Graves). Resumen: **todo** GE de curación pasa por el meta atributo `IncomingHealing`; `HandleIncomingHealing` aplica la reducción de Heridas Graves y clampea sobre `Health`.

> **Corrección de diseño respecto a la spec vieja de este documento:** el modelo **ya NO acumula** varias fuentes hacia un cap [40,60]. El sistema real usa la **reducción activa más fuerte** (estilo LoL clásico: Direct 30% + Poison 40% → 40%, no 70%). Es **inmediata y reutilizable** (armas, hechizos, ticks, parry, armaduras reactivas, bosses) sin barra de buildup, con duración mínima 4 s. Subtipos `Effects.GrievousWounds.Direct` / `.Poison` (Poison ligado a la duración del Veneno).

**Residuales pendientes:**
- **Contenido de curación real** (frascos, hechizos de vida, regeneración) que use `IncomingHealing`.
- Balance de porcentajes y duraciones por fuente.

---

## 6. Arquitectura común de costes de abilities — ✅ Implementada (cerrada)

Cerrada en la campaña de hardening (2026-07-16). `UPantheliaCostAttributeSet` (multiplicadores/flats de Mana y Stamina), resolvedor común con la fórmula `Final = max(0, Base × max(0, Multiplier) + Flat)`, GEs dinámicos separados (`Cost.Stamina` / `Cost.Mana`) y **pago atómico multi-recurso** (`AdditionalResourceCosts`, valida todo antes de cobrar, rollback defensivo). Migrada a ataques, dodge, parry y hechizos. Detalle en `State_GAS.md`.

**Residual:** cablear el coste de la **transición a guardia** y del **impacto bloqueado** con sus fuentes (ver Guard Break en `State_Combat.md` sección 11); balance de los multiplicadores/flats por el árbol/equipo (contenido).

---

## 7. Attack Speed — ❌ Diseñado, no implementado

Debe definir su interacción con: play rate del montage, ventanas de trace, combo/follow-up, notifies, root motion y los límites que le pongan el árbol/equipo. No implementar hasta cerrar ese diseño (mover una sola pieza sin las demás rompe el timing del combate).

## 8. Sprint / migración de `UPlayerActionsComponent` — ❌ Fase separada

Hoy Sprint/Walk funciona por el componente legacy (sin Tick). Para migrarlo a GAS: crear la ability de sprint, definir coste/drenaje de estamina, reglas de cancelación (ataque/dodge/hit/muerte), migrar el input, validar feeling y retirar el componente y sus archivos. Ver `State_Overview.md` sección 2.12.

## 9. Zonas persistentes de daño — ❌ Solo testbed

`BP_FireArea` es testbed y no está colocado. Un sistema real (`Zona de ability → Instigator del atacante`; `Hazard ambiental → ASC ambiental propio`) debe diseñar ownership, stats, duración, stack, GameplayCue y Save/streaming antes de crear contenido productivo.

## 10. Assets legacy conservados a propósito

No son olvidos; se conservan deliberadamente (ver `State_Hardening.md` sección 4):
- **`AB_Diosa01`** — reparentado a `UPlayerAnimInstance`, compila; validación runtime pendiente de que una escena lo use (el nivel actual usa `ABP_Player`).
- **`BP_BaseKatana`** — ⚠️ **ELIMINADO en Fase 4I** (ya no existe; no restaurar). Antes figuraba como conservado.
- **`BP_HealthPotion`** — asset vacío, instancia retirada; el pickup funcional es `BP_PotionHeal`. Clasificar/eliminar más adelante.
- **`BP_FireArea`** — testbed, cero instancias; no reutilizar como base productiva.

## 11. Sistemas cerrados (ya NO son pendientes)

Para que ningún chat futuro los reabra: arquitectura común de costes · multi-recurso Mana+Stamina · daño elemental base y buildup · Heridas Graves · lifecycle de boss/fases/interrupciones · colisión y consumo de proyectiles · progresión transaccional · Data Validation (7 familias) · lifecycle de delegates/cooldown tasks/Widget Controllers · locks de la lista de abilities · hardening de EffectActor/equipamiento/Weapon Trace/lock-on/HUD · eliminación de `UBlockComponent`. Detalle en `State_Hardening.md` y `State_Validation.md`.

## 12. Migración MetaHuman — cerrada; residuales editoriales no bloqueantes

La migración del jugador a MetaHuman está **cerrada y aprobada** (validada en PIE en las fases funcionales y, después, en build Development Win64 **cocinada, empaquetada y con smoke cocinado PASS**; Fases 4G/4H/4I aprobadas). Ver `State_Overview.md`, `State_Combat.md` sección 10, `State_GAS.md`, `State_Validation.md` y `State_Hardening.md`. Lo que queda **no bloquea** gameplay, cook, package ni el smoke final.

**Pendientes reales de gameplay** (loop, no migración):
- **Respawn del jugador** — no implementado. El jugador muere pero no reaparece; ASC/PlayerState se conservan a propósito para soportarlo.
- **Checkpoint / hoguera** — no implementado.
- **Restore/reset de atributos y estados al respawn** (limpiar `State.Dead`, buildup, debuffs, tags temporales; reactivación controlada del avatar tras el checkpoint — ver `State_GAS.md`).
- **Respawn/reset de enemigos** al usar el checkpoint.
- **SaveGame del checkpoint.**

**Deuda editorial / técnica no bloqueante:**
- **Dissolve multipart (opcional)** — el dissolve actual es monomesh (slot 0); una política uniforme para Body/Face/grooms queda como decisión futura. **No** reutilizar a ciegas el dissolve del slot 0.
- **Physics Asset / asset final de armas** — el arma placeholder podía atravesar el suelo; la solución va **en el arma o su Physics Asset**, no en el jugador ni en DeathPresentation.
- **25 secuencias `MetaHumanMigration` cocinadas** (33 en disco) — presentes en el paquete pero **sin dependencia funcional** del Pawn de migración; revisar solo con una futura herramienta de auditoría/resave, **no** reconstruir montages a ciegas.
- **28 warnings de PoseAsset (Manny) out-of-date** — no bloqueantes.
- **`UnexpectedLoad` de `GC_MeleeImpact` y `GC_Dodge_Perfect`** — GameplayCues, no bloqueante.
- **Referencias soft/management a `SKM_Manny` / `SKM_Quinn`** (paquetes ausentes) — metadata residual; **no reparar a ciegas**.
- **Limpieza futura de TestBoss / Manny** — campaña independiente (son contenido **conservado y compartido**, no eliminado).
- **Clasificación futura de `BP_ThirdPersonCharacter`** (conservado, con referencias hard).
- **Shipping build** — futura (hasta ahora solo Development).
- **Herramienta `Montage Batch Tools`** (deseable) — comparar slots/secciones/notifies/branching/curvas/timings entre montages.

> **Assets legacy — cambios de estado (Fase 4I):** `BP_BaseKatana` y `BP_ThirdPersonGameMode` **fueron eliminados** (ya no figuran como "conservados"; no restaurar). Conservados: `BP_ThirdPersonCharacter` (referencias hard), `BP_Boss`/TestBoss (asset y familia; solo se retiró la **instancia** del mapa), Manny ARPG y `/Game/My_Manny` (compartido, no incluido en la limpieza autorizada), y `MetaHumanMigration` (74 assets, como rollback/deuda). Tabla canónica en `Code_Review.md`.

---

## 13. Notas transversales

Cuando se implemente cualquiera de estos:
1. Crear su `State_XXX.md` dedicado (o, para el Sistema de Daño Elemental + Escalado, integrarlo en `State_Abilities.md` sección 7 y `State_GAS.md`, ya que extiende sistemas existentes)
2. Eliminar su sección de este archivo
3. Mencionarlo en `State_Overview.md` (sección 6) y en los archivos relacionados con referencias cruzadas
4. Si introduce nuevos Gameplay Tags, registrarlos en `FPantheliaGameplayTags` y documentarlos en `State_GAS.md` sección 4
