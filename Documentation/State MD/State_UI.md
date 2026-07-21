# State_UI — Sistema de Interfaz de Usuario

> **Propósito:** Estado del sistema de UI: HUD, widget controllers, barras de progreso, menú de atributos. Lee `State_Overview.md` primero para contexto general.

---

## 1. Flujo de inicialización general — ✅ Funcional

1. `MainCharacter::InitAbilityActorInfo()` se llama en `PossessedBy` (server) y `OnRep_PlayerState` (client)
2. Cachea ASC y AttributeSet desde el `PantheliaPlayerState`
3. Llama a `PantheliaHUD::InitOverlay(PC, PS, ASC, AS)`
4. HUD crea el `UPantheliaUserWidget` raíz y el `UOverlayWidgetController`
5. WidgetController bindea callbacks a los atributos vía lambdas y broadcasta valores iniciales

Esta cadena garantiza que el orden sea: GAS listo → controller con datos → widget visible. Si el widget se crea antes que GAS esté listo, las barras saldrían a 0.

---

## 2. APantheliaHUD — ✅ Funcional

Mantiene dos punteros principales:
- `OverlayWidgetController` (para el HUD principal)
- `AttributeMenuWidgetController` (para el menú de atributos)

**Patrón:** Lazy construction. El getter de cada controller comprueba si existe; si no, lo crea con `NewObject<T>`, llama a `SetWidgetControllerParams(WCParams)`, `BindCallbacksToDependencies()`, y lo devuelve. Esto evita acoplar el HUD con el orden exacto de inicialización de cada widget.

---

## 3. Base Widget + Base WidgetController — ✅ Funcional

### UPantheliaUserWidget

Base de todos los widgets del juego. Tiene:
- `WidgetController` (`UObject*`) — referencia al controller
- `SetWidgetController(UObject* InWidgetController)` — setter que asigna y luego dispara `WidgetControllerSet`
- `WidgetControllerSet()` — `BlueprintImplementableEvent` que el WBP usa para suscribirse a los delegates del controller

**Patrón:** Cualquier widget que use un controller hereda de aquí. El WBP en Blueprint implementa `WidgetControllerSet` y ahí hace los binds.

### UPantheliaWidgetController

Base abstracta para todos los widget controllers. Tiene:
- Referencias cacheadas: `PlayerController`, `PlayerState`, `AbilitySystemComponent`, `AttributeSet`
- `SetWidgetControllerParams(FWidgetControllerParams&)` — setter de los 4 punteros
- `BroadcastInitialValues()` — virtual, `BlueprintCallable`
- `BindCallbacksToDependencies()` — virtual, `BlueprintCallable`

`FWidgetControllerParams` es un struct con los 4 punteros, para pasarlos en una sola llamada en vez de 4 argumentos.

---

## 4. OverlayWidgetController — ✅ Funcional

Controller del HUD principal (barras de vida/maná/stamina/poise + mensajes contextuales).

### Delegates principales

- `OnHealthChanged`, `OnMaxHealthChanged`
- `OnManaChanged`, `OnMaxManaChanged`
- `OnStaminaChanged`, `OnMaxStaminaChanged`
- `OnPoiseChanged`, `OnMaxPoiseChanged`
- `MessageWidgetRowDelegate` — para mensajes de pickup/buff

### Binds en BindCallbacksToDependencies

Cada atributo se bindea con una lambda al delegate `GetGameplayAttributeValueChangeDelegate(Attribute)` del ASC. Cuando cambia, broadcastea el nuevo valor.

### Sistema de mensajes

Se suscribe a `UPantheliaAbilitySystemComponent::EffectAssetTags` (ver `State_GAS.md` sección 3). Filtra tags `Message.*`, busca su fila en `MessageWidgetDataTable` (estructura `FUIWidgetRow`) y broadcastea `MessageWidgetRowDelegate`. El WBP muestra el toast con icono, texto y widget asociado.

---

## 5. PantheliaProgressBar — ✅ Funcional

Widget de barra de progreso con "ghost bar" (delayed damage indicator estilo Souls/JRPG):

- Material `M_UI_HealthBar` con dos parámetros: `CurrentHealth` (salta al nuevo valor inmediatamente) y `PreviousHealth` (lerps gradualmente hacia `CurrentHealth`)
- `bGlobeInitialized` — evita ghost bars espurias durante la cascada de inicialización de GAS (cuando los valores pasan de 0 → MaxHealth en la inicialización, no debería verse "daño")
- `ForceSetPercent()` — cuando cambia el MAX (no activa ghost bar)
- `SetProgressBarPercent()` — cuando cambia el valor actual (activa ghost si bajó)
- `LerpCachedValue()` — animación interna del lerp
- `UpdateBoxSize()` — ajusta el tamaño visual si la barra cambia de longitud

---

## 6. Menú de Atributos — ✅ Funcional

Sistema completo que muestra los 5 primarios y 13 secundarios del jugador, con botones `+` en los primarios para subirlos. **El gasto de puntos ya está implementado** (clases 264-267): la fila de puntos disponibles se actualiza en vivo, los botones `+` se habilitan/deshabilitan según el saldo, y pulsarlos sube el atributo correcto descontando un punto. La lógica C++ (`UpgradeAttribute`, `GE_EventBasedEffect`) vive en `State_Progression.md` sección 4; aquí se documenta la parte de UI.

### 6.1 Fila de puntos, botones y gasto (clases 264-267)

- **`WBP_AttributePointsRow`** — duplicado de `WBP_TextValueRow` sin `NamedSlot` ni `AttributeTag`, label fijo "Attribute Points". Muestra el número con `SetNumericalValueInt` (heredada al duplicar — **no** por acceso directo al `TextBlock_Value` interno, que Unreal bloquea por ser referencia del árbol de diseño). Se bindea a `AttributePointsChangedDelegate` del controller (nodo exacto en el buscador: "Assign Attribute Points Changed Delegate").
- **Habilitar/deshabilitar botones** — `WBP_Button::SetButtonEnabled(bool)` → `Button->SetIsEnabled`; `WBP_TextValueButtonRow::SetButtonEnabled(bool)` reenvía a su `WBP_Button`; `WBP_AttributeMenu::SetButtonsEnabled(int32 AttributePoints)` hace `Branch (AttributePoints > 0)` y aplica `true`/`false` encadenado sobre las **5 filas primarias** (`Row_Endurance`, `Row_Hardness`, `Row_Resilience`, `Row_Resonance`, `Row_Spirit` — Panthelia tiene 5, no 4 como el curso).
- **Clic → gasto** — como la variable interna `Button` no se puede exponer entre Blueprints (Unreal bloquea el "ojo" de las variables auto-creadas al marcar "Is Variable" sobre un widget del árbol de diseño), `WBP_Button` expone un **Event Dispatcher `OnButtonClicked`**. La cadena real en el Event Graph de `WBP_Button` es: `Event PreConstruct → UpdateBoxSize → UpdateButtonBrushes → UpdateText → Bind Event to OnClicked (Target = Get Button) → OnClicked_Event`. `WBP_TextValueButtonRow` escucha ese dispatcher (`Assign OnButtonClicked`, sí accesible cross-Blueprint) → `GetAttributeMenuWidgetController` (**sin cast**, ya devuelve el tipo exacto; un cast aquí es error de compilación) → `UpgradeAttribute(AttributeTag)` (variable `AttributeTag` heredada de `WBP_TextValueRow`).

> **Nota de corrección:** el bind de `OnClicked` se hace desde `PreConstruct` (no `Construct` como se documentó antes) — `PreConstruct` corre tanto en el editor (vista de diseño) como en runtime, mientras que `Construct` solo corre en runtime. Para un binding de evento que debe estar listo apenas se crea el widget, `PreConstruct` es la elección correcta.

### 6.2 Sonido reutilizable en `WBP_Button` (clase 268)

Cualquier botón del juego (menú de atributos, y el futuro árbol de habilidades) puede reproducir sonido al click y al hover, **configurable por instancia sin tocar código** — pensado explícitamente para reutilizarse en sistemas de UI futuros.

- **Variables nuevas en `WBP_Button`:** `OnClickedSound` y `OnHoveredSound`, ambas tipo **`Sound Base`** (Instance Editable). Se usa `Sound Base` y no `SoundWave`/`SoundCue` directamente porque es la clase **padre** de ambos — acepta cualquiera de los dos como asset asignado, sin atarse a un tipo de sonido concreto.
- **Clic (cadena extendida):** `OnClicked_Event → Play Sound 2D (Sound = Get OnClickedSound) → Call OnButtonClicked`. El `Play Sound 2D` se insertó **entre** el Custom Event y la llamada al dispatcher — el dispatcher `OnButtonClicked` (usado por `WBP_TextValueButtonRow` para el gasto de puntos, sección 6.1) no se tocó ni se duplicó.
- **Hover (nuevo, no existía antes):** un segundo `Get Button` → `Bind Event to OnHovered` → `OnHovered_Event` → `Play Sound 2D (Sound = Get OnHoveredSound)`. El hover **no** dispara ningún Event Dispatcher hacia afuera — solo suena localmente, porque a diferencia del clic, ningún widget externo necesita *reaccionar* a que el mouse pase por encima.
- **Asignación:** ambas variables tienen un valor por defecto en `WBP_Button`, sobreescribible por instancia en cualquier Blueprint que lo use (p. ej. el botón de abrir el menú de atributos puede sonar distinto a los botones `+` de atributos primarios).

> **Descartado deliberadamente:** el pulido visual de esta clase del curso (colores/materiales de los botones) **no se implementó** — es una decisión de dirección de arte propia de Panthelia, no algo que adaptar del curso.

> **Orden crítico (dos veces en esta pantalla):** tanto el `SetWidgetController` de la fila de puntos como el `Assign Attribute Points Changed Delegate` del menú deben ir **antes** de `BroadcastInitialValues` en la cadena secuencial. Si el bind ocurre después del broadcast inicial, la fila/los botones nunca reciben el valor inicial y quedan en blanco hasta el primer cambio real.

> **Fix tecla O (cerrar el menú):** `OpenAttributeMenu()` usaba `FInputModeUIOnly`, que enruta **todo** el input a Slate y bloquea las Input Actions del `PlayerController` — por eso `O` no llegaba a `ToggleAttributeMenu` para cerrar. Fix en `PantheliaPlayerController.cpp`: `FInputModeGameAndUI` (+ `SetHideCursorDuringCapture(false)`), que deja interactuar con el menú y mantiene vivas las Input Actions del juego. (`bTriggerWhenPaused` del `IA_OpenAttributeMenu` ya estaba en `True`; no era el problema.)

### Cadena de objetos involucrados

```
PlayerController (input "O", ver State_Input.md)
    └── crea/destruye WBP_AttributeMenu
            └── obtiene UAttributeMenuWidgetController vía PantheliaAbilitySystemLibrary
                    └── lee de PantheliaAttributeSet.TagsToAttributes (ver State_GAS.md)
                    └── lee descripciones de UPantheliaAttributeInfoAsset
                    └── broadcast FAttributeInfoSignature por cada atributo
            └── filas WBP_TextValueButtonRow y WBP_TextValueRow filtran por su AttributeTag
```

### UAttributeMenuWidgetController

Hereda de `UPantheliaWidgetController`.

- En `BindCallbacksToDependencies()` itera el `TagsToAttributes` del AttributeSet y se suscribe a cada delegate `GetGameplayAttributeValueChangeDelegate(Attribute)` del ASC.
- En `BroadcastInitialValues()` itera el mismo TMap y broadcastea el valor actual de cada atributo.
- Todo data-driven, ni un solo `if (tag == X)` hardcodeado.

**Delegate público:** `FAttributeInfoSignature AttributeInfoDelegate` — broadcastea un `FPantheliaAttributeInfo` (tag + nombre + descripción + valor) por cada atributo.

**Referencia:** `UPantheliaAttributeInfoAsset* AttributeInfo` — Data Asset con la metadata de cada atributo.

### UPantheliaAttributeInfoAsset

Data Asset que contiene un `TArray<FPantheliaAttributeInfo>`. Cada entrada tiene:
- `FGameplayTag AttributeTag`
- `FText AttributeName` (texto localizable)
- `FText AttributeDescription`
- `float AttributeValue` (placeholder, se rellena en runtime con el valor real)

**Asset físico:** `Content/Blueprints/AbilitySystem/Data/DA_AttributeInfo`.

**Función crítica:** `FindAttributeInfoForTag(const FGameplayTag&)` busca en el array por tag y devuelve la info correspondiente. Si no la encuentra, log de error.

### Ability Info — `UPantheliaAbilityInfoAsset` + `UOverlayWidgetController` (clases 239-242, adaptado)

Infraestructura data-driven para describir habilidades y enviarlas a la UI. **Gemela** del sistema de Attribute Info, pero para hechizos.

**`UPantheliaAbilityInfoAsset`** (Data Asset, padre `UDataAsset`, en `AbilitySystem/Data/`) contiene un `TArray<FPantheliaAbilityInfo>`. El struct lleva sufijo `Asset` en la clase por la colisión UHT (igual que `FPantheliaAttributeInfo`/`UPantheliaAttributeInfoAsset`). Cada `FPantheliaAbilityInfo`:
- `AbilityTag` (FGameplayTag, EditDefaultsOnly) — clave, se asigna en el editor
- `InputTag` (FGameplayTag, BlueprintReadOnly **sin** EditDefaultsOnly) — se rellena en **runtime**, no en el editor (el input asignado puede cambiar al reasignar hechizos)
- `Icon` (TObjectPtr<const UTexture2D>)
- `BackgroundMaterial` (TObjectPtr<const UMaterialInterface>)
- `FindAbilityInfoForTag(Tag, bLogNotFound=false)` — devuelve struct vacío si no existe (comprobar con `.AbilityTag.IsValid()`).
- **Asset:** `DA_AbilityInfo` (empieza vacío hasta que existan hechizos del sistema de corazones), asignado en `BP_OverlayWidgetController` → Class Defaults.

**`UOverlayWidgetController`** (modificado): delegate `FAbilityInfoSignature AbilityInfoDelegate` (DYNAMIC multicast, BlueprintAssignable, para bindear los slots del HUD desde Blueprint), referencia `AbilityInfo` (TObjectPtr<UPantheliaAbilityInfoAsset>, EditDefaultsOnly), y `OnInitializeStartupAbilities(ASC)`: con el patrón if/else de timing en `BindCallbacksToDependencies`, crea un `FForEachAbility`, bindea un lambda que saca el `AbilityTag`, busca en `DA_AbilityInfo`, rellena el `InputTag` desde la spec y broadcastea; se lo pasa a `ForEachAbility` del ASC (ver `State_GAS.md`).

**Adaptaciones soulslike (divergencias del curso):**
- **HUD de spell globes (Diablo) NO construido.** Todo el UMG de la clase 242 (6 globos, `WP_HealthMana Spells`, `WP_SpellGlobe`) se omite. El HUD de hechizos será soulslike y se diseñará con el sistema de corazones elementales.
- **Multiplayer omitido:** el override de `OnRep_ActivateAbilities` (re-broadcast en cliente) no se implementa (single-player).
- **Nota de diseño futura (HUD soulslike):** cada slot de hechizo tendrá su `InputTag`, escuchará `AbilityInfoDelegate`, comparará `Info.InputTag` con el suyo (`MatchesTagExact`) y pondrá icono+material si coincide. Mismo patrón que el curso, distinta disposición visual.

### UPantheliaAbilitySystemLibrary

Hereda de `UBlueprintFunctionLibrary`. Funciones estáticas `BlueprintPure`:
- `GetOverlayWidgetController(WorldContextObject)` — devuelve el `UOverlayWidgetController` desde el HUD del PC local
- `GetAttributeMenuWidgetController(WorldContextObject)` — idem para el menú de atributos

Encapsulan la navegación `PC → HUD → GetXxxWidgetController(PC, PS, ASC, AS)` que de otra forma habría que repetir cada vez.

### Widgets de Blueprint

- **`WBP_AttributeMenu`** — Panel principal con un `ScrollBox` que contiene las filas. En su lógica de Blueprint, en `WidgetControllerSet` (evento implementable desde C++), se suscribe al `AttributeInfoDelegate` del controller y llama a `BroadcastInitialValues`.
- **`WBP_TextValueButtonRow`** — Fila para atributos primarios (con botón `+`). Tiene una variable `AttributeTag` (Gameplay Tag editable por instancia). Cuando recibe un broadcast del delegate, compara su `AttributeTag` con el del payload — si coincide, actualiza el texto y valor.
- **`WBP_TextValueRow`** — Versión sin botón, para atributos secundarios.
- **`WBP_Button`** y **`WBP_WideButton`** — Botones reutilizables con propiedades expuestas (texto, fuente, brushes para estados normal/hover/pressed, tamaño, y desde la clase 268 sonido de click/hover vía `OnClickedSound`/`OnHoveredSound` — ver sección 6.2). Diseñados para mantener consistencia visual y sonora en todo el juego.

### Por qué este diseño es escalable

Para añadir un nuevo atributo al juego y al menú:
1. Declararlo en `PantheliaAttributeSet` con `ATTRIBUTE_ACCESSORS`
2. Registrar su tag en `FPantheliaGameplayTags`
3. Añadir la entrada al `TagsToAttributes` del AttributeSet
4. Añadir una fila al `DA_AttributeInfo` con nombre y descripción
5. Arrastrar una `WBP_TextValueRow` al menú y asignarle el tag

**Cero modificaciones** al `AttributeMenuWidgetController` ni al `WBP_AttributeMenu`. Exactamente el patrón que necesitamos para escalar.

---

## 7. Barra de vida de Boss — ✅ Implementado

`WBP_BossHealthBar` — widget con `PantheliaProgressBar` (ghost bar funcionando) + nombre del boss. Se activa mediante un trigger box en el nivel cuando el jugador entra al área del boss, y se alimenta de los delegates `OnBossHealthChanged` / `OnBossMaxHealthChanged` de `ABossCharacter`.

El detalle completo (clase `ABossCharacter`, el `WBP_BossHealthBar`, y la lógica del Level Blueprint) está documentado en `State_Abilities.md` sección 10, porque la barra está acoplada al boss y a su `AbilitySystemComponent`.

> **Limpieza del Level Blueprint (cierre MetaHuman, Fase 4I):** se eliminaron del Level Blueprint de `Lvl_ThirdPerson` la **BossTriggerBox legacy**, la **creación/control legacy de la boss health bar**, referencias `Unknown` y la variable asociada. Esto **no sustituye** el sistema productivo de health bars (que siguió funcionando en la build cocinada) y **no hubo cambios en Widget Controllers** en 4H/4I. El HUD productivo (barras, menú de atributos, cursor, PlayerState/ASC) se validó en la build con el jugador MetaHuman.

**Pendiente:** ocultar la barra al salir del área y al morir el boss.

---

## 8. Slot de hechizo y cooldown radial (WBP_SpellSlot) — ✅ Funcional

El slot de hechizo del HUD muestra el icono del hechizo, un barrido radial de cooldown tipo reloj, una cuenta atrás numérica y un destello al quedar listo. Implementado para el Firebolt; visualmente completo. Todo el trabajo fue en editor (material + UMG + grafo Blueprint del widget); sin código C++ nuevo salvo el AbilityTask que ya existía.

### 8.1 Material `M_RadialCooldown`

- **Ubicación:** `/Content/UI/Materials/M_RadialCooldown`. **Material Domain:** User Interface. **Blend Mode:** **AlphaComposite (Premultiplied Alpha)** (crítico para la transparencia UI).
- **Cómo funciona:** centra las TexCoord (resta 0.5,0.5), calcula el ángulo con `Atan2`, lo normaliza a 0..1, lo corta contra el scalar param **`Progress`** (0→1) y un `Ceil` produce la cuña. **Salida:** `Constant3Vector` negro (0,0,0) → **Final Color**; `Multiply(Ceil × 0.7)` → **Opacity** (el alpha de la cuña).
- **Orientación del barrido (dos controles NO intercambiables):** la **dirección** de giro la fija un `Multiply (× -1)` entre `Atan2` y la normalización (−1 = horario); el **punto de arranque** (12 en punto) se logra **intercambiando qué canal va a cada entrada del `Atan2`** (el `ComponentMask` R al pin Y, el G al pin X) — una rotación de coords ANTES del `Atan2`. El nodo `Add` se queda en **0.5**.

> **GOTCHAs de materiales UI** (confirmados esta sesión, valen para todo el HUD):
> - **No mover el arranque con el `Add`:** cambiar `Add` de 0.5 a 0.75 **parte el barrido en dos cuñas** ("dos relojes"). El arranque se rota en las coords (intercambio R/G), no en el `Add`.
> - **El sobrebrillo (color > 1.0) se recorta a 1.0 en SDR:** multiplicar ×2 un icono ya claro no se nota — para destellos visibles usar **escala (pop), tinte de color real o alpha**, no sobrebrillo.
> - **`Opacity` vs "Opacity Override":** la transparencia UI se controla con el pin **Opacity** (solo aparece con Blend Mode Translucent/AlphaComposite), no con "Opacity Override".

### 8.2 `WBP_SpellSlot` — variables y flujo

**Variables:** `CooldownMID` (Material Instance Dynamic), `CooldownDuration` (double), `CooldownEndTime` (double), `CooldownTimerHandle`, `WaitCooldownTask` (`PantheliaWaitCooldownChange`), `OverlayWidgetController`, `CooldownReadyFlash` (Widget Animation), `InputTag` y `CooldownTag` (GameplayTag).

**Tres `Image` (capas):** `Image_Background` (fondo, tint negro alpha 0.7), `Image_Icon` (el icono del hechizo, oculto hasta que `SetBrushFromTexture` lo rellena en runtime), `Image_CooldownOverlay` (la cuña radial, **único** con `M_RadialCooldown`, Collapsed por defecto).

**Flujo:**
- **Event Construct:** `Image_CooldownOverlay → GetDynamicMaterial → Set CooldownMID`.
- **ReceiveAbilityInfo:** filtra por `InputTag.MatchesTag` (exacto); si coincide, `SetBrushFromTexture(Icon)` + `SetVisibility(Visible)` sobre `Image_Icon`, guarda `CooldownTag`, y arranca el nodo async `WaitForCooldownChange` (con `IsValid` + `EndTask` del task anterior). Los pines `CooldownStart`/`CooldownEnd` del nodo async se conectan **físicamente** a los custom events `OnCooldownStart`/`OnCooldownEnd`.
- **OnCooldownStart(TimeRemaining):** fija `CooldownDuration`, muestra overlay y texto, calcula `CooldownEndTime = GetGameTimeInSeconds + TimeRemaining`, y arranca un timer looping (0.1s) a `UpdateCooldownDisplay`.
- **UpdateCooldownDisplay (cada 0.1s):** `Remaining = clamp(CooldownEndTime − GetGameTimeInSeconds)`; actualiza el texto; `Progress = 1.0 − (Remaining / CooldownDuration)` → `CooldownMID.SetScalarParameterValue("Progress", Progress)`.
- **OnCooldownEnd:** limpia el timer, colapsa overlay y texto, y reproduce la animación **`CooldownReadyFlash`** (track sobre `Image_Icon`).

**`PantheliaWaitCooldownChange`** (`UAbilityTask` async, en C++): observa el cooldown tag del ASC y dispara `CooldownStart` (con el tiempo restante) al aplicarse el GE de cooldown y `CooldownEnd` al expirar/removerse. Es lo que alimenta al slot. **Un nodo async no llama solo a sus custom events** — hay que cablear sus pines de ejecución.

### 8.3 Capas visuales del slot (modelo mental a respetar)

Dos capas **independientes que pueden coexistir** (un hechizo puede estar en cooldown **y** sin maná a la vez), sobre Images distintas:
- **Barrido radial** (`Image_CooldownOverlay`) — cubre **exclusivamente** el estado de cooldown.
- **Gris / desaturado** (`Image_Icon`) — cubre "todo lo demás que impide lanzar" (maná insuficiente, silencio, bloqueo). **No implementado** (ver pendientes).

**Patrón acordado para el gris:** una sola función en el slot (nombre propuesto `RefreshAvailability`) que evalúe **todas** las razones de indisponibilidad y decida gris vs normal. Añadir una razón nueva (silencio, bloqueo) = sumar una condición en esa función, sin tocar la capa visual.

### 8.4 Compatibilidad futura verificada

El cooldown radial + `WaitForCooldownChange` ya es compatible (sin rehacerse) con:
- **Reducción instantánea de cooldown** (traits): siempre que se implemente como **remover el GE de cooldown y reaplicar uno más corto** (patrón GAS). `WaitForCooldownChange` detecta remove (→ CooldownEnd) y reapply (→ CooldownStart con el nuevo tiempo) y el barrido se reajusta solo. **No** funciona modificar la duración del GE en caliente sin removerlo.
- **Deshabilitar hechizos** (silencio/buff) vía block tags: cooldown y habilitación son independientes; un hechizo silenciado con cooldown activo sigue contando (correcto).
- **Ciclo de hechizos:** el slot se reconfigura solo si recibe un nuevo broadcast de `AbilityInfo` (re-ejecuta IsValid → EndTask → nuevo `WaitForCooldownChange`).

---

## 9. Barra de XP (WBP_XPBar) — ✅ Funcional

Barra de experiencia en la esquina superior derecha del `WBP_Overlay`. Widget en `/Game/UI/Overlay/WBP_XPBar` (padre `PantheliaUserWidget`, canvas raíz `CanvasPanel_41`). Permanece oculta y **aparece al ganar XP**, se rellena de forma suave, muestra un texto `+{XP}` y se **desvanece tras 2.5 s sin XP nueva**.

### 9.1 Material `M_XPBar`

`MD_UI`, **Blend Mode `Translucent`**. Genera el color con un `Step(Progress, TextureCoordinate.R)` multiplicado por un `Constant3Vector` dorado `(1.0, 0.75, 0.1)`; el mismo `Step` va a **Opacity**. Scalar param **`Progress`** (default 0.5).

### 9.2 Flujo del widget

- **Barra suave** (ya funcionaba): `Event Tick → FInterpTo(StartXPProgress, TargetXPProgress, DeltaSeconds, InterpSpeed=5) → Set StartXPProgress → XPBarMID.SetScalarParameterValue("Progress")`. El evento `OnXPPercentChanged_Event` solo hace `Set TargetXPProgress` (el Tick interpola hacia el objetivo).
- **Aparición + texto + fade** (`OnXPGained_Event`): `StopAllAnimations → UnbindAllFromAnimationFinished → SetRenderOpacity(CanvasPanel_41, 1.0) → SetRenderOpacity(Image_XPBarFill, 1.0) → SetVisibility(Visible) → FormatText "+{XP}" + SetText → PlayAnimation(XPGainedFade) (oculta el texto al terminar) → PlayAnimation(XPBarFill) → Retriggerable Delay(2.5) → PlayAnimation(FadeOut) → SetVisibility(Hidden) + SetRenderOpacity(0) del Canvas`.

### 9.3 Bugs resueltos (con sus causas — gotchas de widgets)

- **La barra no reaparecía tras el primer fade.** El `FadeOut` dejaba el Render Opacity de `Image_XPBarFill` en 0 y al reaparecer solo se reseteaba el del Canvas. Fix: resetear **también** `SetRenderOpacity(Image_XPBarFill, 1.0)`.
- **Destello blanco residual al difuminarse.** `M_XPBar` usaba `AlphaComposite`, que **no respeta el Render Opacity heredado del padre** igual que `Translucent` — la barra se dibujaba un frame extra tras el fade. Fix: cambiar `M_XPBar` a **`Translucent`**.
  > Esto **no contradice** el cooldown radial (sección 8), que sí usa `AlphaComposite` correctamente: aquel controla su alpha **directamente** con el scalar `Progress` y no depende de heredar la opacidad del padre. Regla: si un material UI necesita **heredar el Render Opacity del padre** (p. ej. para un fade del widget), usar `Translucent`; si controla su propio alpha, `AlphaComposite` vale.
- **El fade no se interrumpía al ganar XP durante el desvanecimiento.** Causa raíz: un nodo **`Delay` normal** entre `PlayAnimation(XPBarFill)` y `PlayAnimation(FadeOut)` — si se le re-llama con uno pendiente, **ignora la nueva llamada** y el temporizador original sigue. Fix: **`Retriggerable Delay`** (cada llamada reinicia la cuenta desde cero), así el `FadeOut` solo dispara tras 2.5 s completos sin XP nueva.

> **Nota de prueba (tecla L):** la tecla L en `BP_ThirdPersonCharacter` llama directo a `PantheliaPlayerState::AddToXP(150)`, que dispara `OnXPPercentChanged` (mueve la barra) pero **no** `OnXPGained` (la aparición/fade). No es una prueba fiel del flujo completo — el widget se prueba **matando enemigos reales**.

---

## 10. Spell Menu — ⏸️ Pausado en la clase 274 (vitrina del árbol)

La UI del menú de hechizos es la **vitrina del árbol de habilidades** (cuya infraestructura de código ya está lista — ver `State_Progression.md` sección 5). La Sección 24 del curso se **pausó deliberadamente** a mitad para priorizar la Sección 25 (Combat Tricks), que construye piezas de GAS (debuffs, efectos) que el árbol necesitará de todas formas. **No hay dependencia técnica** entre ambas (UI de menú vs. lógica GAS). Toda esta UI es **placeholder a rediseñar** más adelante.

> **Estado de verificación (importante para no asumir):** `WBP_SpellGlobeButton`, `WBP_OffensiveSpellTree` y `WBP_PassiveSpellTree` están **completos y funcionales**. `WBP_EquippedSpellRow` está **en construcción y SIN verificar en el editor** (diseño entregado pero no confirmado que compile ni se haya ejecutado). Al retomar, el primer paso es confirmarlo en el editor.

### 10.1 `WBP_SpellGlobeButton` (`/Game/UI/SpellMenu/`) — ✅ funcional

Botón de hechizo reutilizable para el menú/árbol, duplicado de `WBP_SpellSlot` (sección 8) pero **sin cooldown** (no aplica fuera del HUD de combate).
- **Jerarquía:** `SizeBox_Root` → `Overlay_Root` → [`Image_Background`, `Image_Icon`, `Image_Selection`, `Button_Ring`] (el orden importa — ver el fix de hover).
- **Se eliminó toda la lógica de cooldown heredada** (`Image_CooldownOverlay`, `Text_Cooldown`, y en el Event Graph el bloque `OnCooldownStart`/`OnCooldownEnd`/`WaitForCooldownChange`/timers). Se conserva solo: `WidgetControllerSet` → cast a `BP_OverlayWidgetController` → bind a `AbilityInfoDelegate` → `ReceiveAbilityInfo` → `MatchesTag(InputTag)` → `SetBrushFromTexture` + `SetVisibility` sobre `Image_Icon`.
- **Tamaño configurable por instancia (construido desde cero, no existe en el curso — decisión de escalabilidad):** variables `GlobeWidth`/`GlobeHeight`/`GlowPadding` (categoría "Globe Properties", Instance Editable) + funciones `UpdateGlobeSize` (aplica `Set Width/Height Override` sobre `SizeBox_Root`) y `UpdateGlowPadding` (aplica `Set Padding` sobre los Overlay Slots de `Button_Ring` e `Image_Icon`), ambas llamadas desde `Event PreConstruct`. Así cada instancia (ofensiva 90px, pasiva 60/45px, equipada 70px) tiene su tamaño **sin duplicar la clase**.
- **Fix de hover:** `Button_Ring` debe ser el **último** hijo del Overlay (por encima de `Image_Selection`). Causa: `Image_Selection`, aunque invisible (`RenderOpacity=0`), seguía bloqueando el mouse por estar más arriba en el orden de input. Orden correcto confirmado en el `.t3d`: `Image_Background → Image_Icon → Image_Selection → Button_Ring`.

### 10.2 `WBP_OffensiveSpellTree` — ✅ funcional

Árbol de 3 columnas × 3 botones (90px) con líneas conectoras (`Line1`).
- **Divergencia deliberada del curso:** el curso usa **Wrap Box + Spacers** para las columnas; se descartó por **frágil** (depende de que las alturas acumuladas coincidan por casualidad con el alto del contenedor, y se rompe con cualquier cambio de tamaño). Se reemplazó por `HorizontalBox_Root` → 3× `VerticalBox_ColumnN` con **Padding simétrico** (no Spacers) — determinista.
- **Bug corregido:** la variable `BoxHeight` tenía su Default Value en `0` (se corrompió durante los rediseños estructurales grandes, sin error de compilación que lo delatara). Corregido a `350`.
- **Imprecisión aceptada:** no se logró simetría perfecta con padding idéntico en las 3 columnas (causa no diagnosticada); Romelt optó por un ajuste manual pragmático (columna 3 con padding L=0, R=30), dado que toda la HUD es placeholder.

### 10.3 `WBP_PassiveSpellTree` — ✅ funcional

Duplicado de `WBP_OffensiveSpellTree` con `BoxHeight` reducido a `100`. Una sola fila (`HorizontalBox_Passives`) con 3 botones a 60×60, `GlowPadding` ~4-5, separados por Padding (30+30 = 60px entre ellos, mismo patrón simétrico).

### 10.4 `WBP_EquippedSpellRow` — 🔨 en construcción, **sin verificar en editor**

Duplicado de `WBP_PassiveSpellTree`. **Diseño entregado pero NO confirmado** que se haya ejecutado ni compilado (no hay `.t3d` ni capturas todavía). Diseño previsto:
- `SizeBox_Root`: `BoxWidth=600`, `BoxHeight=140`.
- `HorizontalBox_Root` → `VerticalBox_OffensiveSection` (label "Ofensivas" + `HorizontalBox_OffensiveSlots` con **6** `VerticalBox_SlotN`, cada uno con un `Text` mostrando la tecla — **`1`,`2`,`3`,`4`,`5`,`T`** — y un `WBP_SpellGlobeButton` a 60×60) + `VerticalBox_PassiveSection` (label "Pasivas" + `VerticalBox_PassiveSlots`, diseñado **escalable a N slots**, arrancando con 3 instancias a 45×45 sin label, apiladas).
- **Mapeo de teclas:** los 6 slots ofensivos corresponden **exactamente** a los InputTags que ya existen en código (`InputTag.Spell.1` a `.5` + `InputTag.Spell.Ultimate`, tecla T) — ver `State_Input.md` y `State_GAS.md`. No hubo que inventar la cantidad: los 6 InputTags ya definían 6 slots.
- **Las pasivas no tienen InputTag** (correcto: una pasiva no se activa por input; está equipada y su efecto es constante).

> **Lecciones de UMG (de esta sección, valen para todo el HUD):**
> - **`Set Padding` sobre un widget dentro de un Overlay no tiene atajo directo.** El widget (p. ej. `Button_Ring`) no conecta directo a `Set Padding` (que pertenece a `OverlaySlot`, otro tipo). Patrón correcto: `Get <Widget>` → `Get Slot` (devuelve `PanelSlot` genérico) → **`Cast to Overlay Slot`** → `Set Padding`. Sin el Cast, el pin `Target` queda sin conexión ("this blueprint (self) is not a OverlaySlot").
> - **Un `Size Box` solo acepta un hijo directo.** Para cambiar su contenido por otro tipo de contenedor: **Cut** (Ctrl+X) el hijo viejo, insertar el nuevo contenedor, **Paste** el contenido dentro y mover cada pieza a su destino.
> - **Un `Image` con `RenderOpacity=0` sigue bloqueando el mouse** si está por encima en el orden Z del Overlay — invisible ≠ no interactuable. Para quitar el hit-testing: `Visibility = Hidden`/`Collapsed`, o reordenar el Overlay para que el widget interactivo quede el último (lo que se hizo aquí).
> - **Wrap Box + Spacer para columnas de altura fija es frágil:** depende de cálculos de "cabe o no cabe". Preferir `HorizontalBox`/`VerticalBox` fijos + `Padding` (determinista).
> - **Verificar el Default Value real de una variable Blueprint tras rediseños estructurales grandes** — se corrompió un valor (`BoxHeight` a 0) sin ningún error de compilación que lo señalara.

### 10.5 Al retomar (horizonte)

Terminar y verificar `WBP_EquippedSpellRow` en el editor, y continuar con la **clase 275** (Spell Menu Widget: el contenedor que junta ofensivo + pasivo + equipada + descripción + puntos) en adelante (clases 275-302). El `SpellMenuWidgetController` se bindeará a `OnSkillPointsChangedDelegate` (PlayerState) y `OnSkillNodeChangedDelegate` (componente del árbol) — ver `State_Progression.md` sección 5.1.

---

## 11. Pendientes del sistema de UI

### 11.1 Lógica de subida de atributos primarios — ✅ Implementada (clases 264-267)

Ya no es pendiente. El gasto de puntos funciona de punta a punta: `AttributePoints` en el `PlayerState` (Fase 1 de progresión), `UpgradeAttribute` en el ASC + `GE_EventBasedEffect` con SetByCaller por primario, y el flujo de UI (fila de puntos, botones habilitados por saldo, Event Dispatcher del botón → `UpgradeAttribute`). Detalle de UI en la sección 6.1; detalle de lógica en `State_Progression.md` sección 4.

### 11.2 UI pendiente

- **HUD de objetivo bloqueado** (cuando se implemente lock-on visual)
- **HUD de hechizo equipado** (cuando lleguen los elementos)
- **Inventario / Equipamiento** (sin diseño todavía)
- **Mapa / Pantalla de pausa** (sin diseño todavía)
- **Toasts de Quemadura / Electrocución / Saturación** (cuando se implementen efectos de estado)
- **Barras de buildup elemental e iconos de estado** — los 4 buildup (`Fire/Storm/Water/Nature`) ya existen en el AttributeSet (ver `State_GAS.md`); falta la UI que los muestre (barras que suben a 100 y decaen, iconos de estado activo). El sistema de estados ya es funcional; solo falta su representación visual.
- **`DA_AttributeInfo`:** el rediseño de estados añadió **8 filas** (4 de daño porcentual + 4 de Defense Shred) y expone atributos nuevos vía `TagsToAttributes`; el menú de atributos los mostrará cuando el diseñador quiera exponerlos. Revisar la semántica de las filas Buildup si se muestran.

### 11.3 Pendientes del slot de hechizo (sección 8)

- **Limpieza de nodos de diagnóstico** en `WBP_SpellSlot` antes de darlo por cerrado: en Event Construct (Print String "MID OK"/"MID NULL") y en `UpdateCooldownDisplay` (Print String + LogString del valor de Progress). Están en `DevelopmentOnly` (no afectan a release) pero conviene retirarlos.
- **Gris por indisponibilidad** (`RefreshAvailability`, sección 8.3): desaturar/oscurecer `Image_Icon` cuando el hechizo no se puede lanzar.
  - **Maná:** **pospuesto deliberadamente.** Se descartó la opción simple (campo `ManaCost` en `FPantheliaAbilityInfo`) a favor de leer el coste del **GE de coste en runtime** (para que buffs/debuffs de coste se reflejen solos en el HUD). Romelt prefiere esperar a esa infraestructura antes que montar un placeholder a reemplazar. El maná insuficiente **ya** impide lanzar a nivel GAS; solo falta la capa visual.
  - **Silencio / bloqueo:** no existe sistema aún. Cuando exista: un tag tipo `State.Silenced` / `Block.Spells` en la lista de "tags que bloquean activación" de las abilities de hechizo; el slot lo escucha con `RegisterGameplayTagEvent` y llama a `RefreshAvailability`.
- **Feedback de intento fallido:** sonido + respuesta visual (parpadeo/sacudida) cuando el jugador intenta lanzar un hechizo no disponible. En GAS se engancha al método que se llama cuando una ability falla activación por block tag.
- **Ciclo de hechizos (asignar/desasignar a slots):** el sistema que reasigne hechizos **debe re-emitir el broadcast de `AbilityInfo`** a los slots afectados; el slot ya está preparado para reconfigurarse al recibirlo. Si no se re-emite, el slot sigue escuchando el cooldown tag viejo.
