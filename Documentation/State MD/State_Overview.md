# Panthelia — Overview del Proyecto

> **Propósito:** Punto de entrada del Project Knowledge. Contiene resumen, decisiones de arquitectura, estructura de directorios, convenciones y glosario. Cambia poco — actualízalo solo cuando se tome una decisión arquitectónica nueva o se reorganicen directorios.

> **Otros documentos del proyecto:** `State_GAS.md`, `State_Abilities.md`, `State_Combat.md`, `State_AI.md`, `State_Input.md`, `State_UI.md`, `State_Progression.md`, `State_Pending.md`, `State_Validation.md` (validación de Data Assets), `State_Hardening.md` (campaña de robustez), `State_Architecture_Future.md` (**diseño prospectivo** del árbol/corazones/loadout — leerlo antes de crear habilidades o nodos nuevos). Cuando un sistema de `State_Pending.md` se implementa, se documenta en el archivo temático que le corresponde (p. ej. el parry y el melee del jugador → `State_Combat.md`; el daño elemental → `State_Abilities.md`/`State_GAS.md`; los niveles y el árbol → `State_Progression.md`; la IA de bosses → `State_AI.md`) y se elimina de Pending.

---

## 1. Resumen del proyecto

**Panthelia** es un juego tipo Soulslike en Unreal Engine 5.8 con énfasis en combate elemental, parry y un árbol de habilidades complejo. **Sin multiplayer.**

**Pilares de gameplay:**
- Combate físico y mágico con espada + brazo elemental izquierdo
- Sistema de 4 elementos (Fuego, Agua, Tormenta, Naturaleza), cada uno con tipos de daño físico y mágico
- Dos tipos de parry: metálico (arma) y mágico (brazo izquierdo)
- Efectos de estado elementales (Quemadura, Electrocución, Saturación)
- Sistema de postura/poise para staggers
- Árbol de habilidades profundo que modifica hechizos y efectos

**Prioridades arquitectónicas:**
1. **Escalabilidad** — el árbol de habilidades debe poder modificar cualquier valor de hechizo/efecto/parry sin reescribir clases
2. **Modularidad** — sistemas independientes que se comuniquen por delegates e interfaces, no por referencias directas
3. **Datos sobre código** — usar Data Tables, Gameplay Effects y MMCs en lugar de hardcodear valores

---

## 2. Decisiones de arquitectura principales

### 2.1 GAS (Gameplay Ability System) es el backbone

Todo lo relacionado con atributos, daño, efectos de estado, habilidades y modificadores pasa por GAS. No usar variables float sueltas en personajes para vida/maná/estamina.

**Razón:** GAS permite que el árbol de habilidades añada/modifique efectos sin tocar el código base. Un nodo del árbol será un GameplayEffect aplicado, no un `if` en C++.

**ASC custom (`UPantheliaAbilitySystemComponent`):** Heredamos del ASC base para añadir el delegate `EffectAssetTags` que broadcastea los Asset Tags de cualquier GE aplicado. Permite que la UI reaccione a efectos sin acoplarse a clases concretas. Ver `State_GAS.md`.

### 2.2 Jerarquía de personajes

```
ACharacter (Unreal)
└── APantheliaCharacterBase (ABSTRACT, implementa IAbilitySystemInterface)
    ├── AMainCharacter (jugador, ASC vive en PlayerState)
    └── APantheliaEnemy (enemigos, ASC vive en el propio actor)
        └── ABossCharacter
```

**Razón de la diferencia ASC:**
- **Jugador:** ASC en `APantheliaPlayerState` porque persiste entre respawns y se replica al cliente correctamente.
- **Enemigos:** ASC en el propio actor porque no necesitan persistir.

### 2.3 Orden de inicialización de atributos

Esto es **crítico** y está documentado en `PantheliaCharacterBase::InitializeDefaultAttributes()`:

1. **Primarios** primero (Hardness, Resonance, Resilience, Endurance, Spirit)
2. **Secundarios** después (MaxHealth, MaxMana, etc. — dependen de primarios vía MMCs)
3. **Vitales** al final (Health = MaxHealth, Mana = MaxMana, etc.)

**Por qué importa:** Si inicializas Health antes que MaxHealth, el clamp en `PreAttributeChange` lo deja en 0.

### 2.4 Patrón Widget Controller

La UI **no accede directamente** al AttributeSet. Sigue esta cadena:

```
AttributeSet → ASC delegates → WidgetController → UserWidget (Blueprint)
```

**Razón:** El widget de Blueprint solo conoce su controller. Si cambias el AttributeSet, solo actualizas el controller. Los widgets siguen funcionando.

### 2.5 Interfaces para desacoplamiento

- `IMainPlayer` — marca un actor como el jugador (sin métodos por ahora, solo para casting)
- `IEnemy` — interfaz de enemigos (highlight, select)
- `IFighter` — algo que puede dar daño (`GetDamage()`)
- `ICombatInterface` — interfaz de combate del personaje. Métodos:
  - `GetPlayerLevel()` — nivel del personaje, para MMCs
  - `GetCombatSocketLocation(const FGameplayTag& MontageTag)` — ubicación del socket de origen del ataque según el tag del montage (arma/mano/pie/boca). Centralizada en C++ en `APantheliaCharacterBase`, no se implementa por Blueprint. Ver `State_Abilities.md` sección 5.
  - `GetHitReactMontage()` — BlueprintNativeEvent + BlueprintCallable. Devuelve el montage de hit react configurado en el BP del personaje
  - `GetStaggerMontage()` — BlueprintNativeEvent. Montage de aturdimiento (stagger)
  - `GetFlinchThreshold() const` → float (default 10.f) — % de MaxPoise que un golpe debe superar para causar flinch
  - `ResetPoiseRegenTimer()` (default vacío) — reinicia el timer de regeneración de postura; lo llama el AttributeSet con cada golpe
  - `GetDefensiveElement() const` → `EPantheliaElement` (default `None`) — elemento defensivo para la tabla de afinidades de daño
  - `Die()` — pure virtual. Maneja la muerte del personaje (ragdoll + dissolve + lifespan). Implementada en `APantheliaCharacterBase` y sobreescrita en `APantheliaEnemy`
  - `UpdateFacingTarget()` — BlueprintImplementableEvent + BlueprintCallable, pensada para Motion Warping. **Existe pero actualmente no se usa** — Motion Warping fue revertido por incompatibilidad visual con el estilo soulslike
  - Implementada en `APantheliaCharacterBase` (no en las subclases — evitar herencia duplicada que UHT no permite)

**Patrón:** Cuando un sistema necesita preguntarle algo al personaje, se hace vía interfaz, no casteando a la clase concreta.

### 2.6 Gameplay Tags como singleton nativo

En lugar de usar `FGameplayTag::RequestGameplayTag(FName("..."))` disperso por el código (propenso a typos, no refactorizable), todos los tags del juego viven en un struct singleton `FPantheliaGameplayTags`. El acceso es `FPantheliaGameplayTags::Get().Attributes_Secondary_Armor`. Si un tag se renombra, el compilador avisa. Ver `State_GAS.md`.

### 2.7 Helpers vía BlueprintFunctionLibrary

Cuando una operación requiere navegar una cadena de objetos (algo frecuente), se encapsula en una función estática dentro de `UPantheliaAbilitySystemLibrary`. Esto evita repetir el casting feo en cada llamada y permite usarlo limpiamente desde Blueprints.

Funciones actuales:
- `GetOverlayWidgetController(WorldContextObject)` / `GetAttributeMenuWidgetController(WorldContextObject)` — navegan `PC → HUD → WidgetController`. Ver `State_UI.md`.
- `InitializeDefaultAttributes(WorldContextObject, CharacterClass, Level, ASC)` — inicializa los atributos de un enemigo según su arquetipo y nivel, usando el Data Asset del GameMode. Ver `State_Abilities.md`.
- `GiveStartupAbilities(WorldContextObject, ASC)` — otorga a un ASC las abilities del array `CommonAbilities` del Data Asset.
- `GetCharacterClassInfo(WorldContextObject)` — devuelve el `DA_CharacterClassInfo` del GameMode. Punto de acceso centralizado para cualquier sistema que necesite consultar arquetipos, coeficientes de daño o abilities compartidas.

### 2.8 Funcionalidades descartadas por diseño

Decisiones tomadas que **no se implementarán**. Documentadas aquí para que ningún chat futuro las proponga de nuevo:

- **Texto flotante de daño** — descartado. Los soulslikes no muestran números de daño flotantes. El feedback viene de la barra de vida, efectos de impacto, sonidos y animaciones.
- **`CritResistance` como atributo pasivo** — descartado permanentemente. En Panthelia los críticos se evitan **activamente** (bloqueo, esquive), no pasivamente con un stat. No añadir este atributo.
- **`BlockChance` como atributo del enemigo** — descartado. El bloqueo de Panthelia es una acción del jugador con timing, no una probabilidad pasiva del enemigo.

### 2.9 Datos vs código: hacia una "base de datos" de balanceo

Principio guía adoptado (alineado con la prioridad de escalabilidad/modularidad del proyecto): **separar los datos de diseño del código y mantener juntos los datos que se diseñan juntos.** El objetivo es que un cambio de balance sea editar un dato en un sitio, no buscar un valor disperso por el código.

Panthelia ya aplica esto parcialmente y bien: Data Assets (`DA_CharacterClassInfo`), Curve Tables (`CT_Damage`, `CT_PrimaryAttributes_*`, `CT_DamageCalculationCoefficients`), el `TMap` `DamageTypes` por hechizo, `FAbilityAttributeScaling`, y los mapas data-driven de tags. El ExecCalc itera sobre datos, no sobre `if`s hardcodeados.

Dónde **no** se está cumpliendo todavía (deuda a corregir, ya señalada en `Code_Review.md`):
- Resistencias elementales fijadas por un Override placeholder desde `Resilience` (valor de diseño escondido en un GE).
- Ratios de escalado (`AttributeScalings`) fijos en cada `GA_*` en vez de en una tabla central.
- Valores sueltos hardcodeados (p. ej. el cap de `AttackSpeed`, umbrales de postura por enemigo dispersos).

Reglas prácticas derivadas:
1. **Si algo es un número de balanceo, vive en un Data Asset / Curve Table, no en C++ ni disperso por Blueprints.** Un valor de diseño no debería estar en más de un sitio.
2. **Para los sistemas grandes que vienen (árbol de habilidades, sistema elemental, heridas graves), diseñar primero la estructura de datos** (qué nodos/efectos/parámetros existen y cómo se relacionan) y construir el código encima. Reduce el riesgo de tener que reorganizar datos a mitad de implementación.
3. **Sistemas reutilizables + "glue", sin acoplar directamente.** Cada sistema (componentes como `WeaponTraceComponent`/`LockonComponent`, el ExecCalc, las abilities) se mantiene independiente; se conectan vía interfaces (`ICombatInterface`), tags y GameplayEffects — nunca con referencias mutuas directas entre sistemas. Ver 2.5 y 2.7.
4. **Vista observa simulación.** El estado del juego vive en el AttributeSet/ASC (simulación); la UI y los visuales solo **leen** de él (patrón Widget Controller, 2.4). Un sistema es de gameplay **o** de presentación, no ambos.

> Nota: el AttributeSet monolítico actual va en contra del principio de módulos pequeños — su división por dominio (combate / resistencias / buildup) está recomendada en `Code_Review.md` y es más barata cuanto antes se haga.

> **Costes de recursos:** existe una **arquitectura común de costes** (`UPantheliaCostAttributeSet` + resolvedor único, fórmula `Final = max(0, Base × max(0, Mult) + Flat)`), con GEs dinámicos separados por recurso (`Cost.Stamina` / `Cost.Mana`) y pago **atómico** multi-recurso. La `Base` sale de la fuente de cada dominio (ataques → arma; dodge/parry/hechizos → ability). Detalle en `State_GAS.md`.

### 2.10 Estados elementales: la ability no define el estado, el Data Asset sí

Decisión arquitectónica central del sistema de estados (Sección 25 ampliada), pensada para el árbol complejo:

```
La ability declara el buildup (identidad de acumulación)
Un Data Asset global (DA_ElementalStatusConfig) define qué hace cada estado
Los GEs del build (árbol/equipo) modifican ATRIBUTOS del atacante
El AttributeSet del objetivo dispara y aplica el payload leyendo esos atributos
```

Ninguna ability configura daño/duración/frecuencia del estado, y ningún perk añade `if` en código: conceden Gameplay Effects que suben atributos (`StatusPower`, daño porcentual, Defense Shred, `MagicDamage`), y el cálculo del estado los lee al detonar. Así cualquier fuente de Fuego produce la misma Quemadura base, modificable muchas veces por árbol y equipo de forma acumulable. Detalle en `State_GAS.md` y `State_Combat.md` sección 12.

**Cadena reservada (aún NO implementada) para los corazones elementales:**
```
El corazón equipado concederá una passive ability
El pipeline central emitirá eventos de combate (golpear a un objetivo quemado, etc.)
La passive ability reaccionará según atributos/perks
Los daños secundarios usarán una marca antirrecursión
```
Hoy solo están **reservados los tags y eventos**; no hay lógica de corazones en `AMainCharacter` ni en `GA_Firebolt` (evitar hardcodearla ahí es parte de la decisión). **La arquitectura completa de destino del árbol, los corazones, las familias de hechizos y el loadout está en `State_Architecture_Future.md`** — de lectura obligada antes de crear cualquier habilidad, nodo o sistema de equipo, para no construir algo que haya que rehacer.

### 2.11 EffectActors y zonas persistentes de daño

Políticas de *source* (quién es el instigador del efecto), cerradas en el hardening:
- **Pickups** → `TargetSelf` (el efecto se aplica al que lo recoge).
- **Zona creada por una ability** → source = **Instigator del atacante**.
- **Hazard ambiental** (lava, veneno de escenario) → source = **ASC ambiental propio**.
- `BP_FireArea` es **solo testbed histórico** (Sphere C++ + Box Begin/End simétricos, radio 1200, cero instancias) — **no reutilizar como arquitectura productiva**. Un sistema real de zonas persistentes debe diseñar ownership, stats, duración, stack, GameplayCue y Save/streaming antes de crear contenido (ver `State_Pending.md`).
- Robustez del EffectActor (ref-count por actor, fail-closed, cleanup) en `State_Hardening.md`.

### 2.12 Sprint y `UPlayerActionsComponent` (legacy conservado a propósito)

`UPlayerActionsComponent` **se conserva** porque Sprint/Walk aún dependen de él (funciona sin Tick). Su migración a GAS es una **fase separada** (crear la ability de sprint, definir coste/drenaje de estamina, reglas de cancelación por ataque/dodge/hit/muerte, migrar el input, validar feeling y recién entonces retirar el componente). No migrar por inercia hasta abrir esa fase. Ver `State_Pending.md`.

---

## 3. Estructura de directorios (módulo PantheliaProject)

```
Source/PantheliaProject/
├── Public/
│   ├── PantheliaElementTypes.h    enum EPantheliaElement (header manual)
│   └── PantheliaLogChannels.h     categoría de log LogPanthelia (.cpp en Private/)
├── AbilitySystem/         GAS: AttributeSet, ASC, MMCs, ExecCalc
│   ├── Abilities/         UPantheliaGameplayAbility (base), UPantheliaDamageGameplayAbility, UPantheliaProjectileSpell, UPantheliaPlayerAttackAbility
│   ├── Data/              UPantheliaAttributeInfoAsset, UPantheliaAbilityInfoAsset, UPantheliaLevelUpInfo, UPantheliaSkillTreeInfo, UPantheliaCharacterClassInfo
│   ├── ExecCalc/          UExecCalc_Damage
│   ├── ModMagCalc/        UMMC_MaxHealth, MaxMana, MaxStamina
│   ├── UPantheliaSkillTreeComponent   componente del árbol (vive en el PlayerState)
│   ├── PantheliaAbilityTypes.h/.cpp        FPantheliaGameplayEffectContext (flag de crítico)
│   └── PantheliaAbilitySystemGlobals.h/.cpp  alloc del context custom
├── AI/                    IA: PantheliaAIController (enemigos), y el sistema del boss:
│   ├── PantheliaBossBrainComponent   decisiones del boss (UActorComponent)
│   ├── Data/              UPantheliaBossProfile   datos del boss (acciones/fases/stats)
│   └── StateTree/         PantheliaBossStateTreeTasks   tasks del flujo del boss
├── Actor/                 PantheliaEffectActor, APantheliaProjectile
├── AssetManager/          UPantheliaAssetManager (entry point de inicialización)
├── Animations/            AnimInstance, AnimNotifies, WeaponTraceNotifyState, ComboWindowNotifyState
├── Characters/            PantheliaCharacterBase, MainCharacter, PantheliaEnemy, BossCharacter, PlayerActionsComponent
│   └── Components/         PantheliaDeathPresentationComponent (+ Types) — capa reusable de muerte (ragdoll multipart, ver State_GAS.md)
├── Combat/                WeaponTraceComponent, LockonComponent, BlockComponent, EquipmentComponent,
│                          PantheliaWeapon, PantheliaWeaponDefinition, PantheliaWeaponTypes
│                          [legacy huérfanos: CombatComponent, TraceComponent, BaseWeapon]
├── Game/                  PantheliaGameModeBase, PantheliaGameplayTags
├── Input/                 UPantheliaInputConfig, UPantheliaInputComponent
├── Interfaces/            MainPlayer, Enemy, CombatInterface  [legacy huérfano: Fighter]
├── Library/               UPantheliaAbilitySystemLibrary (BlueprintFunctionLibrary)
├── Player/                PantheliaPlayerController, PantheliaPlayerState
└── UI/
    ├── HUD/               PantheliaHUD
    ├── Widgets/           PantheliaUserWidget, PantheliaProgressBar
    └── WidgetController/  PantheliaWidgetController, OverlayWidgetController, AttributeMenuWidgetController
```

> El Custom Effect Context (`FPantheliaGameplayEffectContext`) y `UPantheliaAbilitySystemGlobals` están documentados en `State_GAS.md` (sección 6). Registro del Globals en `Config/DefaultGame.ini`.

> Nota: la ubicación física exacta de cada `.h/.cpp` puede variar según cómo los hayas organizado en disco. Lo importante es la **agrupación lógica**.

---

## 4. Convenciones de código

### Naming
- Clases C++: `APantheliaCharacterBase`, `UPantheliaAttributeSet` (prefijo `Panthelia` para evitar colisiones con Unreal)
- Componentes custom: `UCombatComponent`, `UTraceComponent` (sin prefijo del juego)
- Interfaces: `IMainPlayer`, `IEnemy`, `IFighter`, `ICombatInterface`
- MMCs: `UMMC_MaxHealth`, `UMMC_MaxStamina`

### Includes
- En `.h`: forward declarations siempre que sea posible
- En `.cpp`: includes completos
- Categorías en includes: por carpeta (`#include "Combat/BaseWeapon.h"`)

### UPROPERTY
- Componentes: `VisibleAnywhere` (creados en constructor)
- Configurables por diseño: `EditAnywhere` o `EditDefaultsOnly` + `BlueprintReadOnly`
- Estado runtime: `VisibleAnywhere` + `BlueprintReadOnly`
- Pointers UObject: usar `TObjectPtr<>` en código nuevo (UE5)

### GAS
- Macro `ATTRIBUTE_ACCESSORS` para getters/setters de cada `FGameplayAttributeData`
- Clampear en `PreAttributeChange` (preventivo) y `PostGameplayEffectExecute` (definitivo)
- `OnRep_X` siempre llama a `GAMEPLAYATTRIBUTE_REPNOTIFY`

### Logging
- Categoría de log propia **`LogPanthelia`**, declarada en `PantheliaLogChannels.h/.cpp`. El código nuevo usa `UE_LOG(LogPanthelia, Log/Warning/Error, TEXT("..."))` en vez de `LogTemp`.
- **`PantheliaLogChannels.h` va en `Public/` raíz** (no junto a `PantheliaProject.h`), para estar en el include path global del módulo — una ubicación incorrecta rompió una compilación.
- Para loguear `__FUNCTION__` usar `%hs` (char ANSI), no `%s`.

### Comentarios
- En español, explicativos para principiante
- Explicar **por qué**, no **qué** hace el código
- **No eliminar comentarios `//` existentes** al modificar archivos, salvo que se elimine el código asociado

### Actor Tags (bando)
- `BP_PantheliaEnemy` lleva el Actor Tag `"Enemy"` (el tag del jugador se documenta en la entrada del jugador productivo, arriba).
- Estos tags los lee `UPantheliaAbilitySystemLibrary::IsNotFriend(A, B)` (BlueprintPure) para distinguir bandos y evitar friendly fire (usado en `GA_MeleeAttack` y en el Weapon Trace — ver `State_Combat.md` sección 9). También los usa `MMC_MaxHealth` para el escalado de salud de enemigos (`State_GAS.md` sección 2).
- **Jugador productivo — migración a MetaHuman ✅ CERRADA (validada en PIE y en build Development Win64 cocinada).** El pawn jugable en runtime **ya no es** `BP_ThirdPersonCharacter`, sino la cadena MetaHuman:
  ```
  BP_PantheliaPlayerCharacter → BP_PantheliaPlayerMetaHumanBase → AMainCharacter → APantheliaCharacterBase → ACharacter
  ```
  `BP_PantheliaGameMode` es el GameMode global; `DefaultPawnClass = BP_PantheliaPlayerCharacter`; `GameDefaultMap = Lvl_ThirdPerson`. Rutas productivas bajo `/Game/Characters/Player/Production/`. **`BP_ThirdPersonCharacter` es legacy y NO se instancia en runtime** (conserva referencias hard y un error histórico de `LevelUp` ajeno a la migración; ver `Code_Review.md`). Al configurar tags/colisiones/referencias del jugador, usar la cadena MetaHuman. **No** hay Manny oculto ni runtime retargeting: el Body MetaHuman **es** el `CharacterMesh0` autoritativo de `ACharacter::GetMesh()` (Face, grooms, LODSync y MetaHuman Component forman la presentación multipart), lo que mantiene compatibles colisión, sockets, combate, arma, ragdoll y muerte del C++ existente.

  ```
  Migración MetaHuman productiva: CERRADA
  Development Cook/Package (Win64): PASS
  Smoke cocinado: PASS
  ```
  El jugador se validó en PIE durante las fases funcionales y, después, en builds Development Win64 **cocinadas y archivadas** (Fases 4G/4H/4I aprobadas). La build cocinada validó la muerte: `State.Dead`, ragdoll multipart, Face siguiendo el Body, grooms, arma desacoplada, WeaponTrace cerrado, finalización de DeathPresentation y **PlayerState/ASC preservados**. Los residuales de `MetaHumanMigration`, Manny y TestBoss son **deuda editorial o contenido compartido**: no bloquean gameplay, cook, package ni el smoke final (ver `State_Pending.md` y `State_Hardening.md`). **Próxima prioridad del loop: respawn + checkpoint/hoguera** (no implementados).
- El personaje jugable lleva el Actor Tag `"Player"`; `BP_PantheliaEnemy` lleva `"Enemy"`.

### Entorno de desarrollo (gotchas)
- **Visual Studio (UE 5.8):** tras regenerar los archivos del proyecto, el desplegable **Solution Configuration** se resetea a **"Development"** (target *Game*), lo que provoca crashes/comportamiento raro al abrir. Hay que volver a ponerlo manualmente en **"Development Editor"**. Es un patrón recurrente, no un bug del código.
- **Cámara del jugador en C++ (clase 262):** `AMainCharacter` tiene los componentes de cámara en C++ — `CameraBoom` (`USpringArmComponent`, referido como `SpringArmComp`), `PlayerCameraComponent` y `LevelUpNiagaraComponent`. El lock-on (`State_Combat.md` sección 3) escribe en `SpringArmComp->TargetOffset`, así que si esos componentes se pierden (p. ej. editando `MainCharacter` desde un snapshot viejo), `Tab` crashea. Ver el caso en `Code_Review.md` sección 8.

---

## 5. Dependencias del módulo (PantheliaProject.Build.cs)

Módulos públicos actualmente incluidos:
- `Core`, `CoreUObject`, `Engine`, `InputCore`
- `EnhancedInput` — para el nuevo sistema de input
- `GameplayAbilities`, `GameplayTags`, `GameplayTasks` — para GAS
- `AnimGraphRuntime` — para librerías de animación
- `UMG` — requerido para `UWidgetComponent` (barras de vida en mundo, etc.)
- `Niagara` — para los efectos de impacto (`BloodEffect`, etc.)

**Posibles adiciones futuras:**
- `Slate`, `SlateCore` — si se necesita UI desde C++ más avanzada
- `NavigationSystem` — para AI enemigos
- `AIModule` — para behavior trees
- Módulos de **StateTree**: `StateTreeModule` y `GameplayStateTreeModule` — para la IA del boss (`State_AI.md`). Confirmados en `Build.cs`.

---

## 6. Cómo usar los documentos del Project Knowledge

**Para Claude (cualquier chat):**
- Lee `State_Overview.md` primero para entender el contexto general
- Consulta el `State_*.md` específico del sistema sobre el que vas a trabajar
- Si propones algo que contradice una decisión aquí, justifícalo explícitamente
- Si implementas algo nuevo, sugiere al usuario qué documento actualizar

**Para el usuario:**
- Actualiza el archivo específico cuando termines un sistema o tomes una decisión
- Re-súbelo al Project Knowledge reemplazando la versión anterior
- Cuando un sistema de `State_Pending.md` se implemente, documéntalo en el archivo temático correspondiente (o crea uno dedicado si es un sistema grande sin hogar natural) y elimina la entrada de pending

---

## 7. Glosario rápido

- **GAS:** Gameplay Ability System, el framework de Unreal para habilidades/atributos/efectos
- **ASC:** AbilitySystemComponent, el componente que gestiona GAS en un actor
- **GE:** GameplayEffect, una "receta" de modificaciones de atributos
- **MMC:** ModMagnitudeCalculation, función custom para calcular el valor de un modificador en un GE
- **Attribute Set:** Conjunto de atributos (Health, Mana, etc.) replicados que GAS gestiona
- **GameplayTag:** Etiqueta jerárquica para identificar estados, eventos, mensajes
- **AnimNotify / AnimNotifyState:** Eventos disparados desde animaciones para sincronizar lógica
- **Asset Manager:** Singleton de Unreal que gestiona carga de assets y se inicializa muy temprano. Heredamos de él para registrar nuestros tags nativos.
- **Data Asset:** Asset de UE que solo contiene datos (sin lógica). Permite editar valores desde el editor sin recompilar. Ej: `DA_AttributeInfo`.
- **BlueprintFunctionLibrary:** Clase con solo funciones estáticas, llamables desde Blueprints. Útil para helpers globales.
- **BlueprintImplementableEvent:** Función declarada en C++ pero implementada en Blueprint. Útil para que C++ dispare lógica de UI sin conocerla.
- **BlueprintNativeEvent:** Como BlueprintImplementableEvent, pero con implementación por defecto en C++ que el Blueprint puede sobrescribir.
- **SetByCaller:** Magnitud de un GameplayEffect que se calcula en tiempo de aplicación (no en el asset), pasada por código vía tag. El ability asigna el valor con `AssignTagSetByCallerMagnitude(SpecHandle, Tag, Valor)` y el consumidor (MMC o ExecCalc) lo lee con `Spec.GetSetByCallerMagnitude(Tag)`. Permite efectos genéricos parametrizables.
- **ExecCalc (Execution Calculation):** Clase que hereda de `UGameplayEffectExecutionCalculation`. Puede capturar atributos de source y target, leer SetByCaller magnitudes y modificar múltiples atributos en una sola ejecución. Solo funciona con GEs Instant/Periodic, y no soporta predicción. Se usa para cálculos de daño complejos. Ver `State_Abilities.md`.
- **TStaticFuncPtr:** Alias de UE para punteros a función estática, compatible con TMap.
- **Curve Table / ScalableFloat:** Una Curve Table mapea un valor (ej. nivel) a otro (ej. daño). Un `FScalableFloat` es un float que apunta a una fila de Curve Table y se evalúa con `GetValueAtLevel(nivel)`. Permite escalar valores por nivel sin hardcodear.
