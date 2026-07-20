# Panthelia Developer Suite — Roadmap

## Estado actual

Versión de referencia:

- PDS v0.4 Alpha 3.
- Último commit publicado: `21507b1901d857145c0e40e7cee134e9377498c7`.
- PDS-58 cerrado y publicado.
- Build correcto.
- Suite PDS: 21/21.
- Ruta MCP validada como no interactiva.
- Ruta dashboard validada como interactiva.

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

Estado: cerrado y publicado.

Resultado definitivo:

- MCP y automatización usan validación no interactiva.
- `bSilent = true`.
- `ShowMessageLogSeverity.Reset()`.
- `GIsSilent` protegido mediante `TGuardValue<bool>`.
- El scope del guard queda limitado a `ValidateAssetsWithSettings`.
- Output Log permanece activo.
- Dashboard conserva validación interactiva y progreso visual.
- Documentación añadida en `KNOWN_LIMITATIONS.md`.
- Build correcto.
- Test específico: 1/1.
- Suite completa: 21/21.
- Verificación funcional MCP y dashboard completada.
- Verificación read-only: cero paquetes nuevos ensuciados.

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

## `OutputPath` y `BaselinePath`

Estado: reservado para v0.5.

Clasificación:

- Trabajo de contrato/schema futuro.
- No es una funcionalidad ausente: ambos campos ya existen.
- El pendiente es revisar una posible duplicación o representación redundante.

Restricciones:

- No modificar en v0.4.
- Revisar junto con el diseño completo de contratos de v0.5.
- Evitar romper compatibilidad con consumidores actuales de MCP y automatización.

## Prioridad actual

No hay bugs funcionales confirmados pendientes después de PDS-58.

Antes de iniciar otra implementación de PDS debe definirse uno de estos objetivos:

- alcance de PDS v0.4 Alpha 4; o
- alcance contractual de PDS v0.5.

Cualquier trabajo nuevo debe recibir un identificador nuevo. No deben reutilizarse PDS-42, PDS-43, PDS-58 ni PDS-66 para tareas diferentes.
