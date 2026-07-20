# Panthelia Developer Suite v0.5 Alpha 1 — Informe de validación

## Identificación

- Unreal Engine: UE 5.8.
- Base Git: `ea88647b52855a9801645541bc395cc18e4392a0`.
- Automation API: `0.5.0-alpha1`.
- Plugin Version: `13`.
- Plugin VersionName: `0.5.0-alpha1`.
- Estado durante la validación: validado localmente y apto para publicación.

Este informe registra el estado técnico observado antes de la publicación y no intenta almacenar un hash autorreferencial del commit que lo contiene.

## Alcance

- PDS-67 — Normalización del contrato de rutas.
- PDS-68 — Disciplina de reinicio del schema reflejado.
- PDS-69 — Contrato MCP persistente v0.5.
- Siete archivos de implementación aplicados inicialmente:
  - `PantheliaDeveloperSuite.uplugin`.
  - `PDSAutomationService.cpp`.
  - `PantheliaDeveloperSuiteToolset.h`.
  - `PDSAutomationTests.cpp`.
  - `PDSAutomationService.h`.
  - `PDSAutomationTypes.h`.
  - `PDS_v0.5-alpha1_MCP_Contract.md`.
- Dos documentos finales del Gate G:
  - `ROADMAP.md`.
  - `PDS_v0.5-alpha1_VALIDATION_REPORT.md`.
- Sin cambios en servicios nativos de rutas.
- Sin tools nuevas ni inputs de rutas arbitrarias.

## Gate C — Build

- Target: `PantheliaProjectEditor Win64 Development`.
- Unreal cerrado durante el build.
- No se utilizó Live Coding.
- Exit code: `0`.
- Resultado: Succeeded.
- Duración: `51.63 s`.
- Errores: `0`.
- Warnings: `0`.
- UHT correcto.
- Cuatro archivos generados.
- DLL y LIB del plugin enlazadas.

## Gate D — Automation Tests

- `PathContractV05`: 1/1 PASS, `0.007397 s`.
- `ToolsetSchema`: 1/1 PASS, `0.008525 s`.
- `StatusContract`: 1/1 PASS, `0.006913 s`.
- Suite completa: 22/22 PASS, `0.194550 s`.
- Failed: `0`.
- Skipped: `0`.
- Errores: `0`.
- Warnings: `0`.
- `RefreshTools` completado con `returnValue=true`.

## Gate E — Schema MCP servido

- Se utilizó una sesión nueva posterior al refresh.
- Versión declarada: `0.5.0-alpha1`.
- Se expusieron exactamente siete tools:
  - `GetStatus`.
  - `ListSnapshots`.
  - `ExportProjectSnapshot`.
  - `ValidateProfile`.
  - `SetLatestSnapshotAsBaseline`.
  - `CompareLatestSnapshotWithBaseline`.
  - `CompareLatestTwoSnapshots`.
- Los parámetros permanecieron sin cambios:
  - `GetStatus`: ninguno.
  - `ListSnapshots`: `limit`.
  - `ExportProjectSnapshot`: ninguno.
  - `ValidateProfile`: `profile`, `maxIssues`.
  - `SetLatestSnapshotAsBaseline`: ninguno.
  - `CompareLatestSnapshotWithBaseline`: `maxEntries`.
  - `CompareLatestTwoSnapshots`: `maxEntries`.
- Propiedades presentes:
  - `timestampedSnapshotPath`.
  - `baselinePath`.
  - `markdownReportPath`.
  - `jsonReportPath`.
- Propiedades ausentes:
  - `outputPath`.
  - `outputJsonPath`.
- También permanecieron presentes `durationMilliseconds`, `bInfrastructureFailure` y `bValid`.
- No existen inputs de rutas arbitrarias.

## Gate F — Smoke funcional

| Tool | Parámetros | bSuccess | Duración |
|---|---|---:|---:|
| GetStatus | ninguno | true | 0.599 ms |
| ListSnapshots | limit=20 | true | 445.666 ms |
| ExportProjectSnapshot | ninguno | true | 460.382 ms |
| ValidateProfile | PantheliaCore, 100 | true | 8815.153 ms |
| SetLatestSnapshotAsBaseline | ninguno | true | 188.874 ms |
| CompareLatestSnapshotWithBaseline | maxEntries=100 | true | 95.826 ms |
| CompareLatestTwoSnapshots | maxEntries=100 | true | 99.782 ms |

Resultados estructurados:

- `GetStatus` mantuvo la validez de snapshots desconocida: `bSnapshotValidityKnown=false`, `validSnapshotCount=-1` e `invalidSnapshotCount=-1`.
- `ListSnapshots`: 8 disponibles, 8 inspeccionados, 8 válidos y 0 inválidos; sin truncamiento.
- `ExportProjectSnapshot`:
  - 3811 assets.
  - 227 gameplay tags.
  - 76 montages.
  - Snapshot timestamped legible.
  - Snapshot latest existente y legible.
- `ValidateProfile`:
  - 589 solicitados, comprobados y válidos.
  - 0 inválidos.
  - Sin fallo de infraestructura.
  - Sin cancelación.
  - Verificación read-only correcta.
  - 0 paquetes ensuciados.
  - Reportes Markdown y JSON generados.
- Baseline nuevo legible y válido.
- Latest contra baseline: sin cambios.
- Latest two snapshots: sin cambios.
- Durante el smoke funcional, toda escritura de automatización quedó limitada a `Saved/PantheliaDeveloperSuite`.
- Ningún archivo versionado adicional fue modificado por el smoke.

## Verificación manual de PDS-58

- Se ejecutó una segunda llamada controlada a `ValidateProfile` con `PantheliaCore` y `maxIssues=100`.
- Resultado: 589/589 válidos.
- Duración: `4557.247 ms`.
- Paquetes nuevos ensuciados: `0`.
- No apareció ventana de progreso.
- No se abrió automáticamente Message Log.
- No apareció diálogo.
- No apareció prompt.
- Output Log permaneció activo.

Los mensajes de carga y compilación de assets permanecieron en Output Log y no constituyen UI interactiva.

## Issues de contenido observados

Los siguientes issues fueron no bloqueantes y ajenos a PDS v0.5:

- `DA_AbilityInfo` sin `BackgroundMaterial`.
- `DA_PantheliaPlayer_Sword_Basic` con `WeaponName` vacío.
- `DA_Sword_Basic` con `WeaponName` vacío.
- `DA_Sword_Basic_MH` con `WeaponName` vacío.
- En una ejecución apareció además un warning de `PoseAsset` desactualizado dentro de `BP_Boss`; no reapareció en la segunda ejecución.
- El issue informativo del filtrado del perfil.

Los warnings pueden variar según el estado de carga de assets. Los errores estructurales fueron cero.

## Resultado final

- Gate C: PASS.
- Gate D: PASS.
- Gate E: PASS.
- Gate F: PASS.
- Verificación manual PDS-58: PASS.
- Sin bloqueantes.
- PDS v0.5 Alpha 1 apto para publicación mediante un commit aislado.
- La publicación efectiva se verifica mediante el historial Git.
