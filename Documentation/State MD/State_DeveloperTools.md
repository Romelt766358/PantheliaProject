# State_DeveloperTools.md — Panthelia Developer Suite (PDS)

> **Para todos los chats del proyecto:** Panthelia tiene un plugin **Editor-only** propio que puede **inspeccionar, validar, exportar y comparar** información estructurada del proyecto desde dentro de Unreal, tanto por dashboard como por **siete tools MCP**. **Antes de pedirle a Romelt inspección manual extensa** (abrir Blueprints, listar assets, copiar valores, exportar `.t3d`), comprobar si PDS puede obtener el dato de forma estructurada.
>
> **Límite que hay que respetar con precisión:** PDS **observa, valida, exporta y compara**. **No** edita C++, **no** modifica Blueprint graphs, **no** añade notifies, **no** corrige assets automáticamente y **no** guarda `.uasset`. No sustituye la compilación, ni las pruebas PIE/runtime, ni la política de respaldo de `Content/`.

**Versión y estado:** `0.5.0-alpha2` (Automation API `0.5.0-alpha2`), UE 5.8, cerrada y publicada el 2026-07-21. Commit `edb91e0a159bcd1f2d753d83fa2a17b70a998e6b`.

---

## 1. Qué es y dónde vive

Ruta: `Plugins/PantheliaDeveloperSuite`. Su propósito es reducir la inspección manual, los errores de configuración detectados tarde y la transferencia incompleta de información entre Unreal y los agentes de IA.

**Dos módulos Editor, con separación deliberada:**
- **`PantheliaDeveloperSuite`** (core genérico, **no** conoce GAS ni las clases de Panthelia): Project Doctor / Data Validation, Project Snapshot e historial, baseline, comparación de snapshots, diff de assets/Gameplay Tags/montages, dashboard Slate, Snapshot Diff Browser, Automation API y toolset MCP.
- **`PantheliaDeveloperSuitePanthelia`** (adaptador con conocimiento del juego): catálogo semántico de hechizos, inspección de `UPantheliaProjectileSpell`, validación semántica de Blueprints de hechizos, inspección cross-asset de `CastMontage` y Gameplay Events, contributor del dominio `semanticDomains.spells`, y sus tests.

**Versiones autoritativas** (no usar el texto de una ventana como autoridad): `GetStatus.automationApiVersion` → `describe_toolset` → `PantheliaDeveloperSuite.uplugin` → los contratos versionados.
```
Plugin / Automation API: 0.5.0-alpha2   ·   Snapshot schema: 0.4.0-alpha1
Diff report schema: 0.4.0-alpha1-diff   ·   Spell semantic domain: spells@1.0.0
```

---

## 2. Las siete tools MCP

Toolset: `PantheliaDeveloperSuite.PantheliaDeveloperSuiteToolset`

| Tool | Para qué |
|---|---|
| `GetStatus()` | Disponibilidad de PDS, `automationApiVersion`, proyecto, engine, snapshot más reciente, baseline, conteo e issues. **Primera llamada de cualquier sesión.** |
| `ListSnapshots(limit)` | Historial de snapshots exportados. |
| `ExportProjectSnapshot()` | Exporta un snapshot completo (metadata, rutas, conteos, semantic domains, issues). |
| `ValidateProfile(profile, maxIssues)` | Ejecuta Data Validation + los `IsDataValid` del proyecto + validadores PDS sobre un perfil. |
| `SetLatestSnapshotAsBaseline()` | Fija el último snapshot como referencia. **Solo tras confirmar que representa un estado aprobado.** |
| `CompareLatestSnapshotWithBaseline(maxEntries)` | Diff contra la baseline. |
| `CompareLatestTwoSnapshots(maxEntries)` | Diff de la sesión reciente (assets, tags, montages, records semánticos, campos modificados, comparabilidad de schema, issues, truncamiento). |

**Perfiles de validación:** `PantheliaCore` · `GameContent` · `ExternalContent` · `EntireProject`.

> **Regla crítica de interpretación:** una validación puede **completar correctamente** y devolver `bSuccess = false` porque encontró **errores reales de contenido**. El estado `bValidationCompleted = true` + `bInfrastructureFailure = false` + `bSuccess = false` **no** es un fallo de la tool. Distinguir siempre fallo de infraestructura vs. errores de contenido, y por origen (Panthelia Core / Game Content / External Content). Límite de issues en la respuesta: 100 por defecto, 500 máximo — **los reportes persistidos siguen completos** (`markdownReportPath`, `jsonReportPath`).

---

## 3. Dashboard dentro de Unreal

Acceso: `Tools → Panthelia Developer Suite`. Acciones: validar selección / Panthelia Core / Game Content / External Packs / proyecto entero; exportar snapshot; comparar los dos últimos; fijar baseline; comparar con baseline; **inspeccionar montages seleccionados**; abrir el **Snapshot Diff Browser**; abrir la carpeta de salida.

**Dashboard vs MCP:** el dashboard es interactivo (progreso visual, validación de la selección actual). El MCP es **no interactivo**, no abre progreso ni Message Log, conserva el Output Log, devuelve DTOs estructurados y **no acepta rutas arbitrarias** elegidas por el agente.

---

## 4. Quién usa qué (separación de agentes)

- **Codex de escritorio** (workspace `C:\Panthelia Project\PantheliaProject`): editar `.h`/`.cpp` y tests, revisar diffs, **compilar con Unreal cerrado**, auditorías Git, documentación, stage/commit/push controlados.
- **Codex dentro del Terminal de Unreal + MCP**: ejecutar las siete tools, consultar estado, validar, exportar/listar snapshots, fijar baseline, comparar, e inspeccionar resultados **desde el proceso real del editor**.

> **Regla de refresh (imprescindible).** Tras modificar tipos reflejados, `UFUNCTION`s, `USTRUCT`s, `UENUM`s o el schema MCP: cerrar Unreal por completo → compilar `PantheliaProjectEditor Win64 Development` → abrir un proceso nuevo → ejecutar `ModelContextProtocol.RefreshTools` → cerrar sesiones MCP antiguas → abrir sesión nueva → `describe_toolset` → confirmar API `0.5.0-alpha2`, siete tools y nombres de campos actuales. **Sin este reinicio un agente puede conservar un schema cacheado y diagnosticar errores inexistentes.**

---

## 5. Dominio semántico de hechizos (`spells@1.0.0`)

Único dominio semántico implementado hoy. **No inventar dominios que no existen** ni afirmar que un sistema sin dominio está "validado semánticamente".

Cubre **hechizos de proyectil concretos** derivados de `UPantheliaProjectileSpell`, capturando entre otros `projectile.socketTag`, `projectile.spawnEventTag`, `projectile.castMontage` y `projectile.montageEventPresent` (calculado con `CastMontage` + `ProjectileSpawnEventTag`, **nunca** con `SocketTag`). El contrato completo socket-vs-evento y la lista de plantillas abstractas vs. concretos está en `State_Abilities.md` (sección de `UPantheliaProjectileSpell` / `GA_RangedAttack`).

**Plantillas abstractas** (excluidas del catálogo, por `CLASS_Abstract`, no por nombre ni ruta): `GA_RangedAttack`, `GA_MultiProjectile_Base`. **Records concretos:** `GA_RangedAttack_Shaman`, `GA_RangedAttack_TestBoss`, `GA_Firebolt`.

---

## 6. Garantías read-only y salidas

PDS escribe **únicamente** bajo `Saved/PantheliaDeveloperSuite` (snapshots, reportes JSON/Markdown, diffs). Las operaciones verifican que no se ensucien packages (`bReadOnlyVerificationPassed`, *newly dirtied packages = 0*). No guarda `.uasset` ni recupera assets no versionados.

---

## 7. Flujo recomendado al cerrar una fase

Editar C++ en Codex de escritorio → cerrar Unreal → compilar → abrir proceso nuevo → `ModelContextProtocol.RefreshTools` → sesión MCP nueva → `GetStatus` → compilar/guardar **solo** los Blueprints autorizados → tests → suite PDS → `ValidateProfile` → `ExportProjectSnapshot` → inspección read-only del JSON → `CompareLatestTwoSnapshots` → **solo si el estado es aprobado** `SetLatestSnapshotAsBaseline` → `CompareLatestSnapshotWithBaseline` → auditoría Git por rutas exactas → documentación → stage exacto → commit aislado → push sin force.

> **No fijar baseline antes de confirmar que el snapshot representa un estado aprobado.**

---

## 8. Limitaciones conocidas y pendientes del plugin

- **Campos ausentes vs. string vacío** no siempre se distinguen; `FScalableFloat` tiene representación limitada; los **rename/move** se ven como borrado + alta; cobertura parcial de **montage events**; los **validadores externos** quedan fuera del control de PDS.
- **`Content/` está en `.gitignore`**, así que los `.uasset` modificados **no** están tracked ni publicados en Git. **No afirmar que esos assets están en Git.**
- **Expansiones futuras** (pendientes menores del plugin): nuevos dominios semánticos más allá de `spells@1.0.0`, mayor cobertura de montage events, y mejoras de diff (rename/move).

**Documentos autoritativos del plugin** (en el repositorio): `PDS_v0.5-alpha2_MCP_AND_SPELL_SEMANTICS_CONTRACT.md`, `PDS_v0.5-alpha2_VALIDATION_REPORT.md`, `ROADMAP.md`, `KNOWN_LIMITATIONS.md`, `PantheliaDeveloperSuite.uplugin`. Los documentos de Alpha 1 se conservan como historial.
