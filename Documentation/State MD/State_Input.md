# State_Input — Sistema de Input

> **Propósito:** Estado del sistema de input (Enhanced Input). Lee `State_Overview.md` primero para contexto general.

---

## 1. APantheliaPlayerController — ✅ Funcional

Usa **Enhanced Input** (no el sistema legacy de InputComponent). `IMC_Default` se asigna en `BeginPlay` al `UEnhancedInputLocalPlayerSubsystem`.

> **Abilities productivas que activan el input (migración MetaHuman, sin cambios en el sistema de input):** `InputTag.Dodge` → `GA_PantheliaPlayer_Dodge`; `InputTag.Block.Physical` → `GA_PantheliaPlayer_ParryPhysical`. El Enhanced Input y el ruteo de pulsaciones no cambiaron; solo cambian las abilities productivas concedidas por `startupAbilities`. Ver `State_Combat.md` sección 10. Movimiento, cámara, lock-on, dodge y guardia/parry funcionaron en el ejecutable **cocinado** (Fases 4G-4I); **sin** cambios de mappings.

### Input Actions actuales

| Input Action | Tipo | Tecla | Descripción |
|---|---|---|---|
| `IA_Move` | `FVector2D` | WASD / stick | Movimiento basado en rotación de cámara |
| `IA_Look` | `FVector2D` | Mouse / stick | Control de cámara |
| `IA_OpenAttributeMenu` | `bool` | `O` | Toggle del menú de atributos |

### Interaction Trace

`InteractionTrace()` se llama en cada tick. Hace un line trace desde la cámara y, si golpea algo que implementa `IEnemy`, dispara el highlight. Útil para ver qué enemigo enfocas antes de hacer lock-on.

---

## 2. Menú de Atributos (toggle desde el controller) — ✅ Funcional

### Variables del controller

- `OpenAttributeMenuAction` (`UInputAction*`) — referencia al asset del Input Action
- `AttributeMenuClass` (`TSubclassOf<UUserWidget>`) — clase del WBP del menú, asignable desde Blueprint
- `AttributeMenuWidget` (`UUserWidget*`) — instancia activa del widget
- `bAttributeMenuOpen` (`bool`) — estado actual

### Funciones

- `ToggleAttributeMenu()` — alterna entre abrir y cerrar según `bAttributeMenuOpen`
- `OpenAttributeMenu()`:
  1. Crea el widget con `CreateWidget<UUserWidget>(this, AttributeMenuClass)`
  2. Lo añade a la pantalla (`AddToViewport`)
  3. Pausa el juego (`SetPause(true)`)
  4. Muestra el cursor (`bShowMouseCursor = true`)
  5. Cambia `InputMode` a `UIOnly` para poder interactuar con los botones del menú
- `CloseAttributeMenu()` — hace todo al revés: quita el widget, reanuda el juego, oculta cursor, vuelve a `InputMode` `GameOnly`

### Configuración crítica del Input Action

`IA_OpenAttributeMenu` tiene **"Trigger When Paused"** activado en el asset del Input Action. Sin esto, una vez que el menú pausa el juego, no se podría volver a apretar `O` para cerrarlo — el input estaría bloqueado por la pausa.

### Decisión soulslike

**No hay botón de menú en pantalla.** Solo se accede con tecla. Coherente con el estilo del género (Dark Souls, Elden Ring no muestran botones flotantes para abrir el menú durante el gameplay).

---

## 3. Sistema de Input de Abilities (GAS-driven) — ✅ Funcional

El sistema de input de habilidades usa un enfoque data-driven basado en GameplayTags. No hay bindings hardcodeados — todo pasa por el Data Asset `DA_InputConfig`.

### Clases nuevas

**`UPantheliaInputConfig`** (Data Asset)
- Contiene un array de `FPantheliaInputAction` (struct: InputAction + FGameplayTag)
- Función `FindAbilityInputActionForTag(FGameplayTag, bLogNotFound)` para lookup
- Asset en el editor: `DA_InputConfig`

**`UPantheliaInputComponent`** (hereda de `UEnhancedInputComponent`)
- Función template `BindAbilityActions(InputConfig, Object, PressedFunc, ReleasedFunc, HeldFunc)`
- Itera el InputConfig y bindea los 3 callbacks a CADA InputAction con su tag como parámetro
- Configurado como Default Input Component Class en Project Settings
- `ETriggerEvent::Started` → Pressed, `Completed` → Released, `Triggered` → Held (cada frame)

### Callbacks en APantheliaPlayerController

- `AbilityInputTagPressed(FGameplayTag)` — vacío por ahora
- `AbilityInputTagReleased(FGameplayTag)` → llama `GetASC()->AbilityInputTagReleased()`
- `AbilityInputTagHeld(FGameplayTag)` → llama `GetASC()->AbilityInputTagHeld()`
- `GetASC()` — lazy init, castea solo la primera vez al `UPantheliaAbilitySystemComponent`
- Variable cacheada: `TObjectPtr<UPantheliaAbilitySystemComponent> PantheliaASC`
- `InputConfig` — `TObjectPtr<UPantheliaInputConfig>`, `EditDefaultsOnly`, se asigna en BP

### Input Actions de Abilities creados (todos Axis 1D float)

| Input Action | Tag | Descripción |
|---|---|---|
| IA_LightAttack | InputTag.LightAttack | Ataque ligero / contrataque |
| IA_HeavyAttack | InputTag.HeavyAttack | Ataque pesado |
| IA_Block_Physical | InputTag.Block.Physical | Bloqueo/parry físico |
| IA_Block_Magic | InputTag.Block.Magic | Bloqueo/parry mágico |
| IA_Dodge | InputTag.Dodge | Esquive normal/perfecto |
| IA_Spell_1 | InputTag.Spell.1 | Slot hechizo 1 |
| IA_Spell_2 | InputTag.Spell.2 | Slot hechizo 2 |
| IA_Spell_3 | InputTag.Spell.3 | Slot hechizo 3 |
| IA_Spell_4 | InputTag.Spell.4 | Slot hechizo 4 |
| IA_Spell_5 | InputTag.Spell.5 | Slot hechizo 5 |
| IA_Spell_Ultimate | InputTag.Spell.Ultimate | Habilidad definitiva |

Todos añadidos al `IMC_Default`. Todos mapeados a teclas físicas en el editor.

### Decisión de diseño resuelta

InputTags son **semánticos** (qué hace el jugador), no físicos (qué tecla presiona). El mapeo físico vive en el IMC y puede cambiarse en runtime sin tocar código.

Bloqueo/parry y esquive/esquive perfecto usan el MISMO InputTag — el timing lo determina la lógica del juego, no un input diferente. El contrataque (post-perfect block) también usa `InputTag.LightAttack` pero con `Activation Required Tags: State.PerfectBlock`.

> **Ruteo del release de parry/bloqueo:** el sistema de input custom no alimenta de forma fiable el `InputReleased` interno de GAS. Por eso, al soltar el botón de bloqueo, `PlayerController::AbilityInputTagReleased` llama a `GetASC()->NotifyBlockInputReleased(InputTag)`, que termina la guardia en la ability de parry activa (mismo patrón explícito que el combo melee con `NotifyComboInputPressed`). Ver `State_Combat.md` sección 11.3.

### Orden de ruteo de una pulsación (`AbilityInputTagPressed`) — el orden importa

Una pulsación real recorre esta cadena, y **una misma pulsación debe tener un solo destino**:

```
Pulsación
  → NotifyComboInputPressed(InputTag)          (buffer del combo melee)
  → NotifyDodgeFollowupInputPressed(InputTag)  (buffer del follow-up post-dodge)
  → ¿el dodge la aceptó?
       ├── Sí → NO continúa (la pulsación ya tiene destino)
       └── No → AbilityInputTagPressed(InputTag)  (activación normal)
```

- **Por qué el combo va primero:** la primera pulsación que *activa* una ability de ataque no debe encontrarse a sí misma ya activa y bufferizar por accidente el golpe siguiente.
- **Por qué el dodge va después:** durante `State.Dodge.Active` las abilities de ataque están **bloqueadas**, pero el dodge sí debe poder capturar el input como *intención futura* (el follow-up).
- **Por qué se corta si el dodge lo acepta:** si el follow-up consume el Light/Heavy, esa misma pulsación no debe intentar además activar un ataque normal.

> Solo entra al buffer una **pulsación real** (`Started`); el follow-up **no** usa `Held`. Ver el sistema completo en `State_Combat.md` sección 13.

---

## 4. Input pendiente (no relacionado con abilities)

- **Lock-on:** `IA_ToggleLockon`, `IA_SwitchTarget`
- **Interacción:** `IA_Interact` (puertas, items, NPCs)
- **UI extra:** `IA_OpenInventory`, `IA_OpenMap`, `IA_Pause`
