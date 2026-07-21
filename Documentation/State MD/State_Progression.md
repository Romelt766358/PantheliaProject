# State_Progression.md — Sistema de Progresión (Niveles, XP, Puntos)

> Sistema **nuevo** de diseño propio (NO del curso). Cimiento de la progresión del personaje: niveles, XP, puntos de atributo y puntos de árbol. Conecta con el árbol de habilidades (`State_Pending.md` sección 2) y con el gasto de atributos (`State_UI.md`).

> **Validación transversal (cierre MetaHuman):** `PlayerState` y ASC continuaron válidos en la build productiva cocinada y **después de la muerte** del jugador. **Respawn y checkpoint siguen pendientes** (ver `State_Pending.md` sección 12) — no hay restauración de progresión al reaparecer porque no hay reaparición todavía.

---

## 1. Diseño acordado (modelo estilo Dragon Age, no souls clásico)

- **Sin tienda ni moneda común.** La XP solo sirve para subir de nivel; no se comercia.
- **Al subir de nivel:** 5 puntos de atributo + 1 punto de árbol (configurable por nivel en el Data Asset).
- **Arranque:** el jugador empieza con **3 puntos de árbol** disponibles (para explorar builds desde el principio).
- **Niveles terminados en 0 y 5** otorgan puntos de árbol extra (decisión de diseño; configurado en el Data Asset, no en código).
- **Nodos del árbol cuestan 1–5 puntos** (ajustable por nodo; el árbol es trabajo futuro).
- **XP de enemigos por nivel** vía curva (preparado para auto-leveling futuro, diferido).
- **Rendimiento decreciente por re-kill** tras hoguera (gancho preparado, no funcional: depende de hoguera/respawn y SaveGame, que no existen).
- **Objeto de reseteo (estilo "Rennala"):** resetea a nivel 1 **conservando la XP**. Como la XP re-sube el nivel, el personaje termina en el **mismo nivel**, pero con todos los puntos reembolsados sin gastar y los atributos a base → reespec completo para cambiar de build sin quedar débil. Es para experimentar. (Implementación: Fase 4.)
- **Nivel máximo:** TBD — lo define la longitud del array del Data Asset.

---

## 2. Fase 1 — Cimiento (✅ implementada y validada en runtime)

### 2.1 `UPantheliaLevelUpInfo` (Data Asset)

Padre `UDataAsset`. Ruta: `AbilitySystem/Data/PantheliaLevelUpInfo.h/.cpp`.

- Struct **`FPantheliaLevelUpEntry`** (se llama `Entry`, **no** `Info`, por la colisión de nombres UHT — ver `Code_Review.md`):
  - `LevelUpRequirement` (int32) — XP **incremental** para pasar del nivel anterior a este (el código acumula internamente). La entrada del nivel 1 debe ser 0.
  - `AttributePointAward` (int32, default 5)
  - `SkillPointAward` (int32, default 1)
- **Convención de índices:** el índice del array **es** el nivel. Índice 0 = placeholder; longitud del array = nivel máximo + 1.
- `FindLevelForXP(int32 InXP)` — acumula los requisitos incrementales y devuelve el nivel correcto, con cap al nivel máximo (no se sale del array).

### 2.2 `APantheliaPlayerState` (modificado)

Los datos de progresión son **contadores planos**, **no** atributos de GAS (mismo criterio que `Level`):
- `XP` (=0), `AttributePoints` (=0), `SkillPoints` (=3). Getters `GetXP()` / `GetAttributePoints()` / `GetSkillPoints()`.
- **Delegates** no dinámicos (`FOnPlayerStatChanged`, OneParam int32) para que la UI se bindee (en Fase 2, desde el `OverlayWidgetController`): `OnXPChangedDelegate`, `OnLevelChangedDelegate`, `OnAttributePointsChangedDelegate`, `OnSkillPointsChangedDelegate`.
- `AddToXP(int32)` [BlueprintCallable] — **entrada principal**: suma XP y llama al bucle de subida.
- `AddToAttributePoints`, `AddToSkillPoints`, y setters directos para los cuatro contadores.
- `LevelUpInfo` (TObjectPtr, EditDefaultsOnly) — referencia al Data Asset; se asigna en `BP_PantheliaPlayerState` → Class Defaults.
- `UpdateLevelFromXP()` (privado) — **única fuente de verdad de la subida de nivel**: sube de uno en uno mientras la XP lo permita, otorgando los premios de cada nivel cruzado. El objeto de reseteo (Fase 4) reutilizará este mismo bucle.

### 2.3 Setup de editor (hecho)

- `DA_LevelUpInfo` con progresión personalizada (corregido un desfase de índice +1).
- `BP_PantheliaPlayerState` ya existía; se le asignó `DA_LevelUpInfo`.
- `BP_PantheliaGameMode` ya tenía `BP_PantheliaPlayerState` como Player State Class.

### 2.4 Lo que la Fase 1 todavía NO hace

- Subir de nivel **no recalcula** `MaxHealth`/`MaxMana`/`MaxStamina` aún. Los MMC ya leen el nivel, pero falta **reaplicar el GE de atributos secundarios** al subir → pendiente. (Al subir de nivel sí se **rellenan** vida y maná a sus máximos **actuales**, ver 2.5; lo que falta es recalcular los máximos.)

---

## 3. Fase 2 — Fuente de XP de muerte (`SendXPEvent`, C++ puro) — ✅ implementada y validada en PIE

Cuando un enemigo muere, otorga XP al jugador. Es la **Fase 2** (fuente de XP de muerte). Toda la lógica vive en `SendXPEvent` dentro de `PantheliaAttributeSet.cpp`, en **C++ puro**.

**Flujo de `SendXPEvent`:**
1. Cast del target a `APantheliaEnemy`; si falla o `BaseXPReward <= 0`, return silencioso.
2. XP final con el multiplicador de **rendimientos decrecientes por re-kill**: usa `EnemyID` + `GetEnemyKillCount` + `GetXPMultiplierForKillCount` + `RecordEnemyKill`. Los bosses con `EnemyID = NAME_None` → multiplicador 1.0 sin registro.
3. Verifica que el `SourceCharacter` implementa `UPantheliaPlayerInterface` y `UCombatInterface`.
4. Detecta subidas de nivel: `Execute_GetPlayerLevel` + `Execute_GetXP` + `Execute_FindLevelForXP(CurrentXP + FinalXP)` → `NumLevelUps`.
5. Si `NumLevelUps > 0`: llama `Execute_LevelUp` (efectos visuales en BP) y **rellena** vida y maná a sus máximos actuales (`SourceASC->SetNumericAttributeBase(GetHealthAttribute(), MaxHealth)` y lo mismo para maná) — comportamiento soulslike estándar.
6. `Execute_AddToXP(FinalXP)` — actualiza XP, Level, AttributePoints, SkillPoints y dispara los delegates de UI (que mueven la `WBP_XPBar`, ver `State_UI.md`).

**Configuración del enemigo (editor):** `BaseXPReward` (p. ej. 600 para el WarriorBoss) y `EnemyID` (p. ej. `WarriorBoss_01`, o `NAME_None` para no registrar re-kills).

> **Por qué C++ puro y no la cadena GA→GE.** Inicialmente el XP de muerte se enrutaba por Blueprint: el enemigo enviaba un `GameplayEvent` (`Attributes.Meta.IncomingXP`) con `SendGameplayEventToActor`, una ability pasiva `GA_ListenForXPEvents` lo escuchaba y aplicaba `GE_EventBasedEffect` sobre el meta atributo `IncomingXP`. Esa cadena estaba rota en varios puntos frágiles a la vez (pin `then` vs `EventReceived` en `WaitGameplayEvent`, el nodo deprecado `BP_ApplyGameplayEffectSpecToSelf` que falla silenciosamente en UE5.8, Modifier Op en `ADD` en vez de `Override`). Como la lógica de concesión ya existía en C++ (el bloque `IncomingXP`, ver `State_GAS.md`), se migró `SendXPEvent` a C++ puro: más robusto y escalable. Lección registrada en `Code_Review.md`.

> **Reserva (no borrar):** `GA_ListenForXPEvents`, `GE_EventBasedEffect` y el meta atributo `IncomingXP` + su bloque en `PostGameplayEffectExecute` **se conservan intactos** como gancho para XP de **otras fuentes** futuras (áreas de bonificación, consumibles, eventos de historia, árbol). Ya no son la ruta del XP de muerte. Pendiente opcional: sacar `GA_ListenForXPEvents` de las startup abilities del jugador (hoy está activa sin recibir nada por esa vía).

---

## 4. Fase 3 — Gasto de puntos de atributo (clases 264-267) — ✅ implementada

> **Garantía transaccional (Bloque 4A, hardening 2026-07-16).** XP, Level, Attribute Points, Skill Points y NodeRanks se consolidaron con **validación antes de commit**: premios negativos clamped/rechazados; **3 Skill Points iniciales**; subir de nivel **no** rellena Health/Mana/Stamina (no refill); los gastos y unlocks **no publican estados parciales** (no se ve un contador a medio cambiar si la operación falla); los GEs del Skill Tree son Infinite con `StackingType = None`; y el SaveGame futuro **serializa NodeRanks y reconstruye el estado GAS desde ahí** (nunca serializa el estado GAS directamente). Aplica a esta fase y a las secciones 3, 5 y al árbol.

El jugador gasta `AttributePoints` para subir sus atributos primarios desde el menú de atributos. **Sin RPC de servidor en ningún punto** (Panthelia es single-player; el curso usaba `Server_UpgradeAttribute`, omitido). La UI está en `State_UI.md` sección 6.

### 4.1 Puente de los puntos hacia Blueprint (`AttributeMenuWidgetController`)

El `PlayerState` ya tenía los contadores, getters, `AddToAttributePoints`/`AddToSkillPoints` y los delegates C++ (`FOnPlayerStatChanged`, no dinámicos) de la Fase 1. Lo que faltaba era exponerlos a Blueprint:
- Delegate dinámico `FOnPlayerPointsChangedSignature` (int32 NewValue).
- **Dos instancias `BlueprintAssignable` separadas:** `AttributePointsChangedDelegate` y `SkillPointsChangedDelegate`. **Decisión de escalabilidad de Romelt:** mantenerlas separadas aunque el menú actual solo use la de atributos, para que el **futuro árbol de habilidades** se bindee a la de skill points sin tocar este controller.
- `BindCallbacksToDependencies` bindea (`AddLambda`) a los delegates C++ del PlayerState (casteando a `APantheliaPlayerState`) y reenvía el valor por los delegates dinámicos. `BroadcastInitialValues` emite el valor inicial de ambos.

### 4.2 El gasto del punto (`UpgradeAttribute`)

- **`UPantheliaAbilitySystemComponent::UpgradeAttribute(const FGameplayTag& AttributeTag)`** — la lógica real:
  1. Comprueba `GetAttributePoints() > 0` vía la interfaz (protección ante clics duplicados o UI desincronizada; si no hay puntos, no hace nada).
  2. Envía un `FGameplayEventData` a sí mismo con `SendGameplayEventToActor` (`EventTag = AttributeTag`, `EventMagnitude = 1.f`).
  3. Resta 1 al saldo (`AddToAttributePoints(-1)`).
- **`IPantheliaPlayerInterface`** — getters nuevos `GetAttributePoints()` / `GetSkillPoints()` (saldo **disponible**; distintos de `GetAttributePointsReward`/`GetSkillPointsReward`, que devuelven el premio de un nivel concreto). `AMainCharacter` los implementa delegando al `PlayerState`.
- **`UAttributeMenuWidgetController::UpgradeAttribute(...)`** — función puente: castea el ASC a `UPantheliaAbilitySystemComponent` y delega. Es lo que llama el Blueprint del botón.

### 4.3 Aplicación del incremento (`GE_EventBasedEffect`)

El evento enviado por `UpgradeAttribute` lo recibe la ability pasiva `GA_ListenForXPEvents` (la misma que se conservó como reserva de XP, sección 3), que enruta la magnitud al `SetByCaller` correcto. El `GE_EventBasedEffect` ganó **5 modificadores** nuevos (`ModifierOp = Add`, `SetByCaller`), uno por primario, con `DataTag = Attributes.Primary.Hardness/Resonance/Resilience/Endurance/Spirit`. Coexisten con el modificador de `Attributes.Meta.IncomingXP` (ese en `Override`).

> Con esto se **cierra el pendiente `State_UI.md` 11.1** (la lógica de subida de atributos primarios). Los 5 atributos suben de forma independiente (ver el bug 2.1 corregido en `Code_Review.md`: la rama `else` del `ForEach` debía usar literal `0.0`, no `EventMagnitude`).

---

## 5. Árbol de Habilidades — infraestructura ✅ implementada (falta poblar los nodos)

El árbol es el **pilar central** del juego: muchos nodos que modifican hechizos, parrys, efectos de estado y atributos, varias veces. En esta etapa se construyó **toda la infraestructura** (Data Asset + componente + integración con GAS); lo que queda es **poblar el Data Asset con los nodos reales** (sección 24 del curso + diseño soulslike). El diseño de gameplay del árbol (modelo Dragon Age, costes, etc.) está en la sección 1 y en `State_Pending.md`.

### 5.1 La arquitectura en capas (quién decide qué)

```
UPantheliaSkillTreeInfo (Data Asset)     ← QUÉ existe          (lo define el diseñador en el editor)
        ↓
USkillTreeComponent (en PlayerState)     ← QUÉ tiene el jugador (NodeRanks = lo único que guarda el SaveGame)
        ↓
UPantheliaAbilitySystemComponent         ← CÓMO se manifiesta  (GiveAbility / GEs Infinite)
        ↓
SpellMenuWidgetController (futuro)        ← UI                  (solo lee y pide; nunca decide)
```

Regla de oro: **la UI nunca llama a `GiveAbility` ni aplica GEs directamente.** Pide `TryUnlockNode` y el componente valida y ejecuta. El `SpellMenuWidgetController` de la sección 24 encaja sin fricción: se bindeará a `OnSkillPointsChangedDelegate` (PlayerState) y a `OnSkillNodeChangedDelegate` (componente).

### 5.2 `UPantheliaSkillTreeInfo` (Data Asset, padre `UDataAsset`) — el "QUÉ existe"

En `AbilitySystem/Data/`. Contiene un `TArray<FPantheliaSkillNodeInfo>`. Cada nodo define:
- `NodeTag` (clave primaria), `NodeName`, `NodeDescription` (`FText`, listos para el Rich Text de la sección 24).
- `MaxRank`, `CostPerRank`, `LevelRequirement`.
- `PrerequisiteNodeTags` (`FGameplayTagContainer` — "necesitas Y y Z desbloqueados").
- **Tres piezas de otorgamiento, opcionales y combinables** (un nodo puede hacer una, dos o las tres):
  1. `GrantedAbility` (`TSubclassOf<UGameplayAbility>`) — desbloquea un hechizo. El tag se **deriva del CDO** de la clase, **no** se duplica como campo aparte (anti-desincronización: un campo duplicado se puede quedar desactualizado por un typo; derivarlo del CDO elimina esa clase entera de bugs).
  2. `GrantedEffects` (`TArray<TSubclassOf<UGameplayEffect>>`) — deben ser **Infinite** (para poder removerse al reespec).
  3. `SetByCallerMagnitudes` (`TMap<FGameplayTag, FScalableFloat>`) — magnitudes escaladas por rango vía Curve Table. **Esta es la joya de escalabilidad:** definir "+20 / +35 / +50 MaxHealth" según el rango se hace **en el editor, sin tocar código**.
- `FindNodeInfoForTag` (usa `MatchesTagExact` — la clave primaria exige coincidencia exacta).

### 5.3 `UPantheliaSkillTreeComponent` (padre `UActorComponent`) — el "QUÉ tiene el jugador"

- Se crea en el constructor de `APantheliaPlayerState` (`CreateDefaultSubobject`), así que **aparece automáticamente en `BP_PantheliaPlayerState`** al compilar (no hay que añadirlo a mano). Vive en el PlayerState por la misma razón de persistencia que la Etapa 4: **sobrevive al respawn**.
- **Estado interno:**
  - `NodeRanks` (`TMap<FGameplayTag, int32>`) — **lo ÚNICO que el SaveGame guardará** (la decisión del jugador).
  - `ActiveNodeEffectHandles` (`TMap<FGameplayTag, TArray<FActiveGameplayEffectHandle>>`) — contabilidad de runtime para poder remover los efectos exactos de cada nodo; **nunca se guarda**. No es `UPROPERTY` (UHT no soporta `TMap` con `TArray` como valor, y no hace falta: son structs planos sin UObjects).
- **API pública (toda BlueprintCallable):**
  - `GetNodeRank(tag) → int32` (0 = no desbloqueado).
  - `CanUnlockNode(tag) → bool` (**const, sin efectos** — la UI la llama cada frame para pintar los botones). Valida en orden: existe, no está al máximo, nivel de personaje suficiente, puntos suficientes, prerequisitos cumplidos.
  - `TryUnlockNode(tag) → bool` (**único punto de compra**) — valida con **las mismas reglas** que `CanUnlockNode`, gasta puntos vía `SetSkillPoints` del PlayerState, sube el rango, aplica a GAS y broadcastea.
  - `ReapplyAllNodes()` — reconstruye todo el estado de GAS desde `NodeRanks` (la pieza del futuro SaveGame; **idempotente**).
- **Delegate:** `OnSkillNodeChangedDelegate` (`FOnSkillNodeChanged`, TwoParams: tag + nuevo rango) — para el futuro `SpellMenuWidgetController`.
- **Helpers privados:** `GetPantheliaPlayerState`, `GetPantheliaASC`, `ApplyNodeToGAS`, `RemoveNodeEffects`, `GetAbilityTagFromClass` (deriva el tag del CDO).

### 5.4 Los tres tipos de nodo (en `ApplyNodeToGAS`)

1. **Nodo de atributos:** aplica un GE `Infinite` + `SetByCaller` escalado por rango, con el patrón **QUITAR+REAPLICAR** (el mismo de `RefreshSecondaryAttributes`) y el handle guardado en `ActiveNodeEffectHandles`.
2. **Nodo que desbloquea un hechizo:** `GiveAbility` a nivel = rango (usando `GetSpecFromAbilityTag`/`SetAbilityLevel` de la Etapa 2). **No asigna `InputTag`** — **desbloquear ≠ equipar**. Equipar (asignar el input) será la sección 24, clases ~290+, una acción distinta por diseño.
3. **Nodo que modifica un hechizo existente:** un GE que concede un **tag** que la ability consulta al activarse (el patrón de dos mitades de `State_Pending.md`).

### 5.5 Validaciones de datos en runtime (degradación con gracia)

- Warning si un GE de nodo no es `Infinite`.
- Error si una ability de nodo no tiene tag bajo la raíz `Abilities` (los rangos 2+ no podrían subirle el nivel).
- Warning si un save contiene un nodo que **ya no existe** en el Data Asset (rediseño del árbol entre versiones) → se omite, **no crashea**.

### 5.6 Principio de persistencia (fijado para el futuro SaveGame)

> **"GAS nunca se guarda; GAS se reconstruye desde datos guardados."** Solo se serializa la **decisión** del jugador (`NodeRanks`), nunca el estado interno del ASC (specs, handles, magnitudes). Al cargar, `ReapplyAllNodes()` reconstruye todo GAS desde cero de forma idempotente. Este principio evita corromper el estado de GAS entre versiones del juego y es el que hace seguro todo el acoplamiento árbol↔GAS.

---

## 6. Roadmap de fases

- **Fase 1** ✅ — cimiento: `PlayerState` + Data Asset + bucle de subida.
- **Fase 2** ✅ — fuente de XP de muerte en C++ puro (`SendXPEvent`, sección 3), con rendimientos decrecientes y subida de nivel + relleno de vida/maná. El recálculo de secundarios al subir **ya funciona** (`RefreshSecondaryAttributes`, ver `State_GAS.md`). Opcional pendiente: curva de XP por nivel en `DA_CharacterClassInfo` para auto-leveling.
- **Fase 3** ✅ — gasto de puntos de atributo (`UpgradeAttribute` + `GE_EventBasedEffect`, sección 4); cierra el pendiente de `State_UI.md` 11.1.
- **Fase 4** — objeto de reseteo (reusa `UpdateLevelFromXP`; el enganche a inventario queda pendiente).
- **Fase 5** — andamios diferidos: rendimiento decreciente por re-kill (el cálculo ya existe en `SendXPEvent`; falta el reset por hoguera/SaveGame), gasto real de puntos de árbol.

---

## 7. Relación con otros sistemas

- **Árbol de habilidades** (sección 5 de este documento): los `SkillPoints` son su moneda. Su infraestructura ya está implementada y documentada aquí; lo que falta (poblar nodos, equipar, SaveGame, UI) está en `State_Pending.md` sección 2.
- **Fricción 1 del árbol** (`Code_Review.md`) — ✅ **resuelta**: el nivel de las abilities ya es subible nodo a nodo por el árbol (Etapa 2: `SetAbilityLevel` en el ASC), **independiente** del nivel del personaje. Es distinto del recálculo de atributos secundarios al subir de nivel, que también está resuelto (`RefreshSecondaryAttributes`).
- **Gasto de atributos** (`State_UI.md` 11.1): la Fase 3 conecta los botones `+` del menú de atributos con los `AttributePoints`.
