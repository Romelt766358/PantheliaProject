# Code Review — Panthelia (revisión 2: estado + escalabilidad)

> **Alcance:** revisión de solo lectura sobre `/mnt/project/` cruzada con los sistemas planificados en los `.md`. No se corrigió ni creó código.
> **Foco de esta revisión:** (1) qué cambió desde el review anterior, (2) calidad del código nuevo, (3) **escalabilidad a futuro** frente a lo planificado (árbol de habilidades, sistema elemental, efectos de estado, parry, equipamiento).
> **Sustituye** al review anterior, que quedó desactualizado (D1 y C3 ya están resueltos).

---

## 1. Resumen ejecutivo

El proyecto **mejoró de forma clara** desde la última revisión. El refactor que centralizó el daño (D1) está bien hecho, el Weapon Trace System es código de calidad, y se cerraron varios bugs. La base data-driven del sistema de daño es, de hecho, **el activo de escalabilidad más fuerte** que tiene el proyecto: añadir tipos de daño, atributos escalables o resistencias no obliga a tocar el ExecCalc.

Quedaban **dos hallazgos abiertos del review anterior**, ya cerrados: C2 (los null-checks del ExecCalc) se arregló, y C1 (la firma de `GetCombatSocketLocation`) se resolvió corrigiendo la firma y, de paso, centralizando la función en C++ (hubo reportes contradictorios sobre si C1 estaba realmente roto; lo relevante es que el estado final es correcto — ver sección 3). De los puntos de fricción antes del árbol, **la Fricción 1 (nivel de abilities fijado a 1) ya está resuelta** (Etapa 2 de la fundación GAS — sección 5.1) y solo queda **la Fricción 3** (el placeholder de resistencias con Override), pospuesta a propósito hasta el primer nodo/pieza de resistencia. La Fricción 2 (ratios de escalado) quedó **cerrada por decisión de diseño**: el árbol solo modificará atributos, no ratios individuales por hechizo.

**Veredicto de escalabilidad:** la arquitectura está bien encaminada para lo planificado; ninguna fricción es estructural ni cara de resolver, pero conviene limpiarlas en orden antes de apilar el árbol encima.

---

## 2. Estado de los hallazgos del review anterior

| ID | Hallazgo | Estado actual |
|----|----------|---------------|
| C1 | Firma de `GetCombatSocketLocation` desincronizada | ✅ **Resuelto** (firma corregida + función centralizada en C++; ver nota de discrepancia abajo) |
| C2 | Null-derefs en `ExecCalc_Damage` (ClassInfo, curvas, interfaces) | ✅ **Resuelto** |
| C3 | Proyectil usado sin verificar el spawn | ✅ **Resuelto** |
| D1 | Escalado de daño divergente (melee no escalaba) | ✅ **Resuelto** (centralizado) |
| D2 | `UTraceComponent` legacy fuera de GAS | ✅ **Resuelto**: el melee del jugador se migró a GAS (`UPantheliaPlayerAttackAbility` + Weapon Trace + sistema de armas data-driven). Los archivos legacy quedan huérfanos, pendientes de borrado físico. Ver `State_Combat.md` sección 10 |
| M9 | `#define ECC_Projectile` apuntaba al canal equivocado | ✅ **Resuelto** |
| (typo) | Struct local `PanthaliaDamageStatics` (con 'a') en `ExecCalc_Damage.cpp` | ✅ **Corregido** a `PantheliaDamageStatics` |

---

## 3. Hallazgos vigentes

### C1 y C2 — cerrados tras verificación en sesión de implementación

- **C1 (firma de `GetCombatSocketLocation`) → resuelto.** Sobre este hallazgo hubo reportes contradictorios entre sesiones de implementación: uno lo declaró falso positivo (afirmando que el código ya tenía el parámetro), y otro posterior reportó que la implementación efectivamente estaba **sin** el parámetro y lo corrigió. El snapshot de solo lectura disponible para esta revisión coincide con el segundo (mostraba `GetCombatSocketLocation_Implementation()` sin `MontageTag`), pero al estar desincronizado no permite reconstruir la historia con certeza. **Lo que importa es el estado final**, reportado y consistente: la firma quedó como `GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag)` y la función se **centralizó en C++** en `APantheliaCharacterBase`, ramificando por tag (`Montage.Attack.Weapon` → arma; `RightHand/LeftHand/RightFoot/LeftFoot/Mouth` → sockets del mesh) con fallback seguro que evita spawns en 0,0,0. Ya no se implementa por Blueprint; cualquier override Blueprint previo (p. ej. en `BP_Boss`) debe borrarse. Detalle en `State_Abilities.md` sección 5.
- **C2 (null-derefs en `ExecCalc_Damage`) → resuelto.** Se añadieron defaults seguros neutros (`ArmorPenCoeff = 0.f` → sin penetración, `EffArmorCoeff = 1.f` → mitigación estándar), niveles con fallback a 1 si el cast a `CombatInterface` falla, y guards anidados (`if (ClassInfo && ClassInfo->DamageCalculationCoefficients)`, luego `if (ArmorPenCurve)` / `if (EffArmorCurve)` antes de cada `Eval`). Ante mala configuración el cálculo degrada con gracia en vez de crashear.
- **Typo corregido:** durante la verificación se encontró el struct local `PanthaliaDamageStatics` (con 'a') en `ExecCalc_Damage.cpp` (líneas 13, 20, 39, 55, 57) — typo real, confinado al archivo. Corregido a `PantheliaDamageStatics`.

### Menores nuevos (del código revisado en esta sesión)

- **M10 — Comentario desactualizado en `PantheliaDamageGameplayAbility.h`:** el bloque de cabecera dice "El cálculo ocurre en `SpawnProjectile()`". Tras el refactor, ocurre en `ApplyDamageScalingToSpec()`. El código es correcto; el comentario miente.
- **M11 — `AttributeScalings` "máximo 2" no está forzado:** es una regla de diseño escrita en comentarios, pero nada impide añadir 3+ entradas en el editor. Si el límite importa, un check en `PostEditChangeProperty` o un `checkf` lo haría explícito.
- **M12 — `ResolveWeaponMesh` fallback frágil:** la estrategia 2 toma "el primer `StaticMeshComponent`". Cuando un enemigo tenga varios (arma + escudo + adorno), podría agarrar el equivocado. La estrategia 1 (tag `"Weapon"`) es la robusta; conviene estandarizar el tag en todos los enemigos y tratar el fallback como último recurso.
- **M13 — `WeaponTraceComponent` tickea siempre con early-return:** `bCanEverTick = true` y el tick sale temprano si `!bIsTracing`. Barato, pero `SetComponentTickEnabled(true/false)` en Activate/Deactivate evitaría el coste por frame fuera de los swings.
- **M14 — Sin `HasAuthority` en la aplicación del daño del Weapon Trace:** `ApplyGameplayEffectSpecToSelf` se llama sin check de autoridad, a diferencia del proyectil que sí lo tiene. Irrelevante en single player, pero es una inconsistencia que mordería si algún día hay red.

---

## 4. Código nuevo: evaluación

Lo revisado en esta sesión está, en general, bien hecho:

- **`ApplyDamageScalingToSpec` / `MakeDamageSpec` / `CauseDamage`** (cierre de D1): la lógica de escalado vive en un único sitio y las tres rutas de daño (melee, proyectil, weapon trace) la comparten. Null-checks correctos en cada paso (`SpecHandle.IsValid()`, `SourceASC`, `PAS`, `FuncPtr`). El uso de `TagsToAttributes` para resolver el atributo del escalado sin `if/else` es exactamente el patrón que escala bien. Esto es trabajo sólido.
- **`UWeaponTraceComponent`:** componente reutilizable de verdad (no acoplado a enemigo), null-checks en todo el camino, lista de ignorados por swing bien gestionada, modo debug útil, y el patrón "spec diferido" mantiene una sola fuente de verdad del daño. El filtrado con `IsNotFriend` está integrado correctamente.
- **`UWeaponTraceNotifyState`:** mínimo y correcto, con guards de null en `MeshComp`/`Owner`.

Una sutileza que conviene tener documentada (no es bug): el Weapon Trace usa `SweepMultiByChannel`, que **sí** devuelve actores en `Overlap` (a diferencia del `SweepSingleByChannel` del lock-on, que solo ve `Block`). Por eso golpea a un jugador configurado con `Fighter = Overlap`. Es decir, el sistema depende de que el objetivo responda al canal Fighter con Overlap o Block; con `Ignore` no se detectaría. Es un requisito de configuración implícito que vale la pena no olvidar al añadir actores dañables.

---

## 5. Evaluación de escalabilidad a futuro

Cruzo el código actual con cada sistema planificado en los `.md`. Para cada uno: qué tan listo está el terreno y qué fricción hay.

### 5.1 Árbol de Habilidades (pilar central) — terreno mayormente listo, 3 fricciones

Es la prioridad declarada del proyecto ("muchos aspectos de hechizos y efectos modificables varias veces"), así que merece el análisis más detallado.

**A favor (el diseño actual lo facilita):**
- El escalado por atributos es el mecanismo natural del árbol: un nodo que sube `MagicDamage` (un GE Infinite con un modifier) hace que **todos** los hechizos con ratio sobre `MagicDamage` escalen automáticamente, sin tocar cada hechizo. Esto es justo lo que pide el diseño "data, no código".
- El lookup genérico vía `TagsToAttributes` significa que añadir atributos modificables por el árbol no toca el código de escalado.
- `DamageTypes` por hechizo es un `TMap` editable, y se evalúa por nivel — base apta para que el árbol suba el daño base de un hechizo subiendo su nivel.

**Fricción 1 — el nivel de las abilities estaba fijado a 1** — ✅ **RESUELTA (Etapa 2 de la sesión de fundación GAS).** El ASC ahora tiene `GetSpecFromAbilityTag`, `GetAbilityLevelFromTag` y `SetAbilityLevel` (ver `State_GAS.md`), que permiten al árbol subir el nivel de un hechizo nodo a nodo; tras `MarkAbilitySpecDirty`, la siguiente activación lee el nivel nuevo y `DamageTypes`/`PoiseDamage` (que ya usaban `GetValueAtLevel(GetAbilityLevel())`) escalan solos. **Decisión de diseño confirmada:** el nivel de las abilities es **independiente** del nivel del personaje — solo el árbol lo sube. El comentario de `AddCharacterAbilities` ya no dice "más adelante vendrá del nivel del personaje"; documenta que nivel 1 = rango inicial por diseño.

**Fricción 2 — los ratios de escalado viven fijos en cada ability** — ✅ **Decisión cerrada: son correctos donde están.** `AttributeScalings` es `EditDefaultsOnly` en el GA (parte del Class Default Object). El árbol de habilidades **solo modificará atributos**, no ratios. Un nodo que sube `MagicDamage` hace que todos los hechizos con ratio sobre ese atributo escalen más automáticamente — es más limpio que modificar ratios por hechizo en runtime y no requiere infraestructura adicional. La diferenciación entre hechizos (que uno escale "más" que otro con el mismo atributo) se define en el diseño base de cada `GA_*`, no en el árbol.

**Fricción 3 — el placeholder de resistencias bloqueaba modificadores externos** — ✅ **CERRADA (rediseño de estados elementales).** El modifier **Override** que fijaba las 4 resistencias desde `Resilience` en `GE_SecondaryAttributes` se **eliminó**. Ahora las resistencias elementales pueden recibir `Add`/`Multiply` de nodos del árbol y equipo. Con esto, tanto la Etapa 3 del plan de fundación como esta fricción quedan resueltas.

### 5.2 Sistema Elemental (corazones) — bien preparado, una decisión de diseño abierta

**A favor:** `GetDefensiveElement()` ya existe en `ICombatInterface` (el jugador devuelve `None` hoy); un corazón equipado solo necesita setear un campo que esa función lea. El patrón de "equipar = otorgar un set de abilities + un GE Infinite de pasivas" ya está rodado (`GiveStartupAbilities`, `CommonAbilities`). La tabla de afinidades ya cubre los 4 elementos. El terreno está listo.

**Decisión abierta — elemento ofensivo dinámico.** Hoy el tipo de daño (y por tanto su elemento) está fijo en el `DamageTypes` de cada hechizo. Si el diseño quiere que "el mismo hechizo cambie de elemento según el corazón equipado", el sistema actual no lo soporta: cada hechizo tiene su elemento hardcoded. Las salidas limpias son dos: (a) hechizos distintos por corazón (el corazón otorga su propio set), o (b) un mecanismo de "imbuir" que reescriba el tag de daño del spec antes de aplicarlo. La opción (a) encaja mejor con lo ya construido y no requiere código nuevo de daño.

### 5.3 Efectos de Estado Elementales (buildup) — el molde ya existe

Este es el sistema **mejor preparado** de todos, porque el código actual ya tiene el patrón exacto que necesita: el sistema de Postura es un buildup inverso (se acumula daño hasta un umbral → se dispara una ability → se resetea). Quemadura/Electrocución/etc. son lo mismo: un atributo `XBuildup` que sube con GEs, un check en `PostGameplayEffectExecute` que al llegar al máximo hace `TryActivateAbilitiesByTag` de la detonación y resetea, y un timer de decay calcado de `ResetPoiseRegenTimer`. **Riesgo bajo, hay precedente directo.**

**Única consideración:** son 4+ atributos nuevos con todo su boilerplate. Ver 5.6 (AttributeSet monolítico).

### 5.4 MagicResistance / MagicPenetration — copia del patrón de Armor

El paso 3 (mágico) del ExecCalc está vacío, pero el paso 2 (Armor + ArmorPenetration) ya es la plantilla exacta. Implementarlo es replicar esa lógica con los atributos mágicos (ya capturados en el ExecCalc, aunque sin usar). Riesgo bajo, sin sorpresas de escalabilidad.

### 5.5 Parry — greenfield, pero la postura ayuda

Es el menos preparado: `UBlockComponent` está vacío y quedan decisiones de diseño abiertas (componente vs ability, ventana perfecta, stacks de imbuir). Lo que sí ayuda es que la postura ya está implementada y la decisión "el parry daña postura" ya está tomada, así que la parte de daño del parry reutiliza `MakeDamageSpec` + `PoiseDamage`. El resto es construcción nueva; no hay deuda que estorbe, simplemente está por hacer.

### 5.6 Riesgo transversal — el AttributeSet monolítico

`UPantheliaAttributeSet` ya concentra vitales, secundarios, las 4 resistencias y los meta-atributos. Con los buildup elementales (4-8 más) y lo que traiga el equipamiento, va a crecer mucho, y cada atributo arrastra su `OnRep` + accessor + `DOREPLIFETIME` + entrada en `TagsToAttributes`. A medio plazo conviene **partirlo en varios AttributeSets por dominio** (p. ej. uno de combate, uno de resistencias/elementos, uno de buildup). GAS soporta varios AttributeSets en un mismo ASC sin problema, y la macro de captura del ExecCalc seguiría funcionando. Hacerlo pronto, antes de que el set sea enorme, es más barato que después.

---

## 6. Recomendaciones priorizadas

Ordenadas para despejar el camino *antes* de construir el árbol, que es donde converge casi todo:

1. ~~Cerrar C1 / blindar C2~~ — **hechos** (C2 arreglado con guards y defaults seguros; C1 resuelto y la función centralizada en C++ con fallback a `GetComponentLocation()` en vez de 0,0,0).
2. ~~Resolver el nivel de las abilities~~ (fricción 1 del árbol y antiguo D6) — **hecho** (Etapa 2: `SetAbilityLevel`/`GetAbilityLevelFromTag`/`GetSpecFromAbilityTag` en el ASC; el nivel es independiente del personaje y subible por el árbol).
3. **Quitar el Override placeholder de resistencias** cuando se aborde árbol/equipamiento (fricción 3 / Etapa 3). Mientras exista, ningún sistema externo puede modificar resistencias.
4. ~~Decidir dónde viven los ratios de escalado~~ — **cerrado por decisión de diseño.** El árbol solo tocará atributos; los ratios fijos en `EditDefaultsOnly` son correctos.
5. **Planificar la división del AttributeSet** (5.6) — es la **Etapa 6** del plan de fundación (sección 11), **pospuesta**: solo se abordará si se acercan los buildup elementales; no es fricción bloqueante hoy.
6. **Limpieza menor** (M10–M14): comentario desactualizado, tick del trace, tag "Weapon" estandarizado, etc.

---

## 7. Conclusión

El proyecto está en mejor forma que en el review anterior y, lo más importante para lo que viene, **las decisiones data-driven del núcleo de daño son las correctas para un juego que quiere un árbol de habilidades muy modificable**. Las fricciones que señalo no son fallos de arquitectura: son piezas placeholder o provisionales (nivel fijo, Override de resistencias) que cumplieron su función para llegar hasta aquí, y que toca jubilar justo antes de que el árbol se apoye en ellas. Si se cierran C1/C2 y se resuelven el nivel de abilities y el placeholder de resistencias, el terreno queda despejado para construir el árbol, el sistema elemental y los efectos de estado sobre cimientos que ya están pensados para ellos.

---

## 8. Nota sobre la sincronización de `/mnt/project` (importante para futuras revisiones)

El snapshot de solo lectura `/mnt/project` está **parcialmente desincronizado** con el proyecto real, y se ha confirmado varias veces. **No asumir que una versión corta/vieja de un archivo en el snapshot es la real** — verificar con el reporte de implementación o pedir el archivo actual antes de afirmar que algo falta o está roto.

> **Matiz (ni todo está desfasado, ni el desfase se limita a "alta rotación"):** algunos archivos volátiles (`ASC`, `ExecCalc_Damage`, `PlayerAnimInstance`, `PantheliaCharacterBase`) se desincronizan a menudo, y hay archivos que sí son fiables para lectura directa (p. ej. `BossCharacter.h`, `PantheliaEnemy.h/.cpp` una sesión). **Pero el desfase no está confinado a esa lista** — `MainCharacter` no estaba en ella y también estaba viejo (ver casos). La lectura del snapshot vale para orientarse; la regla firme es: **para *editar* un archivo tocado en sesiones previas, confirmar la versión actual antes de tomar el snapshot como base.**

> **Re-sync completo (fecha reciente):** tras una tanda grande de cambios hechos con ChatGPT/Codex vía MCP (IA del boss, lock-on, y más), Romelt **borró y re-subió todos los `.h/.cpp`** al Project Knowledge. Así que **a fecha de esa sesión el snapshot estaba al día** y se leyó código real con confianza (p. ej. `PantheliaBossProfile.h`, `PantheliaBossBrainComponent.h`). La regla de verificación **sigue vigente** porque el snapshot vuelve a desfasarse con cada sesión de código posterior — un re-sync es una foto puntual, no una garantía permanente.

Casos confirmados:
- **C1 (`GetCombatSocketLocation`):** el snapshot mostró durante varias sesiones la firma vieja sin `MontageTag`. Esto generó reportes contradictorios (un chat lo llamó falso positivo, otro lo corrigió). Resuelto definitivamente: el código real tiene la firma correcta y la función centralizada (confirmado cuando el snapshot por fin reflejó el cambio).
- **`PantheliaAbilitySystemComponent.cpp`:** la copia del snapshot (~178 líneas) no tenía `NotifyParryImpact`, pero el real sí lo llama.
- **`ExecCalc_Damage.cpp`:** algunas lecturas del snapshot no mostraban la lógica de parry/bloqueo (`ApplyParryBlockMitigation`), que en el real está completa.
- **`PlayerAnimInstance.cpp`:** apareció como versión vieja (~34 líneas, sin `UpdateGuardState`), pero el real tiene el método funcionando.
- **`PantheliaAbilitySystemComponent.h` (recurrente):** el snapshot **no** tenía `NotifyParryImpact`, que sí existe en el real y que `PantheliaAttributeSet.cpp` llama (`HandleParryReaction`). Al regenerar el ASC desde el snapshot, la función se perdió → fallo de compilación, resuelto reconstruyéndola. `NotifyParryImpact(bool bParried)` no busca por InputTag (no viene del input sino del pipeline de daño): itera abilities activas, castea a `UPantheliaParryAbility` y llama a la activa. **El usuario confirmó que mantendrá el ASC actualizado en el snapshot como prioridad**, y que antes de tocar archivos desincronizables (ASC, ExecCalc, PlayerAnimInstance, CharacterBase) subirá primero el archivo real.
- **`MainCharacter.h/.cpp` (contraejemplo importante):** **no** estaba en la lista de "alta rotación" y **aun así estaba desactualizado** en el snapshot — le faltaba la migración de cámara a C++ de la clase 262 (`CameraBoom`/`SpringArmComp`, `PlayerCameraComponent`, `LevelUpNiagaraComponent`). Al editarlo tomando el snapshot como base, se entregó una versión "completa" que había perdido toda la cámara → `CameraBoom` desapareció de `BP_ThirdPersonCharacter` y `Tab` (lock-on) crasheaba (`StartLockon` escribía en `SpringArmComp->TargetOffset` con `SpringArmComp == nullptr`, obtenido por `FindComponentByClass`). Se reconstruyó desde la **transcripción** de la sesión anterior (no desde memoria). **Regla reforzada:** cualquier archivo modificado en sesiones anteriores fuera del snapshot puede estar desfasado, esté o no en la lista de alta rotación — confirmar con Romelt o revisar transcripciones antes de tratar el snapshot como "la verdad actual" para editar.

**Regla de trabajo:** los State docs se mantienen a partir de los archivos **entregados** por los reportes de implementación, no de lecturas crudas del snapshot cuando hay aviso de desincronización. Cuando el snapshot y un reporte se contradicen, prevalece el reporte (Romelt confirma sobre el proyecto real), dejando constancia de la discrepancia.

> **Reforzado (sesiones recientes):** Romelt corrigió a Claude por afirmar el estado de archivos sin verificar (la clase 207). Regla: **leer el archivo real / el `.md` / el reporte antes de afirmar el estado del proyecto**, nunca asumir. Corolario (sesión XP del boss): cuando algo "no se llama" o no funciona, **diagnosticar con logs trazando el flujo**, en vez de re-pedir configuración que Romelt ya confirmó. En aquel caso el boss estaba bien configurado desde el principio (`BaseXPReward=600`); insistir en revisar la config fue el camino equivocado — los logs incrementales localizaron el fallo real (la cadena Blueprint).

---

## 9. Patrón: clases del curso que chocan con la arquitectura de Panthelia

Hay un desajuste estructural recurrente entre el curso y Panthelia que conviene evaluar **antes** de implementar cualquier clase que toque montages o sockets:

> **El curso asume que los montages de ataque viven en `CharacterBase.AttackMontages`. En Panthelia los montages del jugador viven en `PantheliaWeaponDefinition` (data-driven por arma), y los tags de socket (`Montage.Attack.*`) están entrelazados con los enemigos, sus notifies `AN_MontageEvent` y `GetCombatSocketLocation`.**

**Caso documentado — clase 207 (refactor de socket/montage tags), implementada y revertida por completo.** Intentaba renombrar `Montage.Attack.*` → `CombatSocket.*`, añadir `Montage.Attack.1/2/3/4`, meter un `SocketTag` en `FTaggedMontage`, convertir `LightAttackMontages`/`HeavyAttackMontages` a `TArray<FTaggedMontage>` y añadir `GetTaggedMontageByTag`. Se revirtió porque:
- Renombrar los tags rompía **todos** los enemigos existentes y sus notifies `AN_MontageEvent`.
- Rompía el proyectil mágico y los enemigos ranged que usan `GetCombatSocketLocation`.
- Desincronizaba la documentación (`Guia_Nuevo_Enemigo` mapea `Montage.Attack.*` a sockets).
- El beneficio (separar identidad de socket para el sonido) **no era necesario**: en Panthelia el `ImpactSound` ya va por entrada del array / por arma (ver `State_Combat.md` sección 9).

**Estado final de 207:** revertido a pre-207 (conservando 205-206). `FTaggedMontage` = `MontageTag` + `ImpactSound` (sin `SocketTag`); tags de socket sin renombrar; `LightAttackMontages`/`HeavyAttackMontages` siguen siendo `TArray<UAnimMontage*>`; no existen `GetTaggedMontageByTag` ni overrides relacionados.

Es el **segundo caso** (tras la sesión de parry) en que una clase del curso choca con la arquitectura. Filtro a aplicar siempre: ¿esta clase asume montages/sockets en `CharacterBase`? Si sí, adaptarla a `WeaponDefinition` o descartarla, no implementarla tal cual.

---

## 10. Lecciones de implementación (GAS y UI)

Lecciones técnicas recurrentes, para no repetir tropiezos:

- **Preferir C++ puro sobre cadenas GA→GE→AttributeSet cuando la lógica ya existe en C++.** Las cadenas Blueprint de GAS tienen muchos puntos frágiles que cuesta diagnosticar; si la lógica ya está replicada en C++, migrar es más robusto y escalable. Caso: el XP de muerte se migró de `GA_ListenForXPEvents → GE_EventBasedEffect → IncomingXP` a `SendXPEvent` en C++ puro (ver `State_Progression.md` 2.5).
- **`WaitGameplayEvent` — pin `then` vs `EventReceived`:** `then` dispara **al activarse la ability** (inmediato, datos vacíos); `EventReceived` dispara **al llegar el evento**. Conectar la lógica a `EventReceived`, no a `then`.
- **Nodos Blueprint deprecados con prefijo `BP_`:** `BP_ApplyGameplayEffectSpecToSelf` (con `ErrorType=1`) **falla silenciosamente en UE5.8**. Usar la versión vigente del nodo.
- **Material UI y Render Opacity heredado:** `AlphaComposite` **no respeta** el Render Opacity heredado del padre; para fades de un widget (que dependen de heredar la opacidad) usar `Translucent`. Si el material controla su propio alpha (como el cooldown radial), `AlphaComposite` es correcto. Ver `State_UI.md` secciones 8 y 9.
- **`Delay` vs `Retriggerable Delay`:** un `Delay` normal, re-llamado con uno pendiente, **ignora la nueva llamada** (no reinicia). Para temporizadores que deben reiniciarse en cada disparo (p. ej. el fade de la barra de XP que debe posponerse con cada kill), usar `Retriggerable Delay`.

**De la sesión de puntos de atributo (clases 264-267):**
- **Los MMC que leen datos externos a GAS (el nivel del personaje vía interfaz) no se recalculan solos** cuando ese dato cambia — GAS solo vigila lo que declara en `RelevantAttributesToCapture`. Cualquier stat futuro dependiente del nivel (u otra fuente externa) necesita refresco explícito `Remove` + `Apply` guardando el handle (patrón `RefreshSecondaryAttributes`, ver `State_GAS.md`).
- **`FActiveGameplayEffectHandle` por valor** (retorno de función o miembro de clase) exige `#include "GameplayEffectTypes.h"`; un forward declaration no basta (el compilador necesita el tamaño del struct).
- **Blueprint — nodos `Get` de variable son puros:** no tienen pin de ejecución, nunca se conectan a un `then`. Solo por su pin de dato.
- **Blueprint — "Is Variable" sobre un widget anidado del árbol de diseño no se puede exponer** entre Blueprints (el "ojo" queda bloqueado). Solución: un **Event Dispatcher** que reenvíe el evento hacia afuera, no forzar la exposición.
- **Blueprint — para llegar a la función de un widget anidado, encadenar dos `Get`** (`Get contenedor → Get interno → función`), salvo que el contenedor exponga una función propia que resuelva el acceso.
- **Blueprint — el nombre de un nodo "Assign …" en el buscador no siempre es el intuitivo** ("Assign Attribute Points Changed Delegate", no "Assign On Attribute Points Changed"): verificar antes de asumir.
- **Blueprint — castear al tipo que una función ya devuelve es un error de compilación** ("this blueprint (self) is not a X"), no una redundancia inofensiva (ej. `GetAttributeMenuWidgetController` ya devuelve el tipo exacto).
- **`FInputModeUIOnly` bloquea las Input Actions del PlayerController;** para menús que se cierran con la misma tecla que los abre, usar `FInputModeGameAndUI`.

**De la sesión de fundación GAS + árbol (auditoría):**
- **Vida del estado = vida de lo que describe.** El estado persistente de GAS (flags de inicialización, handles de GEs `Infinite`) debe vivir en el **ASC**, no en el Pawn: el ASC del jugador (en el PlayerState) sobrevive al respawn y el de los enemigos muere con ellos — ambas semánticas correctas **sin código especial**. (Etapa 4.)
- **Binds con `AddUObject(this)` donde `this` es persistente NO se autolimpian** cuando muere *otro* objeto (el Pawn). Requieren guarda explícita contra el doble bind, o cada respawn acumula binds. (Etapa 4, hallazgo sutil.)
- **`const` protege al objeto, no a lo que apunta:** una función `const` puede escribir en los miembros de *otro* objeto a través de un puntero. Esto eliminó la necesidad del `mutable` del handle. (Etapa 4.)
- **`FScopedAbilityListLock` es obligatorio** al iterar la lista de specs del ASC si dentro puede concederse/removerse una ability. (Etapa 2.)
- **Punteros a structs dentro de arrays del ASC son de vida corta** (el array se realoja): usar y soltar en el mismo scope, nunca guardar como miembro. Por eso `GetSpecFromAbilityTag` no es BlueprintCallable. (Etapa 2.)
- **Derivar tags del CDO en vez de duplicarlos como campos** del Data Asset elimina una clase entera de bugs por desincronización de datos. (Etapa 5, `GetAbilityTagFromClass`.)
- **Principio de persistencia de GAS:** "GAS nunca se guarda; GAS se reconstruye desde datos guardados". Solo se serializa la decisión del jugador (`NodeRanks`), no el estado interno del ASC. (Etapa 5.)
- **Convención `0 = no tiene`** unificada entre `GetAbilityLevelFromTag` (ASC) y `GetNodeRank` (árbol): una sola consulta responde existencia + valor. (Etapas 2 y 5.)
- **Diagnóstico de "Accessed None":** distinguir errores de **inicialización** (fallan al empezar/entrar en una zona) del **ruido de teardown** (solo al cerrar PIE, con la funcionalidad correcta en juego). El Output Log alrededor de `BeginTearingDown` es la prueba decisiva. (Ver sección 12.)

---

## 11. Plan de fundación GAS + árbol (auditoría — registro y seguimiento)

Una auditoría técnica confirmó que **los cimientos de GAS son escalables** para un árbol de habilidades complejo (ASC en PlayerState, meta atributos, tags nativos, `TagsToAttributes`, patrón handle+remove+apply, contadores planos separados de GAS) y produjo un plan de 6 etapas:

| Etapa | Descripción | Estado |
|-------|-------------|--------|
| 0 | Verificar archivos de alta rotación antes de editar | ✅ Hecha |
| 1 | Clamps seguros en AttributeSet + tags raíz nativos | ✅ Completada (ver `State_GAS.md`) |
| 2 | Nivel de abilities independiente (`SetAbilityLevel` etc.) | ✅ Completada — **cierra Fricción 1** |
| 3 | Quitar el Override de resistencias en `GE_SecondaryAttributes` | ⏸️ Pospuesta — cambio de asset; se hará junto al primer nodo/pieza de resistencia (**Fricción 3**) |
| 4 | Estado de inicialización del Pawn → ASC | ✅ Completada — **cierra el riesgo de respawn (hallazgo 2.1)** |
| 5 | Esqueleto del árbol (`SkillTreeInfo` + `SkillTreeComponent`) | ✅ Completada (ver `State_Progression.md` sección 5) |
| 6 | Dividir el AttributeSet monolítico | ⏸️ Pospuesta — la mayor (toca ExecCalc + GEs); solo si se acercan los buildup elementales (ver 5.6) |

**Riesgo de respawn (hallazgo 2.1) — cerrado por la Etapa 4.** El estado de inicialización vivía en el Pawn; en un respawn el Pawn se recrea reseteado pero el ASC (en el PlayerState) sobrevive, lo que habría reaplicado atributos por defecto y dejado una instancia `Infinite` huérfana. Mudar el estado al ASC lo resuelve estructuralmente. Las protecciones son **preventivas**: aún no existe flujo de muerte/respawn del jugador.

---

## 12. Ruido de teardown en `WBP_BossHealthBar` (conocido, sin acción)

Al **cerrar** PIE aparecen errores `Accessed None` en `WBP_BossHealthBar` ("Assign On Boss Max Health Changed", "SetText") y en `Lvl_ThirdPerson` (`BroadcastInitialValues`). **Es ruido cosmético, no un bug:** los errores no aparecen durante la partida, solo tras `BeginTearingDown` (al pulsar Stop). Al apagar, el `BP_Boss` se destruye antes que el widget → su referencia queda `None` → esos nodos se quejan en su última reevaluación. Apareció ahora porque es el primer log en que el jefe **murió en combate** (`HP 39.3 → 0.0`), lo que dispara broadcasts de limpieza que la ruta "jefe vivo" no tocaba; el orden de destrucción en PIE tampoco es determinista. **No está relacionado con el código de esta sesión.** Recomendación: **no tocar nada** (añadir validaciones ensuciaría un grafo funcional por un mensaje estético). Opción futura si molesta al depurar: un `Unbind` de los delegates del jefe en el `Event Destruct` del widget.

**Warning de `UGameplayEffect::StackingType` en `PantheliaAttributeSet.cpp`** — warning de compilación/log independiente, detectado durante la sesión de IA del boss. Benigno y no relacionado con esa IA; probablemente ligado a un GE que declara stacking sin todos los campos que UE espera. Anotado para revisar cuando se toque el AttributeSet (candidato natural: la Etapa 6, división del AttributeSet). No bloquea nada hoy.

**De la serie de lock-on / soft-lock:**
- **Un nodo Blueprint impuro con el pin `Exec` sin conectar se poda** ("X was pruned because its Exec pin is not connected") y su `ReturnValue` cae al valor por defecto, aunque lo uses en un `Branch`. Caso: `IsNotFriend` en `GA_MeleeAttack`; fix = cablear su Exec (`ForEachLoop → IsNotFriend → Branch`). Regla: si una función tiene pin de ejecución, hay que conectarlo para que corra.
- **Separar "punto lógico" de "punto visual" en el lock-on:** `LockonTargetPoint` (gameplay/cámara/validación, en `APantheliaEnemy`) vs `LockonWidget`/`WBP_Lockon` (solo el icono). Una única función `GetLockonLocation()` como fuente de verdad del punto lógico evita que cámara, soft-lock, LoS y scoring miren a sitios distintos.
- **Dos consultas de línea de visión con propósitos distintos** no deben colapsarse en una: adquirir un target (respeta el toggle de "requerir LoS") y romper el lock por pared (LoS real contra el target actual) son decisiones diferentes; mezclarlas ata la ruptura al toggle de adquisición sin querer.
- **Migrar daño de "radio + evento" a Weapon Trace:** el bloque viejo (`WaitGameplayEvent`/`GetLivePlayersWithinRadius`/`CauseDamage`) se eliminó de `GA_MeleeAttack` en favor del `WeaponTraceNotifyState`. Menos nodos frágiles, una sola vía de daño (el patrón spec-diferido).

---

## 13. Auditoría de la Sección 25 (Combat Tricks) — hallazgos y fixes

Auditoría con Fable 5 del trabajo de Combat Tricks (clases 303–315 + post-315), leyendo el código real archivo por archivo. **Veredicto: buena calidad**; las adaptaciones de diseño (rechazo del colapso a un tipo, cadena de dos saltos, `FScalableFloat`) están bien razonadas y las correcciones sobre el curso son reales (fuga de memoria del spec, API de tags deprecada 5.3+, `ResolveDeathWeaponMesh`, bloqueo de muertos con Target). Se detectaron y **corrigieron** los siguientes hallazgos.

- **🔴 Crítico 1 — Contaminación del contexto compartido en melee multi-objetivo.** Un swing aplica el mismo spec a varios objetivos compartiendo el objeto de contexto (ref-counting); los productores escribían solo en éxito y nunca limpiaban, así que un enemigo "heredaba" el debuff/knockback/parry del anterior. **Fix:** patrón **escribir-siempre** (ver `State_GAS.md`, context). Convención `ZeroVector = "no salió"`.
- **🟠 Alto 2 — GEs dinámicos: stacking inoperante + colisión de nombres.** `Debuff()` creaba una definición nueva por aplicación (el stacking de GAS agrupa por *definición*, así que no agregaba) con `FName` fijo (terreno indefinido si el anterior sigue vivo). **Fix:** debuffs → **caché de definiciones** (`CachedDebuffEffects`, daño/duración por SetByCaller); tags temporales → **`MakeUniqueObjectName`**. Dos estrategias opuestas, cada una correcta para su caso (ver `State_GAS.md`).
- **🟠 Decisión 3 — i-frames no bloquean ticks de DoT.** Confirmado deseado (Elden Ring). Documentado en el código dónde se cambiaría si se quisiera lo contrario.
- **🟠 Decisión 4 — parry perfecto niega el debuff.** Confirmado deseado (Lies of P). `DetermineDebuff` reubicado tras el bloque de parry, recibe `bWasPerfectParry`.
- **🟡 Menores corregidos:** dado unificado a `<=` (era `<`, sesgado); Launch y Knockback ahora **excluyentes** (Launch primero); guard de `DebuffFrequency <= 0`; primer tick explícito (`bExecutePeriodicEffectOnApplication`); log de HP en overkill (guarda `OldHealth`).
- **Incidente de snapshot durante la auditoría:** la primera lectura se hizo sobre un snapshot **desincronizado** que no tenía el HeavyKnockback → falso hallazgo ("§3.3 no entregada"). Tras la resubida, el reporte resultó fiel. **Refuerza la regla de la sección 8:** verificar frescura del snapshot con una señal concreta (aquí, `bKnockbackIsHeavy` / "14 bits") antes de auditar o editar.
- **Incidente de edición (atrapado):** un `str_replace` consumió una llave de cierre del `else` de `bFatal`; el chequeo de balance de llaves (quedó en +1) lo detectó y se repuso. El chequeo funcionó como red de seguridad.

**Lecciones (amplían la sección 10):**
- **Contexto de GAS compartido en multi-objetivo:** todo productor de resultado debe escribir SIEMPRE (éxito y fracaso), no solo en éxito. `SetIsCriticalHit` es el patrón de referencia.
- **Stacking de GEs dinámicos agrupa por definición:** para que agregue, cachear UNA definición por tipo y variar solo el spec (SetByCaller); crear una definición por aplicación rompe el stacking.
- **`NewObject` con nombre fijo mientras el objeto vive = terreno indefinido:** `MakeUniqueObjectName` para objetos que coexisten; caché para objetos que deben ser "el mismo".
- **`Period = 0` en un GE periódico** no es "sin ticks": convierte el modificador en agregado continuo. Guard obligatorio.
- **El chequeo de balance de llaves** atrapa los `str_replace` que se comen llaves adyacentes — verificación estándar tras cada edición estructural.

**De la depuración del WarriorBoss (GapCloser / WeaponTrace):**
- **`SweepMultiByChannel` puede devolver `false` con hits válidos en el array.** Decidir por `HitResults.Num() > 0`, no por el `bool` de retorno (el booleano depende de si hubo un bloqueo, no de si hubo overlaps útiles).
- **Un canal de colisión con `DefaultResponse = Ignore` en `DefaultEngine.ini` no detecta nada** hasta que algún componente responde a él. El canal `Fighter` necesitó respuesta explícita en el constructor base. La cápsula no siempre genera el hit: el trace puede resolverse contra `CharacterMesh0`/physics asset.
- **Slots de animación inconsistentes rompen `PlayMontageAndWait` de forma intermitente:** si el AnimBP espera un slot (`FullBody`) y los montages están en otro, el montage "no se reproduce". Unificar el slot entre AnimBP y todos los montages del personaje.
- **Mejora modular anotada (no urgente):** separar en `WeaponTraceComponent` el `WeaponMeshComponent` (arma visual/física) del `TraceSocketSourceComponent` (fuente de los sockets del trace), para soportar sin ambigüedad enemigos con sockets en el skeleton y otros con arma como StaticMesh.

---

## 14. Rediseño de estados elementales (buildup determinista) — estado

- **El sistema de estados elementales ya está implementado** (no es "futuro"): buildup determinista a 100, `DA_ElementalStatusConfig` global, escalado por `StatusPower`/`MagicDamage`, Defense Shred, Heridas Graves universales y pipeline de curación `IncomingHealing`. Ver `State_GAS.md` y `State_Combat.md` sección 12.2. Sustituye por completo la ruta aleatoria `DebuffChance`/`DetermineDebuff`.
- **Heridas Graves = "gana la más fuerte", no acumulativa** (cambia la spec vieja de `State_Pending`).
- **`MagicResistance`/`MagicPenetration` ya mitigan el daño mágico directo** en el `ExecCalc`. **Curvas mágicas independientes en standby:** faltan las filas `MagicPenetration`/`EffectiveMagicResistance` en `CT_DamageCalculationCoefficients`; el código usa **fallback a las curvas físicas** (`ArmorPenetration`/`EffectiveArmor`) hasta que se creen. No exigirlas todavía.
- **El warning de `UGameplayEffect::StackingType`** (sección 12) **ya no corresponde a la ruta nueva** de estados: la unicidad se hace por eliminar+reaplicar por tag, no por `StackingType`. El warning, si persiste, viene de otro GE.
- **Limitación conocida (aceptada):** el daño de los payloads de estado entra por `IncomingDamage` **ya calculado** y **no vuelve a pasar** por las defensas del `ExecCalc` (Armor/MagicResistance). Es intencional: el daño mágico *directo* sí pasa por defensas; el daño de *estado ya resuelto* no las repite.
- **La advertencia del AttributeSet monolítico sigue vigente y es más relevante ahora** (el rediseño añadió ~20 atributos): la división por dominio (Etapa 6) gana peso.

---

## 15. Sistema de armas v2.1 — hallazgos cerrados y decisiones

**Hallazgos cerrados** (los cuatro eran fallos reales del sistema de armas del jugador):
- **El `WeaponDefinition` no aportaba buildup** → todas las armas ligeras compartían la identidad elemental de la ability; cambiar de arma no cambiaba el buildup. Ahora `BuildupAmounts` vive en el arma.
- **Los costes de estamina del arma no se usaban** (`LightAttackStaminaCost`/`HeavyAttackStaminaCost` eran datos muertos).
- **Las continuaciones del combo eran gratis:** como todo el combo vive en **una** activación, `CommitAbility()` solo cobraba el primer golpe. Ahora `AdvanceAndPlayNext` llama a `CommitAbilityCost` por golpe.
- **Los sockets del arma estaban desconectados del trace** (`SetupWeaponTraceForCurrentAttack` enviaba solo el mesh) → todas las armas dependían de los nombres por defecto. Prerequisito para una segunda familia de armas.
- **Trace inseguro al desequipar:** el componente podía **auto-adoptar un mesh arbitrario** del owner. Ahora el jugador usa fuente externa explícita + `ClearExternalWeaponTraceSource()` en `UnequipWeapon()`.

**Decisiones registradas:**
- **La v1 de la especificación se descartó** por estar escrita contra la **arquitectura elemental antigua** (la aleatoria). **La v2.1 es la definitiva.** Lección: una instrucción de implementación escrita contra un diseño ya superado es peor que ninguna — **verificar contra qué versión de la arquitectura se escribió** antes de ejecutarla.
- **Alcance de costes ampliado con auditoría (2026-07-16):** lo que era "el patrón `Cost.Stamina` cubre solo Light/Heavy" se generalizó **deliberadamente y con auditoría** a una arquitectura común (`UPantheliaCostAttributeSet`, resolvedor único, pago atómico multi-recurso) para ataques/dodge/parry/hechizos. Ver `State_GAS.md`.
- **Validar el montage ANTES de cobrar recursos** (arma → array → índice → montage no nulo → `CommitAbilityCost`). Cobrar primero y descubrir después que no hay golpe válido deja al jugador sin estamina y sin ataque.
- **Ausencia de arma/definición ≠ coste cero:** es configuración inválida y bloquea el ataque (un `0` explícito sí es válido).
- **Estado: ✅ validado en runtime.** C++ implementado y revisado estáticamente; compilación y pruebas PIE **confirmadas por Romelt**. Los cuatro fixes se dan por funcionales.

**Lecciones del sistema de dodge:**
- **Terminar una ability y activar otra en el mismo call stack es una carrera.** `EndAbility(GA_Dodge)` inicia una limpieza que puede no haber asentado los tags (`State.Dodge.Active`) cuando el ataque vuelve a comprobarlos → activación rechazada de forma irregular. Fix: **diferir al siguiente tick** (`SetTimerForNextTick`).
- **`TryActivateAbility()` devolviendo `true` no garantiza que la activación completara.** Para saberlo de verdad, comprobar un **efecto observable** del éxito — aquí, si la ability **consumió el contexto efímero**. Si sigue pendiente, tratar como fallo y limpiar (evita "contexto fantasma").
- **Consumir-y-resetear en la misma operación** (`ConsumeAttackEntryContext`) evita que un contexto de entrada contamine ataques posteriores. Y resetearlo también en `AbilityActorInfoSet()`, porque el ASC del jugador **persiste entre Pawns**.
- **Un enum, no dos booleanos,** para estados mutuamente excluyentes (`EPantheliaDodgeBufferedAction`): hace **imposible** representar "Light y Heavy bufferizados a la vez".
- **Limpiar el buffer SIEMPRE en `EndAbility`** y copiar la acción a una local solo en los caminos legítimos: así una interrupción (muerte, hit react) no dispara un ataque tardío.

---

## 16. Campaña de hardening (13-16 julio 2026) — lecciones reutilizables

Ronda grande de bugfixing (ChatGPT/Codex + Fable vía MCP), cerrada y validada. El detalle vive en `State_Hardening.md` (robustez) y `State_Validation.md` (Data Assets); aquí solo las lecciones que se repetirán:

- **Estado terminal idempotente por handle exacto:** para lifecycles con callbacks asíncronos (acciones de boss, cooldown tasks), guardar el `Handle` exacto y comparar contra él evita que un callback de *otra* entidad cierre la actual. `TrySetTerminalState` es el patrón.
- **Fail-closed, no fail-open:** ante datos inválidos (mesh/spec/socket/radio nulos, dependencias del HUD incompletas), **no hacer nada** es más seguro que continuar con un default. El Weapon Trace, el EffectActor y el `InitOverlay` lo aplican.
- **Ref-count por actor en efectos de zona** (`0→1` aplica, `1→N` no reaplica, `N→1` mantiene, `1→0` retira): evita el doble-apply y el retiro prematuro cuando varias fuentes solapan.
- **Ruta transaccional para reemplazos** (equipamiento): validar y preparar el candidato **antes** de destruir el anterior; si algo falla, el estado previo sobrevive. Nunca "destruir y luego intentar crear".
- **Iterar la lista de abilities siempre con `FScopedAbilityListLock`/`ForEachAbility`** dentro del ASC.
- **Delegates con `AddUObject`/`AddWeakLambda` + bind idempotente** (`bCallbacksBound`): ni fugas ni doble-bind ni callbacks sobre objetos muertos.
- **DFS de grafos sin retener referencias a entradas de un `TMap`** que pueda rehashear durante el recorrido (validador del árbol).
- **La validación del editor es más barata que el bug en runtime:** siete familias de Data Assets con `IsDataValid`; los warnings de contenido (nombres/materiales vacíos) se dejan no bloqueantes a propósito.

---

## 17. Migración a MetaHuman — workarounds y avisos (2026-07-17)

- **Override explícito de `CharacterMesh0` en padre e hijo (workaround validado).** `BP_PantheliaPlayerMetaHumanBase` y `BP_PantheliaPlayerCharacter` fijan explícitamente `Skeletal Mesh = SKM_PantheliaPlayer_Body`, `Anim Class = ABP_PantheliaPlayer` y `Physics Asset Override = None`. Ambos apuntan a la **misma copia canónica** (no hay dos fuentes de contenido).
- **Prohibido `Reset to Inherited` sobre mesh / AnimBP / Physics override sin una prueba aislada.** Hacerlo resolvió el mesh como `None` en runtime → `Equip fallido: WeaponHandSocket no existe en mesh None`. La jerarquía depende de overrides explícitos verificados.
- **No regenerar automáticamente `PHYS_PantheliaPlayer_Body`.** Tiene constraints extra conservadas a propósito (ragdoll aprobado visualmente, sin regresión). 21 cuerpos / 22 constraints, pelvis raíz.
- **No editar assets con Python/MCP mientras PIE está activo.** Crash histórico (`PlayLevel.cpp:553`, GameInstance de PIE retenida por `TransBuffer`). Regla: detener PIE, esperar `CleanupWorld` y confirmar `bSessionEnded=true` antes de operaciones batch; no envolver una sesión PIE en una transacción editorial.
- **Warnings `EnsureDependenciesAreLoaded`** en secuencias duplicadas: deuda editorial no bloqueante (sin refs `None`, sin ensure en PIE). La validación de cook posterior (Fases 4G-4I) pasó con estos warnings presentes; siguen siendo no bloqueantes (ver la sección 18 y `State_Validation.md`).
- **Dependencias package-level residuales** y `retargetSourceAsset` bajo `MetaHumanMigration`: no limpiar por el track visible (ver el protocolo en `State_Pending.md` sección 12).
- **Error legacy `LevelUp` en `BP_ThirdPersonCharacter`:** preexistente, ajeno a la migración; el Blueprint legacy no se instancia en runtime productivo.
- **Fuente de verdad única por dominio** (Body/Physics/AnimBP, locomoción, `DA_PantheliaPlayer_Sword_Basic`, `GA_PantheliaPlayer_Dodge`, `GA_PantheliaPlayer_ParryPhysical`, `GA_Stagger`+`AM_PantheliaPlayer_Stagger`, Assembly MetaHuman, muerte por clases comunes + DeathPresentation): **no crear una segunda variante productiva paralela sin razón funcional.**

---

## 18. Changelog de cierre MetaHuman (Fases 4G-4H-4I, 2026-07-17)

**Configuración / nivel:**
- `DefaultEngine.ini`: `GlobalDefaultGameMode` productivo (`BP_PantheliaGameMode`).
- `Lvl_ThirdPerson`: se retiraron la instancia de `BP_Boss` (TestBoss) de prueba y lógica legacy del Level Blueprint (BossTriggerBox, creación/control de la boss health bar legacy, referencias Unknown y su variable). El **WarriorBoss productivo** sigue en el mapa y entró al package.

**Assets eliminados (Fase 4I) — no restaurar:**
```
/Game/Combat/Weapons/BP_BaseKatana
/Game/ThirdPerson/Blueprints/BP_ThirdPersonGameMode
```

**NO modificado durante 4H/4I:** C++, abilities, montages, timings, costes, tags, balance, assets productivos del jugador. **Sin PIE separado** en 4H/4I (validación por build cocinada + smoke; ver `State_Validation.md`).

**Riesgos residuales (no bloqueantes):** 25 secuencias `MetaHumanMigration` cocinadas; PoseAssets Manny out-of-date; `UnexpectedLoad` de `GC_MeleeImpact` y `GC_Dodge_Perfect`; refs soft/management a paquetes ausentes (`SKM_Manny`/`SKM_Quinn`); TestBoss y Manny aún compartidos/referenciados.

**Rollback / evidencia (conservar, no borrar):**
```
Backup:  C:\Panthelia_Backups\MetaHumanMigration_PrePhase4I_20260717_174204
Archives: ...\Saved\Phase4G_Retry\Archive\Windows  ·  ...\Saved\Phase4I_Final_Retry\Archive\Windows
Logs/audits: ...\Phase4I_Final_Retry\run_phase4i_retry.log · CodexTemp\phase4h_audit.json ·
             CodexTemp\phase4i_post_delete_audit.json · CodexTemp\phase4i_final_retry_readonly_audit.json ·
             CodexTemp\phase4i_final_retry.ps1
```

### 18.1 Tabla canónica de assets legacy

| Asset / familia | Estado final | Acción |
|---|---|---|
| `BP_BaseKatana` | **Eliminado** | No restaurar |
| `BP_ThirdPersonGameMode` | **Eliminado** | No restaurar |
| `BP_ThirdPersonCharacter` | Conservado | Tiene referencias hard |
| `BP_Boss` (asset) | Conservado | La **instancia** del mapa fue retirada |
| TestBoss | Conservado | Referencias existentes |
| Manny ARPG | Conservado | Compartido y muy referenciado |
| `/Game/My_Manny` | Conservado | Fuera de la limpieza autorizada |
| MetaHumanMigration | Conservado (rollback/deuda) | 74 assets |
| Secuencias MetaHumanMigration | Conservadas | 33 disco / 25 cook |
| `RTG_Manny_To_Diosa` | No entra al cook productivo | Mantener según rollback |
| `SKM_Manny` (Heroes ausente) | Metadata/soft residual | No reparar a ciegas |
| `SKM_Quinn` (ausente) | Metadata/soft residual | No reparar a ciegas |
