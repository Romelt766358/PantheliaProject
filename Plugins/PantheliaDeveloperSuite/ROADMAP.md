# Panthelia Developer Suite — Roadmap

## Estado actual

Versión de referencia:

- PDS v0.5 Alpha 1.
- Implementación completada y validada en UE 5.8.
- Release preparado para publicación mediante un commit aislado.
- La publicación efectiva debe comprobarse en el historial Git.
- Suite PDS: 22/22.
- Schema MCP v0.5 validado tras build con Unreal cerrado, `RefreshTools` y una sesión MCP nueva.
- Smoke funcional: 7/7 tools.
- PDS-58 comprobado manualmente como no interactivo: sin ventana de progreso, Message Log automático, diálogo ni prompt.

Histórico conservado: PDS v0.4 Alpha 3 tuvo como último commit publicado `21507b1901d857145c0e40e7cee134e9377498c7`.

## PDS-42 — Ordenamiento cosmético

Estado: aceptado y diferido.

Clasificación:

- Prioridad P4.
- Mejora cosmética.
- No es un bug funcional.
- No bloquea herramientas ni releases actuales.

Alcance conocido:

- Ajustes de orden o presentación.
- No debe abrirse como trabajo técnico prioritario.
- Solo tendría sentido dentro de una futura pasada específica de UX o pulido visual.

## PDS-43 — Diagnósticos de invalidación de caché

Estado: comportamiento actual aceptado.

Clasificación:

- Comportamiento intencional.
- No es un bug confirmado.
- Mejora potencial de observabilidad.
- No requiere cambios inmediatos.

El sistema actual ya dispone de caché e invalidación funcional. Cualquier trabajo futuro relacionado con diagnósticos debe distinguirse de una corrección funcional.

## PDS-58 — Supresión de UI durante automatización

Estado: cerrado y publicado en su alcance histórico; comprobado nuevamente para PDS v0.5 Alpha 1.

Resultado histórico:

- MCP y automatización usan validación no interactiva.
- `bSilent = true`.
- `ShowMessageLogSeverity.Reset()`.
- `GIsSilent` protegido mediante `TGuardValue<bool>`.
- El scope del guard queda limitado a `ValidateAssetsWithSettings`.
- Output Log permanece activo.
- Dashboard conserva validación interactiva y progreso visual.
- Documentación añadida en `KNOWN_LIMITATIONS.md`.
- Build correcto.
- Test específico histórico: 1/1.
- Suite histórica: 21/21.
- Verificación funcional MCP y dashboard completada.
- Verificación read-only: cero paquetes nuevos ensuciados.

Verificación PDS v0.5 Alpha 1:

- La segunda llamada controlada a `ValidateProfile` completó 589/589 assets válidos.
- No apareció ventana de progreso, Message Log automático, diálogo ni prompt.
- Los mensajes de carga y compilación de assets permanecieron en Output Log.
- No se produjeron paquetes nuevos ensuciados.

## PDS-66 — Campo `bValid`

Estado: comportamiento intencional aceptado.

Clasificación:

- No es un bug confirmado.
- No es un ticket de caché.
- No debe reutilizarse para otra tarea.
- `bValid` mantiene un significado contractual dentro del sistema correspondiente.

Cualquier modificación futura necesitaría una revisión específica del contrato en el que participa.

## Microoptimización de caché

Estado: diferida.

Identificador: ninguno asignado.

Clasificación:

- Optimización menor.
- No es un bug.
- No hay evidencia actual que justifique priorizarla.

Trabajo futuro posible:

- Medir el coste real antes de cambiar código.
- Evaluar enumeración y parsing.
- Revisar invalidación basada únicamente en `mtime`.
- Revisar propiedad y ciclo de vida de las cachés.
- Evaluar concurrencia solo si PDS deja de ejecutarse de forma serial y síncrona.
- Ampliar pruebas únicamente cuando exista un caso medible.

No debe confundirse con PDS-43 ni PDS-66.

## PDS-67 — Normalización del contrato de rutas

Estado: implementado y validado.

El trabajo que antes estaba reservado para v0.5 queda completado con la normalización de los nombres reflejados:

- `TimestampedSnapshotPath` para exportación.
- `BaselinePath` como ruta canónica del baseline.
- `MarkdownReportPath` y `JsonReportPath` para reportes.
- Eliminación de las propiedades reflejadas `OutputPath` y `OutputJsonPath`.
- Tipos nativos internos sin cambios.
- Frontera de traducción limitada a `FPDSAutomationService`.
- Sin aliases de compatibilidad v0.4.
- Automation API `0.5.0-alpha1`.
- Las siete tools y sus parámetros permanecen sin cambios.

## PDS-68 — Disciplina de reinicio del schema reflejado

Estado: implementado y validado.

Registro del procedimiento:

- Build con Unreal cerrado.
- Proceso nuevo del editor.
- `ModelContextProtocol.RefreshTools`.
- Sesión MCP nueva.
- `describe_toolset` posterior.
- Motivo: evitar falsos positivos por schema cacheado.

## PDS-69 — Contrato MCP persistente v0.5

Estado: implementado y validado.

Registro:

- Nuevo archivo `PDS_v0.5-alpha1_MCP_Contract.md`.
- Negociación mediante `automationApiVersion`.
- Migración explícita de nombres.
- Prohibición de fallbacks silenciosos a campos v0.4.
- Prompts locales no versionados fuera del alcance del commit.

## Prioridad actual

No quedan bloqueantes confirmados para PDS v0.5 Alpha 1.

Tras su publicación, cualquier trabajo nuevo debe recibir un identificador nuevo.

PDS-42, PDS-43, PDS-66 y la microoptimización conservan sus estados anteriores.

No deben reutilizarse identificadores existentes.
