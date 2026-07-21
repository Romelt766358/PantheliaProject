# Plan maestro definitivo de UI de Panthelia v1.2

**Proyecto:** PantheliaProject  
**Motor:** Unreal Engine 5.8  
**Modalidad:** Single-player  
**Tecnología:** C++ + Gameplay Ability System + UMG + CommonUI/CommonInput  
**Fecha:** 17 de julio de 2026  
**Estado:** APROBADO PARA EJECUCIÓN  
**Sustituye:** Plan inicial y Plan Maestro v1.1  
**Base de convergencia:** revisión técnica de Kimi K3 + revisión técnica, UX y artística de Fable

---

# 0. Propósito y autoridad del documento

Este documento define el plan que se seguirá para reconstruir la interfaz completa de Panthelia.

No es un documento de ideas ni una solicitud de revisión. Sus decisiones se consideran aprobadas salvo que:

1. Una auditoría del proyecto demuestre que una premisa es falsa.
2. Unreal Engine 5.8 impida técnicamente una solución.
3. Un sistema de gameplay futuro cambie su contrato.
4. El director del proyecto apruebe explícitamente una modificación.

Cuando aparezca uno de esos casos, el cambio debe registrarse antes de implementarse.

Este plan gobierna:

- Arquitectura técnica.
- Navegación.
- Integración con GAS.
- HUD.
- Menús.
- Hoguera.
- Árbol de habilidades.
- Producción de assets mediante IA.
- Dirección artística.
- Input y mando.
- Accesibilidad.
- División del trabajo entre agentes.
- Migración de sistemas legacy.
- Validación.
- Fases de implementación.

---

# 1. Objetivo final

Panthelia tendrá una UI completa, modular, escalable y coherente con un soulslike single-player.

La UI final debe:

- Mantener una presentación sobria durante combate.
- Ser ceremonial y rica en hoguera, árbol y pantallas de progresión.
- Funcionar con teclado, ratón y mando.
- Adaptarse a 1080p, 1440p, 4K, 16:9 y 21:9.
- Observar los sistemas de gameplay sin duplicar su estado.
- Integrarse con GAS mediante los Widget Controllers existentes.
- Permitir que pantallas futuras se incorporen sin ampliar continuamente el PlayerController o el HUD.
- Usar assets visuales originales producidos mediante un pipeline controlado de IA.
- Mantener todo texto localizable.
- Ser compatible con las futuras arquitecturas de corazones, dos armas, hechizos y build de hoguera.

---

# 2. Decisiones definitivas de convergencia

Las siguientes decisiones quedan cerradas.

## 2.1 CommonUI

Se usará CommonUI para:

- Pantallas completas.
- Stacks.
- Activación y desactivación.
- Acción Back.
- Foco.
- Navegación con mando.
- Capas.
- Cambio de dispositivo.
- Glifos mediante CommonInput.

UMG normal se mantiene para:

- Componentes internos.
- Barras.
- Slots.
- Tooltips.
- HUD.
- Widgets dentro del mundo.
- Lock-on mediante Widget Component.
- Piezas que no necesitan lifecycle de activatable.

## 2.2 Propietario central

Se creará:

```cpp
UPantheliaUIManagerSubsystem : public ULocalPlayerSubsystem
```

Se elige `ULocalPlayerSubsystem` porque:

- CommonUI organiza sus layouts por Local Player.
- La UI pertenece conceptualmente al jugador local.
- Evita resolver manualmente qué jugador posee la interfaz.
- Es el punto idiomático para navegación y layouts por jugador.

La persistencia exacta del Root Layout durante cambios de mapa se verificará en el spike técnico antes de convertirse en contrato de runtime.

## 2.3 Widget Controllers

Se conserva el patrón:

```text
Gameplay / GAS
→ delegates
→ Widget Controller
→ Widget
```

No se migrará globalmente a MVVM.

No se creará una UInterface para compartir el contrato de Widget Controller.

Se usará un contrato espejo:

```text
UPantheliaUserWidget
UPantheliaActivatableWidget
```

Ambas clases expondrán:

```cpp
SetWidgetController(UObject*)
WidgetControllerSet()
```

Se añadirá en ambas un comentario recíproco obligatorio indicando que el contrato debe mantenerse sincronizado.

No se creará un helper estático mientras la implementación compartida siga siendo trivial. Se extraerá una utilidad propia de UI solo si la lógica común crece.

## 2.4 Escritura de C++

El C++ estructural de Panthelia se diseña y entrega desde el chat de arquitectura.

Codex/MCP no escribe el C++ del sistema.

Flujo:

```text
Chat de arquitectura
→ entrega .h/.cpp completos

Usuario
→ reemplaza archivos y compila

Codex/MCP
→ opera assets ya compilados dentro de Unreal Editor
```

## 2.5 Propiedad de apertura y navegación

El PlayerController conserva los bindings de Enhanced Input que conectan gameplay con UI:

- Abrir pausa.
- Abrir atributos.
- Abrir otra pantalla directa si se aprueba.

El cuerpo de esos handlers se reduce a delegar en el manager:

```text
Input Action
→ PlayerController
→ UIManager.PushScreen(ScreenTag)
```

El PlayerController no:

- Crea widgets.
- Añade widgets al viewport.
- Pausa.
- Despausa.
- Cambia cursor.
- Cambia Input Mode.
- Destruye pantallas.

CommonUI administra la navegación interna:

- Back.
- Confirm.
- Tabs.
- Foco.
- Cambio entre pantallas.

## 2.6 Pausa como decisión de diseño

Los menús de gameplay pausan deliberadamente.

Esta es una decisión de identidad para Panthelia, no un efecto accidental.

Consecuencia obligatoria:

> Ningún consumible de combate puede usarse desde una pantalla que pausa.

Los consumibles de emergencia vivirán en quick slots o inputs de gameplay.

## 2.7 Pausa y hoguera

La UI tendrá dos espacios funcionales distintos.

### Pausa: consulta

Desde pausa se podrá:

- Consultar atributos.
- Consultar árbol.
- Consultar hechizos.
- Consultar inventario.
- Consultar equipamiento.
- Revisar controles.
- Cambiar opciones.
- Salir o volver al menú.

Por defecto, no se realizarán cambios permanentes del build desde pausa.

### Hoguera: transformación

En hoguera se podrá:

- Subir atributos o nivel.
- Gastar puntos del árbol.
- Cambiar corazón.
- Cambiar armas.
- Cambiar hechizos.
- Cambiar armadura.
- Hacer respec.
- Aplicar el build.

Esta regla puede modificarse únicamente mediante una decisión de gameplay documentada.

## 2.8 Profundidad de navegación

La profundidad máxima será tres niveles funcionales:

```text
Gameplay o Hoguera
→ Pantalla o sección
→ Modal
```

No se crearán flujos de profundidad cuatro.

Los selectores secundarios se resolverán mediante:

- Panel interno.
- Drawer.
- Selector dentro de la pantalla.
- Modal cuando sea confirmación.

## 2.9 Nombre canónico de controller de hechizos

Nombre canónico:

```text
SpellLoadoutWidgetController
```

Los comentarios actuales que mencionan `SpellMenuWidgetController` se actualizarán cuando esos archivos se modifiquen por una entrega funcional.

No se hará un cambio de C++ aislado únicamente para renombrar comentarios.

## 2.10 Unicidad de pantallas

Regla global:

> Solo puede existir una instancia activa por `ScreenTag`.

No se añade `bAllowDuplicate` al registry.

## 2.11 Toasts

El `OverlayWidgetController` seguirá siendo el único traductor:

```text
Message Gameplay Tag
→ MessageWidgetRow
```

Habrá exactamente un consumidor visual activo de `MessageWidgetRowDelegate`.

Cuando el nuevo sistema Toast se conecte:

- Se conecta el nuevo consumidor.
- Se desconecta el widget legacy en el mismo cambio.
- No se permiten mensajes duplicados.

Prioridad inicial de mensajes:

```text
Immediate
Deferred
```

### Immediate

- Objeto clave.
- Error crítico.
- Tutorial obligatorio.
- Mensaje imprescindible para la acción actual.

### Deferred

- Área descubierta.
- Punto disponible.
- Información no urgente.
- Mensajes que distraigan durante boss.

Durante combate intenso o boss, los diferibles esperan en cola.

## 2.12 Vertical slices

Vertical slice técnico y funcional:

```text
Pausa + Atributos + Modal + Toast + Tooltip
```

Razones:

- Datos reales.
- Controller real.
- Operación transaccional real.
- Navegación completa.
- Foco, input y pausa comprobables.
- Superficie contenida.

Vertical slice artístico:

```text
Árbol de habilidades en wireframe de alta fidelidad
```

Este segundo slice valida el máximo ceremonial de la dirección visual sin inventar gameplay funcional.

## 2.13 Árbol compartido

La geometría del árbol no cambia con el corazón.

El corazón funciona como lente:

- Misma posición de nodos.
- Mismas conexiones.
- Mismos rangos invertidos.
- Cambian materiales.
- Cambian patrones.
- Cambian gemas.
- Cambia la interpretación elemental.
- Cambia la descripción.

Esto comunica que existe un árbol compartido, no cuatro árboles.

## 2.14 Perfect Dodge y Perfect Parry

Feedback por defecto:

- VFX en mundo.
- SFX distintivo.
- Feedback de combate.
- Sin texto arcade en HUD.

Accesibilidad:

- Indicador simbólico o textual opcional.
- El feedback no dependerá solo del color ni solo del sonido.

## 2.15 Dirección artística

Nombre provisional aprobado:

> Liturgia Elemental

Identidad:

- Antigua.
- Ritual.
- Sobria.
- Precisa.
- Mística.
- Elemental.
- Original.
- No copia de FromSoftware.

El sistema iconográfico elemental será propio y geométrico.

No se aceptarán iconos genéricos de:

- Llama.
- Gota.
- Rayo.
- Hoja.

como identidad principal sin reinterpretación mediante los sellos litúrgicos de Panthelia.

## 2.16 Documentación

La Fase 1 se concentra en tres documentos:

```text
UI_MASTER_PLAN.md
UI_ART_BIBLE.md
UI_INPUT_SPEC.md
```

Los documentos se subdividen únicamente si su tamaño se vuelve inmanejable.

El manifiesto de assets comenzará como sección del Art Bible y podrá separarse durante la fábrica de assets.

---

# 3. Invariantes técnicas

## 3.1 La UI no decide gameplay

La UI:

- Observa.
- Presenta.
- Solicita.
- Confirma.
- Muestra errores.

La UI no:

- Otorga abilities.
- Aplica Gameplay Effects.
- Descuenta puntos directamente.
- Equipa directamente.
- Cambia el corazón directamente.
- Modifica `NodeRanks`.
- Reconstruye el build.
- Valida reglas autoritativas.

## 3.2 Fuentes de verdad

Fuentes de verdad válidas:

- GAS.
- PlayerState.
- AttributeSet.
- SkillTreeComponent.
- EquipmentComponent.
- Build/LoadoutComponent futuro.
- SaveGame futuro.
- Sistemas de combate.
- Boss.
- GameUserSettings.
- Componentes propietarios de cada dominio.

## 3.3 Data-driven

Se usarán:

- Data Assets.
- Developer Settings.
- Gameplay Tags.
- Material Instances.
- String Tables.
- Data Tables existentes.
- Soft references donde aporten valor.

No se hardcodearán clases de pantalla en gameplay.

## 3.4 Event-driven

Evitar:

- Tick de widgets sin necesidad.
- Property Bindings por frame.
- Casts repetidos.
- Polling.
- Reconstrucciones completas.
- Lectura constante del ASC desde widgets.

Usar:

- Delegates.
- Event Dispatchers.
- Controllers.
- ListView/TileView.
- Actualizaciones localizadas.
- Invalidación medida.

## 3.5 Hardening

Los nuevos Widget Controllers deben mantener los contratos existentes:

- Bindings idempotentes.
- `bCallbacksBound` o equivalente.
- `AddUObject`.
- `AddWeakLambda` cuando corresponda.
- Cleanup.
- Sin callbacks tardíos.
- Sin referencias circulares.
- Fallar cerrado.

## 3.6 Single-player

No se implementan:

- RPCs.
- Replicación de UI.
- Navegación de lobby.
- Sincronización multiplayer.
- Arquitecturas innecesarias para red.

## 3.7 Assets binarios

Solo un agente modifica un `.uasset` o `.umap` a la vez.

No se realizan merges binarios como flujo normal.

---

# 4. Sistemas actuales que se preservan

## 4.1 Flujo de inicialización

Se conserva el punto de entrada:

```text
MainCharacter::InitAbilityActorInfo
→ APantheliaHUD::InitOverlay
```

La integración futura será:

```text
InitOverlay
→ HUD crea u obtiene controller
→ HUD obtiene widget
→ UI Manager publica el widget en UI.Layer.GameHUD
```

El HUD mantiene la creación y parametrización de Widget Controllers mientras no exista una razón arquitectónica demostrada para moverla.

El manager administra:

- Capas.
- Presentación.
- Navegación.
- Unicidad.
- Input.
- Pausa.
- Foco.

## 4.2 Clases y contratos preservados

- `UPantheliaUserWidget`.
- `UPantheliaWidgetController`.
- `FWidgetControllerParams`.
- `UPantheliaAbilitySystemLibrary`.
- `APantheliaHUD::InitOverlay`.
- `UOverlayWidgetController`.
- `UAttributeMenuWidgetController`.
- Delegates del PlayerState.
- Delegates del AttributeSet.
- Pipeline de Ability Info.
- Pipeline `Message.*`.
- Async tasks actuales.
- Hardening actual.

## 4.3 Assets preservados nominalmente

Clasificación inicial:

> Conservar lógica, auditar y hacer reskin o migración controlada.

- `PantheliaProgressBar`.
- Ghost bar.
- `WBP_XPBar`.
- `WBP_SpellSlot`.
- `M_RadialCooldown`.
- `WBP_BossHealthBar`.
- `WBP_Button`.
- `WBP_AttributeMenu`.

No se reconstruyen desde cero salvo evidencia documentada.

## 4.4 Guard de overlay existente

El guard existente contra overlay duplicado se preserva.

El spike comprobará que sigue funcionando cuando el overlay se publique en el Root Layout en vez de depender únicamente de `AddToViewport`.

Pruebas:

- Reconstrucción del Pawn.
- Respawn futuro.
- Cambio de mapa.
- PIE repetido.
- Cambio de PlayerController.

---

# 5. Arquitectura definitiva

## 5.1 Flujo general

```text
Gameplay / GAS / PlayerState / Components
                ↓
Widget Controllers o fuente válida
                ↓
Widgets UMG / CommonUI
                ↓
UPantheliaUIManagerSubsystem
                ↓
WBP_PantheliaRootLayout
                ↓
Capas
```

## 5.2 `UPantheliaUIManagerSubsystem`

Responsabilidades:

- Registrar o localizar Root Layout.
- Abrir pantalla.
- Cerrar pantalla.
- Hacer pop.
- Gestionar Back.
- Gestionar modales.
- Gestionar cola de Toast.
- Resolver clases desde registry.
- Evitar duplicados.
- Calcular pausa efectiva.
- Calcular Input Mode efectivo.
- Calcular cursor efectivo.
- Calcular visibilidad del HUD.
- Restaurar foco.
- Limpiar contenido por mundo.
- Reaccionar a dispositivo activo.
- Garantizar un overlay publicado.
- Aplicar tema visual.

No debe:

- Leer atributos.
- Gastar puntos.
- Equipar.
- Aplicar GEs.
- Resolver nodos.
- Contener reglas de gameplay.
- Ser un Service Locator general.

## 5.3 API inicial

```cpp
bool PushScreen(FGameplayTag ScreenTag);
bool PopScreen();
bool CloseScreen(FGameplayTag ScreenTag);
bool IsScreenOpen(FGameplayTag ScreenTag) const;
```

No se crea un payload genérico inicial.

Los parámetros específicos se añaden cuando existan al menos dos casos reales que justifiquen una abstracción común.

## 5.4 `UPantheliaActivatableWidget`

Herencia:

```text
UCommonActivatableWidget
└── UPantheliaActivatableWidget
```

Responsabilidades:

- Activación.
- Desactivación.
- Back.
- Foco inicial.
- Sonido.
- Animación.
- Contrato de Widget Controller.
- Cleanup.
- Declarar sus políticas.
- Restauración de foco.

## 5.5 Root Layout

Nombre definitivo:

```text
WBP_PantheliaRootLayout
```

Una instancia por Local Player.

## 5.6 Capas

### `UI.Layer.GameHUD`

Tipo:

- Se decide en el spike entre contenedor permanente o stack con root permanente.

Contenido:

- HUD.
- Boss bar.
- Lock-on 2D.
- Prompts.
- Feedback de combate.

### `UI.Layer.Menu`

Tipo:

```text
UCommonActivatableWidgetStack
```

Contenido:

- Pausa.
- Atributos.
- Árbol.
- Hechizos.
- Inventario.
- Equipamiento.
- Hoguera.
- Opciones.
- Referencia de controles.

### `UI.Layer.Modal`

Tipo:

```text
UCommonActivatableWidgetStack
```

Contenido:

- Confirmaciones.
- Errores.
- Advertencias.
- Descartar cambios.
- Respec.
- Salir.

### `UI.Layer.Toast`

Tipo:

- Contenedor no activatable.
- Cola.
- No roba foco.
- No participa en Back.

### `UI.Layer.Cinematic`

Tipo:

- Contenedor no interactivo.

Contenido:

- Subtítulos.
- Letterbox.
- Transiciones.
- Prompts cinematográficos.

### `UI.Layer.Debug`

Tipo:

- Contenedor Development/Editor.

V1:

- Visualizador de foco.
- Visualizador de pila de pantallas.

Se amplía solo bajo demanda.

## 5.7 Registry

Data Asset:

```text
DA_UIRegistry
```

Entrada conceptual:

```cpp
USTRUCT(BlueprintType)
struct FPantheliaUIScreenEntry
{
    GENERATED_BODY()

    FGameplayTag ScreenTag;
    TSoftClassPtr<UCommonActivatableWidget> WidgetClass;
    FGameplayTag LayerTag;
    EPantheliaUIPausePolicy PausePolicy;
    EPantheliaUIInputPolicy InputPolicy;
    EPantheliaHUDVisibilityPolicy HUDVisibilityPolicy;
};
```

No incluye `bAllowDuplicate`.

## 5.8 Punto de montaje

```text
Project Settings
→ UPantheliaUISettings
    → TSoftObjectPtr<UPantheliaUIRegistry>
        → DA_UIRegistry
            → Soft Classes
```

Durante el spike se evaluará si la UI Policy debe alojar o consultar esa referencia.

No se usa un path hardcodeado.

## 5.9 Tags

### Nativos

```text
UI.Layer.GameHUD
UI.Layer.Menu
UI.Layer.Modal
UI.Layer.Toast
UI.Layer.Cinematic
UI.Layer.Debug
UI.Action.Back
UI.Action.Confirm
```

### Data-driven

```text
UI.Screen.*
UI.Modal.*
UI.Toast.*
```

No se crea `UI.State.*` sin consumidor.

## 5.10 Controllers de dominio

Existentes:

```text
OverlayWidgetController
AttributeMenuWidgetController
```

Futuros, solo cuando exista su dominio:

```text
SkillTreeWidgetController
SpellLoadoutWidgetController
InventoryWidgetController
EquipmentWidgetController
BonfireWidgetController
SettingsWidgetController
MainMenuWidgetController
```

No se crea:

```text
BossEncounterWidgetController
CharacterWidgetController
```

La boss bar puede seguir consumiendo directamente al boss como fuente.

---

# 6. Política de pausa, input, cursor, foco y HUD

## 6.1 Único propietario

Solo el UI Manager puede modificar:

- `SetPause`.
- Cursor.
- Input Mode.
- Foco.
- Visibilidad del HUD.
- Restauración de estado.

## 6.2 Política declarada por pantalla

Cada pantalla declara:

- Requiere pausa.
- Política de input.
- Cursor.
- Foco inicial.
- Visibilidad del HUD.

## 6.3 Visibilidad del HUD

Enum conceptual:

```text
Visible
Dimmed
Hidden
```

El manager calcula la política efectiva usando la pantalla más restrictiva de la pila.

Ejemplos:

```text
Pausa raíz
→ Dimmed

Atributos
→ Hidden o Dimmed según wireframe final

Modal
→ Hereda pantalla inferior

Hoguera
→ Hidden

Opciones completas
→ Hidden
```

## 6.4 Política inicial

Pantallas durante gameplay:

```text
GameAndUI + pausa
```

Menú principal:

```text
UIOnly o equivalente validado
```

Modales:

- Heredan pausa.
- Bloquean pantalla inferior.
- No despausan al cerrarse si permanece una pantalla pausada.

## 6.5 `bTriggerWhenPaused`

La acción que cierre o alterne una pantalla pausada debe funcionar mientras el juego está pausado.

El spike debe verificar:

- Toggle con la misma tecla.
- Back.
- `bTriggerWhenPaused`.
- Animaciones UMG.
- Foco.
- Cierre de modal.
- Retorno a pantalla anterior.

## 6.6 Input Held

Prueba obligatoria:

```text
Mantener ataque
→ abrir menú
→ soltar ataque
→ cerrar menú
→ la ability no se activa
→ no queda input bufferizado
```

Repetir con:

- Guardia.
- Lock-on.
- Movimiento.
- Input de hechizo.

## 6.7 Restauración

Al hacer pop:

- Restaurar foco anterior.
- Recalcular pausa.
- Recalcular cursor.
- Recalcular Input Mode.
- Recalcular HUD.
- No asumir retorno a gameplay.

---

# 7. Dirección funcional de pantallas

# 7.1 HUD permanente

Elementos permanentes:

- Health.
- Mana.
- Stamina.
- Espacio reservado en composición para quick slots futuros.
- Recurso de progresión acumulado, discreto, si el diseño final lo requiere.

## 7.2 HUD contextual

- Poise del jugador.
- Buildups.
- Arma activa.
- Segundo slot de arma.
- Boss bar.
- Lock-on.
- Prompts.
- Toasts.
- Área descubierta.
- Tutoriales.
- Feedback de interacción.

No mostrar permanentemente:

- Defense Shred.
- Los cuatro buildup vacíos.
- Puntos disponibles.
- Información de cálculo interno.

## 7.3 Buildups

Solo existen visualmente cuando:

```text
Buildup > 0
```

Diferenciación obligatoria:

- Icono.
- Color.
- Forma.
- Patrón.

Al activarse el estado:

- La barra puede transformarse en icono de estado activo.
- Mostrar duración solo si es legible y útil.

## 7.4 Dos armas

No usar dos slots simétricos que parezcan mano izquierda y derecha.

Direcciones a comparar en wireframe:

1. Cartuchos superpuestos.
2. Selector vertical.
3. Slot principal con indicador secundario.
4. Transición animada durante cambio.

La decisión visual final se toma tras probar legibilidad.

## 7.5 Quick slots

El sistema todavía no existe.

El HUD debe reservar espacio conceptual para evitar un rediseño total.

No se implementan quick slots falsos.

## 7.6 Boss HUD

Mostrar:

- Nombre.
- Vida.
- Estado elemental relevante.
- Postura solo si el jugador puede interpretarla y aprovecharla.
- Intro.
- Salida.

Ocultar:

- Al morir.
- Al abandonar el área.
- Al terminar el encuentro.

Durante boss:

- Diferir toasts no urgentes.
- Ocultar tutoriales no críticos.
- Ocultar área descubierta.
- Mantener recursos, buildup y quick slots.

## 7.7 Pausa

Pantallas de consulta:

- Atributos.
- Árbol.
- Hechizos.
- Inventario.
- Equipamiento.
- Opciones.
- Referencia de controles.
- Volver al menú.
- Salir.

No realizar cambios permanentes del build por defecto.

## 7.8 Atributos

Conservar:

- Controller.
- Data Asset.
- Delegates.
- Puntos.
- Operación transaccional actual durante la migración.

Objetivo final:

- Consulta en pausa.
- Gasto permanente en hoguera.

La transición de la operación actual a la regla de hoguera será una fase de gameplay documentada, no un cambio visual accidental.

## 7.9 Árbol

Estado:

> Bloqueado para implementación funcional final hasta existir resolución completa por corazón.

Se puede desarrollar:

- Wireframe.
- Art direction.
- Navegación.
- Estados visuales.
- Contrato de datos.
- Tooltip.
- Vista read-only.

### Geometría

No cambia al cambiar corazón.

### Nodos universales

- Plata envejecida.
- Marfil.
- Sin gema elemental.

### Nodos elementales

- Gema o sello del corazón activo.
- Patrón y material elemental.

### Tooltip

Orden:

1. Nombre.
2. Rango actual/máximo.
3. Efecto universal actual.
4. Efecto con corazón activo.
5. Delta del siguiente rango.
6. Coste.
7. Requisitos no cumplidos.

No mostrar los cuatro corazones simultáneamente por defecto.

## 7.10 Equipado de hechizos

Estado:

> Bloqueado hasta existir contrato final.

Debe separar:

- Desbloqueo.
- Equipado.
- Variante elemental.
- Familia.
- Input.
- Coste.
- Cooldown.

## 7.11 Inventario

Estado:

> Bloqueado hasta cerrar el modelo de ítems.

Solo:

- Wireframe.
- Contrato.
- Arte.
- Navegación read-only si existe fuente real.

## 7.12 Equipamiento

Estado:

> Bloqueado hasta existir dos slots lógicos y sistema final de armadura.

No asumir manos.

## 7.13 Hoguera

Estado funcional:

> Bloqueado hasta existir Build/LoadoutComponent.

Contrato aprobado:

```text
AppliedBuild
PendingBuild
```

Flujo:

1. Abrir hoguera.
2. Copiar AppliedBuild.
3. Editar PendingBuild.
4. Validar.
5. Confirmar.
6. Reconstruir runtime.
7. Publicar.
8. Cancelar descarta todo.

### Presentación

No mostrar nombres técnicos.

```text
PendingBuild
→ Cambios sin consagrar

Apply
→ Consagrar
```

“Consagrar” es provisional hasta cerrar el vocabulario del lore.

### Confirmaciones

Confirmar:

- Aplicar el conjunto.
- Respec.
- Salir con cambios pendientes.
- Descartar.

No confirmar cada cambio individual.

### Estructura

Hub con secciones:

- Nivel/Atributos.
- Corazón.
- Armas.
- Hechizos.
- Armadura.
- Árbol.
- Respec.

La entrada muestra resumen del build.

## 7.14 Opciones

Implementar únicamente funciones reales y persistentes.

- Video.
- Audio.
- Gameplay.
- Controles.
- Accesibilidad.

No mostrar opciones ficticias.

## 7.15 Referencia de controles

Pantalla obligatoria:

- Accesible desde pausa y opciones.
- Lee bindings reales.
- Se adapta al dispositivo.
- Compatible con remapeo futuro.

## 7.16 Menú principal

- Nueva partida.
- Continuar, cuando SaveGame exista.
- Cargar, cuando SaveGame exista.
- Opciones.
- Créditos al final del proyecto.
- Salir.

## 7.17 Muerte

Puede existir placeholder visual.

Respawn completo queda bloqueado al sistema real.

## 7.18 Carga

No mostrar porcentajes inventados.

## 7.19 Créditos

Se posponen al final.

Incluir:

- Fuentes.
- Assets.
- Audio.
- Licencias.
- Herramientas.
- Atribuciones requeridas.

---

# 8. Dirección artística: Liturgia Elemental

## 8.1 Base material

- Obsidiana.
- Plata envejecida.
- Marfil.
- Pergamino oscuro.
- Grabados.
- Reliquias.
- Astrolabios.
- Manuscritos.
- Vidrio de reliquia.

## 8.2 Restricción del vidrio

Permitido:

- Grueso.
- Imperfecto.
- Con inclusiones.
- Gema.
- Cápsula.
- Reliquia.

Prohibido:

- Glassmorphism.
- Paneles transparentes modernos.
- Cristal limpio como superficie principal.

## 8.3 Jerarquía

### Gameplay

- Sobrio.
- Poco ornamento.
- Alta legibilidad.
- Animaciones breves.
- Información contextual.

### Menús

- Mayor riqueza.
- Espacio negativo.
- Composición ceremonial.
- Navegación clara.

### Árbol y hoguera

- Máximo ceremonial.
- Flujos de energía.
- Sellos.
- Grabados.
- Capas rituales.

## 8.4 Sistema iconográfico

Crear cuatro sellos elementales geométricos originales.

Todos los iconos deben derivar del mismo lenguaje:

- Grosor.
- Ángulos.
- Curvatura.
- Simetría.
- Ritmo.
- Ornamentación.

HUD y menús compartirán:

- Tipografía.
- Iconografía.
- Proporciones.
- Lenguaje de materiales.

## 8.5 Elementos

### Fuego

- Grietas.
- Ceniza.
- Incandescencia.
- Calor.

### Agua

- Refracción.
- Ondas.
- Condensación.
- Cristal de reliquia.

### Tormenta

- Filamentos.
- Cortes.
- Chispas.
- Vibración.

### Naturaleza

- Raíces.
- Vetas.
- Verdín.
- Crecimiento.

No depender únicamente del color.

## 8.6 Tipografía

- Fuentes reales.
- Licencia verificada.
- Soporte español.
- Preparada para inglés.
- Sin texto dentro de imágenes.
- Jerarquías semánticas.

## 8.7 Animación

- Corta.
- Informativa.
- Sobria.
- Sin rebotes modernos.
- Sin escalados excesivos.
- Compatible con reducción de movimiento.
- Sin parpadeos superiores a 3 Hz.

---

# 9. Assets mediante IA

## 9.1 Uso

IA para:

- Concept boards.
- Iconos.
- Marcos.
- Paneles.
- Fondos.
- Máscaras.
- Patrones.
- Emblemas.
- Variaciones.

## 9.2 Prohibido

- Generar una pantalla final completa para recortarla.
- Texto.
- Pseudotexto.
- Letras ornamentales falsas.
- Iconos copiados.
- Assets sin manifest.
- Paletas aproximadas fuera del Art Bible.
- Familias inconsistentes.

## 9.3 Kit modular

Masters:

```text
Panel
Slot
Botón
Tooltip
Separador
Corner caps
Anillo
Máscaras
Patrones
Fondos
Cursor
Retícula
Sellos elementales
```

## 9.4 Reglas de producción

Cada asset:

- PNG transparente cuando corresponda.
- Fuente maestra en alta resolución.
- Padding consistente.
- Centro óptico consistente.
- Bordes limpios.
- Sin halos.
- Nombre estable.
- Prompt registrado.
- Modelo registrado.
- Fecha.
- Edición manual.
- Licencia.
- Ruta fuente.
- Ruta Unreal.
- Prueba a tamaño real mínimo en 1080p.
- Validación contra sello litúrgico canónico.
- Verificación de paleta exacta.

## 9.5 Estructura

Fuera de Unreal:

```text
ArtSource/UI/
├── Concepts/
├── Masters/
├── Icons/
├── Panels/
├── Backgrounds/
├── Masks/
├── Fonts/
├── Licenses/
└── Manifests/
```

Dentro de Unreal:

```text
/Game/UI/
├── Core/
├── Common/
├── HUD/
├── Screens/
├── Components/
├── Styles/
├── Materials/
├── Textures/
├── Icons/
├── Audio/
├── Data/
└── Debug/
```

La estructura real se ajustará después de la auditoría.

## 9.6 Repositorio

- Licencias y manifests siempre versionados.
- Masters razonables dentro de `ArtSource/UI`.
- Assets pesados mediante Git LFS si se aprueba.
- Export final optimizado dentro de `/Game/UI`.
- No guardar archivos temporales innecesarios.

---

# 10. División del trabajo

## 10.1 Chat de arquitectura

Responsable de:

- Diseño técnico.
- C++.
- Archivos completos.
- Explicación.
- Corrección de compilación.
- Mantenimiento de comentarios existentes.

## 10.2 Codex/MCP

Responsable de:

- Plugins.
- Project Settings.
- WBP.
- Data Assets.
- Materiales.
- Referencias.
- Foco.
- Navegación.
- Pruebas.
- Capturas.
- Dumps.
- Guardado.

## 10.3 Fable

Responsable de:

- Revisión arquitectónica.
- UX.
- Dirección artística.
- Coherencia.
- Revisión de implementación.
- Detección de deuda.
- Revisión de contratos.
- Revisión de documentación.

## 10.4 Kimi K3

Responsable de:

- Contexto largo.
- Inventarios.
- Dependencias.
- Nomenclatura.
- Coherencia global.
- Data-driven.
- Revisión documental.

## 10.5 Grok 4.5

Responsable de:

- Revisión adversarial.
- Edge cases.
- Lifetime.
- Foco.
- Input.
- Rendimiento.
- Accesibilidad.
- Simplificación.

## 10.6 Regla operativa

Un solo owner por asset binario.

Crear:

```text
Documentation/UI/UI_ASSET_OWNERSHIP.md
```

Campos:

```text
Asset
Owner
Fase
Estado
Bloqueado
Checkpoint
```

---

# 11. Documentación operativa

## 11.1 Documentos base

```text
Documentation/UI/UI_MASTER_PLAN.md
Documentation/UI/UI_ART_BIBLE.md
Documentation/UI/UI_INPUT_SPEC.md
```

## 11.2 Informes

```text
Documentation/UI/Reviews/
├── UI_REVIEW_KIMI.md
├── UI_REVIEW_FABLE.md
├── UI_REVIEW_CONVERGENCE.md
└── UI_REVIEW_GROK.md
```

## 11.3 Checkpoint por entrega

Antes:

1. Commit.
2. Objetivo.
3. Archivos autorizados.
4. Assets autorizados.
5. Owner.
6. Criterios.

Después:

1. Compilar.
2. Guardar.
3. Reabrir.
4. Probar.
5. Capturar.
6. Documentar.
7. Commit.

---

# 12. Plan de ejecución

# Fase 0 — Auditoría de solo lectura

## Objetivo

Conocer el estado real sin modificar nada.

## Auditar

- `/Game/UI`.
- Widgets.
- Widget Components.
- Materiales.
- Texturas.
- Sonidos.
- Animaciones.
- Referencias.
- Widget Trees.
- Event Graphs.
- Property Bindings.
- Tick.
- Casts.
- Lógica de gameplay.
- Duplicados.
- Huérfanos.
- Anchors.
- Resoluciones.
- Foco.
- Navegación.
- Input.
- Pausa.
- PlayerController.
- HUD.
- AbilitySystemLibrary.
- `InitOverlay`.
- `Message.*`.

## Assets nominales

- `PantheliaProgressBar`.
- Ghost bar.
- `WBP_XPBar`.
- `WBP_SpellSlot`.
- `M_RadialCooldown`.
- `WBP_BossHealthBar`.
- `WBP_Button`.
- `WBP_AttributeMenu`.

## Clasificación

- Conservar.
- Conservar lógica y rehacer visual.
- Migrar.
- Sustituir.
- Deprecar.
- Eliminar después.
- Bloqueado.
- No determinado.

## Entregables

```text
UI_LEGACY_AUDIT.md
UI_ASSET_INVENTORY.md
UI_REFERENCE_GRAPH.md
UI_SCREENSHOT_INDEX.md
UI_TECHNICAL_DEBT.md
```

## Prohibido

- Guardar cambios.
- Borrar.
- Reparentar.
- Activar plugins.
- Modificar C++.
- Crear carpetas finales.

## Criterio de salida

Ninguna pieza conocida queda sin clasificar.

---

# Fase 1 — Especificación y dirección visual

## Objetivo

Cerrar la solución antes de construirla.

## Documentos

### `UI_MASTER_PLAN.md`

Incluye:

- Arquitectura.
- Pantallas.
- Flujos.
- Fuente de verdad.
- Dependencias.
- Estado implementable o bloqueado.
- Controller.
- Capa.
- Pausa.
- Input.
- Visibilidad HUD.

### `UI_ART_BIBLE.md`

Incluye:

- Liturgia Elemental.
- Paleta exacta.
- Tipografía.
- Sistema iconográfico.
- Sellos.
- Materiales.
- Reglas IA.
- Naming.
- Manifest inicial.

### `UI_INPUT_SPEC.md`

Incluye:

- Bindings de apertura.
- CommonUI.
- CommonInput.
- Foco.
- Back.
- Confirm.
- Tabs.
- Pausa.
- Cursor.
- Accesibilidad.
- Tests Held.
- `bTriggerWhenPaused`.

## Concept boards

1. Liturgia Elemental.
2. Relicario Astral.
3. Manuscrito Alquímico.

Liturgia Elemental es la base aprobada, pero las otras dos sirven como contraste para cerrar decisiones visuales.

## Wireframes

- HUD.
- Pausa.
- Atributos.
- Árbol.
- Toast.
- Modal.
- Tooltip.
- Hoguera.
- Opciones.
- Referencia de controles.

## Criterio de salida

- Arquitectura visual aprobada.
- Paleta aprobada.
- Tipografía aprobada.
- Sellos aprobados.
- Vertical slice aprobado.
- No implementación funcional todavía.

---

# Fase 2 — Spike técnico CommonUI

## C++ desde chat de arquitectura

- Dependencias.
- `UPantheliaUIManagerSubsystem`.
- `UPantheliaActivatableWidget`.
- `UPantheliaUISettings`.
- Registry mínimo.
- Enums de políticas.
- Tags.
- Policy si procede.
- Integración mínima con HUD.

## MCP

- Activar plugins.
- Project Settings.
- Viewport Client.
- Root Layout.
- Stacks.
- Pantalla de prueba.
- Modal.
- Toast.
- Controller Data.
- Glifos.
- Foco.
- Input.

## Pruebas

- Abrir/cerrar.
- Back.
- Toggle.
- Mouse.
- Teclado.
- Mando.
- Cambio de dispositivo.
- Pausa.
- `bTriggerWhenPaused`.
- Animaciones durante pausa.
- Modal.
- Toast.
- Held input.
- Cambio de mapa.
- PIE repetido.
- Overlay único.
- Cambio de PlayerController.
- Cleanup.

## Criterio de salida

No continuar si CommonUI rompe:

- Enhanced Input.
- Ability bindings.
- Pausa.
- Foco.
- Overlay.
- Cambio de mapa.
- Lifecycle.

---

# Fase 3 — Vertical slice funcional

## Contenido

- Pausa.
- Atributos.
- Modal.
- Toast.
- Tooltip.
- Tema visual.
- Glifos.
- Mando.
- Sonido.
- Animación.

## Migración legacy obligatoria

Una vez validado el nuevo camino, eliminar del PlayerController:

```text
OpenAttributeMenu
CloseAttributeMenu
ToggleAttributeMenu
AttributeMenuClass
AttributeMenuWidget
bAttributeMenuOpen
```

También eliminar el binding legacy específico si queda reemplazado por el handler que delega al manager.

Criterio:

```text
El PlayerController no contiene creación, ownership ni lifecycle de AttributeMenu.
```

## Message pipeline

En el mismo cambio:

- Nuevo Toast se suscribe.
- Consumidor visual legacy se desconecta.
- Un solo mensaje visible por evento.

## Resoluciones

- 1920×1080.
- 2560×1440.
- 3840×2160.
- 16:9.
- 21:9.
- Safe zone.
- UI scale.

## Criterio de salida

El vertical slice debe estar cercano a calidad final.

---

# Fase 4 — Vertical slice artístico del árbol

## Objetivo

Validar el máximo ceremonial.

## Entregables

- Wireframe de alta fidelidad.
- Geometría fija.
- Selector de corazón.
- Nodo universal.
- Nodo elemental.
- Estado bloqueado.
- Estado disponible.
- Estado comprado.
- Estado máximo.
- Tooltip completo.
- Transición visual entre corazones.
- Microcopy inicial.

No conectar gameplay imaginario.

---

# Fase 5 — Reskin y migración de piezas existentes

- Overlay.
- Resource bars.
- Ghost bar.
- XP.
- Spell slots.
- Cooldown.
- Boss bar.
- Button.

Regla:

> Lógica preservada salvo evidencia documentada.

---

# Fase 6A — Pantallas con fuente real

- HUD.
- Boss HUD.
- Pausa.
- Atributos read-only.
- Opciones reales.
- Referencia de controles.
- Muerte placeholder.
- Menú principal parcial.

---

# Fase 6B — Pantallas bloqueadas

Solo wireframe, contrato y arte hasta existir gameplay:

- Árbol funcional por corazón.
- Equipado final de hechizos.
- Hoguera.
- Dos armas.
- Inventario.
- Equipamiento.
- Continuar.
- Cargar.
- Respawn completo.

---

# Fase 7 — Fábrica de assets

Proceso:

1. Master.
2. Aprobación.
3. Variantes.
4. Limpieza.
5. Normalización.
6. Manifest.
7. Importación.
8. Configuración.
9. Material.
10. Prueba a tamaño real.
11. Aprobación.
12. Bloqueo de versión.

---

# Fase 8 — Hardening y rendimiento

## Auditar

- Tick.
- Bindings.
- Delegates.
- Lifetime.
- Foco.
- Pausa.
- Input.
- Soft references.
- Overdraw.
- Blur.
- Draw calls.
- Texturas.
- Materiales.
- ListView.
- Invalidación.
- Retainer Panels.

Retainers solo después de medición.

## Robustez

- Missing icon.
- Missing class.
- Missing controller.
- Data inválida.
- Estado vacío.
- Loading.
- Error.
- Cleanup.
- Cambio de mundo.
- Respawn.
- PIE repetido.

---

# Fase 9 — Localización y accesibilidad

## Desde el primer widget nuevo

- `FText`.
- String Tables.
- Soporte español.
- Preparación inglés.
- Sin texto en texturas.

## Accesibilidad

- UI scale.
- Tamaño de subtítulos.
- Contraste.
- Fondo de subtítulos.
- Alternativas al color.
- Reducción de movimiento.
- Reducción de flashes.
- Camera shake.
- Hold/toggle.
- Foco visible.
- Indicador opcional de Perfect Dodge/Parry.
- Sin parpadeos superiores a 3 Hz.

---

# 13. Validación editorial

Extender:

```text
PantheliaDataValidationUtils
```

## Familia 8 — UI Registry

- ScreenTag único.
- Capa válida.
- Clase válida.
- Soft class cargable.
- Política válida.
- Sin duplicados.

## Familia 9 — UI Theme

- Fuentes.
- Materiales.
- Texturas.
- Sonidos.
- Fallbacks.
- Valores válidos.
- Referencias válidas.

Extender las familias existentes cuando se añadan datos de:

- Skill Tree.
- Ítems.
- Equipamiento.
- Corazones.
- Hechizos.

---

# 14. Criterios de aceptación por pantalla

## Funcionalidad

- Abre.
- Cierra.
- Back.
- Pausa.
- Datos correctos.
- Error.
- Empty.
- Loading.
- Disabled.
- Sin duplicados.

## Input

- Ratón.
- Teclado.
- Mando.
- Foco.
- Restauración.
- Cambio de dispositivo.
- Toggle pausado.
- Sin ability accidental.
- Sin buffer atrapado.

## Visual

- 1080p.
- 1440p.
- 4K.
- 16:9.
- 21:9.
- Safe zone.
- UI scale.
- Texto largo.
- Iconos faltantes.
- Contraste.
- Tamaño mínimo.

## Arquitectura

- Sin lógica autoritativa.
- Sin Tick innecesario.
- Sin polling.
- Sin casts repetidos.
- Cleanup correcto.
- Controller correcto.
- Registry válido.
- Un solo consumidor de mensajes.
- Un solo owner de pantalla.

## Documentación

- Manifest.
- Capturas.
- Tags.
- Tests.
- Decisiones.
- Ownership.
- Commit.

---

# 15. Decisiones todavía dependientes de gameplay

Estas no bloquean la Fase 0 ni el spike.

## 15.1 Pérdida de recurso al morir

Debe definirse si el recurso de progresión:

- Se pierde.
- Se recupera.
- No se pierde.

Esto afectará:

- HUD.
- Hoguera.
- Nivel.
- Muerte.
- Recuperación.

No se inventará una solución de UI antes de la decisión.

## 15.2 Verbo de aplicación

“Consagrar” es provisional.

Debe confirmarse con el lore.

## 15.3 Dos armas

La arquitectura conceptual está aprobada.

El contrato runtime todavía no existe.

## 15.4 Inventario

No se construye hasta cerrar el modelo de ítems.

## 15.5 SaveGame

No se habilitan Continue/Load hasta ser reales.

---

# 16. Orden inmediato de trabajo

El orden que se seguirá es:

1. Ejecutar Fase 0 de solo lectura.
2. Crear los entregables de auditoría.
3. Actualizar el mapa real de dependencias.
4. Completar los tres documentos de Fase 1.
5. Crear concept boards y wireframes.
6. Aprobar Art Bible.
7. Diseñar el C++ del spike.
8. Compilar.
9. Ejecutar Fase 2 mediante MCP.
10. Validar CommonUI.
11. Construir vertical slice de Atributos.
12. Desmantelar camino legacy.
13. Validar árbol de alta fidelidad.
14. Migrar/reskinear piezas funcionales.
15. Continuar solo con pantallas que tengan fuente real.

---

# 17. Veredicto final

La arquitectura aprobada es:

```text
Gameplay / GAS / Components
        ↓
Widget Controllers o fuentes válidas
        ↓
UMG + CommonUI
        ↓
UPantheliaUIManagerSubsystem por LocalPlayer
        ↓
WBP_PantheliaRootLayout
        ↓
Stacks para Menu y Modal
Contenedores para GameHUD, Toast, Cinematic y Debug
```

La estrategia de producción aprobada es:

```text
Auditar
→ especificar
→ probar CommonUI
→ vertical slice funcional
→ vertical slice artístico
→ migrar piezas reales
→ construir solo contra gameplay existente
→ fabricar assets modulares
→ hardening
→ localización y accesibilidad
```

El mayor activo actual es la infraestructura reactiva de GAS y Widget Controllers.

El mayor problema actual es la navegación y ownership dispersos.

El mayor riesgo operativo es la coexistencia temporal de caminos legacy y nuevos o la edición simultánea de assets binarios.

Este documento queda aprobado como plan de ejecución de la UI de Panthelia.

---

## Estado

- [x] Plan inicial.
- [x] Revisión Kimi K3.
- [x] Plan v1.1.
- [x] Revisión Fable.
- [x] Convergencia.
- [x] Plan definitivo v1.2.
- [ ] Fase 0 ejecutada.
- [ ] Fase 1 aprobada.
- [ ] Spike CommonUI validado.
- [ ] Vertical slice funcional aprobado.
- [ ] Vertical slice artístico aprobado.
