# Panthelia Developer Suite — Limitaciones conocidas

## PDS-58 — Data Validation interactiva y no interactiva

### Contrato de ejecución

Panthelia Developer Suite distingue explícitamente dos contextos de validación:

- Dashboard interactivo:
  - usa `bNonInteractive = false`;
  - permite el progreso visual normal de Unreal Engine;
  - conserva el comportamiento predeterminado de `ShowMessageLogSeverity`;
  - no modifica `GIsSilent`.

- Automatización y MCP:
  - usan `bNonInteractive = true`;
  - configuran `FValidateAssetsSettings::bSilent = true`;
  - ejecutan `ShowMessageLogSeverity.Reset()` para impedir la apertura automática del Message Log;
  - protegen exclusivamente la llamada a `ValidateAssetsWithSettings` mediante `TGuardValue<bool>` sobre `GIsSilent`.

### Motivo del guard global

En Unreal Engine 5.8, la ruta interna de Data Validation puede llamar a
`WaitForAssetCompilationIfNecessary` con progreso visual habilitado por defecto,
incluso cuando `FValidateAssetsSettings::bSilent` está activo.

El guard temporal de `GIsSilent` impide que `FScopedSlowTask::MakeDialog()` cree
una ventana durante la validación no interactiva. La espera de compilación,
carga de assets, validación, descarga y captura de resultados continúan
ejecutándose normalmente.

El scope del guard debe permanecer limitado exclusivamente a
`ValidateAssetsWithSettings`.

### Riesgos residuales

`GIsSilent` es un estado global del proceso. Durante la llamada protegida,
cualquier código ejecutado dentro de Data Validation que consulte ese valor
también observará el modo silencioso.

La implementación presupone el contrato actual de Panthelia Developer Suite:
las llamadas MCP se ejecutan de forma serial y síncrona en el game thread.

`TGuardValue<bool>` restaura el valor anterior de `GIsSilent` al salir del scope,
incluidos escenarios de anidamiento correctamente estructurado.

### APIs externas no cubiertas

La protección no puede garantizar que validators de terceros o código externo
que abra ventanas mediante APIs que no consulten `GIsSilent` permanezcan
silenciosos.

Cualquier validator externo que invoque directamente ventanas, diálogos,
notificaciones o Message Logs por mecanismos independientes debe revisarse
por separado.

### Verificación

El test automatizado
`Panthelia.DeveloperSuite.Automation.ValidationSettingsContract`
comprueba la configuración de los modos interactivo y no interactivo.

La ausencia completa de ventanas durante una validación MCP no puede
comprobarse íntegramente mediante los tests automatizados actuales. Requiere
una comprobación visual manual en Unreal Editor.

Validación funcional confirmada para PDS-58:

- Ruta MCP:
  - validación completada;
  - Output Log conservado;
  - sin progreso visual;
  - sin Message Log automático;
  - sin diálogos ni prompts observados.

- Ruta del dashboard:
  - validación completada;
  - progreso visual normal conservado;
  - `GIsSilent` no modificado;
  - resultados y reportes generados normalmente.
