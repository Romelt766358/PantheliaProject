# UI_REVIEW_KIMI — Revisión técnica del Plan Maestro de UI de Panthelia

> **Documento:** revisión quisquillosa del *Plan maestro para rehacer la interfaz completa de Panthelia* (propuesta arquitectónica previa a implementación, 2026-07-17).
> **Emitido por:** Kimi (revisión técnica solicitada en §15 del plan).
> **Destinatarios:** Fable y Codex, para el proceso de convergencia descrito en §18 del plan (`UI_REVIEW_CONVERGENCE.md`).
> **Base de la revisión:** código real del repositorio (`PantheliaProject.Build.cs`, `PantheliaProject.uproject`, `PantheliaPlayerController.cpp`, `PantheliaHUD.h/.cpp`, `PantheliaUserWidget.h`, `PantheliaWidgetController.h`, `PantheliaAbilitySystemLibrary.h/.cpp`) cruzado con `State_UI.md`, `State_Input.md`, `State_GAS.md`, `State_Abilities.md`, `State_Combat.md`, `State_Progression.md`, `State_Pending.md`, `State_Validation.md`, `State_Hardening.md`, `State_Architecture_Future.md` y `Code_Review.md`.
> **Formato:** Hallazgo / Severidad / Evidencia / Riesgo / Recomendación / Cambio concreto al plan, según lo solicitado en §15.

---

## 0. Veredicto

El plan es sólido en lo estratégico. Son aciertos que deben conservarse intactos:

- Fase 0 de auditoría de solo lectura antes de tocar nada.
- Propiedad exclusiva de assets binarios (§8.1) y checkpoints.
- Rechazo a la migración MVVM (§4.9).
- Conservación de la capa reactiva GAS → Widget Controllers (§1.1).
- `AppliedBuild`/`PendingBuild` en hoguera y "la UI no decide gameplay" (§3.1, §5.8).
- No generar pantallas completas como una sola imagen con IA (§7.2).
- Validación editorial de Data Assets (§11).
- Criterios de aceptación con datos vacíos/inválidos (§14).

Pero tiene **2 fallos críticos, 4 altos y varios medios/menores**.

**Decisión solicitada: NO autorizar Fase 0 hasta corregir H1 y H2.** El resto puede converger en una v1.1 del plan sin bloquear la auditoría.

---

## 1. Hallazgos

### H1 — El plan viola la Invariante 20 del proyecto (Codex/MCP no escribe C++)

- **Severidad:** Crítica
- **Evidencia:** §8.3 asigna a Codex "Cambios C++", "Integración de CommonUI", "UI Manager", "Clases base". `State_Architecture_Future.md` §10 (invariante 20) y §12: *"el C++ (.h/.cpp) se diseña y escribe en el chat de arquitectura/documentación; Codex/MCP no escribe C++ — solo tareas editoriales sobre assets ya compilados"*.
- **Riesgo:** la pieza más delicada del plan (todo el C++ de Fase 2) queda asignada al agente que el propio proyecto le tiene prohibido ese rol. Si se ejecuta tal cual, se rompe el gobierno de trabajo aprobado por Romelt.
- **Recomendación:** el C++ (`UPantheliaUIManagerSubsystem`, `UPantheliaActivatableWidget`, structs del registry, enums de políticas) se diseña y entrega desde el chat de arquitectura. Codex/MCP hace lo editorial: activar plugins, editar `.uproject`/`.ini`, crear WBP hijos, Data Assets, pruebas, dumps y capturas.
- **Cambio concreto al plan:** reescribir §8.3 en dos carriles ("C++ — chat de arquitectura" / "Editor vía MCP — Codex") y añadir la invariante 20 a §3 (Principios obligatorios).

### H2 — La interfaz `IPantheliaWidgetControllerConsumer` (§4.5) colisiona con el contrato existente y es sobrearquitectura

- **Severidad:** Crítica
- **Evidencia:** `PantheliaUserWidget.h` ya define `SetWidgetController(UObject*)` (UFUNCTION) + `WidgetControllerSet()` (BlueprintImplementableEvent). Si la interfaz declara `SetWidgetController` como `BlueprintNativeEvent`, UHT exige `SetWidgetController_Implementation` en las clases implementadoras → colisión directa en `UPantheliaUserWidget` o renombrado forzado del contrato que usan hoy `WBP_AttributeMenu`, `WBP_SpellSlot`, `WBP_BossHealthBar` y `WBP_XPBar`. Además el contrato actual es deliberadamente genérico (`UObject*`): la boss bar recibe un `ABossCharacter*`, no un WidgetController.
- **Riesgo:** refactor del contrato base funcional con riesgo de romper todos los WBP actuales; UInterface + BlueprintNativeEvent + herencia de widgets es fuente clásica de errores UHT. El propio plan duda de la interfaz ("Debe revisarse si conviene…").
- **Recomendación:** eliminarla. Patrón **contrato espejo**: `UPantheliaActivatableWidget` replica el mismo par `SetWidgetController`/`WidgetControllerSet` (~10 líneas). Dos familias, mismo contrato nominal, cero UInterface.
- **Cambio concreto al plan:** sustituir §4.5 por "convención de contrato espejo" y tachar la interfaz de §12 (nomenclatura).

### H3 — Omisión de `UPantheliaAbilitySystemLibrary` y del punto de entrada `InitOverlay`

- **Severidad:** Alta
- **Evidencia:** `PantheliaAbilitySystemLibrary.cpp` (líneas 33-67) es la vía real por la que los WBP obtienen controllers (PC → HUD → controller). No aparece en §1.1, §4 ni §19 del plan. Tampoco se define quién crea el overlay tras la migración ni dónde viven las clases de controller, que hoy son UPROPERTY del HUD (`PantheliaHUD.h` líneas 34-47: `OverlayWidgetClass`, `OverlayWidgetControllerClass`, `AttributeMenuWidgetControllerClass`).
- **Riesgo:** dos puntos de creación compitiendo (HUD + manager) → doble overlay, pendiente ya documentado en `State_GAS.md` §2 para el futuro respawn. Los WBP no rehechos de inmediato perderían su fuente de controllers.
- **Recomendación:** conservar `MainCharacter → APantheliaHUD::InitOverlay` como único punto de entrada (MainCharacter no se toca), pero `InitOverlay` delega en el manager para añadir el overlay a `UI.Layer.GameHUD`. La librería se actualiza internamente para consultar al manager manteniendo sus firmas. Las clases de controller migran a una única fuente (manager/registry).
- **Cambio concreto al plan:** añadir la librería y el flujo `InitOverlay` a §1.1; añadir a §4.2 la responsabilidad "crear controllers de dominio con `FWidgetControllerParams`"; añadir a Fases 2/5 la migración de las UPROPERTY del HUD.

### H4 — Pantallas agendadas antes que sus sistemas de gameplay

- **Severidad:** Alta
- **Evidencia:** según `State_Pending.md` y `State_Architecture_Future.md`: no existen Build/LoadoutComponent (hoguera §5.8), HeartDefinition (árbol resuelto por corazón §5.4), sistema de ítems (inventario §5.6), SaveGame (§5.10 cargar/continuar), respawn (§5.11 muerte). El equipamiento actual es **1 slot** (`UPantheliaEquipmentComponent`, `State_Combat.md` §10.1), no 2 (§5.7). `DA_SkillTree` está sin poblar. El plan lo admite en Riesgo 9, pero aun así agenda Árbol y Hoguera como construcción normal en Fase 5.
- **Riesgo:** construir pantallas "definitivas" contra contratos imaginarios → rehacerlas cuando llegue el gameplay real; o peor: que la UI defina de facto el gameplay (inversión de §3.1).
- **Recomendación:** Fase 5 en dos carriles: (a) con fuente de verdad existente: HUD, boss bar, atributos, pausa, opciones (`UGameUserSettings` ya existe), muerte placeholder; (b) bloqueadas: árbol, equipado de hechizos, hoguera, equipamiento 2 slots, inventario, cargar → solo wireframe + spec de contrato hasta que exista el gameplay.
- **Cambio concreto al plan:** reordenar §10 Fases 5-6; en `UI_SCREEN_MAP.md` marcar cada pantalla con "Dependencia de gameplay: X / ninguna".

### H5 — Pausa/input/cursor sin semántica de composición; el gotcha de GameAndUI no queda fijado

- **Severidad:** Alta
- **Evidencia:** `State_UI.md` §6.1 documenta el fix real: `FInputModeUIOnly` bloqueaba las Input Actions y la tecla O no cerraba el menú → `FInputModeGameAndUI` (confirmado en `PantheliaPlayerController.cpp` líneas 286-294). El plan propone `EPantheliaUIInputPolicy` por pantalla pero no fija el default ni quién gana cuando un modal se abre sobre un menú pausado (¿doble `SetPause`? ¿quién despausa?). Tampoco cubre la interacción entre el input routing de CommonUI y el `BindAbilityActions` custom: un ataque en Held al abrir un menú podría seguir alimentando `AbilityInputTagHeld`.
- **Riesgo:** despausas erráticos al cerrar modales, cursor perdido, abilities activándose con el menú abierto.
- **Recomendación:** (1) política por defecto de pantallas de juego = GameAndUI + pausa (hereda el fix documentado); Menu puro solo en menú principal. (2) Solo el manager toca `SetPause`/cursor/InputMode, con **ref-count** de peticiones y restore al hacer pop. (3) Prueba obligatoria en Fase 2: "mantener ataque en Held → abrir menú → soltar → cerrar", verificando que no se activan abilities ni quedan buffers.
- **Cambio concreto al plan:** ampliar §4.2 (ref-count de pausa) y §10 Fase 2 (prueba Enhanced Input ↔ CommonUI).

### H6 — Toast/Cinematic/Debug no pueden ser stacks de activatables

- **Severidad:** Media
- **Evidencia:** `UCommonActivatableWidgetStack` solo acepta `UCommonActivatableWidget`; los toasts no deben robar foco ni entrar en la cadena de Back (patrón Lyra: stacks para Game/Menu/Modal, contenedores simples para el resto).
- **Riesgo:** toasts que capturan foco; Back que cierra un toast en vez del menú.
- **Recomendación:** solo GameHUD/Menu/Modal son stacks; Toast/Cinematic/Debug son contenedores no interactivos del root layout alimentados por cola.
- **Cambio concreto al plan:** §4.3 — anotar el tipo de contenedor de cada capa.

### H7 — `UI.Toast.*` duplica el pipeline `Message.*` existente

- **Severidad:** Media
- **Evidencia:** `State_GAS.md` §3 + `State_UI.md` §4: ASC → `EffectAssetTags` → OverlayWidgetController filtra `Message.*` → `MessageWidgetDataTable` → toast. §4.2 propone `ShowToast()` sin decir cómo convive con ese pipeline.
- **Riesgo:** dos sistemas de mensajes divergentes.
- **Recomendación:** la capa Toast consume el pipeline `Message.*` existente; `UI.Toast.*` se reserva a mensajes que no nacen de un GameplayEffect (p. ej. "Punto de árbol disponible").
- **Cambio concreto al plan:** §4.3 (Toast) — definir fuentes.

### H8 — Clasificación insuficiente de widgets existentes concretos

- **Severidad:** Media
- **Evidencia:** `State_UI.md`: `WBP_SpellSlot` (§8, "compatibilidad futura verificada"), `M_RadialCooldown` (gotchas documentados), `WBP_BossHealthBar` (funcional, creado desde Level Blueprint con trigger), `WBP_XPBar` (3 bugs resueltos), `PantheliaProgressBar` + ghost bar, `WBP_Button` (sonidos configurables). El plan los cubre genéricamente y §10 Fase 5 lista "Boss HUD" como construcción nueva.
- **Riesgo:** rehacer lo ya validado; perder gotchas caros (AlphaComposite vs Translucent, Retriggerable Delay, orden de hijos en Overlay, "Image invisible bloquea el mouse").
- **Recomendación:** pre-clasificar ya: re-skin conservando lógica para SpellSlot, XPBar, ProgressBar y BossHealthBar; migrar `WBP_AttributeMenu` a activatable conservando su controller. La creación de la boss bar sale del Level Blueprint → manager/capa GameHUD, cerrando de paso el pendiente "ocultar al salir del área / al morir el boss".
- **Cambio concreto al plan:** §1.1 con lista nominal; Fase 0 con checklist de estos 7 assets.

### H9 — Tags `UI.*` sin hogar definido y nombre del root indeciso

- **Severidad:** Media
- **Evidencia:** el proyecto registra tags nativos en `FPantheliaGameplayTags` (`State_GAS.md` §4); el plan introduce 6 jerarquías `UI.*` sin decir si van al singleton C++ o a `DefaultGameplayTags.ini`. §12 propone dos nombres de root (`WBP_UIRoot` vs `WBP_PantheliaRootLayout`).
- **Riesgo:** strings con typos al pedir pantallas (el registry valida, pero los llamadores no tienen compile-safety); renombrado binario costoso después.
- **Recomendación:** capas y acciones (pocos, estables) → nativos en el singleton; pantallas/modales/toasts (muchos, de diseño) → ini + validación del registry. Elegir un único nombre de root ahora.
- **Cambio concreto al plan:** decisión cerrada en §12.

### H10 — `DA_UIRegistry` sin cargador definido

- **Severidad:** Media
- **Evidencia:** §4.6 no dice quién carga el Data Asset ni cuándo; un path hardcodeado desde el subsystem violaría el propio §3.3.
- **Recomendación:** referencia soft al registry desde un `UDeveloperSettings` propio (config PantheliaUI) o colgado del `UCommonUIPolicy` custom. Respuesta a la pregunta 6: **Data Asset para los datos + DeveloperSettings/UIPolicy como punto de montaje**.
- **Cambio concreto al plan:** §4.6 + §4.2 — cadena de carga explícita.

### H11 — `BossEncounterWidgetController` innecesario

- **Severidad:** Baja
- **Evidencia:** la boss bar bindea directamente a `ABossCharacter` (delegates propios + `BroadcastInitialValues`); el boss ES la fuente de verdad y ya es `UObject`.
- **Recomendación:** tacharlo de §4.8; el widget sigue consumiendo al boss vía contrato espejo. Anotar "fuentes heterodoxas se conservan".
- **Cambio concreto al plan:** §4.8.

### H12 — La validación editorial debe extender el sistema existente, no crear uno nuevo

- **Severidad:** Baja
- **Evidencia:** `State_Validation.md`: 7 familias con `IsDataValid` / `PantheliaDataValidationUtils`.
- **Cambio concreto al plan:** §11 → "familias 8 (UIRegistry) y 9 (UITheme) dentro de `PantheliaDataValidationUtils`": unicidad de ScreenTag, capa válida, soft class cargable, fuentes/materiales no nulos.

### H13 — Dependencias CommonUI imprecisas; los glifos deben montar sobre CommonInput

- **Severidad:** Baja
- **Evidencia:** `.uproject` sin CommonUI (confirmado); `Build.cs` sin módulos (solo `UMG`). "Configurar Viewport Client **si lo requiere**" → lo requiere para el root layout por policy: `GameViewportClientClassName=/Script/CommonUI.CommonGameViewportClient` + `UCommonUIPolicy` con RootLayoutClass. §1.2 pide un "sistema de glifos" como construcción propia.
- **Cambio concreto al plan:** Fase 2 con lista exacta: plugin CommonUI (arrastra CommonInput; EnhancedInput ya está activo), módulos `"CommonUI", "CommonInput"` en Build.cs, viewport client, UIPolicy custom, `CommonInputBaseControllerData` para teclado/ratón y mando. §1.2 "sistema de glifos" → "integración CommonInput". Nota: Slate/SlateCore **no** hace falta descomentarlos (llegan por dependencia transitiva) salvo que se escriba Slate crudo.

### H14 — `FUIRequestPayload`/`FUIToastPayload` = tipo maleta prematuro

- **Severidad:** Baja
- **Recomendación:** la API del manager arranca con overloads simples tipados (p. ej. `ShowToast(FText, UTexture2D*)`); los payloads genéricos solo cuando haya 2+ casos reales.
- **Cambio concreto al plan:** mover a §17 (decisiones pospuestas).

### H15 — Pantalla "Personaje" y `CharacterWidgetController` sin dominio

- **Severidad:** Baja
- **Evidencia:** §5.2 lista "Personaje" en el menú de pausa sin spec en §5.x; `CharacterWidgetController` (§4.8) sin datos claros que no pertenezcan ya a otros controllers.
- **Cambio concreto al plan:** definirla (¿stats + equipo read-only?) o quitarla de ambas listas.

### H16 — `UI.State.*` sin consumidor

- **Severidad:** Baja
- **Evidencia:** ninguna sección del plan usa esos tags.
- **Cambio concreto al plan:** quitarlos hasta tener un caso real (el propio §3.3 lo exige: nada sin consumidor).

### H17 — `ArtSource/` sin política de repositorio

- **Severidad:** Baja
- **Evidencia:** §7.5-7.6 ubican la fuente maestra fuera de `/Content` pero no dicen si entra al repo (existe `.gitattributes`; ¿LFS?) o se queda fuera. Afecta al tamaño del repo y a los checkpoints de §9.4.
- **Cambio concreto al plan:** decisión explícita en §7.6.

### H18 — Oportunidad perdida: el manager puede cerrar el pendiente del doble overlay

- **Severidad:** Baja (positiva)
- **Evidencia:** `State_GAS.md` §2 anota el riesgo de doble overlay en el futuro respawn.
- **Recomendación:** el manager garantiza unicidad del overlay; `InitOverlay` consulta al manager antes de crear. Además: los nuevos controllers deben heredar explícitamente los contratos de hardening (`bCallbacksBound` idempotente — confirmado en `PantheliaWidgetController.h` línea 73 —, `AddUObject`/`AddWeakLambda`).
- **Cambio concreto al plan:** §4.2 + criterios de aceptación de Fase 2.

---

## 2. Respuestas a las 20 preguntas de §15

1. **¿La arquitectura conserva correctamente los sistemas actuales?** Mayormente sí. Omisiones en H3 (librería, InitOverlay, UPROPERTYs del HUD) y H8 (widgets concretos).
2. **¿Clases o contratos omitidos?** `UPantheliaAbilitySystemLibrary`, `FUIWidgetRow`/`MessageWidgetDataTable`, `PantheliaProgressBar` (citada solo genéricamente), `WBP_SpellSlot`/`WBP_BossHealthBar`/`WBP_XPBar`/`WBP_Button`, el patrón "boss como controller", y los contratos de hardening que los nuevos controllers deben heredar explícitamente.
3. **¿CommonUI encaja o hay alternativa mejor?** CommonUI es la elección correcta para un soulslike con mando: foco gamepad, Back, capas y cambio de dispositivo mantenidos por Epic (referencia Lyra). Un stack propio reinventaría justo lo más caro. Aprobar con el spike aislado ya previsto.
4. **¿`ULocalPlayerSubsystem` es el propietario correcto?** Sí. CommonUI se organiza por LocalPlayer; es el hogar idiomático. `UGameInstanceSubsystem` valdría en single-player pero es semánticamente peor.
5. **¿La interfaz es necesaria?** No. Contrato espejo (H2).
6. **¿Registry: Data Asset, Developer Settings o ambos?** Ambos: datos en Data Asset, punto de montaje en DeveloperSettings/UIPolicy (H10).
7. **¿Capas suficientes?** Sí, corrigiendo el tipo de contenedor de Toast/Cinematic/Debug (H6).
8. **¿División de Widget Controllers correcta?** Sí, salvo H11 (`BossEncounterWidgetController`, tachar) y H15 (`CharacterWidgetController`, definir o tachar). No dividir `OverlayWidgetController` ahora: rompería delegates validados sin ganancia.
9. **¿Respeta la arquitectura futura de corazones/familias/build?** En principios sí (no `if Heart == X`, UI no decide, `PendingBuild`). En agenda y roles no (H4, H1).
10. **¿Qué pantallas dependen de sistemas inexistentes?** Árbol (resolución por corazón), equipado de hechizos, hoguera (BuildComponent), equipamiento 2 slots, inventario (ítems), cargar/continuar (SaveGame), muerte/respawn (H4).
11. **¿Qué es sobrearquitectura?** La interfaz de §4.5 (H2), los payloads genéricos (H14), `UI.State.*` (H16), `BossEncounterWidgetController` (H11), y `DA_UIStyle_*` antes del vertical slice (posponer).
12. **¿Riesgos no identificados?** Ref-count de pausa en la pila (H5); routing CommonUI ↔ EnhancedInput custom con ataques en Held (H5); reuso/caché de activatables con binds — definir "no cachear instancias en V1"; doble overlay en respawn (H18); re-enlace de controllers al cambiar de mundo (pregunta 15).
13. **¿Cómo integrar la validación de Data Assets?** Extender `PantheliaDataValidationUtils` con las familias 8 (UIRegistry) y 9 (UITheme), siguiendo el patrón de las 7 existentes (H12).
14. **¿Dependencias exactas de UE 5.8 para CommonUI?** Lista en H13. Verificarla contra la documentación de 5.8 dentro del propio spike antes de tocar nada: no dar por cerrado lo que no se haya compilado.
15. **¿Ciclo de vida del Root Layout entre mapas?** El root layout lo crea la UIPolicy en el viewport: **sobrevive** al cambio de mapa. Lo que muere por mundo es HUD/Pawn/PC: el contenido de las capas es por-mundo y los controllers se re-enlazan en cada `InitOverlay` (los params apuntan al PC/PS/ASC/AS nuevos). Regla: root persistente, contenido por mundo, limpieza de capas en el manager al cambiar de mundo.
16. **¿Carga asíncrona de pantallas?** V1: `LoadSynchronous` de la soft class + medición. Async con `FStreamableManager` y precarga (p. ej. SkillTree) solo si hay hitch medido. Nada de loading-UI preventivo.
17. **¿Qué datos usan soft references?** Soft: clases de pantalla en el registry, fondos/ilustraciones del theme, iconos de pantallas concretas. Hard: fuentes base, materiales core, texturas del HUD permanente. Los `TObjectPtr` duros de `DA_AbilityInfo`/`DA_AttributeInfo` se mantienen (son pequeños; coherencia).
18. **¿Qué sistemas no tocar?** `UPantheliaWaitCooldownChange`, `M_RadialCooldown`, `PantheliaProgressBar` + ghost bar, pipeline `Message.*`, delegates del PlayerState, contratos de hardening de controllers, cadena `InitAbilityActorInfo`, `BindAbilityActions`. Del PlayerController solo se retira el bloque del AttributeMenu **al final** de la migración (Fase 5), no en Fase 2.
19. **¿Orden de implementación de menor riesgo?** 0 auditoría → 1 spec → 2 cimientos → **vertical slice = migración del menú de atributos real** (la pantalla completa más pequeña con datos reales; mejor que un "HUD nuevo" abstracto) → re-skin de overlay/boss bar/XP/spell slots (lógica intacta) → pausa → opciones (GameUserSettings real) → muerte placeholder → menú principal → shells bloqueadas tras su gameplay.
20. **¿Contradicciones con los State docs?** H1 (invariante 20), H5 (GameAndUI no fijado como default), H8 (boss HUD tratado como nuevo vs migrable), H4 (árbol asume resolución inexistente). Sin conflicto con `State_Progression.md` ("GAS no se guarda") ni `State_Hardening.md` (delegates): el plan los respeta.

---

## 3. Checklist de cambios para la v1.1 del plan (priorizado)

1. **H1 (crítico):** reescribir §8.3 (C++ solo desde el chat de arquitectura); invariante 20 a §3.
2. **H2 (crítico):** eliminar §4.5 → contrato espejo; tachar la interfaz de §12.
3. **H3:** añadir AbilitySystemLibrary + InitOverlay + UPROPERTYs del HUD a §1.1 / §4.2 / Fases 2 y 5.
4. **H4:** Fase 5 en dos carriles; pantallas bloqueadas marcadas con su dependencia de gameplay en `UI_SCREEN_MAP.md`.
5. **H5:** política GameAndUI por defecto + ref-count de pausa en §4.2 + prueba de input CommonUI↔EnhancedInput en Fase 2.
6. **H6/H7:** tipo de contenedor por capa en §4.3; fuentes del Toast (pipeline `Message.*` + `UI.Toast.*`).
7. **H8:** pre-clasificación nominal de los 7 widgets existentes en §1.1 y checklist en Fase 0.
8. **H9/H10/H13:** hogar de los tags `UI.*`, cargador del registry, lista exacta de dependencias CommonUI (plugins, módulos, viewport client, UIPolicy, CommonInput).
9. **H11/H14/H15/H16:** tachar `BossEncounterWidgetController`, payloads genéricos y `UI.State.*`; definir o eliminar la pantalla "Personaje".
10. **H12/H17/H18:** validación como familias 8-9 de `PantheliaDataValidationUtils`; política de repo para `ArtSource/`; unicidad del overlay como criterio de aceptación de Fase 2.

---

## 4. Petición a la convergencia (§18 del plan)

- **Aceptada sin cambios:** H1, H2, H4, H5, H8 (se consideran los hallazgos de mayor impacto; su corrección es previa a Fase 0 en el caso de H1/H2).
- **Aceptada con discusión:** H3 (la forma exacta de la delegación HUD→manager), H6/H7 (diseño de capas y fuentes de toast), H9/H10 (hogar de tags y montaje del registry), H13 (lista de dependencias, a verificar en el spike).
- **Menores, incorporar directamente:** H11-H18.
- **Sin conflicto previsto con la revisión de Fable** salvo en el vertical slice exacto (pregunta 18 de §16 para Fable vs. respuesta 19 aquí: este informe propone el menú de atributos migrado como vertical slice; si Fable propone otro por criterio de UX, decidir en convergencia con el criterio "datos reales + menor superficie").

**Estado:** revisión Kimi completada. Pendiente: revisión Fable → `UI_REVIEW_CONVERGENCE.md` → plan v1.1.
