# Panthelia Developer Suite — Contrato MCP v0.5 Alpha 1

**Automation API:** `0.5.0-alpha1`
**Engine objetivo:** Unreal Engine 5.8
**Ticket:** PDS-67
**Carácter del cambio:** ruptura deliberada del schema de rutas de respuesta.

## 1. Negociación de versión

Los agentes deben consultar primero:

- `GetStatus.automationApiVersion`; o
- la versión publicada por `describe_toolset`.

Interpretación:

- `0.4.x`: contrato antiguo con `outputPath` y `outputJsonPath`.
- `0.5.x`: contrato explícito de rutas descrito en este documento.

No se debe inferir la versión comprobando qué campo aparece.

## 2. Tools disponibles

PDS v0.5 Alpha 1 conserva exactamente siete tools y sus parámetros de entrada:

1. `GetStatus()`
2. `ListSnapshots(limit)`
3. `ExportProjectSnapshot()`
4. `ValidateProfile(profile, maxIssues)`
5. `SetLatestSnapshotAsBaseline()`
6. `CompareLatestSnapshotWithBaseline(maxEntries)`
7. `CompareLatestTwoSnapshots(maxEntries)`

No se añaden entradas de rutas arbitrarias.

## 3. Contrato de rutas

### 3.1 ExportProjectSnapshot

Campos canónicos:

- `timestampedSnapshotPath`: ruta operativa del snapshot timestamped recién escrito.
- `latestSnapshotPath`: ruta de `latest.json`.
- `timestampedSnapshot.filePath`: ruta incluida en la metadata obtenida por readback.
- `bTimestampedSnapshotReadable`: indica si esa metadata fue parseada correctamente.
- `bLatestSnapshotExists`: confirma la existencia de `latest.json`.

`timestampedSnapshotPath` puede existir aunque el readback de metadata falle.

El campo v0.4 `outputPath` no existe en esta respuesta.

### 3.2 SetLatestSnapshotAsBaseline

Campo canónico:

- `baselinePath`: única ruta operativa de `baseline.json`.

Metadata relacionada:

- `previousBaseline.filePath`, válida cuando `bPreviousBaselineReadable` es `true`.
- `newBaseline.filePath`, válida cuando `bNewBaselineReadable` es `true`.

El campo v0.4 `outputPath` queda eliminado. `baselinePath` es la única autoridad operativa.

### 3.3 ValidateProfile

Campos de informes:

- `markdownReportPath`
- `jsonReportPath`

Los campos v0.4 `outputPath` y `outputJsonPath` no existen.

La ejecución MCP continúa siendo no interactiva según PDS-58:

- no abre progreso visual;
- no abre Message Log automáticamente;
- conserva Output Log;
- no cambia la ruta interactiva del dashboard.

### 3.4 CompareLatestSnapshotWithBaseline

Campos de informes:

- `markdownReportPath`
- `jsonReportPath`

Los campos v0.4 `outputPath` y `outputJsonPath` no existen.

### 3.5 CompareLatestTwoSnapshots

Campos de informes:

- `markdownReportPath`
- `jsonReportPath`

Los campos v0.4 `outputPath` y `outputJsonPath` no existen.

## 4. Semántica de éxito

### Validación

- `bValidationCompleted`: la ejecución terminó con estado `Completed`.
- `bSuccess`: la ejecución terminó y no contiene errores bloqueantes.
- `bInfrastructureFailure`: indica fallo de infraestructura.
- `bCancelled`: reservado por el contrato; UE 5.8 no lo produce actualmente en esta ruta.
- `executionState`: valor informativo para logging.

Una validación puede completar correctamente y devolver `bSuccess = false` por errores reales de contenido.

### Operaciones y diffs

`bSuccess` representa el éxito operativo y de persistencia según el DTO correspondiente. Los issues deben revisarse incluso cuando exista una ruta de salida.

## 5. Paths operativos y metadata

Los paths operativos y `metadata.filePath` no son aliases intercambiables:

- el path operativo puede conocerse antes o aunque falle el readback;
- `metadata.filePath` solo es confiable cuando el flag de legibilidad correspondiente es `true`;
- `FPDSAutomationSnapshotMetadata::bValid` se conserva intencionalmente según PDS-66.

## 6. Límites de respuesta

Los límites crudos siguen exponiéndose junto con los límites aplicados y máximos:

- historial: default 20, máximo 500;
- issues: default 100, máximo 500;
- entradas de diff: default 100, máximo 500.

Los informes persistidos conservan la información completa aunque la respuesta MCP esté truncada.

## 7. Migración de agentes desde v0.4

Sustituciones obligatorias:

| v0.4 | v0.5 |
|---|---|
| `ExportProjectSnapshot.outputPath` | `ExportProjectSnapshot.timestampedSnapshotPath` |
| `SetLatestSnapshotAsBaseline.outputPath` | eliminar; usar `baselinePath` |
| `ValidateProfile.outputPath` | `markdownReportPath` |
| `ValidateProfile.outputJsonPath` | `jsonReportPath` |
| `CompareLatestSnapshotWithBaseline.outputPath` | `markdownReportPath` |
| `CompareLatestSnapshotWithBaseline.outputJsonPath` | `jsonReportPath` |
| `CompareLatestTwoSnapshots.outputPath` | `markdownReportPath` |
| `CompareLatestTwoSnapshots.outputJsonPath` | `jsonReportPath` |

No mantener fallbacks silenciosos a los nombres v0.4 en clientes que declaren compatibilidad con `0.5.x`.

## 8. Disciplina de refresh tras desplegar tipos reflejados

Después de compilar los cambios de v0.5:

1. arrancar Unreal Editor desde un proceso completamente nuevo;
2. ejecutar `ModelContextProtocol.RefreshTools`;
3. reiniciar las sesiones de todos los agentes MCP;
4. ejecutar `describe_toolset`;
5. confirmar los cuatro nombres nuevos y la ausencia de los dos nombres antiguos.

Sin estos pasos, un agente puede conservar un schema cacheado de v0.4 y producir un falso diagnóstico.
