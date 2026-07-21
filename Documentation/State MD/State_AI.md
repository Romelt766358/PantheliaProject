# State_AI.md — Inteligencia Artificial (Bosses y Enemigos)

> Dominio de IA del juego. Los **bosses** usan un sistema propio basado en **StateTree** (documentado aquí). Los **enemigos normales** usan **Behavior Tree** con activación de abilities por tag (documentado en `State_Abilities.md` sección 8 y `State_Combat.md` sección 9 — pendiente de consolidar aquí en el futuro).

---

> **Nota (cierre MetaHuman, Fase 4I):** la **instancia** de prueba de `BP_Boss` (TestBoss) se **retiró de `Lvl_ThirdPerson`** junto con lógica legacy del Level Blueprint (BossTriggerBox, health bar legacy). **El asset `BP_Boss` y la familia TestBoss se conservan** — no confundir retirar la instancia con eliminar el sistema o el asset. El **WarriorBoss productivo sigue en el mapa** y entró al package. **No hubo cambios de arquitectura de IA en 4H/4I.**

## 1. Sistema de IA del Boss — StateTree + BossBrain + BossProfile + GAS ✅ primera integración funcional

Implementado con ChatGPT/Codex vía MCP. El `BP_WarriorBoss` ya ataca repetidamente gobernado por StateTree; el **Behavior Tree del boss quedó deshabilitado**.

### 1.1 Arquitectura en 4 capas (separación de responsabilidades)

```
StateTree    → FLUJO       (cuándo hacer qué; el "bucle" de comportamiento)
BossBrain    → DECISIONES  (qué acción elegir, validar, ejecutar, y su estado runtime)
BossProfile  → DATOS       (qué acciones/fases/stats existen — Data Asset, sin código)
GAS          → EJECUCIÓN   (la ability real que hace el ataque)
```

Esta separación es deliberada y escalable: añadir un ataque nuevo es **editar el Data Asset** (una entrada en el array de acciones), no tocar código ni el StateTree. El StateTree no sabe *qué* ataques existen; el BossBrain no sabe *cuándo* se le llama; el BossProfile no sabe *cómo* se ejecuta nada.

### 1.2 `UPantheliaBossProfile` (Data Asset) — el "QUÉ existe"

Data Asset central de un boss. **No escala por nivel del jugador** (decisión soulslike); los stats son manuales y fijos por boss.

- **Identidad:** `BossID`, `BossDisplayName`.
- **`StatsPresets`** (`TArray<FPantheliaBossStatsPreset>`) — cada preset tiene `PresetID`, `MaxHealth`, `Armor`, `MagicResistance`, `MaxPoise`, las 4 resistencias elementales, `BaseWalkSpeed`, `DefensiveElement`. Para la demo se usa un único preset `DemoDefault`; **el array es un gancho futuro para variar stats por punto de historia, NO por nivel del jugador**.
- **`PhaseDefinitions`** (`TArray<FPantheliaBossPhaseDefinition>`) — cada fase tiene `PhaseID`, `EnterHealthPercent` (la fase se activa cuando `HealthPercent <= EnterHealthPercent`; Phase1 = 1.0, Phase2 = 0.6/0.5…), `WeightMultiplier`, y `ExplicitActionPool` (si está vacío, el selector usa todas las acciones válidas para la fase).
- **`ActionDefinitions`** (`TArray<FPantheliaBossActionDefinition>`) — el corazón del sistema. Cada acción define:
  - `ActionTag` (identidad interna, ej. `BossAction.Melee.ShortSlash`) y `AbilityTag` (la ability GAS real que activa, ej. `Abilities.Attack.Melee`) — **la acción es un envoltorio de datos sobre una ability**.
  - `ActionType` (`EPantheliaBossActionType`: Melee, Ranged, GapCloser, Reposition, Punish, Special).
  - `ValidPhases` (en qué fases está disponible).
  - **Filtros de contexto:** `MinDistance`/`MaxDistance`, `MaxAngle` (ángulo contra el forward del boss; 0 = de frente, 180 = detrás), `bRequiresLineOfSight`.
  - **Selección ponderada:** `Weight` (probabilidad relativa), `Cooldown` (segundos), `bCanExtendCombo`.
  - **Puertas por tags GAS:** `RequiredOwnerTags` (el boss debe tenerlos todos) y `BlockedOwnerTags` (si el boss tiene alguno, la acción se bloquea — pensado para `Effects.Stagger`, `Effects.HitReact`, `State.Recovering`).

### 1.3 `UPantheliaBossBrainComponent` (`UActorComponent`) — las "DECISIONES"

Vive en el boss (`BP_WarriorBoss` lo usa). Mantiene el estado runtime de la acción actual y expone la API que el StateTree consume.

- **Estado runtime de una acción** (`EPantheliaBossActionRuntimeState`): `Selected → Starting → Running → Finished` (o `Failed` / `Interrupted`). Con `EPantheliaBossActionFailureReason` para diagnóstico.
- **API principal (BlueprintCallable):**
  - `InitializeBossFromProfile()` — carga stats/fases/acciones del `BossProfile`.
  - `UpdatePhaseFromHealth()` — recalcula la fase activa según el % de vida.
  - `SelectAction(TargetActor, OutAction)` — elige una acción válida (filtros de distancia/ángulo/LoS/tags/cooldown + peso × `WeightMultiplier` de fase).
  - `TryExecuteAction(TargetActor, ActionTag)` — activa la ability GAS de la acción.
  - `RefreshActionRuntimeState()` — actualiza el estado leyendo GAS; `MarkActionInterrupted()`; `ClearCurrentAction()`.
  - **Consultas puras** (para las condiciones del StateTree): `HasSelectedAction`, `GetCurrentActionTag`/`GetCurrentAbilityTag`, `GetCurrentActionState`, `GetLastFailureReason`, `IsActionRunning`, `HasActionFinished`, `HasActionFailed`.
  - **Cooldowns:** `ResetActionCooldown(tag)`, `ResetAllActionCooldowns()`.

### 1.4 Tasks de StateTree — el "FLUJO"

En `AI/StateTree/PantheliaBossStateTreeTasks.h/.cpp`. El bucle validado:

```
Face Target → Select Boss Action → Execute Selected Boss Action
   → Wait For Boss Action → Clear Boss Action → Delay → (repetir)
```

- **Face Target** — rota el boss hacia el objetivo antes de atacar.
- **Select Boss Action** — pide al BossBrain una acción (`SelectAction`).
- **Execute Selected Boss Action** — la activa (`TryExecuteAction`).
- **Wait For Boss Action** — espera a que termine. **Actualizado (2026-07-16): ya no es polling.** El Brain guarda el `FGameplayAbilitySpecHandle` **exacto** de la acción y escucha `ASC->OnAbilityEnded`; la task reacciona al evento de fin real de la ability.
- **Clear Boss Action** — limpia el estado para la siguiente iteración.

**Decisión clave:** se **eliminó el contexto obligatorio `AIController`** de las tasks — funcionan con el **StateTree Component** directamente sobre el pawn, sin exigir un AIController. Más flexible y menos acoplado.

### 1.4b Lifecycle exacto de una acción (cerrado en la campaña de hardening)

Estados: `Selected → Starting → Running → Finished / Interrupted / Failed`. Semántica precisa:
- **Finished** — comenzó y terminó normalmente; **conserva** cooldown/memoria.
- **Interrupted** — comenzó y fue cancelada; es **terminal**, conserva cooldown/memoria y **espera reacciones**.
- **Failed** — **no** comenzó; **no** consume cooldown/memoria.

`TrySetTerminalState` hace el cierre **idempotente**: un callback de *otra* ability no puede cerrar la acción actual (se compara contra el handle exacto). **La muerte del boss es terminal:** cancela/limpia la acción activa y los delegates; ninguna acción se reanuda tras morir.

### 1.4c Fases reactivas y locomoción secuencial

- **Fases reactivas:** los cambios de `Health`/`MaxHealth` recalculan la fase; al cruzar el umbral se actualiza el ID/tag y se emite el delegate, pero **no se cancela la acción en curso** — la nueva fase se usa en la **siguiente** selección. Los `PhaseID` deben ser únicos (`Phase1`, `Phase2`); `ValidPhaseTags` es el contrato preferido de las acciones.
- **Locomoción (corrección de StateTree):** las tasks hermanas de StateTree **no** se ejecutan en secuencia por sí solas. El flujo real es `PrepareAction → ExecuteAction → RecoverAfterAction → ApproachOrRetry`. `Prepare Boss Action` rota y selecciona en su secuencia interna; `Approach Or Retry` mueve con `MoveToActor`, con **histéresis**, retry corto si la acción no está disponible y retry lento si hubo fallo técnico. **`AttackCycle.Tasks Completion = All`** es obligatorio para que Execute + Wait no terminen antes de tiempo.

### 1.5 Gameplay Tags

Nuevas raíces nativas (en `PantheliaGameplayTags`): `Boss.Phase.*`, `Boss.Action.*`, `Boss.State.*`. Ver `State_GAS.md`.

### 1.6 Módulo

`GameplayStateTreeModule` (u equivalente) añadido a `PantheliaProject.Build.cs`. Ver `State_Overview.md` sección 5.

---

## 2. Estado y pendientes

**✅ Validado:** flujo StateTree → BossBrain → BossProfile → GAS; rotación previa al ataque (Face Target); ciclo de ataque repetitivo del WarriorBoss.

### 2.1 Acción `Boss.Action.GapCloser.Leap` — ✅ validada en PIE

Primer GapCloser funcional de punta a punta (validado contra `BP_ThirdPersonCharacter_C_0`):
```
BossBrain selecciona Boss.Action.GapCloser.Leap → GA_WarriorBoss_GapCloser
  → Dash Avatar Toward Actor → PlayMontageAndWait → montage en slot FullBody
  → WeaponTraceNotifyState abre la ventana → WeaponTrace detecta al jugador → GE_Damage aplica daño (incluido overkill fatal)
```
- **Fixes que lo estabilizaron:** una conexión errónea donde `OnFailed` del dash pasaba por `EndAbility` pero seguía hacia el ataque (corregida); un nodo `Break TaggedMontage` recreado por sospecha de corrupción; y sobre todo la **detección del `WeaponTrace`** (ver `State_Combat.md` sección 9: decidir por `HitResults.Num()`, respuesta del canal Fighter).
- **Slot `FullBody` (importante):** todos los montages de ataque del WarriorBoss y su AnimBP (`ABP_WarriorBoss`: BlendSpace de locomoción → `Slot 'FullBody'` → Output) deben usar el slot **`FullBody`**. Slots inconsistentes eran parte del problema de "el montage no se reproducía".
- **Arma:** `SM_Sword` (Static Mesh Component), sockets `WeaponBase`/`WeaponTip` en ella — ver `State_Combat.md` sección 9.

### 2.2 Ruido de logs del BossBrain (conocido, no es bug)

El `BossBrain` evalúa acciones con frecuencia y descarta muchas por `Reason=distance or angle` / `Reason=cooldown` (p. ej. el GapCloser fuera de `[300, 900]`, o un melee recién usado en cooldown, o el boss demasiado cerca tras el propio GapCloser). Es comportamiento correcto de selección, no un fallo. **Pendiente de limpieza:** encerrar los logs de rechazo tras un flag `bLogActionSelection` para el modo no-debug.

**Pendiente conocido (independiente de esta IA):** warning en `PantheliaAttributeSet.cpp` por `UGameplayEffect::StackingType` — benigno, anotado en `Code_Review.md`.

**Próximas tareas (roadmap del sistema):**
1. Múltiples acciones y pesos en el `BossProfile` (ya soportado por datos; falta **poblarlo** con acciones/fases/telegraphs definitivos).
2. ~~Transición entre fases por vida~~ — ✅ **cerrada** (fases reactivas, ver 1.4c).
3. ~~Integrar interrupciones (stagger / hit react)~~ — ✅ **infraestructura cerrada** (estado `Interrupted` terminal + `BlockedOwnerTags`); falta el contenido de reacción.
4. Sustituir el fallback `GetPlayerCharacter` por un `TargetActor` enlazado de verdad.
5. Añadir evaluators y condiciones de StateTree adicionales según haga falta.
6. ~~Reemplazar el polling por callbacks de fin de `GameplayAbility`~~ — ✅ **cerrada** (`OnAbilityEnded` + handle exacto, ver 1.4/1.4b).
7. Locomoción de acercamiento/reposicionamiento ya operativa (`Approach Or Retry` + `MoveToActor` + histéresis, ver 1.4c); el GapCloser.Leap funciona. Falta poblar más acciones de movimiento y afinar el feeling.

**Limpieza / balance pendientes del GapCloser:** quitar los prints temporales de diagnóstico de `GA_WarriorBoss_GapCloser` (`DASH FINISHED`, `PLAY GAPCLOSER MONTAGE`, etc.) y desactivar `bDebugMode`/`bLogTraceDebug` del `WeaponTraceComponent` al cerrar la validación; ajustar `StopDistance` del dash, rango/cooldown/ángulo del GapCloser, `Recover` del StateTree y el timing del `WeaponTraceNotifyState` cuando se pase a balance.

> **Mejora técnica futura (modularidad):** separar en `WeaponTraceComponent` el `WeaponMeshComponent` (arma visual/física real) del `TraceSocketSourceComponent` (de dónde leer `WeaponBase`/`WeaponTip`), para que un enemigo pueda usar sockets en el skeleton y otro en el arma sin ambigüedad. No urgente — el setup actual funciona. Anotado en `Code_Review.md`.

> **Relación con lo ya documentado:** los pendientes de IA de `State_Combat.md` (combos del WarriorBoss por selección aleatoria, movimiento entrecortado idle↔run) se re-encuadran con este sistema: los "combos" saldrán de las acciones ponderadas + `bCanExtendCombo` del `BossProfile`, y la locomoción es la tarea 7.
