# State_Architecture_Future.md — Árbol de habilidades, corazones y build de hoguera (diseño prospectivo)

> **Naturaleza de este documento (leer primero):** esto **no** describe sistemas implementados. Es el **plano arquitectónico aprobado** (v1.0, 2026-07-16) de cómo deben encajar el árbol de habilidades, los corazones elementales, el loadout y los hechizos en el futuro. Su función es que ningún hechizo, nodo o pieza de equipo se construya hoy como sistema aislado que luego haya que rehacer. **Cualquier chat que vaya a crear una habilidad, un nodo del árbol o un sistema de equipo debe leer las invariantes (sección final) antes de escribir código.** Lo ya implementado se documenta en los otros `State_*.md`; aquí solo el destino.

> **Prerequisito técnico ya cerrado:** el **Pawn productivo MetaHuman y su pipeline de animación están cerrados** (migración aprobada; ver `State_Overview.md`). Futuras modificaciones de dodge/parry/ataques deben apuntar a los **assets de Production** (`/Game/Characters/Player/Production/`); **no** crear nuevas dependencias sobre `MetaHumanMigration`. La arquitectura futura del árbol/corazones/loadout **no** cambia por la migración.

Inspiración: Dragon Age Inquisition (habilidad principal + evoluciones), Black Myth Wukong (muchas mejoras alrededor de una base) y el giro propio de Panthelia: **un único árbol compartido entre los 4 corazones**, donde un mismo nodo se resuelve distinto según el corazón equipado.

---

## 1. Decisiones de diseño confirmadas

- **Un solo corazón activo** (Fuego / Agua / Tormenta / Naturaleza). Determina: el moveset elemental de hechizos, la variante activa de cada familia, el elemento ofensivo y defensivo del jugador, sus pasivas, y la interpretación elemental de ciertos nodos compartidos. **Cierra la duda que estaba abierta en `State_Pending.md`** ("¿uno o dos corazones?" → uno). Solo se cambia en **hoguera**.
- **El árbol es compartido, no cuatro copias.** Un nodo (`Skill.Projectile.Improvement01`) se compra **una sola vez**; su efecto depende del corazón activo (Fuego → +daño; Agua → −coste/−cooldown; Tormenta → +velocidad/+buildup; Naturaleza → +daño de postura). La decisión persistente es *"el jugador posee Mejora I de la familia Proyectil"*; la resolución runtime es *"con el corazón actual, Mejora I concede estos modificadores"*. Cambiar de corazón **no** reembolsa ni borra el nodo — solo cambia su interpretación.
- **Las familias de hechizos existen por encima de los elementos.** La unidad de diseño es `SpellFamily.Projectile` / `.Beam` / `.Aura` / `.Area` / `.DashAttack` / `.Counter`, no `Firebolt`. Cada corazón aporta una **variante** de cada familia; las 4 variantes comparten clase C++ base, flujo de activación, costes, spawn, cancelación, integración con lock-on, modificadores del árbol y contratos de datos. Cambian solo: clase del proyectil, tipo de daño, buildup, VFX/SFX/Cues, valores base y la resolución de sus perks. **Asumir reutilización elemental desde el principio.**
- **Dos slots de arma activos** (`Weapon Slot 0` / `Weapon Slot 1`), alternables en combate. **No representan manos.** Las dagas duales son **una sola arma lógica** (un `WeaponDefinition`, un moveset, un slot; puede tener dos meshes visuales). **El jugador no equipa escudos** (los enemigos sí, en su diseño).
- **Hoguera vs combate:** la **composición** de los slots (qué arma en cada uno, armadura, corazón, nodos, hechizos equipados) solo cambia en **hoguera**; el **slot activo** (`ActiveWeaponSlotIndex = 0|1`) cambia en **combate**, sin tocar el build persistente. El cambio de arma activa debe respetar estados de combate (no en mitad de una ventana de daño, no dejar el WeaponTrace apuntando al arma anterior, transaccional en animación/moveset/sockets/sonido).

---

## 2. Principios arquitectónicos (contratos que el código debe respetar)

- **GAS nunca se guarda; se reconstruye desde datos.** Se persiste: nivel, XP, puntos, `NodeRanks`, corazón, las dos armas de slot, slot activo, armadura, familias/hechizos equipados. **No** se persiste: handles de spec/effect, specs activas, cooldowns del ASC, tags temporales, timers, montages, proyectiles vivos ni punteros a actores de arma. Al cargar, el runtime se reconstruye. (Es el mismo principio ya vigente en el árbol actual — ver `State_Progression.md`.)
- **La UI no decide gameplay.** La UI de hoguera edita un **borrador** y *solicita* operaciones al componente propietario del build; **nunca** llama a `GiveAbility`, aplica GEs, destruye armas, cambia el corazón real ni concede tags directamente.
- **Ninguna ability elemental pregunta por el nombre del corazón.** Prohibido `if (Heart == Fire)`. La ability recibe una **configuración ya resuelta desde datos** y pregunta *"¿qué modificadores activos afectan a este parámetro y a esta familia?"*, no *"¿es Fuego o Agua?"*.
- **Los perks se acumulan.** Muchos parámetros (daño, coste, cooldown, cantidad de proyectiles, intervalo, spread, velocidad, homing, duración, buildup, poise, ventanas de parry/dodge, radios…) se modifican varias veces. El sistema resuelve **una lista de modificadores acumulables**, nunca un booleano por perk.

---

## 3. Arquitectura por capas (destino)

```
UPantheliaSkillTreeInfo      → el grafo compartido y los nodos que existen
UPantheliaSkillTreeComponent → posee NodeRanks, valida la progresión         [ya existe]
UPantheliaHeartDefinition    → variantes elementales + cómo resuelve nodos compartidos
UPantheliaBuildComponent /
  UPantheliaLoadoutComponent → AppliedBuild + PendingBuild (hoguera), valida,
                               resuelve NodeRanks+corazón+equipo, reconstruye runtime
UPantheliaAbilitySystemComp  → recibe abilities/GEs resueltos y ejecuta       [ya existe]
UPantheliaEquipmentComponent → materializa armas, 2 slots, arma activa, WeaponTrace/mesh/anim  [existe, a extender]
Widget Controllers           → solo observan, presentan y solicitan operaciones
```

**Tres estados separados:** progresión **permanente** (XP, nivel, puntos, `NodeRanks`, desbloqueos) · **build aplicado** (corazón, 2 armas, slot activo, armadura, hechizos equipados, asignaciones de input — serializable) · **build pendiente** (`PendingBuild`, copia editable en hoguera; al cancelar se descarta y `AppliedBuild` queda intacto). El **runtime derivado** (abilities concedidas, variante por familia, GEs de nodos, modificadores, pasivas, datos de arma, input tags, HUD) se reconstruye **al confirmar**. La hoguera puede limpiar el estado transitorio de combate (montages, timers, buildup, debuffs, guardia, dodge, trace, proyectiles pendientes…) **sin** destruir el ASC indiscriminadamente.

---

## 4. Dos tipos de nodo del árbol

El árbol actual (`SkillTreeInfo`/`SkillTreeComponent`/`NodeRanks`/`GrantedAbility`/`GrantedEffects`/`SetByCallerMagnitudes`/`ReapplyAllNodes`) sigue siendo válido, pero debe distinguir:

- **Nodo universal directo** — su efecto no depende del corazón (+MaxHealth, +Stamina, mejora genérica del dodge…). Usa el flujo actual: `Nodo → GrantedAbility/GrantedEffects → GAS`.
- **Nodo resuelto por build** — su efecto depende del corazón. **No** debe contener los cuatro resultados a la vez; declara que requiere resolución (`ResolutionMode = HeartResolved`, `AffectedFamilyTag = SpellFamily.Projectile`) y el corazón activo determina la concesión concreta. Separar *qué compró el jugador* de *cómo se manifiesta con el build actual*. (Enum propuesto `EPantheliaSkillNodeResolutionMode { DirectUniversal, HeartResolved }` — nombres orientativos.)

---

## 5. Corazones y modificadores de parámetros

- **`UPantheliaHeartDefinition`** (Data Assets `DA_Heart_Fire/Water/Storm/Nature`): identidad (`HeartTag`, `Element`, icono, VFX/SFX, elemento defensivo), **variantes por familia** (`SpellFamily.X → GA_X_<Elemento>`), **pasivas** (abilities pasivas + GEs permanentes mientras esté equipado + listeners como `Event.Parry.Success` + sinergia con `WeaponElement`), y **resoluciones de nodos** (`NodeTag → resultado para este corazón`).
- **Capa de modificadores de parámetros de ability** (para lo que **no** conviene volver atributo GAS global: cantidad de proyectiles, intervalo, spread, velocidad, homing, ángulo, radios, nº de pulsos, ventanas internas): tags `Ability.Parameter.*`, un struct `FPantheliaAbilityParameterModifier { ParameterTag, Operation(Add/Multiply/Override), Magnitude(FScalableFloat), Priority }`.
- **Orden de resolución determinista (invariante):** `base → suma de Add → producto de Multiply → Override de mayor prioridad → clamp de seguridad`. Nunca depender del orden accidental del array.
- **Costes y cooldowns reutilizan lo existente:** la familia consulta sus modificadores resueltos vía el hook de costes ya presente (`GetAdditionalResourceCostModifiers()` sobre la arquitectura común de `State_GAS.md`); **no** duplicar `CheckCost`/`ApplyCost` ni hardcodear cooldowns por Blueprint elemental. `SpellFamily + corazón + NodeRanks → multiplicador/plano de recurso y de cooldown`.

---

## 6. Familia multi-proyectil y adaptación de las clases 316-319

Jerarquía destino: `UPantheliaProjectileSpell → UPantheliaMultiProjectileSpell → GA_ProjectileFamily_Base → {GA_Projectile_Fire/Water/Storm/Nature}`. **`GA_Firebolt` permanece como proyectil único** (no se convierte en multi-proyectil). La clase genérica resuelve cantidad (base + perks), spread, pitch, intervalo de spawn (**incluido 0 = simultáneo**), velocidad, homing parcial, límite de seguridad y cancelación; soporta spawn simultáneo o secuencial sin cuatro implementaciones; mantiene el spec **por proyectil**; un multi-proyectil **puede** impactar varias veces al mismo objetivo; la ability no termina hasta cumplir montage + secuencia.

**Adaptación de las clases del curso (divergencias registradas):**
- **316 (spread):** se conserva la matemática del abanico y la idea de cantidad variable; se **descarta** convertir `GA_Firebolt` en multi-proyectil y la regla `cantidad = AbilityLevel`. La infra pasa a la familia `MultiProjectile`.
- **317 (múltiples proyectiles):** se adapta a una ability C++ genérica, no acoplada a Fuego, cantidad desde base+perks.
- **318 (homing):** homing **parcial y configurable** (retraso, duración, aceleración, ángulo máximo, target lógico del lock-on, **capacidad real de fallar**); sin clic sobre el suelo.
- **319 (click Niagara):** **se omite** — es de movimiento top-down por clic; Panthelia es tercera persona con control directo, sin `CachedDestination`; no tocar el PlayerController por esto.

---

## 7. Input de hechizos y `AbilityLevel`

- El jugador equipa **familias** en slots de input (`InputTag.Spell.1 → SpellFamily.Projectile`); al cambiar de corazón, el mismo input activa la variante del corazón activo (`GA_Projectile_Fire` vs `GA_Projectile_Water`) **sin re-equipar la barra**. Son operaciones **distintas**: desbloquear familia ≠ mejorar familia ≠ equipar en un input ≠ resolver variante por corazón.
- **`AbilityLevel` es un detalle técnico, no un concepto de UI.** Se sigue usando internamente para evaluar `FScalableFloat`, pero la fuente de verdad son los nodos; el nivel técnico se **recalcula** al reconstruir el build y **no** se persiste. Los perks laterales no tienen que mapearse todos a `AbilityLevel` (pueden aplicar modificadores/tags sin tocarlo).

---

## 8. SaveGame futuro

Persiste: `XP, PlayerLevel, AttributePoints, SkillPoints, NodeRanks, SelectedHeartTag, WeaponSlot0, WeaponSlot1, ActiveWeaponSlotIndex, ArmorLoadout, EquippedSpellFamilies, InputAssignments`. Al cargar: `cargar → asignar NodeRanks/build → ReapplyAllNodes/RebuildRuntimeBuild → materializar equipo → reconstruir UI`. **Debe degradar con warnings y fallbacks (no crashear)** ante: un nodo eliminado entre versiones, un arma inexistente, un corazón renombrado, un slot inválido o datos de versión anterior.

---

## 9. Orden recomendado de implementación

**A** — Multi-proyectil genérico (`UPantheliaMultiProjectileSpell`, `GA_Firebolt` intacta, spread, cantidad configurable, spawn simultáneo/secuencial, cancelación de timers, múltiples impactos). **B** — Homing parcial (componente reutilizable de guía, target lógico, retraso/duración/ángulo, que aún pueda fallar). **C** — Variantes elementales (Blueprint base de familia + 4 hijos que cambian solo datos). **D** — Resolver perks compartidos (tags de familia/parámetro, modificadores acumulables, conectar SkillTree con HeartDefinition, resolver `Mejora I` distinto por corazón). **E** — Build de hoguera (`AppliedBuild`/`PendingBuild`, confirmación transaccional, SaveGame, corazón+armadura+2 armas, reconstrucción de GAS desde datos).

---

## 10. Invariantes que no deben romperse

1. Un nodo compartido se compra una sola vez. 2. Cambiar de corazón no elimina `NodeRanks`. 3. Ninguna ability elemental contiene `if Heart == X`. 4. Una familia comparte C++ entre los 4 elementos. 5. `GA_Firebolt` sigue siendo proyectil único salvo rediseño explícito. 6. Un multi-proyectil puede impactar varias veces al mismo objetivo. 7. El intervalo entre proyectiles es configurable, incluido cero. 8. El homing es parcial y configurable. 9. Los dos slots de arma no representan manos. 10. Las dagas duales ocupan un solo slot. 11. El jugador no equipa escudos. 12. El contenido de los slots solo cambia en hoguera. 13. El arma activa se alterna en combate. 14. GAS runtime se reconstruye, no se serializa. 15. La hoguera aplica el build de forma transaccional. 16. La UI nunca concede abilities ni GEs directamente. 17. Los modificadores múltiples usan un orden determinista. 18. El C++ no se duplica por elemento. 19. Los assets elementales contienen datos y presentación. 20. Codex/MCP no escribe C++ (solo tareas editoriales sobre assets ya compilados).

---

## 11. Decisiones aún abiertas

Reglas exactas de cancelación/permiso del cambio de arma en combate · si ambas armas existen físicamente a la vez · estructura de slots de armadura · si descansar reinicia cooldowns · si cambiar de corazón rellena recursos · cuántas familias de hechizo pueden equiparse a la vez · presentación visual del árbol compartido · coste de respec y devolución de puntos · operaciones exactas de los modificadores y sus prioridades · versionado de `NodeTags`/`HeartTags` en SaveGame · qué parámetros usan atributos GAS vs el resolvedor de ability · si ciertas mejoras cambian animación/montage además de números.

---

## 12. Resumen operativo

```
NodeRanks + HeartDefinition + Weapon/Armor Loadout + Equipped Spell Families
  = Resolved Runtime Build
```
La hoguera es el único lugar donde cambia esa composición. En combate: el corazón, el árbol y los slots preparados no cambian; solo se alterna el arma activa; las abilities usan la variante del corazón activo; los perks compartidos se manifiestan según ese corazón; GAS ejecuta un build ya resuelto.

> **Regla de trabajo (registrada):** el C++ (`.h`/`.cpp`) se diseña y escribe en el chat de arquitectura/documentación; Codex/MCP **no escribe C++** — se usa para tareas editoriales sobre assets ya compilados (crear Blueprints hijos, asignar `ProjectileClass`, configurar montages/notifies/Niagara/Cues, revisar Data Assets y tags, dumps y verificación de guardado).
