# Panthelia Developer Suite — Contrato MCP y semántico v0.5 Alpha 2

**Automation API:** `0.5.0-alpha2`
**Snapshot schema:** `0.4.0-alpha1`
**Diff report schema:** `0.4.0-alpha1-diff`
**Spell domain schema:** `spells@1.0.0`
**Engine objetivo:** Unreal Engine 5.8

El dominio `spells@1.0.0` se mantiene porque Alpha 2 todavía era pre-release
cuando se corrigió el contrato. No existía una versión pública anterior de este
dominio que obligara a incrementar el schema.

## 1. Superficie MCP

PDS v0.5 Alpha 2 conserva exactamente siete tools y sus parámetros:

1. `GetStatus()`
2. `ListSnapshots(limit)`
3. `ExportProjectSnapshot()`
4. `ValidateProfile(profile, maxIssues)`
5. `SetLatestSnapshotAsBaseline()`
6. `CompareLatestSnapshotWithBaseline(maxEntries)`
7. `CompareLatestTwoSnapshots(maxEntries)`

No se añaden inputs de rutas arbitrarias.

## 2. Metadata de snapshots

`FPDSAutomationSnapshotMetadata` añade:

- `semanticDomainCount`
- `semanticRecordCount`
- `spellCount`

Los campos anteriores permanecen sin cambios.

## 3. Dominios semánticos

El snapshot añade un objeto raíz:

```json
"semanticDomains": {
  "spells": {
    "schemaVersion": "1.0.0",
    "records": []
  }
}
```

Cada record contiene:

- `recordId`
- `displayName`
- `kind`
- `sourceAssetPath`
- `fields`

El core no conoce GAS ni Panthelia. El dominio `spells` lo aporta el módulo Editor-only `PantheliaDeveloperSuitePanthelia`.

## 4. Identidad y determinismo

- `recordId` es la identidad dentro de un dominio.
- Para hechizos, `recordId` es el path de la Generated Class.
- Un rename o move que cambie el path se representa como `RemovedRecord + AddedRecord`.
- No existe seguimiento de identidad estable a través de renames en v1.
- Los contributors no pueden usar fields que comiencen por `$`.
- `$displayName`, `$kind` y `$sourceAssetPath` quedan reservados al comparador.
- Maps, arrays, tags y records se serializan en orden determinista.
- Floats se proyectan con seis decimales.

## 5. Compatibilidad histórica

Un snapshot anterior sin `semanticDomains` sigue siendo válido.

Cuando un dominio existe solo en uno de los snapshots:

- se marca como no comparable;
- se emite un issue informativo;
- no se generan altas o bajas masivas;
- `bHasChanges` no cambia únicamente por esa ausencia histórica.

Un `semanticDomains` presente pero estructuralmente inválido hace fallar el parse para evitar comparar datos corruptos.

## 6. Diff semántico

`FPDSAutomationDiffResult` añade conteos completos y una lista acotada:

- `previousSemanticRecordCount`
- `currentSemanticRecordCount`
- `addedSemanticRecordCount`
- `removedSemanticRecordCount`
- `modifiedSemanticRecordCount`
- `changedSemanticFieldCount`
- `semanticDomainNotComparableCount`
- `totalSemanticChangeCount`
- `returnedSemanticChangeCount`
- `bSemanticChangesTruncated`
- `semanticChanges`

`modifiedSemanticRecordCount` cuenta records únicos con al menos un field modificado. No cuenta fields.

Los informes Markdown y JSON persistidos permanecen completos. Solo la respuesta MCP se limita mediante `maxEntries`.

## 7. Limitación de fields ausentes — PDS-78

El comparador v1 obtiene los valores mediante lookup con fallback a string vacío.

Por tanto:

- field ausente;
- field presente con valor `""`;

se consideran equivalentes. Esta limitación es aceptable para `spells@1.0.0`, cuyo contributor emite un conjunto estable de fields. Un futuro contributor que necesite distinguir ambos estados deberá introducir un schema nuevo o una representación explícita.

## 8. Muestra de FScalableFloat — PDS-77

El catálogo y el validador v1:

- verifican la referencia de curva mediante `FScalableFloat::IsValid()`;
- serializan valor raw, curve table, row, registry type y valor en nivel 1;
- validan rangos utilizando el valor de nivel 1.

Esto no garantiza que todos los niveles posibles de una curva sean válidos. Se trata de una muestra canónica inicial. Ampliar el muestreo requiere definir primero el rango técnico de niveles relevante para Panthelia.

## 9. Catálogo de hechizos

El dominio `spells` cubre Blueprints cuya Generated Class hereda de `UPantheliaProjectileSpell`.

Campos principales:

- clase padre;
- tags de ability e input;
- política de activación;
- costes principal y adicionales;
- cooldown;
- efecto y tipos de daño;
- escalados;
- buildup;
- postura;
- dodge y defensa;
- heridas graves;
- knockback y launch;
- clase de proyectil;
- montage;
- `projectile.socketTag`;
- `projectile.spawnEventTag`;
- `projectile.castMontage`;
- `projectile.montageEventPresent`;
- cantidad, máximo, spread, intervalo, pitch y velocidad;
- configuración de soft homing.

`projectile.montageEventPresent` es un campo derivado. Permite que una ruptura del contrato montage/tag aparezca también en el diff.

### 9.1 Contrato de spawn del proyectil

`SocketTag` representa únicamente el socket físico de spawn. `SpawnProjectile`
lo usa para llamar a `GetCombatSocketLocation`. Ejemplos válidos son:

- `Montage.Attack.LeftHand`;
- `Montage.Attack.RightHand`;
- `Montage.Attack.Weapon`.

`ProjectileSpawnEventTag` representa el Gameplay Event esperado por
`WaitGameplayEvent`. Debe coincidir con el `eventTag` del `AN_MontageEvent` en
`CastMontage`. No se asigna automáticamente desde `SocketTag`. Puede coincidir
con `SocketTag`, pero esa coincidencia debe declararse explícitamente en el
Blueprint.

Los campos semánticos finales son:

- `projectile.socketTag`;
- `projectile.spawnEventTag`;
- `projectile.castMontage`;
- `projectile.montageEventPresent`.

`projectile.montageEventPresent` se deriva usando `CastMontage` y
`ProjectileSpawnEventTag`, nunca `SocketTag`.

### 9.2 Política de candidatos

- Se incluyen las clases concretas derivadas de `UPantheliaProjectileSpell`.
- Se excluyen los Blueprints cuya `GeneratedClass` tenga `CLASS_Abstract`.
- No se usan nombres, sufijos `_Base`, rutas hardcodeadas ni reglas basadas en
  la cantidad de hijos.
- El validator devuelve `NotValidated` defensivamente para clases abstractas.
- El snapshot contributor omite las clases abstractas antes de leer el CDO.
- La validación nativa de las clases abstractas retorna `NotValidated` antes de
  validar campos productivos.

Records concretos definitivos:

- `GA_RangedAttack_Shaman`;
- `GA_RangedAttack_TestBoss`;
- `GA_Firebolt`.

Plantillas excluidas:

- `GA_RangedAttack`;
- `GA_MultiProjectile_Base`.

### 9.3 Configuración final de los records

`GA_Firebolt`:

- `SocketTag = Montage.Attack.LeftHand`;
- `ProjectileSpawnEventTag = Event.Montage.SpawnProjectile`;
- `CastMontage = /Game/Animations/Spells/AM_Standing_ProjectileMagic_Montage`;
- `AN_MontageEvent` en `0.633333 s`, frame `19` a `30 fps`;
- `projectile.montageEventPresent = true`.

`GA_RangedAttack_Shaman`:

- `SocketTag = Montage.Attack.LeftHand`;
- `ProjectileSpawnEventTag = Montage.Attack.LeftHand`;
- `projectile.montageEventPresent = true`.

`GA_RangedAttack_TestBoss`:

- `SocketTag = Montage.Attack.LeftHand`;
- `ProjectileSpawnEventTag = Montage.Attack.LeftHand`;
- `projectile.montageEventPresent = true`.

## 10. Validación semántica

Las clases de hechizo implementan `IsDataValid` bajo `WITH_EDITOR`.

El módulo adaptador añade un `UEditorValidatorBase` para:

- detectar Blueprints de hechizo;
- validar su Generated Class CDO;
- ejecutar reglas intrínsecas;
- verificar el contrato cross-asset entre `CastMontage` y
  `ProjectileSpawnEventTag`.

`SocketTag` se valida de forma independiente como tag del socket físico.

La heurística de montage v1 inspecciona propiedades directas de tipo `FGameplayTag` en Notifies y Notify States. No recorre `FGameplayTagContainer` ni structs anidados. Esta limitación es deliberada mientras el notify real de Panthelia use un tag directo.

## 11. Compatibilidad con Unity Build

En `PDSSnapshotDiffTypes.cpp` se renombraron helpers privados para evitar
colisiones cuando Unreal concatena varios `.cpp` en una misma unidad de
traducción:

- `GetOptionalString` → `GetSnapshotDiffOptionalString`;
- `CountModifiedSemanticRecords` →
  `CountSnapshotDiffModifiedSemanticRecords`.

El comportamiento del snapshot diff no cambió.

## 12. Tests semánticos

Se añadieron o ampliaron las siguientes coberturas:

- `Panthelia.DeveloperSuite.Panthelia.Spell.Validator.ProjectileSpawnEventTag`;
- `Panthelia.DeveloperSuite.Panthelia.Spell.ProjectileTagIndependence`;
- `DerivedMontageField`;
- `CandidateFilter`;
- `ClassPathContract`;
- `ReadOnly`.

Las pruebas verifican la separación de tags, la exclusión de abstractas, la
derivación del evento del montage y la ausencia de paquetes nuevos dirty.

## 13. Read-only y memoria

- El contributor, el exportador y las inspecciones read-only no guardan `.uasset`.
- Esas operaciones no modifican assets; esta garantía no describe los cambios de assets realizados durante la implementación de Alpha 2.
- Los reportes y snapshots se escriben solo bajo `Saved/PantheliaDeveloperSuite`.
- El contributor reporta si tuvo que cargar clases.
- La decisión de ejecutar GC permanece centralizada en el exportador.
- El contributor no llama `CollectGarbage`.
- El exportador realiza como máximo una pasada de GC por operación.

El repositorio ignora `Content/`. Por ello el estado de versionado de los
assets de hechizos se documenta como una limitación de distribución en el
informe Alpha 2 y no implica que los `.uasset` estén respaldados por Git.

## 14. Disciplina de despliegue

Después de compilar cambios reflejados:

1. cerrar Unreal antes del build;
2. abrir un proceso nuevo del editor;
3. ejecutar `ModelContextProtocol.RefreshTools`;
4. reiniciar las sesiones MCP;
5. ejecutar `describe_toolset`;
6. confirmar API `0.5.0-alpha2`, siete tools y los campos semánticos nuevos.
