# State_Hardening.md — Campaña de robustez (13-16 julio 2026)

> Registro de la gran ronda de bugfixing/hardening hecha con ChatGPT/Codex + Fable vía MCP. Aquí viven los **contratos de robustez** (lifecycle, ownership, fail-closed, cleanup) que no encajan en un solo sistema temático. Todo **cerrado y validado**. Los sistemas de gameplay en sí se documentan en sus `.md` (Combat, GAS, AI, Progression); esto es la capa de "que no se rompa".

---

## 1. Proyectiles y colisión (Bloque 3)

Canal de colisión **`Projectile`** con perfiles explícitos:
```
Capsule de personajes → Ignore
Mesh de personajes    → Overlap
Mundo / paredes       → Block
```
Comportamiento de un proyectil:
- Se **consume** en un hit real, un block o un parry.
- Se consume/bloquea contra el mundo según su configuración.
- **Atraviesa** un contacto negado por i-frames de dodge (no se gasta contra un jugador invulnerable).
- **No** debe ser negado si el golpe está marcado `Unavoidable`.
- El fireball normal del Shaman debe estar clasificado `Dodgeable`.

Fuente/objetivo y consumo correctos; el proyectil sigue usando el patrón spec-diferido (la ability crea el spec, el proyectil lo aplica). Ver `State_Abilities.md`.

---

## 2. Lifecycle, delegates y locks del ASC (Bloque 5A)

- **`UPantheliaWaitCooldownChange`** (async task de cooldown): handles exactos, `EndTask` idempotente, guards para callbacks tardíos, cleanup en `BeginDestroy`, `SetReadyToDestroy`, **sin** `RegisterWithGameInstance` (no vive más que su cliente) y **sin** `MarkAsGarbage`.
- **Widget Controllers:** dependencias inmutables por instancia; `bCallbacksBound` **idempotente**; se usan `AddUObject`/`AddWeakLambda` (object/weak-aware); nunca un reset superficial sin unbind completo.
- **Recorridos de la lista de abilities:** todo recorrido global va **dentro del ASC** con `FScopedAbilityListLock` o `ForEachAbility` (nunca iterar la lista sin lock).
- **Hotfixes Blueprint:** `WBP_SpellSlot` (una sola task fuerte; limpia timer/task/visual al reconfigurar o destruir); `GA_HitReact` y `GA_Stagger` (remueven su GE por **handle exacto** en los caminos terminales y, defensivamente, en `EndAbility`); `WBP_AttributeMenu` (eliminado un `BroadcastInitialValues` duplicado).

---

## 3. Hardening residual (Bloque 5B)

- **EffectActor:** ASC source separado del target; source policies explícitas; validación **fail-closed**; se retiran **todos** los stacks de un handle Infinite; cleanup en `EndPlay`; Instant/Duration/Infinite procesados antes de destruir. **Ref-count por actor** (añadido en la auditoría MCP): `0→1` aplica, `1→N` no reaplica, `N→1` mantiene, `1→0` retira/procesa End. Ver el detalle de políticas en `State_Overview.md` sección 2.6 y las respuestas de `State_Pending.md`.
- **Equipamiento — ruta transaccional:** `validar → spawnear candidato → inicializar → validar mesh/sockets → adjuntar/verificar → publicar → destruir anterior → broadcast`. Si algo falla antes de "publicar", **el arma anterior se conserva** (no queda el personaje desarmado por un fallo a mitad).
- **Weapon Trace:** Tick solo durante la ventana; fail-closed por radio/spec/mesh/socket inválido; fuente externa del jugador sin fallback ambiguo; desactivación si el arma desaparece. Ver `State_Combat.md` sección 9-10.
- **Lock-on:** snapshots exactos de movement/spring arm/controller; `SetIgnoreLookInput(true/false)` **balanceado** (no deja el input bloqueado); weak refs a los objetos originales; Tick solo con target; cleanup en `EndPlay`. Ver `State_Combat.md` sección 3.
- **HUD / interacción:** `InitOverlay` valida dependencias/clases y publica **un solo** overlay completo; el Interaction Trace limpia el highlight obsoleto con checks de interfaz/validez.
- **Tags / context:** raíz `Message` nativa; retiradas las solicitudes literales residuales de tags; los casts del custom Effect Context centralizados y validados en un helper único.
- **Fixes de compilación reales:** `OverlayWidgetController.cpp` necesitó incluir directamente `PantheliaGameplayTags.h`; `PantheliaHUD.cpp` necesitó `GameFramework/PlayerState.h` para que `IsValid`/`GetNameSafe` vieran la herencia completa de `APlayerState`.

---

## 4. Limpieza legacy (Bloque 5C) y auditoría de assets

- **`UBlockComponent` eliminado** (retirado de `AMainCharacter` y `BP_ThirdPersonCharacter`, archivos borrados; parry/guardia/Sprint siguieron funcionando). Ver `State_Combat.md` sección 4.
- **Auditoría MCP** — corregidos: llamadas Blueprint duplicadas en cuatro pickups (`BP_HealthCrystal`/`BP_ManaCrystal`/`BP_PotionHeal`/`BP_PotionMana` → **ruta C++ única**); `BP_FireArea` con Sphere+Box → Sphere C++ + Box con Begin/End **simétricos**, radio canónico **1200**, y **retirado del nivel** (solo testbed); doble binding de `IA_Lockon` eliminado; sockets `WeaponBase`/`WeaponTip` creados en el **halberd**; `BP_HealthPotion` (vacío) retirado del nivel.

Assets legacy conservados **a propósito** (ver `State_Pending.md`, respuestas del informe): `BP_HealthPotion` (asset vacío; el pickup funcional es `BP_PotionHeal`), `BP_FireArea` (testbed, cero instancias), `AB_Diosa01` (reparentado a `UPlayerAnimInstance`, compila; runtime pendiente porque el nivel usa `ABP_Player`). *(Nota: `BP_BaseKatana` figuraba aquí como conservado, pero fue **eliminado en la Fase 4I** de la migración MetaHuman — ver `Code_Review.md`, tabla canónica.)*

---

## 5. Preguntas que estos contratos permiten responder

Del checklist del informe maestro: cómo se equipa un arma sin perder la anterior ante un fallo (§3, equipamiento transaccional); cómo aplican/retiran efectos los EffectActors sin duplicados (§3, ref-count); cómo se limpian delegates, cooldown tasks y Widget Controllers (§2); cómo se abre/cierra el Weapon Trace y qué sockets necesita (§3 + `State_Combat.md`); qué assets legacy se conservan y por qué (§4).

---

## 6. Hardening de cook/package (Fases 4G-4I)

Empaquetado Development Win64 de la migración MetaHuman, cerrado y archivado.

**Preflight de puerto (procedimiento estable):** el cook commandlet puede chocar con un listener en `127.0.0.1:8000` (el MCP puede ocupar ese puerto durante el cook). `-nomcp` **no** resolvió el listener. Procedimiento reproducible: 1) cerrar el Unreal Editor; 2) comprobar el puerto; 3) abortar antes de UAT si está ocupado; 4) **no** matar procesos desconocidos; 5) ejecutar `BuildCookRun` externamente. Script reproducible: `CodexTemp/phase4i_final_retry.ps1`.

**Resultados del package (Fase 4I final):**
```
Build PASS · Cook PASS · Stage PASS · Pak/IoStore PASS · Archive PASS
UnrealBuildTool ExitCode=0 · UnrealEditor-Cmd ExitCode=0 · AutomationTool ExitCode=0
0 cook errors · 29 cook warnings · 1258 cooked packages (1265 descubiertos)
```

**Deuda no bloqueante del package:** warnings de PoseAssets Manny out-of-date; referencias soft/management a paquetes inexistentes (`SKM_Manny`/`SKM_Quinn`); `UnexpectedLoad` de GameplayCues (`GC_MeleeImpact`, `GC_Dodge_Perfect`). Ninguno bloquea el cook ni el smoke.
