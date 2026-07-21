# State_Combat — Sistema de Combate y Animaciones

> **Propósito:** Estado del combate físico, detección de golpes, lockon, animaciones y componentes asociados. Lee `State_Overview.md` primero para contexto general.

---

## 1. CombatComponent — ⚠️ Legacy huérfano (retirado del jugador)

**`UCombatComponent`** — Sistema de combos **pre-GAS** del jugador. **Ya retirado** del `AMainCharacter` durante la migración del melee a GAS (sección 10): el combo del jugador ahora lo maneja `UPantheliaPlayerAttackAbility` en C++. El archivo sigue existiendo pero sin uso; su borrado físico está pendiente (ver sección 14). Documentación histórica:
- Array de `UAnimMontage*` con secuencia de ataques
- `ComboCounter` con wrap automático al final del array
- `bCanAttack` reseteado vía AnimNotify al final del montage

---

## 2. TraceComponent — ⚠️ Legacy huérfano (retirado del jugador)

**`UTraceComponent`** — Detección de golpes **pre-GAS** del jugador (box sweep entre `TraceStart`/`TraceEnd`, daño vía `IFighter::GetDamage()` + `TakeDamage()`, sin pasar por GAS). **Ya retirado** del `AMainCharacter` con la migración a GAS (sección 10). El componente ya no está en el jugador; su bloque que usaba `EquippedWeapon` se **neutralizó** (`if (false && ...)` + `FQuat ShapeRotation = FQuat::Identity` de fallback) para que el archivo compile sin romper. Borrado físico pendiente (sección 14). El reemplazo GAS-nativo es `UWeaponTraceComponent` (secciones 9 y 10).

---

## 3. LockonComponent — ✅ Funcional

**`ULockonComponent`** — Target lock estilo Souls:
- Selección de candidatos por **cámara + distancia** (scoring, ver la ampliación abajo), no el primer hit del sweep
- `BreakDistance` para soltar el lockon si el enemigo se aleja
- Solo lockea actores válidos (implementan `IEnemy`, vivos y targeteables)
- Broadcast `OnUpdatedTargetDelegate` cuando el target cambia (incluido a `nullptr` cuando se rompe el lock)

**Quién lo consume:**
- `UPlayerAnimInstance` para alternar entre locomoción libre y de combate (`bIsInCombat`)
- Cámara/controller para orientación (cuando se implemente)

### Fix: lockon ante muerte de enemigos — ✅ Funcional

Cuando un enemigo muere, el lockon se cancela **inmediatamente**, sin esperar a que el actor se destruya (lo que ocurre varios segundos después por el `Lifespan` de la muerte).

- `EndLockon()` se movió de `protected` a `public` en `ULockonComponent` para que el enemigo pueda llamarla
- `APantheliaEnemy::Die()` busca el `LockonComponent` del jugador con `FindComponentByClass<ULockonComponent>()` y llama `EndLockon()` si `CurrentTargetActor == this`
- `TickComponent` además maneja el caso límite donde el actor objetivo se destruye sin pasar por `Die()`: si `IsValid(CurrentTargetActor)` falla con un puntero no nulo, limpia el estado directamente sin llamar `OnDeselect` (evitando un crash por acceso a memoria inválida)

### Detección por canal — requisito `Fighter = Block`

El `ULockonComponent` detecta candidatos con `SweepSingleByChannel` en el canal **Fighter** (`ECC_GameTraceChannel1`). Detalle clave de Unreal: `SweepSingleByChannel` (y `SingleByChannel` en general) solo devuelve hits de tipo **Block**; un actor con `Fighter = Overlap` **no** se detecta.

**Consecuencia:** todo enemigo lockeable debe tener `Fighter = Block` en su Capsule Component (no basta con Overlap).

### Centralización en `BP_PantheliaEnemy` — ✅

El lock-on está **centralizado en el Blueprint padre `BP_PantheliaEnemy`**, de modo que todos sus hijos lo heredan automáticamente. Movidos al padre:
- El Widget Component `LockonWidget` (`WBP_Lockon`, Space = Screen, Visible = false, Z = 35)
- Los eventos `OnSelect` / `OnDeselect` → `SetVisibility(LockonWidget, true/false)`
- `Fighter = Block` en el Capsule Component

Se limpiaron las implementaciones duplicadas que tenía `BP_WarriorBoss`. `BP_Shaman` y futuros enemigos que hereden de `BP_PantheliaEnemy` son lockeables sin configuración adicional.

**Excepción `BP_Boss` (TestBoss):** hereda de `ABossCharacter`, **no** de `BP_PantheliaEnemy`, así que mantiene su propia configuración de lock-on (widget, eventos y `Fighter = Block` propios). Sigue funcionando, pero hay que configurarlo manualmente en él.

**Checklist para hacer lockeable a un enemigo nuevo:**
1. Heredar de `BP_PantheliaEnemy` (implementa `IEnemy` automáticamente) → **los pasos 2-4 ya vienen heredados del padre.**
2. ~~`LockonWidget`~~ — heredado
3. ~~Eventos `OnSelect`/`OnDeselect`~~ — heredados
4. ~~`Fighter = Block`~~ — heredado

> Solo un enemigo que **no** herede de `BP_PantheliaEnemy` (como `BP_Boss`, que viene de `ABossCharacter`) necesita configurar los pasos 2-4 a mano.

### Ampliación soulslike del lock-on (serie de trabajo con Codex/MCP) — ✅ Funcional

Esta serie amplió y estabilizó el sistema sin cambiar su filosofía. Todo validado en runtime. Sigue siendo single-player.

**Selección inicial por cámara/scoring** — la adquisición ya **no** depende de un solo `SweepSingleByChannel` que devuelve el primer actor: ahora busca **múltiples candidatos** y elige el mejor por orientación de cámara + distancia. Un lock-on soulslike debe preferir lo que el jugador **está mirando**, no el primero que aparezca en el radio (evita lock-ons raros con varios enemigos cerca).

**Cambio de target** — `SwitchTarget(float Direction)`: `Direction > 0` cambia a la derecha, `< 0` a la izquierda. Respeta todas las validaciones (target vivo, targeteable, en rango, visible si aplica, y distinto del actual).

**Auto-retarget al morir el target** — al morir el enemigo actual, busca otro válido de inmediato; si no hay, limpia el lock-on. La muerte se maneja **antes** de que el `Lifespan` destruya el actor, así el lock-on no queda apuntando a un cadáver (amplía el "Fix ante muerte" de arriba).

**Auto-lock al golpear con ataque básico** — si **no** hay lock-on activo y un ataque básico impacta a un enemigo válido, se hace auto-lock a ese enemigo. **Pero** si ya hay lock-on sobre A y golpeas a B, **no** cambia de target. Regla de diseño para no robarle el control al jugador en peleas con varios enemigos. (La integración vive en `WeaponTraceComponent` — ver sección 9.)

**Soft-lock melee** — asistencia de orientación para ataques melee **sin** activar el hard lock-on (sin widget, sin `OnSelect`, sin tocar `CurrentTargetActor`). Función `FindBestSoftLockTarget(float RadiusOverride = -1)`: solo actúa si **no** hay `CurrentTargetActor`, usa la misma base de validación, y prioriza enemigos **al frente** y cercanos. Opciones: `bSoftLockOnMeleeAttacks = true`, `SoftLockRadius = 650`, `SoftLockForwardThreshold = 0.25`. API: `SetSoftLockOnMeleeAttacksEnabled(bool)` / `IsSoftLockOnMeleeAttacksEnabled()`. Lo consume `UPantheliaPlayerAttackAbility` (ver `State_Abilities.md`).

**Línea de visión (dos validaciones separadas, distinción importante):**
- `HasRequiredLineOfSightToCandidate()` — para **adquisición / switch / retarget / soft-lock**. Respeta el toggle de "requerir LoS para adquirir". Evita lockear a enemigos tras una pared.
- `HasClearLineOfSightToTarget()` — comprueba la LoS real contra el target **actual**. La **ruptura por pared usa esta directamente**, para no depender del toggle de adquisición.
- Ambas usan el **punto de lock-on** del target, no `GetActorLocation()`.

**Ruptura por oclusión con delay** — si el target queda oculto por una pared durante un tiempo, se rompe el lock-on. Opciones: `bBreakLockonWhenLineOfSightBlocked = true`, `LineOfSightBreakDelay = 0.75`. El delay evita cortes bruscos al pasar por columnas o bordes (no romper al primer frame bloqueado).

**Smoothing de cámara** — interpolación en vez de snap rígido. Opciones: `bSmoothLockonCameraRotation = true`, `LockonCameraRotationInterpSpeed = 12`, `LockonCameraTargetVerticalOffset = -125` (ajustable sin tocar código). Ayuda a la sensación Souls/Lies of P. El helper `GetCameraViewPoint()` centraliza la obtención de ubicación/rotación de cámara.

**Punto de lock-on compartido** — `GetLockonLocation(AActor* TargetActor)` es el **único** punto lógico que consultan cámara, soft-lock, línea de visión, scoring y auto-lock/retarget. Todos miran al mismo sitio.

**`LockonTargetPoint` por enemigo (en `APantheliaEnemy`)** — componente de escena (por defecto ~al torso) que define el punto lógico de lock-on; `GetLockonLocation()` lo consulta si el actor es `APantheliaEnemy`. Se puede mover por Blueprint en cada enemigo/hijo según su tamaño.

> **Modelo mental (lógico vs visual — respetarlo):**
> - **`LockonTargetPoint`** = punto **lógico** de gameplay/cámara/validación.
> - **`LockonWidget`** (Widget Component) = icono **visual**; su posición en mundo la decide el `WidgetComponent` del Blueprint del enemigo. En `BP_PantheliaEnemy` es **hijo de `LockonTargetPoint`** para que siga el punto lógico; los hijos (p. ej. `BP_TrainingDummy`) ajustan su `Relative Location` si el icono debe ir en otro offset.
> - **`WBP_Lockon`** = solo el **dibujo** del icono; **no** decide posición en mundo.

**Organización interna de `ULockonComponent`** — se reorganizó por responsabilidades (API pública, config de búsqueda, config de oclusión, config de cámara, retarget/switch, opciones de asistencia, estado runtime, búsqueda/scoring, validación/LoS, aplicación/limpieza de estado). Limpieza de deuda, sin cambio de comportamiento. **Refactor mayor futuro** (helpers `FTargetingCandidate`, `UTargetingSettingsDataAsset`, un scoring helper, un camera controller helper) queda **explícitamente pospuesto** — el cleanup actual basta; no sobre-arquitecturar todavía.

**`BP_TrainingDummy`** — maniquí de pruebas: hijo de `BP_PantheliaEnemy`, sin IA/ranged/BT (inmóvil), `Fighter = Block`, Actor Tag `Enemy`, recibe daño y muere. Útil para probar lock-on, soft-lock, auto-lock y el target point.

### Pendientes del lock-on

- **Opciones persistentes** (cuando existan menú de opciones + SaveGame): exponer y guardar auto-lock al golpear, soft-lock melee, auto-retarget, romper por pared, smoothing de cámara y su velocidad. No antes de que exista el sistema de opciones.
- **Bosses grandes:** `LockonTargetPoint` al torso superior (no al origen); a futuro, posible target point por fase o por parte del cuerpo, o varios puntos lockeables. No implementar múltiples puntos hasta tener un boss que lo pida.
- **Camera feel polish** (no urgente): ajuste fino de `LockonCameraTargetVerticalOffset` por tipo de enemigo, curvas de cámara para bosses, delay de ruptura distinto para bosses, y comportamiento cuando el jugador está muy pegado al enemigo.

---

## 4. ~~BlockComponent~~ — ❌ Eliminado (2026-07-16)

**`UBlockComponent` se eliminó** en la limpieza legacy de la campaña de hardening: retirado de `AMainCharacter`, desaparecido de `BP_ThirdPersonCharacter`, y sus archivos `.h/.cpp` borrados. Nunca lo usó el sistema de parry/bloqueo (que vive en `UPantheliaParryAbility`, sección 11); parry, guardia y Sprint/Walk siguieron funcionando tras retirarlo. Se documenta aquí solo para el historial: si algún `.md` o comentario viejo lo menciona como componente vivo, está obsoleto.

---

## 5. BaseWeapon — ⚠️ Legacy huérfano (reemplazado)

**`ABaseWeapon`** — Actor de arma legacy (sockets `TraceStart`/`TraceEnd` que leía el `UTraceComponent`). **Reemplazado** por el nuevo sistema de armas data-driven del jugador: `APantheliaWeapon` + `UPantheliaWeaponDefinition` + `UPantheliaEquipmentComponent` (sección 10.1). Sin uso tras la migración; borrado físico pendiente (sección 14).

---

## 6. Sistema de Animaciones — ✅ Funcional básico

### UPlayerAnimInstance

Variables expuestas al AnimBP:
- `CurrentSpeed` — actualizado por `UpdateSpeed()` (llamado desde AnimBP cada tick)
- `bIsInCombat` — togglea según target del lockon (vía delegate del `LockonComponent`)
- `CurrentDirection` — para locomoción strafe en combate

**Patrón:** El AnimInstance se suscribe a delegates de componentes del personaje. No usa `Cast<>` cada tick, mantiene referencias cacheadas.

### UToggleTraceNotifyState — ⚠️ Legacy

AnimNotifyState que encendía/apagaba `UTraceComponent::bIsAttacking` (sistema legacy del jugador). **Reemplazado** por `WeaponTraceNotifyState` (Weapon Trace GAS, sección 9) en los montages del jugador. Sin uso; borrado físico pendiente (sección 14).

### Audio — MetaSound de randomización (clase 204)

`MS_RandomizedSFX_Template` — MetaSound reutilizable para variar un SFX (evita repetición mecánica). Estructura: Output Mono, `Shuffle WaveAsset:Array` con Auto Shuffle, Wave Player (Mono), 2× `RandomFloat` (pitch −1.5/1.5 y volumen 0.85/1.0), Multiply Float, Multiply Audio by Float → Output. **Fix clave:** conectar `On Finished` del Wave Player al `On Finished` del Output (requerido para OneShot). Es un "Lego brick" de audio reutilizable (alineado con el principio 2.9 de `State_Overview.md`).

---

## 7. Input relacionado con combate

- **Ataque ligero:** ✅ mapeado vía GAS (`InputTag.LightAttack` → `GA_PlayerLightAttack` → `UPantheliaPlayerAttackAbility`, sección 10). El buffer de combo se rutea por `AbilityInputTagPressed` (edge-triggered).
- **Bloqueo / parry:** ✅ implementado (`InputTag.Block.Physical` / `.Magic` → `UPantheliaParryAbility`, sección 11). **Lock-on:** ✅ (sección 3).
- **Esquive:** ✅ implementado (`GA_PlayerDodge`, ver sección 13). **Ataque pesado:** `GA_PlayerHeavyAttack` existe y usa tap-vs-hold (con carga); ver `State_Input.md`.

---

## 8. Sistema de Postura / Flinch / Stagger — ✅ Funcional (C++)

Tres mecánicas distintas construidas sobre el atributo `Poise`/`MaxPoise` y el meta atributo `IncomingPoiseDamage` (ver `State_GAS.md`).

### 8.1 Flinch (HitReact por umbral)

Un **solo golpe** que supere `FlinchThreshold%` de `MaxPoise` activa HitReact (montage + inmovilización temporal). Umbrales por defecto: enemigos normales 10%, bosses 15%. Configurable por Blueprint en Details → Panthelia|Combat|Poise. Reutiliza el sistema de HitReact (`GA_HitReact`, ver `State_Abilities.md` sección 9).

### 8.2 Stagger

**Acumulación** de daño de postura hasta que `Poise` llega a 0. Activa `GA_Stagger` (aturdimiento prolongado). Al stagger, la postura se **resetea a MaxPoise** (modelo Elden Ring). Requiere `GA_Stagger` + `GE_Stagger` creados en Blueprint (pendiente, ver más abajo).

### 8.3 Regeneración de postura

Timer en C++ en `APantheliaCharacterBase`. Tras `PoiseRegenDelay` segundos sin recibir golpes, regenera `PoiseRegenRate` unidades/segundo. `ResetPoiseRegenTimer()` se llama desde el AttributeSet con cada golpe (rama `IncomingPoiseDamage`).

### Propiedades en APantheliaCharacterBase

```
FlinchThreshold  (float, EditAnywhere, default = 10.f)
PoiseRegenRate   (float, EditAnywhere, default = 8.f)
PoiseRegenDelay  (float, EditAnywhere, default = 2.5f)
```

### APantheliaEnemy — nuevas propiedades y callbacks

- `bStaggered` (bool, BlueprintReadOnly) — true mientras `Effects.Stagger` está activo
- `StaggerTagChanged()` — callback registrado vía `RegisterGameplayTagEvent`
- `DefensiveElement` (EPantheliaElement, EditAnywhere, default = None) — elemento defensivo para la tabla de afinidades (ver `State_Abilities.md` sección 7). None = neutro.

### ICombatInterface — métodos añadidos

- `GetFlinchThreshold() const` → float (default 10.f)
- `ResetPoiseRegenTimer()` (default vacío)
- `GetDefensiveElement() const` → EPantheliaElement (default None)
- `GetStaggerMontage()` — BlueprintNativeEvent

### Daño de postura en proyectiles

`PoiseDamage` (FScalableFloat) en `UPantheliaDamageGameplayAbility`. Si > 0, se asigna como `SetByCaller(Damage.Poise)`. El ExecCalc lo escribe a `IncomingPoiseDamage`. **No se mitiga** por Armor ni MagicResistance.

### Decisión de diseño (sobreescribe documento anterior)

**Ambos parrys (físico y mágico) dañan postura.** El parry mágico **ya NO** tiene excepción para el elemento Tierra como decía `Gameplay_Mechanics.md`. Este cambio sobreescribe esa decisión anterior.

### Blueprint pendiente

- Crear `GE_Stagger` (Infinite, Grant Tags: `Effects.Stagger`)
- Crear `GA_Stagger` (misma estructura que `GA_HitReact`, con montage más largo)
- Añadir `GA_Stagger` a `DA_CharacterClassInfo → CommonAbilities`
- Asignar `StaggerMontage` en cada BP de enemigo

---

## 9. Weapon Trace System (hitbox por sweep) — ✅ Implementado

Sistema de detección de hits melee para enemigos, al estilo soulslike (Elden Ring, Lies of P). Reemplaza la detección puntual del curso (overlap esférico de radio fijo en un solo socket) por un **sweep continuo a lo largo de toda la hoja del arma**, de modo que las armas de hoja larga detecten al jugador aunque la punta esté lejos del cuerpo.

### Arquitectura

Componente reutilizable **`UWeaponTraceComponent`** (`Combat/`, hereda de `UActorComponent`). Vive en el actor enemigo y se mantiene fuera de `APantheliaEnemy` para no inflar la clase; pensado para que el jugador u otros actores lo reutilicen en el futuro.

**Notify state `UWeaponTraceNotifyState`** (`Animations/`, misma carpeta que `ToggleTraceNotifyState`, con 's'). Marca la ventana de daño en el montage de ataque:
- `NotifyBegin` → `ActivateTrace()`
- `NotifyEnd` → `DeactivateTrace()` (también limpia la lista de ignorados del swing)

### Parámetros configurables (reales)

- `TraceRadius` (float, default **15**) — grosor del hitbox de la hoja (radio de la esfera barrida)
- `WeaponBaseSocketName` (FName, default **"WeaponBase"**) — socket de la empuñadura
- `WeaponTipSocketName` (FName, default **"WeaponTip"**) — socket de la punta
- `TraceChannel` (default **`ECC_GameTraceChannel1`** = canal *Fighter*) — canal de colisión del sweep
- `bDebugMode` (bool) — dibuja la esfera barrida
- `WeaponMeshComponent` — asignable; si es null, se auto-resuelve en `BeginPlay` buscando un componente con tag `"Weapon"` y, como fallback, el primer `StaticMeshComponent`

### Flujo de funcionamiento

1. La ability `GA_MeleeAttack` construye el spec de daño con `MakeDamageSpec()` (ya escalado) y se lo entrega al componente vía `SetDamageSpec()` **antes** del `PlayMontageAndWait`.
2. El `UWeaponTraceNotifyState` del montage activa el trace en `NotifyBegin` y lo desactiva en `NotifyEnd`.
3. Mientras está activo, cada tick el componente hace un `SweepMultiByChannel` con forma de **esfera barrida** entre `WeaponBase` y `WeaponTip` (equivale a una cápsula a lo largo de la hoja).
4. Por cada actor impactado: si no está en la lista de ignorados del swing **y** `IsNotFriend` devuelve true, recibe el spec pre-construido vía `ApplyGameplayEffectSpecToSelf` y se añade a la lista de ignorados.
5. La lista de ignorados previene múltiples hits del mismo swing (un golpe = un daño por actor).

> **Fix de detección (depuración WarriorBoss).** Dos correcciones para que el trace registrara hits: (1) **decidir por `HitResults.Num() > 0`, no por el booleano** que devuelve `SweepMultiByChannel` — ese `bool` puede ser `false` aunque el array traiga hits válidos (depende del tipo de bloqueo/overlap). (2) El canal **`Fighter` estaba en `DefaultResponse = Ignore`** en `DefaultEngine.ini`; se añadió la respuesta apropiada en el **constructor base** (`PantheliaCharacterBase`) para que los personajes respondan al canal. Nota: la cápsula no siempre genera el hit — el trace puede resolverse contra `CharacterMesh0` / physics asset. Ver la lección en `Code_Review.md`.

> **Patrón "spec diferido" (importante):** el componente **no construye** el daño; solo aplica un spec ya hecho. La ability lo construye con `MakeDamageSpec()` y lo inyecta con `SetDamageSpec()`. Es el mismo patrón que usan los proyectiles (`APantheliaProjectile::DamageEffectSpecHandle`), lo que mantiene una sola fuente de verdad para el daño. Ver `State_Abilities.md` secciones 2 y 7.

> **Auto-lock al golpear (serie lock-on):** cuando un trace impacta a un actor válido y el ataque lo permite, el `WeaponTraceComponent` avisa al `ULockonComponent` para hacer auto-lock — **solo si no hay lock-on activo** (si ya hay target, no cambia). Mantiene el filtrado por bando y la aplicación de daño por GAS intactos; el componente sigue sin construir daño. Ver la regla completa en la sección 3.

### Integración con GAS

El daño pasa por el pipeline existente: spec de `DamageEffectClass` → `ExecCalc_Damage` (Armor, resistencias elementales, afinidades, crítico, daño de postura). **NO** usa `TakeDamage` clásico ni `IFighter`. El escalado por atributos lo aplica `MakeDamageSpec()` mediante el método centralizado `ApplyDamageScalingToSpec()` de la clase base de daño (hallazgo **D1 resuelto** — ver `State_Abilities.md`).

> **Interacción con `AttackSpeed`** (atributo planificado, `State_GAS.md` sección 7.7): la ventana de daño del Weapon Trace está anclada a frames del montage vía `WeaponTraceNotifyState`, así que cuando `AttackSpeed` acelere el play rate, la ventana se acelera con él automáticamente — no hay que tocar el componente. El cap de `AttackSpeed` en **2.5** protege además contra el "tunneling" del sweep (el arma atravesando un objetivo entre dos ticks) a velocidades extremas.

### Feedback de impacto: Gameplay Cue de melee + sonido del arma (clases 205-206, A.6)

Al conectar un golpe, el `WeaponTraceComponent` dispara feedback visual y sonoro:

- **Gameplay Cue de melee** (clase 206): tras aplicar daño a un actor, `PerformTrace` dispara `OwnerASC->ExecuteGameplayCue(GameplayCue.Melee.Impact, CueParams)` con `Location = Hit.ImpactPoint`, `SourceObject = HitActor` (la víctima, para leer su `GetBloodEffect`), `EffectCauser = Owner` (el atacante) y `AggregatedSourceTags += ActiveMontageTag`. El tag de montage activo se fija con `SetActiveMontageTag(FGameplayTag)`. Lo consume el Blueprint **`GC_MeleeImpact`** (`GameplayCueNotify_Static`): en `OnExecute` spawnea el `BloodEffect` de la víctima (`GetBloodEffect(SourceObject)`) y reproduce el `ImpactSound` del `FTaggedMontage` cuyo tag coincide con `AggregatedSourceTags`. Ver el tag en `State_GAS.md`.
- **Sonido de impacto del arma del jugador** (A.6): el arma define su `ImpactSound` (`PantheliaWeaponDefinition`, Weapon|Impact — corte de espada, golpe de hacha…). `PantheliaPlayerAttackAbility::SetupWeaponTraceForCurrentAttack` lo pasa al trace con `SetActiveImpactSound(WeaponDef->ImpactSound)`. `PerformTrace` lo reproduce con `PlaySoundAtLocation` en `Hit.ImpactPoint`, usando un flag local `bImpactSoundPlayedThisTrace` para sonar **una sola vez por frame de trace** aunque el sweep golpee a varios actores (el blood y el Cue sí se disparan por víctima). Es **directo, no por Cue**, por ser single-player.
- **Modelo de diseño (souls):** el sonido de impacto = arma + material del objetivo. Por ahora solo está el **eje arma**. **Gancho futuro documentado** (en `PantheliaWeaponDefinition.h`): un mapa `SurfaceType → Sound` leyendo `Hit.PhysMaterial` (ya disponible en el sweep) para el eje **material** (carne/metal/madera). Requiere crear Physical Materials en el editor; no se implementa hasta entonces.

> **Config de editor pendiente (clase 206):** `GameplayCueNotifyPaths` debe incluir `/Game/Blueprints/AnimNotifies` (Project Settings → Game → Ability System) y reiniciar el editor, o el Cue no se encuentra. La verificación real es que el log no tenga errores "Cue tag not found" (confirmado limpio). La ventana *Gameplay Cue Editor* no se localizó en UE5.8, pero es solo diagnóstico, no necesaria.

**Campos de feedback en `APantheliaCharacterBase`** (heredados por todos los personajes; assets pendientes de asignar por BP):
- `BloodEffect` (`UNiagaraSystem`, Combat|Impact) — efecto que se spawnea cuando **este** personaje es golpeado; se obtiene de la **víctima** vía `GetBloodEffect()` (`BlueprintNativeEvent` en `ICombatInterface`), de modo que cada personaje tiene su propio efecto (sangre roja/verde, chispas). Lo spawnea el `GC_MeleeImpact`.
- `DeathSound` (`USoundBase`, Combat|Impact) — se reproduce con `PlaySoundAtLocation` al inicio de `MulticastHandleDeath` (antes del dissolve). Single-player, directo sin Cue. Ver `State_Abilities.md` sección 9 (muerte).
- El struct `FTaggedMontage` (en `CombatInterface.h`) ganó un campo `ImpactSound` (`USoundBase`): sonido por montage de ataque, leído por el `GC_MeleeImpact` según el tag del golpe.

> **⚠️ Lección de assets (clase 229):** activar **"Kill Particles When Lifetime Has Elapsed"** en **todos** los sistemas Niagara (`GC_MeleeImpact`, efectos de parry, `BloodEffect`…). El símbolo de infinito en el lifetime = fuga de memoria.

### GA_MeleeAttack (ability de ataque enemiga)

- AbilityTag: `Abilities.Attack.Melee` (el BT lo activa con `TryActivateAbilitiesByTag`; ver el sistema melee/ranged/híbrido en `State_Abilities.md` sección 8).
- Construye el spec con `MakeDamageSpec()` y lo entrega al `UWeaponTraceComponent` con `SetDamageSpec()` antes del montage.
- Filtra el objetivo con `IsNotFriend` (Branch antes de aplicar daño), de modo que un enemigo no dañe a otro enemigo.
- Selección de montage **aleatoria** (`RandomIntegerInRange`), sin lógica de combos. El `RandomInt` se promueve a la variable `SelectedMontage` para evitar evaluarlo dos veces (clase 188).
- `bStopWhenAbilityEnds = false` en el `PlayMontageAndWait` (clase 188).
- `GA_HitReact` cancela las abilities con el tag genérico `Abilities.Attack` (clase 188), que cubre tanto melee como ranged: recibir un golpe interrumpe el ataque enemigo.

> **Limpieza al flujo moderno (serie lock-on).** Se eliminó el bloque **viejo** de aplicación de daño por radio/evento (`WaitGameplayEvent → GetCombatSocketLocation → DrawDebugSphere → GetLivePlayersWithinRadius → ForEachLoop → IsNotFriend → CauseDamage`) y quedó solo el flujo moderno con `WeaponTraceComponent`. Flujo final: `ActivateAbility → GetCombatTarget → UpdateFacingTarget → GetAttackMontages → GetRandomTaggedMontageFromArray → Set SelectedMontage → MakeDamageSpec → GetComponentByClass(WeaponTraceComponent) → SetDamageSpec → PlayMontageAndWait → EndAbility`, con `OnCompleted`/`OnInterrupted`/`OnCancelled → EndAbility` y `OnBlendOut` sin conectar. El daño lo aplica ahora el `WeaponTraceNotifyState` del montage (confirmado: dummy y Warrior mueren con ataques ligeros). De paso se corrigió un warning "`IsNotFriend was pruned because its Exec pin is not connected`": `IsNotFriend` es impuro, así que su `Exec` debe cablearse (`ForEachLoop → IsNotFriend → Branch`) o Unreal poda el nodo y usa el valor por defecto. Ver `Code_Review.md`.

### Origen de los sockets del trace — dos estrategias soportadas

El `UWeaponTraceComponent` lee `WeaponBase` (empuñadura) y `WeaponTip` (punta) de un mesh; ese mesh puede ser de dos tipos, y el componente soporta ambos:

- **A) Sockets en el `SkeletalMesh` del personaje** (bajo `hand_r`): el arma "vive" en el propio esqueleto. Es el caso del **TestBoss** (`BP_Boss`), con `WeaponBase`/`WeaponTip` en el skeleton.
- **B) Sockets en un `StaticMeshComponent` de arma** (p. ej. `SM_Sword`): el arma es un componente aparte con sus propios sockets. Es el caso del **WarriorBoss**.

> **WarriorBoss usa la opción B (setup confirmado en depuración):**
> - `WeaponTraceComponent.WeaponMeshComponent = SM_Sword` (apunta a `SM_Sword_GEN_VARIABLE`).
> - Sockets `WeaponBase`/`WeaponTip` en el propio `SM_Sword`.
> - `SM_Sword` es la **espada visual real** (hijo de `CharacterMesh0`, `Parent Socket = hand_r`, tag `Weapon`; ocultarla la quita del viewport).
> - **No** cambiar `WeaponMeshComponent` a `CharacterMesh0`: el sistema ya aplica daño bien, y `WeaponMeshComponent` lo tratan como el arma visual/física real otros sistemas (muerte/dissolve/desprendimiento). La antigua nota que situaba los sockets del WarriorBoss bajo `SK_Mannequin/Hand_R` quedó **obsoleta para este setup**.
> - Todos los **montages de ataque del WarriorBoss** deben usar el slot **`FullBody`** (ver `State_AI.md`).

### Integración con nuevos enemigos

El `UWeaponTraceComponent` necesita configuración por enemigo según **cómo esté representada el arma**:

- **Arma como Static Mesh Component** (ej. WarriorBoss con `SM_Sword`): el componente la auto-resuelve en `BeginPlay` (tag `"Weapon"` y, si no, el primer `StaticMeshComponent`). Basta con dejarlo o asignar el mesh directamente en los Details del componente.
- **Arma como Skeletal Mesh Component heredado de C++** (ej. TestBoss con `FinalWeaponMesh`, heredado de `PantheliaCharacterBase`): el dropdown de los Details **no muestra** componentes heredados de C++. Hay que asignarlo en el **Construction Script**: `Get FinalWeaponMesh` → `Set WeaponMeshComponent` (sobre el `WeaponTraceComponent`).

**Requisito de `GA_MeleeAttack`:** la ability obtiene el componente con `GetComponentByClass(WeaponTraceComponent)`. Si un enemigo melee **no** tiene el componente añadido, el nodo devuelve `None` y se produce un `Accessed None` en runtime. **Todos** los enemigos que usen `GA_MeleeAttack` deben llevar el `UWeaponTraceComponent`.

**TestBoss (BP_Boss):**
- Arma: `FinalWeaponMesh` (Skeletal Mesh Component) — se asigna al trace vía Construction Script (caso de arriba).
- Sockets `WeaponBase` y `WeaponTip` en el **skeleton del personaje** (no en un arma separada), bajo `hand_r`. `WeaponTip` ya existía; `WeaponBase` se creó en la empuñadura del Halberd.
- `AttackMontages` ampliado con montages nuevos, todos con `WeaponTraceNotifyState` en sustitución del antiguo `AN_MontageEvent`.

**Reutilización por el jugador:** el componente está diseñado para cualquier actor desde el inicio, no solo enemigos; migrar al jugador es cuestión de añadirlo y configurar sus sockets/arma.

### Filtrado de bando — `IsNotFriend`

`UPantheliaAbilitySystemLibrary::IsNotFriend(AActor* A, AActor* B)` (BlueprintPure). Compara Actor Tags `"Player"` / `"Enemy"`: devuelve true solo si los actores son de bandos distintos. Elimina el friendly fire entre enemigos. Se usa tanto en `GA_MeleeAttack` como dentro del Weapon Trace. (Actor Tags: ver `State_Overview.md`.)

### Relación con `UTraceComponent` (legacy)

El `UTraceComponent` (sección 2, sistema pre-GAS del jugador: `TakeDamage` + `IFighter` + sweep de caja) queda como legado del jugador. `UWeaponTraceComponent` es independiente y GAS-nativo. A futuro se podría migrar el jugador a él y deprecar el viejo.

### Qué deja obsoleto (solo melee de enemigos)

- `GetCombatSocketLocation` (por tag) — fuera del flujo melee enemigo
- `GetLivePlayersWithinRadius` — fuera del flujo melee enemigo
- `AN_MontageEvent` puntual + `WaitGameplayEvent` en el ataque — sustituido por el sweep continuo

(Siguen existiendo para proyectiles u otros usos.)

### Fix de colisión del WarriorBoss

A la cápsula del WarriorBoss le faltaba el canal `Projectile = Overlap` (el Boss genérico sí lo tenía); sin él los proyectiles lo atravesaban. Corregido en su Blueprint.

### Decisiones de diseño de IA confirmadas

- **Avoidance (RVO):** desactivado — el strafing sin animación se ve mal.
- **Motion Warping** de los ataques enemigos: impreciso/falla a veces. Comportamiento conocido, se pulirá en clases posteriores.
- **Dissolve del WarriorBoss al morir:** pendiente, no urgente (ver `State_Abilities.md` sección 11.4).

---

## 10. Sistema de melee del jugador (armas + combo GAS) — ✅ Funcional

El ataque básico del jugador se **migró de un sistema legacy NO-GAS** (`UCombatComponent` + `UTraceComponent` + `IFighter::GetDamage` + `TakeDamage`) **a GAS**, con un sistema de armas data-driven, combo con buffer de input estilo Dark Souls, y daño que pasa por el mismo pipeline que enemigos y proyectiles (`ExecCalc_Damage`). Los sistemas legacy (secciones 1, 2 y 5) quedan huérfanos.

> **Fixes de armas v2.1 — ✅ validados en runtime** (buildup por arma, stamina por arma y por golpe, seguridad del trace al desequipar, sockets por arma): C++ implementado, compilación y pruebas PIE confirmadas por Romelt. *(La v1 de esta especificación fue **descartada** por usar la arquitectura elemental antigua; la **v2.1 es la definitiva**.)*

> **Assets productivos del jugador MetaHuman** (migración funcional cerrada, validada en PIE). El jugador productivo (cadena `BP_PantheliaPlayerCharacter → BP_PantheliaPlayerMetaHumanBase → AMainCharacter`, ver `State_Overview.md`) usa estos assets **sin duplicar la lógica C++**:
> - **Weapon Definition:** `DA_PantheliaPlayer_Sword_Basic` (actor de arma `BP_Sword_Basic`; daño/poise/buildup/costes/sockets/trace sin cambios respecto a la spec de armas). Socket del Body: **`WeaponHandSocket`** (hueso `hand_r`).
> - **Montages de ataque** (`DefaultSlot`): `AM_PantheliaPlayer_LightAttack01-03`, `HeavyAttack01-02`, `ChargedHeavyAttack` (3 segmentos), `DodgeFollowupLightSword`, `DodgeFollowupHeavySword`. Abilities **compartidas** `GA_PlayerLightAttack`/`GA_PlayerHeavyAttack` (no duplicadas — solo cambian los montages vía Weapon Definition).
> - **Dodge:** `GA_PantheliaPlayer_Dodge` (padre C++ `UPantheliaPlayerDodgeAbility`, en `startupAbilities`, reemplaza a `GA_PlayerDodge_MH`; coste 20 estamina; 8 montages `AM_PantheliaPlayer_Dodge_*`). La ventana de follow-up (frames ~15-50) se conserva por feeling.
> - **Parry físico:** `GA_PantheliaPlayer_ParryPhysical` (padre C++ `UPantheliaParryAbility`, reemplaza a `GA_Parry_Physical_MH`; ventana 0.2 s; input `InputTag.Block.Physical`). Guardia: `GuardStart`/`GuardLoop` + secciones `ParryHit`/`BlockHit`.
> - **Stagger:** `GA_Stagger` **compartida** + `AM_PantheliaPlayer_Stagger`. El **Guard Break reutiliza el pipeline de Stagger** (sin implementación paralela).
> - Validado en PIE: combo ligero/pesado/charged, dodge 8 direcciones + follow-ups, i-frames, Perfect Dodge, guardia vs normal/pesado, Perfect Parry, Stagger, Guard Break, WeaponTrace y daño. La muerte multipart y el handoff del arma en `State_GAS.md` (DeathPresentation).
> - **Validación por build cocinada (Fases 4G-4I, Development Win64):** ataque ligero, WeaponTrace, aplicación de daño, dodge, guardia, parry, muerte y ragdoll multipart con cierre del trace al morir — todo PASS en el ejecutable cocinado. Durante 4H/4I **no** se modificaron costes, timings, notifies, tags ni balance, y **no** se reabrió dodge/parry. Deuda separada (no bloqueante): física definitiva de armas; dissolve multipart (solo si se decide); **respawn no implementado**. Ver `State_Validation.md`.

### 10.1 Sistema de armas data-driven

**`EWeaponType`** (`PantheliaWeaponTypes.h`) — enum del tipo de arma (`Sword`; expandible).

**`UPantheliaWeaponDefinition`** (`PantheliaWeaponDefinition.h`, DataAsset que hereda `UPrimaryDataAsset`) — todos los **datos** del arma:
- `WeaponName`, `WeaponType`
- `WeaponElement` (`EPantheliaElement`: None/Fire/Water/Storm/Nature) — **el mismo enum que usará el corazón elemental**, para sinergias futuras. El arma solo declara su elemento; la lógica de sinergia llega con el sistema del corazón.
- `LightAttackMontages` / `HeavyAttackMontages` (`TArray<TObjectPtr<UAnimMontage>>`)
- `DamageTypes` (`TMap<FGameplayTag, FScalableFloat>`), `AttributeScalings` (máx 2), `PoiseDamage`
- **`BuildupAmounts`** (`TMap<FGameplayTag, FScalableFloat>`, clave = tipo de daño como `Damage.Magical.Fire`) — **el buildup elemental ahora vive en el arma** (fix v2.1). Antes salía del Blueprint de la ability, así que **todas** las armas ligeras compartían buildup y cambiar de arma no cambiaba su identidad elemental. Ahora se puede diseñar limpiamente *"daga de bajo daño y alto Veneno"* vs *"espadón de alto daño y bajo buildup"*. **El arma define solo CUÁNTO se acumula, no qué hace el estado** (eso es `DA_ElementalStatusConfig`; ver sección 12.2).
- **`DodgeLightAttackMontage` / `DodgeHeavyAttackMontage`** — openers post-dodge por arma (sección 13.3); si están vacíos, cae al montage normal `[0]`.
- `LightAttackStaminaCost` (15) / `HeavyAttackStaminaCost` (30) — **ahora sí se usan** (fix v2.1): los lee `CheckCost`/`ApplyCost` de la ability y **cada golpe del combo paga el suyo** (antes solo el primero). Ver `State_GAS.md` (`Cost.Stamina`).
- `WeaponBaseSocketName` (`WeaponBase`) / `WeaponTipSocketName` (`WeaponTip`) — **ahora llegan al trace** (fix v2.1): `SetupWeaponTraceForCurrentAttack()` los envía junto al mesh, así cada arma puede usar sus propios nombres (`SpearBase`/`SpearTip`, `AxeTraceStart`/`AxeTraceEnd`…). Antes todas dependían de los nombres por defecto del componente. **Prerequisito para una segunda familia real de armas.**
- Mejora: `MaxUpgradeLevel` (10), `RequiredUpgradeMaterial` (TSoftClassPtr<AActor>), `UpgradeMaterialCostPerLevel` (TArray<int32>)
- **Bloque "Defense"** (usado por el sistema de parry/bloqueo, sección 11): los multiplicadores de mitigación cruzada — `MagicOnPhysicalParryMultiplier` (0.8), `PhysicalOnMagicParryMultiplier` (0.7), `PhysicalImperfectBlockMultiplier` (0.6), `MagicImperfectBlockMultiplier` (0.5) — más `ParryStaminaCost` / `BlockStaminaCost` y el poise de parry. Centralizar estos valores en el arma (no en la ability) es coherente con la Decisión 2.9.
- **El mesh NO vive aquí.** Los campos de mesh se eliminaron del DataAsset a propósito (decisión de escalabilidad alineada con 2.9 de Overview: datos en el DataAsset, presentación en el BP del arma).

**`APantheliaWeapon`** (`PantheliaWeapon.h/.cpp`, Actor) — la **representación física** del arma:
- Root + `SkeletalMeshComponent` + `StaticMeshComponent` (ambos sin colisión). El mesh se asigna y posiciona en el **viewport del BP del arma**, no por código.
- `GetActiveMeshComponent()` detecta cuál componente tiene mesh (prioriza skeletal); lo usa el Weapon Trace para leer los sockets de la hoja.
- `InitializeFromDefinition()` ya no toca el mesh (hook para lógica futura por datos, p. ej. VFX por elemento). `OnConstruction` override vacío (hook futuro).

**`UPantheliaEquipmentComponent`** (`PantheliaEquipmentComponent.h/.cpp`, `UActorComponent`):
- `EquipWeapon(TSubclassOf<APantheliaWeapon>, UPantheliaWeaponDefinition*)` — spawnea el arma y la attachea al `HandSocketName` (default `WeaponHandSocket`).
- `UnequipWeapon`, `GetEquippedWeapon`, `GetEquippedWeaponDefinition`, delegate `OnWeaponEquipped`.
- `DefaultWeaponClass` / `DefaultWeaponDefinition` — arma inicial automática en `BeginPlay`.
- Un slot por ahora, estructurado para crecer a inventario.

### 10.2 `UPantheliaPlayerAttackAbility` (combo en C++)

Hereda de `UPantheliaDamageGameplayAbility`. Maneja **todo el combo en C++** (el Event Graph del Blueprint queda vacío):
- `EPlayerAttackType` (Light/Heavy) + UPROPERTY `AttackType`.
- `ActivateAbility`: `CommitAbility` (stamina) → si falla, `EndAbility`; si pasa, reproduce el golpe actual.
- `CanActivateAbility` (override): **rechaza reactivar si la ability ya está activa.** Crítico — el input es Held (cada frame); sin esto, `AbilityInputTagHeld` reactivaría la ability en cualquier instante libre y reiniciaría el combo (era el bug "solo golpe 1").
- `TryBufferComboInput()`: marca el buffer si la ventana está abierta (máximo **1**, un bool). Reemplazó al override `InputPressed` (que con input Held se disparaba cada frame).
- `OpenComboWindow()` / `CloseComboWindow()`: los llama el notify. Al cerrar: si hay buffer → `AdvanceAndPlayNext`; si no → deja terminar el montage.
- `PlayCurrentComboMontage()`: **desengancha los callbacks de la task anterior antes de crear la nueva** (evita que el montage viejo termine la ability al encadenar). Crea un `UAbilityTask_PlayMontageAndWait`.
- `AdvanceAndPlayNext()`: avanza `ComboIndex` cíclico y reproduce el siguiente montage.
- `SetupWeaponTraceForCurrentAttack()`: pasa el mesh del arma + el spec al Weapon Trace.
- `MakeWeaponDamageSpec()`: copia `DamageTypes`/`AttributeScalings`/`PoiseDamage` **del arma** a las propiedades heredadas + `ApplyDamageScalingToSpec` (scaling completo, compartido con enemigos y proyectiles).
- `OnMontageCompleted` / `OnMontageInterruptedOrCancelled`: terminan la ability. `EndAbility` resetea `ComboIndex`/flags/task.
- `ResetCombo()`: público, fallback de seguridad (lo llama el ABP vía `MainCharacter`).
- **FIX crítico de carrera:** NO se conecta `OnBlendOut` a la terminación. El blend out empieza antes del fin real del montage; conectarlo hacía que la ability terminara (reseteando el buffer) antes de que el `ComboWindowNotifyState` cerrara la ventana. Solo se termina en `OnCompleted` (fin real) o interrupción.
- **Rotación asistida por soft-lock (serie lock-on):** al calcular hacia dónde mira el ataque, la ability sigue esta prioridad: (1) si hay **hard lock-on** activo, mira al `CurrentTargetActor`; (2) si no, prueba `FindBestSoftLockTarget()` del `ULockonComponent`; (3) si hay soft target, rota hacia `GetLockonLocation(SoftTarget)`; (4) si no, usa la dirección de input/movimiento; (5) si no hay input útil, conserva la rotación actual. El soft-lock **no** cambia `CurrentTargetActor` ni muestra widget — solo orienta el golpe (asistencia sin lock-on visual). Ver `State_Combat.md` sección 3 (soft-lock).

**`UComboWindowNotifyState`** (`ComboWindowNotifyState.h/.cpp`): `NotifyBegin` → busca la ability de ataque activa en el ASC y llama `OpenComboWindow`; `NotifyEnd` → `CloseComboWindow`. **Debe cerrar con margen antes del fin del montage** — si cierra pegado al final, pierde la carrera con `OnCompleted`.

### 10.3 Routing del input de combo

El buffer se rutea **edge-triggered**, no por el input Held:
- `UPantheliaAbilitySystemComponent::NotifyComboInputPressed(InputTag)` — busca la ability de ataque activa y le marca el buffer vía `TryBufferComboInput()`.
- `APantheliaPlayerController::AbilityInputTagPressed` (antes vacío) ahora llama a `GetASC()->NotifyComboInputPressed(InputTag)`. Se dispara una vez por pulsación real (`ETriggerEvent::Started`), no cada frame. Fue clave: el override `InputPressed` de GAS con input Held no era fiable.

### 10.4 Integración en `AMainCharacter` y Weapon Trace

- `AMainCharacter` **añade** `EquipmentComponent` y `WeaponTraceComponent` (en el constructor) y `ResetPlayerCombo()` (BlueprintCallable; busca la ability en el ASC y llama `ResetCombo`, evitando el nodo "Get Active Abilities by Class" difícil de hallar en BP). **Retira** el melee legacy: `UCombatComponent`, `UTraceComponent`, `EquippedWeapon` (legacy `ABaseWeapon`), `WeaponClass`, el spawn legacy en `BeginPlay`, el include de `BaseWeapon`, y la interfaz `IFighter`/`GetDamage()`. Conserva `LockonComponent`, `PlayerActionsComponent` y todo GAS (el `BlockComponent` se eliminó después, ver sección 4). Cada retirada está documentada con comentarios `//`.
- `UWeaponTraceComponent` **añade** `SetWeaponMeshComponent(UPrimitiveComponent*, FName InBaseSocketName, FName InTipSocketName)` — cambio **aditivo** (no afecta a los enemigos, que siguen usando `ResolveWeaponMesh`). Necesario porque el arma del jugador es un Actor separado.

> **Seguridad del trace al desequipar (fix v2.1).** El jugador usa ahora una **fuente de arma externa explícita**: un flag (`bUsesExternalWeaponSource`) que en `true` **prohíbe** `ResolveWeaponMesh` — el mesh **debe** inyectarse desde fuera. `AMainCharacter` lo activa **en el constructor** (no en BeginPlay), para no depender de que el jugador haya atacado al menos una vez. Sin esto, al desequipar el arma el componente podía **auto-adoptar un mesh arbitrario** del owner (p. ej. el del personaje) y seguir haciendo daño.
> **`ClearExternalWeaponTraceSource()`** (llamado por `UnequipWeapon()` **antes** de destruir el actor del arma) desactiva la ventana de trace, limpia el mesh, **invalida el DamageSpec**, limpia el montage tag y el sonido de impacto, quita el permiso de auto-lock, vacía los actores ignorados y reinicia los flags del swing (manteniendo el modo externo activo).
> **Protección ante notifies tardíos:** al abrir la ventana se exige, en modo externo, un **mesh válido** y un **`DamageSpecHandle` válido**; si falta alguno, `bIsTracing = false` y return. Un `WeaponTraceNotifyState` tardío **no puede reactivar** un ataque con un arma ya destruida.
> **Los enemigos no activan el modo externo:** conservan su auto-resolución por tag `Weapon` y su flujo de `GA_MeleeAttack` intactos (requieren prueba de regresión con un enemigo melee).

### 10.5 Decisión de diseño del combo (buffer estilo Dark Souls)

Investigación confirmada: Souls clásico encola hasta 4 inputs; Elden Ring fue criticado por buffer largo/pegajoso (no puedes redirigir el input encolado); Lies of P casi no tiene buffer y se siente menos fluido para encadenar. **Decisión Panthelia:** buffer corto de **máximo 1 input**, ventana acotada por el notify, el combo expira si no encadenas dentro de la ventana. Fluidez sin el problema de ER.

### 10.6 Bugs resueltos durante la migración

1. **Dos espadas:** el jugador spawneaba el arma legacy y la nueva → resuelto al retirar el legacy del `MainCharacter`.
2. **No hacía daño:** doble causa — los montages usaban `ToggleTraceNotifyState` (legacy) en vez de `WeaponTraceNotifyState`, y el DataAsset no tenía `DamageTypes` → resuelto cambiando el notify, rellenando `DamageTypes` y los sockets `WeaponBase`/`WeaponTip`.
3. **Katana mal posicionada:** el mesh estaba en el DataAsset sin viewport → resuelto moviéndolo al BP del arma.
4. **Combo solo hacía el golpe 1** (tres causas encadenadas): (a) `AnimNotify_ResetAttack` reseteaba el combo en cada montage → quitado; (b) el input Held reactivaba la ability cada frame → resuelto con `CanActivateAbility`; (c) carrera de eventos con `OnBlendOut` → resuelto quitando `OnBlendOut` de la terminación + margen en la ventana del notify.
5. **Buffer no fiable:** el override `InputPressed` con input Held se disparaba cada frame → resuelto ruteando el buffer por `AbilityInputTagPressed` (edge-triggered).

### 10.7 Pasos de editor (referencia)

`GE_Cost_LightAttack` (Stamina −15, Instant) / `GE_Cost_HeavyAttack` (−30). `GA_PlayerLightAttack` (parent `PantheliaPlayerAttackAbility`, AttackType=Light, DamageEffectClass=GE_Damage, su CostGE, StartupInputTag=`InputTag.LightAttack`, Instancing=Instanced Per Actor, Ability Tag `Abilities.Attack`, **Event Graph vacío**), concedida en StartupAbilities. EquipmentComponent: DefaultWeaponClass=`BP_Sword_Basic`, DefaultWeaponDefinition=`DA_Sword_Basic`, HandSocketName=`WeaponHandSocket` (socket creado en `hand_r`). `DA_Sword_Basic`: daño `Damage.Physical`, 3 montages light. Los montages llevan `WeaponTraceNotifyState` + `ComboWindowNotifyState` (sin el `ToggleTraceNotifyState` ni `AnimNotify_ResetAttack` legacy). `BP_Sword_Basic`: mesh asignado al StaticMeshComponent y posicionado en el viewport. `ABP_Player`: el reset legacy se reemplazó por `AnimNotify_ResetAttack → Cast MainCharacter → ResetPlayerCombo`.

> Guía completa para crear un arma nueva: **`Flujo_Nueva_Arma_Panthelia.md`** (7 pasos: BP del arma + mesh + sockets + DataAsset + vincular + montages + equipar).

---

## 11. Sistema de Parry / Bloqueo / Guardia — ✅ Funcional

Sistema de defensa activa estilo **Lies of P** ("perfect guard"), implementado en 3 fases (estado/animación, mitigación de daño, reacción) más la guardia upper-body, el knockback y los Gameplay Cues.

> **Nota de fuentes:** el ASC, el `ExecCalc_Damage` y el `PlayerAnimInstance` están **desincronizados en `/mnt/project`** (las copias de solo lectura son versiones viejas). Esta documentación se basa en los archivos entregados por la sesión de implementación, confirmados como funcionales. Ver `Code_Review.md` sección 8.

### 11.1 Diseño general y terminología

- **Terminología del proyecto:** **PARRY** = bloqueo *perfecto* (el golpe se recibe dentro de la ventana). **BLOQUEO** = *imperfecto* (el golpe llega fuera de la ventana, mientras se mantiene la guardia).
- **Dos defensas:** **Física** (tecla E, arma/brazo derecho) y **Mágica** (tecla Q, elemento/brazo izquierdo). Son inputs distintos; para cada uno, el mismo botón hace parry o bloqueo según el **timing**.
- **Una sola clase C++** `UPantheliaParryAbility` parametrizada por `enum EParryType { Physical, Magic }`. Dos Blueprints derivados: **`GA_Parry_Physical`** y **`GA_Parry_Magic`**, que solo fijan el enum y asignan sus montages. Toda la lógica es compartida (por eso mejoras como la guardia upper-body aplican a ambos sin código extra).
- El parry **físico también bloquea hechizos** (solo mitiga, no anula). El daño de postura al enemigo **solo** ocurre en un parry *perfecto del tipo correcto*.
- **La postura NUNCA tiene barra visible** (guardrail soulslike, ver sección 8).
- Los costes de estamina, los multiplicadores y el poise salen del **arma equipada** (`PantheliaWeaponDefinition`, bloque "Defense") — centralizados según la Decisión Arquitectónica 2.9. Ver sección 10.1.

### 11.2 Matriz de defensa

Resultado según la defensa activa y el tipo de daño entrante (multiplicadores configurables en el arma):

| Defensa | vs Daño Físico | vs Daño Mágico |
|---|---|---|
| **Parry Físico perfecto** | anula (×0) + daño de postura al enemigo + efectos | mitiga ×0.8 (`MagicOnPhysicalParryMultiplier`), sin postura |
| **Parry Mágico perfecto** | mitiga ×0.7 (`PhysicalOnMagicParryMultiplier`) | anula (×0) + postura + efectos (+ anular barra de estado, futuro) |
| **Bloqueo Físico** (imperfecto) | mitiga ×0.6 (`PhysicalImperfectBlockMultiplier`) | — |
| **Bloqueo Mágico** (imperfecto) | — | mitiga ×0.5 (`MagicImperfectBlockMultiplier`) |

### 11.3 Fase 1 — Máquina de estados (`UPantheliaParryAbility`)

Hereda de `UPantheliaGameplayAbility`, Instanced Per Actor. Flujo:
1. `ActivateAbility` → arranca la guardia y entra en la ventana de parry (`EnterParryWindow`).
2. `EnterParryWindow` → concede `State.Parry.X` **y** `State.Block.X` (ver 11.6 para por qué ambos desde el inicio) y arranca un timer de `ParryWindow` segundos (default **0.2**).
3. `OnParryWindowExpired` (timer) → si el botón sigue presionado (`bInputHeld`), confirma el bloqueo sostenido (el tag `State.Block.X` ya está puesto, y se retira `State.Parry.X`); si no, termina.
4. `NotifyBlockInputReleased` (lo llama el ASC al soltar el botón) → `EndAbility`.
5. `EndAbility` → `ClearParryBlockTags`, cancela el timer, detiene montages.

- **Helpers:** `GetParryStateTag()` / `GetBlockStateTag()` devuelven el tag según `EParryType`.
- **Ruteo de input:** el sistema de input custom **no** alimenta de forma fiable el `InputReleased` interno de GAS; por eso el ASC notifica explícitamente vía `NotifyBlockInputReleased` (mismo patrón que el heavy attack), conectado desde el `PlayerController::AbilityInputTagReleased`. Ver `State_Input.md`.
- **Config en editor:** `GA_Parry_Physical` (ParryType=Physical, `InputTag.Block.Physical`, tecla E) y `GA_Parry_Magic` (ParryType=Magic, tecla Q). `IA_BlockPhysical`/`IA_BlockMagic` (Digital) mapeados en el IMC y vinculados en el InputConfig.

### 11.4 Fase 2 — Mitigación de daño (`ExecCalc_Damage`)

Helper static `ApplyParryBlockMitigation`, invocado dentro del loop de tipos de daño:
- Rastrea `PhysicalSubtotal` y `MagicSubtotal` por separado.
- Lee el estado defensivo de los `TargetTags` (los `State.Parry.X` / `State.Block.X` del defensor) y los multiplicadores del arma del defensor vía `Equip->GetEquippedWeaponDefinition()`.
- **Prioridad por `if/else if`** (orden crítico): `bParryPhysical` → `bParryMagic` → `bBlockPhysical` → `bBlockMagic`. Garantiza que el parry perfecto gana sobre el bloqueo cuando ambos tags coexisten (relevante por 11.6).
- Devuelve por referencia `bOutWasPerfectParry`, `bOutWasBlocked`, `OutPoiseToAttacker`.
- Se aplica **antes del crítico** y recalcula `TotalDamage`. Escribe el resultado en el context extendido (`FPantheliaGameplayEffectContext`: `bWasParried`, `bWasBlocked`, `ParryPoiseDamageToAttacker` — ver `State_GAS.md`).
- Includes: `PantheliaEquipmentComponent.h`, `PantheliaWeaponDefinition.h`, `PantheliaAbilityTypes.h`.

### 11.5 Fase 3 — Reacción: postura y feedback

En `PantheliaAttributeSet::PostGameplayEffectExecute` (bloque de `IncomingDamage`) se llama a `HandleParryReaction(Props)`:
- Si hubo parry o bloqueo → `NotifyParryImpact(bParried)` en el ASC del **defensor** (dispara animación/feedback: knockback + cue).
- Si fue **parry perfecto** → aplica daño de postura al **atacante** (`SourceASC`, vía `SetNumericAttributeBase` sobre `Poise`). Si la postura del atacante llega a 0 → dispara `Effects.Stagger` con `TryActivateAbilitiesByTag`.
- Ganchos `TODO` comentados: efectos elementales, skill tree, barra de estado, auto-imbuir.

**ASC** (`UPantheliaAbilitySystemComponent`): `NotifyBlockInputReleased(InputTag)` (busca la ability de parry con ese InputTag y le notifica el release) y `NotifyParryImpact(bWasPerfectParry)` (busca **cualquier** ability de parry activa — el impacto lo dispara el golpe recibido, no un input; solo una puede estar activa). Ver `State_GAS.md`.

### 11.6 Guardia sostenida upper-body (esta sesión)

Mientras el jugador mantiene el bloqueo y se mueve en lock-on, el **torso** hace la pose de guardia y las **piernas caminan** con la locomoción (en vez de patinar tiesas). Solo la guardia **sostenida** es upper-body; la entrada y los retrocesos son full-body. Aplica igual al parry mágico.

**Código:**
- `UPlayerAnimInstance`: bool `bIsGuarding` (BlueprintReadWrite) + `UpdateGuardState()` — obtiene el ASC del pawn (`UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent`) y pone `bIsGuarding = true` si tiene `State.Block.Physical` **o** `State.Block.Magic`. (Por eso el parry mágico hereda el comportamiento.) Se llama en el Event Graph del ABP encadenado tras `UpdateDirection`. Archivo en CRLF.
- `UPantheliaParryAbility`: `GuardLoopMontage` (UPROPERTY, Combat|Parry), en el slot `GuardSlot`. `PlayGuardLoopMontage()` lo reproduce con `AnimInstance->Montage_Play` (no AbilityTask). Se llama en `ActivateAbility` (al presionar), **no** al expirar la ventana — fix de un delay de ~200ms que se sentía como lag. El tag `State.Block.X` se concede desde `EnterParryWindow` para que `bIsGuarding` active el blend desde el frame 1. **Seguridad verificada:** como el ExecCalc evalúa Parry antes que Block, tener ambos tags durante la ventana no rompe la mitigación (el parry perfecto gana; el bloqueo solo aplica tras expirar la ventana, cuando se retira `State.Parry.X`). En `EndAbility` se detiene el montage con blend de 0.15s.

**AnimBlueprint (`ABP_Player`):**
- Slot **`GuardSlot`** en DefaultGroup.
- **Layered blend per bone** (`AnimGraphNode_LayeredBoneBlend`): Base Pose ← `Slot 'DefaultSlot'`; Blend Poses 0 ← `Slot 'GuardSlot'`; Blend Weights 0 ← `bIsGuarding`; Layer Setup: Branch Filter **`spine_01`**, **Blend Depth 1**, **Mesh Space Rotation Blend = true**.

> **Reglas críticas de AnimBP aprendidas:** (1) un PoseLink (salida de pose) admite **un solo cable** — las poses no se splittean, la cadena va en serie. (2) El montage de guardia **debe** estar en el slot `GuardSlot`, no en DefaultSlot (si está en DefaultSlot, la guardia afecta todo el cuerpo y las piernas no caminan — fue el bug principal). (3) El nodo "Slot" siempre aparece en el buscador como `Slot 'DefaultSlot'`; el slot real se cambia luego en Details → Slot Name. (4) El blend `bool→float` (`Conv_BoolToDouble`) da un salto instantáneo 0/1; el suavizado viene del Blend In del montage, no del peso.

### 11.7 Knockback de impacto (esta sesión)

Al recibir un golpe en guardia, el personaje da un paso atrás (feel Lies of P / Sekiro).
- `ApplyGuardKnockback(bool bWasPerfectParry)` — `Character->LaunchCharacter(-ForwardVector * Speed, bXYOverride=true, bZOverride=false)`. Horizontal (no salto), respeta colisiones y lock-on.
- UPROPERTY (Combat|Parry): `ParryKnockbackSpeed` (default **600** cm/s — el parry perfecto repele más) y `BlockKnockbackSpeed` (default **350** cm/s — el bloqueo absorbe).
- Se llama desde `NotifyParryImpact`.

> **`NotifyParryImpact` reescrito:** antes hacía `Montage_JumpToSection(ParryHit/BlockHit)` sobre el montage de entrada (full-body, DefaultSlot), lo que pisaba el `GuardLoopMontage` (GuardSlot) y **deformaba la pose**. Ahora **no toca ningún montage**: solo aplica el knockback y dispara el Gameplay Cue. El feel de impacto lo da el retroceso físico. Las UPROPERTY `ParryHitSectionName`/`BlockHitSectionName` siguen en el `.h` pero ya no se usan (reservadas para un posible montage aditivo futuro).

### 11.8 Gameplay Cues de parry (esta sesión — ganchos listos, assets pendientes)

Efectos/sonidos diferenciados por tipo y perfección. Código listo; los assets se crearán en el editor más adelante.
- 4 tags nativos: `GameplayCue.Parry.Physical.Perfect`, `GameplayCue.Parry.Physical.Block`, `GameplayCue.Parry.Magic.Perfect`, `GameplayCue.Parry.Magic.Block` (ver `State_GAS.md`).
- `FireParryCue(bool bWasPerfectParry)` — selecciona el tag según `EParryType` y perfección, calcula la posición (delante del actor + offset Z 80, a la altura del torso), rellena `FGameplayCueParameters` (Location, Normal, Instigator, EffectCauser) y dispara con `ASC->ExecuteGameplayCue(CueTag, CueParams)`. Se llama desde `NotifyParryImpact`. Si el asset `GC_Parry_*` no existe, la llamada es silenciosa (sin crash). Include: `GameplayCueManager.h`.
- **Pendiente (editor):** crear los 4 `GC_Parry_*` (GameplayCueNotify_Static), asignar su CueTag, implementar partículas/sonido/shake en `OnExecute`, verificar rutas del GameplayCueManager.

### 11.9 Montages de guardia

- `AM_Guard_Loop_Fisico` — creado, slot `GuardSlot`, usa `ARPG_Samurai_Anim_UE_Guard_B`. Debe hacer **loop** (sección apuntando a sí misma) o la pose desaparece tras ~1s. Blend In/Out = **0.05**.
- `AM_Guard_Loop_Magico` — **pendiente** (equivalente mágico, mismo slot, mismo loop). El `GuardLoopMontage` está asignado en `GA_Parry_Physical`; queda asignarlo en `GA_Parry_Magic` cuando exista el montage mágico.

> Guía operativa paso a paso para los assets definitivos (montages, slots, AnimBP, knockback, los 4 Cues, diagnóstico por logs): **`Guia_Animaciones_Efectos_Parry.md`**.

### 11.10 Pendientes del sistema de parry

- `AM_Guard_Loop_Magico` (sección 11.9) y los 4 assets `GC_Parry_*` (sección 11.8).
- **Guard Break / guardia y estamina** — ✅ **cerrado** (campaña 2026-07-16). Semántica: llegar a 0 de estamina mientras se sostiene la guardia **no** la rompe inmediatamente; **el siguiente impacto que no pueda pagarse** provoca **Guard Break**. Estamina exactamente igual al coste permite defender y quedar en 0. Costes separados: intento de parry (ability), transición a guardia (propiedad independiente) e impacto bloqueado (arma **del atacante**), todos por la arquitectura común de costes (`State_GAS.md`). Soltar el botón durante la ventana perfecta **no** cancela la ventana — solo impide la transición a guardia sostenida.
- **Barra de estado en parry mágico** — anular la barra de estado/buildup del atacante en un parry mágico perfecto (gancho `TODO` ya comentado en `HandleParryReaction`). Depende del sistema de efectos de estado (ver `State_Pending.md`).
- **Montage aditivo de reacción de torso (opcional)** — feedback de impacto en el torso al recibir un golpe en guardia, ahora que `NotifyParryImpact` ya no salta a secciones de hit. Las UPROPERTY `ParryHitSectionName`/`BlockHitSectionName` siguen en el `.h` reservadas para esto.
- **Auto-imbuir arma tras varios parrys mágicos consecutivos** — mecánica de diseño (de `Gameplay_Mechanics.md`) aún no implementada; encajaría con stacks de un GameplayEffect.

### `UBlockComponent`

**Eliminado** (ver sección 4). Ya no existe en el código.

## 12. Combat Tricks — debuffs, impulso de muerte, knockback e i-frames — ✅ Funcional (auditado)

Sección 25 del curso, adaptada a soulslike multi-tipo. Cubre **estados elementales** (por buildup determinista), **impulso de muerte**, un **sistema de 3 niveles de reacción a golpe** (i-frames, knockback, lanzamiento aéreo) y su infraestructura GAS. Implementado y **auditado con fixes aplicados** (ver `Code_Review.md`). La infraestructura de daño (context extendido, estados por buildup, `IncomingHealing`, tags) está en `State_GAS.md`; aquí va el comportamiento de combate.

> **Reacciones desactivadas por defecto** (`KnockbackChance`, `LaunchChance`, `bKnockbackIsHeavy` en 0): parámetros por ability que el diseñador sube en balance. *(Los estados elementales, en cambio, se activan de forma determinista al llenar la barra de buildup — ver 12.2.)*

### 12.1 Decisión fundacional (clase 303): pipeline multi-tipo conservado

El curso colapsa el daño a **un solo** `DamageType` + un solo `Damage`. **Rechazado**: Panthelia mantiene su `TMap<FGameplayTag, FScalableFloat> DamageTypes` (8 tipos → 4 elementos), su pipeline `MakeDamageSpec`/`ApplyDamageScalingToSpec`, y golpes de daño mixto (arma de fuego = Físico + Fuego). El curso también reconstruye el proyectil para usar `FDamageEffectParams`/`ApplyDamageEffect` como **único** mecanismo de daño; Panthelia lo implementó (ver `State_Abilities.md`) pero lo dejó como **infraestructura reservada para daño secundario**, nunca en el golpe principal. Las 3 rutas de daño reales (proyectil, melee, weapon trace) siguen con el pipeline original.

### 12.2 Estados elementales por **buildup determinista** (rediseño posterior)

> **Esto reemplaza el sistema aleatorio anterior.** El primer diseño (clases 303-311) usaba una **tirada `DebuffChance` por golpe** y dejaba que el último golpe que "acertaba" definiera daño/duración/frecuencia del DoT (`DetermineDebuff` en el ExecCalc). **Ya no existe.** No reintroducir `DebuffChance` ni configurar el daño del estado por ability. El sistema actual es **determinista por barras de buildup** y con la definición del estado en un **Data Asset global**.

Arquitectura en 4 responsabilidades (ver `State_GAS.md` para el detalle GAS y `State_Abilities.md` para la ability):
- **La ability** declara solo su **identidad de acumulación**: `BuildupAmounts` (`TMap<FGameplayTag, FScalableFloat>`, clave = tipo de daño, p. ej. `Damage.Magical.Fire`). Dos ataques de Fuego pueden hacer distinto daño directo y llenar la barra a distinta velocidad. **No** define daño/duración/frecuencia del estado.
- **`ExecCalc_Damage`** calcula el **buildup efectivo** = `base × (1 − ResistElemental/100) × (1.5 si crítico)`. Resistencia clampeada [0,100]; 100 = inmunidad; el bonus plano de `CritDamage` **no** afecta buildup; **un parry perfecto anula todo el buildup** (el bloqueo imperfecto no). No hay tirada.
- **El AttributeSet del objetivo** mantiene 4 barras (`FireBuildup`/`StormBuildup`/`WaterBuildup`/`NatureBuildup`, tags `Attributes.Buildup.*`); al llegar a **100** clampea, resetea a 0 y llama `TriggerElementalStatus()`. Decay de 6/s acelerado por la resistencia (`×(1 + Resist/100)`), con timer de 0.1s activo solo mientras alguna barra tenga contenido.
- **`DA_ElementalStatusConfig`** (referenciado desde `DA_CharacterClassInfo`) define **globalmente** qué hace cada elemento. Una entrada por elemento (dos = error, no elige en silencio).

**Comportamiento por elemento (estado actual):**
- **Fuego → Quemadura** (`Debuff.Burn`, DoT): daño plano + rama opcional de % de vida máxima (desbloqueable por atributo del build); puede aplicar **Defense Shred** (reduce Armor/MagicResistance del objetivo, `Effects.DefenseShred.Burn`) si el atacante tiene esos atributos ofensivos.
- **Naturaleza → Veneno** (`Debuff.Poison`, DoT): como Quemadura + puede aplicar **Heridas Graves** ligadas a su duración (`Effects.GrievousWounds.Poison`) y su propio Defense Shred (`Effects.DefenseShred.Poison`). Dos Defense Shred coexisten y **suman** (GEs `Add`).
- **Tormenta → Electrocución** (`Debuff.Shock`, Burst): detonación única = plano + % vida actual + % vida faltante (ramas desbloqueables); tras el burst, una instancia de **daño de postura** por `IncomingPoiseDamage` (reutiliza Flinch/Stagger).
- **Agua → Saturación** (`Debuff.Saturation`, AttributeDebuff): **payload principal aún pendiente**; hoy ya aplica el tag por duración (para Niagara/UI) + daño de postura. Emite un warning de config recordando el pendiente.

Todos los payloads ofensivos escalan además con `Attributes.StatusPower.*` y con el secundario `MagicDamage` del atacante. Cada personaje tiene 4 `UPantheliaDebuffNiagaraComponent` (Burn/Shock/Saturation/Poison) que se encienden/apagan por el tag y se apagan al morir. **Un solo estado por elemento y objetivo:** al reaplicar se quita el efecto con el mismo tag y se aplica el nuevo (refresca duración, recalcula magnitud con los atributos actuales).

> **`DetermineDebuff` / `Debuff()` del diseño anterior:** el `Debuff()` cacheado del AttributeSet sobrevive como mecanismo de aplicación de GE dinámico, pero **la ruta de activación ya no es aleatoria** — la dispara `TriggerElementalStatus` al llenarse la barra, no una tirada en el ExecCalc.

### 12.3 Sistema de 3 niveles de reacción a un golpe

Diseñado tras consultar cómo lo hacen los Souls. De menor a mayor, y **mutuamente ordenados** (una reacción más fuerte "contiene" a la más débil):

- **Nivel 0 — i-frames (`State.Invulnerable`):** invulnerabilidad temporal. El chequeo vive al **inicio** de `ExecCalc_Damage::Execute_Implementation`: si el target tiene el tag, `return` con daño cero. **Decisión de diseño (Elden Ring):** los i-frames bloquean **solo el daño directo**; los **ticks de un DoT ya aplicado NO se bloquean** (rodar no apaga una quemadura en curso). Se concede con `GrantTemporaryInvulnerability(ASC, Duration)`. **Ya en uso por el dodge** (sección 13), que usa el hijo `State.Invulnerable.Dodge`; el sistema evolucionó a **tags jerárquicos** con esquivabilidad por golpe (`Dodgeable`/`AvoidableNoReward`/`Unavoidable`) — ver `State_GAS.md`.
- **Nivel 1 — Knockback:** empuje horizontal con `LaunchCharacter` (no `AddImpulse`: un personaje vivo no simula física, se mueve por su `CharacterMovementComponent`). Dado `KnockbackChance` por ability; fuerza `KnockbackForceMagnitude`. En las 3 rutas (proyectil: dirección de vuelo; melee/weapon trace: dirección atacante→objetivo).
- **Nivel 2 — Knockback pesado:** **no** es un dado aparte — es un booleano `bKnockbackIsHeavy` que **upgradea** el resultado de la misma tirada de Nivel 1. Concede `State.HeavyKnockback` (1 s vía GE con duración fija) y dispara `GA_HeavyKnockback` (`Effects.HeavyKnockback`) con su montage.
- **Nivel 3 — Lanzamiento aéreo + levantarse:** dado `LaunchChance` propio (campos de contexto separados: `LaunchForce`, no reutiliza `KnockbackForce`). Lanza al aire; concede `State.Airborne` como **loose tag** (`SetLooseGameplayTagCount`, no vía GE — su duración depende de la física real, no de un timer). El override `ACharacter::Landed()` en `PantheliaCharacterBase` detecta el aterrizaje: si tenía `State.Airborne` (distingue un lanzamiento de un salto normal), limpia el tag y dispara `GA_GetUp` (`Effects.GetUp`), que concede i-frames al levantarse.

> **Exclusión Launch vs Knockback (fix de auditoría):** eran dados independientes que podían disparar dos `LaunchCharacter` seguidos. Ahora en `HandleIncomingDamage` son **excluyentes**: se evalúa Launch primero (reacción más fuerte) y solo si no procede, Knockback/Heavy.

### 12.4 Impulso de muerte (death impulse)

Al morir, el cuerpo (y el arma) reciben un empuje físico. `DeathImpulseMagnitude` (`FScalableFloat`) → SetByCaller → vector en el context → `ICombatInterface::Die(const FVector& DeathImpulse)` → `MulticastHandleDeath` aplica `AddImpulse(..., NAME_None, true)` (el `bVelChange=true` ignora la masa). `WeaponDeathImpulseScale` (parámetro propio) reduce la fuerza sobre el arma (necesita mucha menos que el cuerpo).
- **`ResolveDeathWeaponMesh()`** (fix de post-entrega): el arma de un enemigo estándar **no** es `FinalWeaponMesh` sino un mesh separado resuelto por `WeaponTraceComponent`. Esta función localiza dinámicamente el arma real (prioriza `WeaponTraceComponent`, cae a `FinalWeaponMesh`) y se usa en `Die()`, `MulticastHandleDeath()` y `Dissolve()`. Sustituye el parche manual que solo tenía `BP_Boss` en su Construction Script.
- Hoy solo el **proyectil** escribe el impulso de muerte; extenderlo a melee/weapon trace es pendiente opcional. El impulso sobre el **cuerpo** (no el arma) es poco visible — cuestión de magnitud, marcado como no urgente.

### 12.5 Decisión: el parry perfecto niega el debuff (Lies of P)

Un **parry perfecto** anula el **buildup elemental** del golpe; el **bloqueo imperfecto no**. En el `ExecCalc`, el cálculo del buildup corre **después** del bloque de parry y recibe `bWasPerfectParry`; si es `true`, el buildup del golpe es 0.

### 12.6 Pendientes del sistema de Combat Tricks

- **Blueprints (editor):** crear `GA_GetUp` (Ability Tags `Effects.GetUp`; `GrantTemporaryInvulnerability` → `Get Avatar Actor From Actor Info` → `Get Get Up Montage` → `Play Montage and Wait` → `End Ability`) y `GA_HeavyKnockback` (`Effects.HeavyKnockback`); añadir `State.Airborne` y `State.HeavyKnockback` a `ActivationBlockedTags` de `GA_HitReact`; asignar `GetUpMontage`/`HeavyKnockbackMontage` por personaje.
- **Balanceo:** subir `KnockbackChance`/`LaunchChance`/`bKnockbackIsHeavy` por ability (hoy en 0), y configurar el `BuildupAmounts` de cada ability + el `DA_ElementalStatusConfig` global (daño/duración/escalado de cada estado).
- **Impulso de muerte:** extenderlo a melee/weapon trace si se desea; ajustar la magnitud sobre el cuerpo.
- **Limitación estructural — un solo debuff por golpe:** el context transporta **un** resultado de debuff. Un golpe multi-elemento donde acierten dos (p. ej. Fuego+Tormenta) sobreescribiría el primero con el segundo y aplicaría solo uno, en silencio. No ocurre hoy (no hay abilities multi-elemento con debuff); **tenerlo presente al diseñar armas híbridas**. Solución futura: un `TArray` de resultados en el context, o aplicar el debuff directamente desde el ExecCalc.
- **`State.Airborne` al morir** — ✅ resuelto: la limpieza al morir de `PantheliaCharacterBase` ya retira el loose tag `State.Airborne` (y los debuffs y buildup) del ASC, así que no sobrevivirá al futuro respawn. Ver `State_GAS.md` (Etapa 4 / limpieza al morir).

---

## 13. Sistema de Dash / Perfect Dodge / Ataques post-dodge — ✅ Funcional (Fases 3-5)

La evasión del jugador. **Panthelia usa dash direccional, NO roll** (decisión cerrada, no implementar un roll paralelo). Cubre el dash de 8 direcciones, los i-frames, el Perfect Dodge y los ataques de continuación. Animaciones actuales **provisionales**.

### 13.1 Dash de 8 direcciones (Fase 3)

`GA_PlayerDodge` (`UPantheliaPlayerDodgeAbility`), activada por `InputTag.Dodge`. Dash por **montage con root motion** (sin Motion Warping), con la distancia escalada por ejecución. Cuesta estamina, se bloquea por estados incompatibles, **cancela ataques** (y el ataque no cancela externamente al dodge).

- **Con lock-on:** el personaje mantiene su orientación de strafe y el input se clasifica en **8 sectores de 45°** (Forward, ForwardRight, Right, BackwardRight, Backward, BackwardLeft, Left, ForwardLeft) → 8 montages dedicados (`AM_Evade_*`), cada uno con su `AuthoredTravelDistance`.
- **Sin lock-on:** con input de movimiento usa `DodgeForward` y **rota al personaje** hacia el vector de input (así el root motion frontal produce el dash en cualquier dirección, sin necesitar las 8 animaciones); sin input usa `DodgeBackward` (retrocede, sin forzar rotación).

**Los 8 montages** (0.900 s, 60 FPS, `DefaultSlot`) deben llevar **exactamente**: 1× `DodgeIFrameNotify`, 1× `DodgePerfectWindowNotify` (notifies puntuales) y 1× `DodgeFollowupWindowNotifyState` (notify state).

### 13.2 Perfect Dodge (Fase 4)

El Perfect Dodge **no es una ability aparte**: es un **resultado exitoso** de `GA_Dodge` (decisión de diseño — una `GA_PerfectDodge` podría fallar por tags/coste/cooldown *después* de que el dodge ya fue válido). Condiciones:

```
GA_Dodge activa + State.Invulnerable.Dodge + golpe con DodgeResponse = Dodgeable
+ ventana perfecta abierta  →  Event.Dodge.Perfect
```

- **Sistema de esquivabilidad del golpe** (tags jerárquicos de i-frames): `Dodgeable` (evitable y **concede** Perfect), `AvoidableNoReward` (los i-frames lo anulan pero **no** da Perfect), `Unavoidable` (atraviesa los hijos específicos `State.Invulnerable.Dodge`/`.Jump`, pero **no** la invulnerabilidad absoluta del tag padre exacto — ver `State_GAS.md`).
- **Máximo un Perfect por activación** (flag `bPerfectDodgeTriggered`). El listener de `Event.Dodge.HitAvoided` **no** usa `OnlyTriggerOnce`, para que un golpe evitado fuera de la ventana no impida detectar otro válido en el mismo dash.
- **Presentación:** `GameplayCue.Dodge.Perfect` (sonido + Niagara + cámara). **Ninguna lógica jugable vive en el Cue ni en el Niagara.** Las consecuencias jugables futuras (árbol, corazones) escucharán `Event.Dodge.Perfect`.

> **Perfect Window ≠ Follow-up Window.** La primera es **precisión defensiva** (¿el golpe evitado da recompensa?); la segunda es **comodidad ofensiva** (¿cuándo puedo encadenar?). Ampliar la de follow-up **no** facilita el Perfect Dodge.

### 13.3 Ataques post-dodge (Fase 5)

Durante la ventana marcada por `DodgeFollowupWindowNotifyState` (booleano interno `bFollowupWindowOpen`, no un tag: nadie externo lo consulta), el jugador puede encadenar un ataque.

- **El primer input válido gana**, con un **enum** (`EPantheliaDodgeBufferedAction {None, LightAttack, HeavyAttack}`) en vez de dos booleanos — impide estados imposibles (Light y Heavy a la vez). Buffer máximo: 1.
- **El Heavy post-dodge NO entra en tap-vs-hold:** es inmediato, sin `HoldThreshold` ni `ChargedHeavyMontage` (un Heavy lanzado desde el dodge ya es una intención decidida). El Heavy normal conserva su tap-vs-hold.
- **El montage post-dodge es un opener, no un combo aparte:** sustituye solo el primer golpe; tras su `ComboWindowNotifyState` continúa por `LightAttackMontages[1]` / `HeavyAttackMontages[1]` (o vuelve a `[0]` si el array tiene un solo elemento).
- **Por arma, con fallback:** usa `DodgeLightAttackMontage` / `DodgeHeavyAttackMontage` del `WeaponDefinition`; si están vacíos, cae a `LightAttackMontages[0]`/`HeavyAttackMontages[0]`. **No falla por dejarlos vacíos.**
- **Baseline actual: modo inmediato** (`bChainImmediatelyOnFollowupInput = true`): al aceptar el input se copia a local, se limpia el buffer, se cierra la ventana, termina `GA_Dodge`, se retira `State.Dodge.Active` y **el ataque se activa al siguiente tick**. Existe el modo `false` (esperar a que el montage acabe), disponible pero no elegido por feeling.
- **Interrupción segura:** `EndAbility` **siempre** limpia el buffer; solo los caminos legítimos copian la acción antes. Un dodge interrumpido (muerte, hit react) **no** dispara un ataque tardío.

**Ventana final calibrada por feeling:** frames **15–50** (≈0.250-0.833 s en montages de 0.900 s / 60 FPS, ~28%-93% del montage). Las propuestas iniciales más estrechas (0.585-0.810 s) producían demasiados inputs fuera de ventana. **Conservar como baseline hasta que lleguen las animaciones definitivas.**

### 13.4 Al reemplazar las animaciones de dodge (workflow obligatorio)

Las animaciones actuales son **provisionales**. Al sustituirlas **no copiar los timestamps a ciegas**: hay que volver a medir el root motion y recalibrar los tres notifies en cada uno de los 8 montages (i-frames según el movimiento visual real; la ventana perfecta empieza **dentro** de los i-frames; la de follow-up no debe sobresalir de los i-frames restantes ni cerrar después del final real del montage), y revalidar las 8 direcciones. Los openers post-dodge (`AM_DodgeFollowup_*`) necesitan `DefaultSlot` + `WeaponTraceNotifyState` + `ComboWindowNotifyState`, y **no** deben usar la lógica de `ChargedHeavyMontage`.

### 13.5 Preparado para el árbol y los corazones

`GA_Dodge` expone puntos de cálculo ya listos para recibir modificadores: `GetFinalDashDistance()`, `GetFinalIFrameDuration()`, `GetFinalPerfectDodgeWindowDuration()` (hoy devuelven los valores base con clamps). Las consecuencias del Perfect Dodge (recuperar estamina, reducir cooldowns, buffs elementales, cargas de corazón, ralentizar al atacante…) se implementarán como **pasivas/GEs que escuchen `Event.Dodge.Perfect`**, no dentro de `GA_Dodge`.

> **Si el árbol necesita ampliar la ventana de follow-up:** **no mover notifies en runtime**. Preferible un sistema de gracia/buffer temporal calculado por la ability alrededor del Notify State. No implementar hasta que exista un nodo concreto que lo pida.

### 13.6 Pendientes del dodge

- **Animaciones definitivas** (y el workflow de 13.4) + **balance** (distancia, i-frames, ventana perfecta, coste de estamina, ventana de follow-up, recuperación, daño de los openers).
- Probar el modo no inmediato (`bChainImmediatelyOnFollowupInput = false`) como validación técnica secundaria — no bloquea nada.
- **No implementado (a propósito):** roll, Motion Warping en el dodge, i-frames de salto, consecuencias jugables universales del Perfect Dodge, follow-up modificable por árbol, variaciones de dodge por arma/carga.

---

## 14. Pendientes del sistema de combate

### De la migración del melee del jugador (sección 10)
- **`GA_PlayerHeavyAttack`** — crear la ability de ataque pesado (parent `PantheliaPlayerAttackAbility`, `AttackType=Heavy`, su propio CostGE). La lógica C++ ya soporta Heavy; falta el Blueprint y rellenar `HeavyAttackMontages` en el arma.
- **Borrado físico de archivos legacy huérfanos** — `CombatComponent`, `TraceComponent`, `Fighter`, `BaseWeapon`, `ToggleTraceNotifyState`. Sin uso pero aún sin borrar (para no romper referencias de editor). Limpieza final pendiente.
- **`ResetPlayerCombo` / `ResetCombo`** — quedaron como fallback de seguridad; el flujo normal ya no los necesita (el combo se resetea en `EndAbility`/`CloseComboWindow`). Revisar si conviene quitarlos en limpieza futura.
- **Coste de stamina por arma** — hoy el CostGE usa un valor fijo. Falta integrar el `LightAttackStaminaCost`/`HeavyAttackStaminaCost` del arma al GE (vía MMC o SetByCaller) para que cada arma defina su propio coste.
- **Mejora de arma (upgrade)** — estructura lista en el DataAsset (`MaxUpgradeLevel`, `RequiredUpgradeMaterial`, `UpgradeMaterialCostPerLevel`). Falta la lógica de aplicar mejoras (usar el nivel de mejora como "level" en `GetValueAtLevel` de las curvas de daño) y el sistema de inventario/materiales.
- **Sinergia arma-elemento** — el `WeaponElement` ya vive en el arma (mismo enum que el corazón elemental). Falta el sistema del corazón y la lógica de bonus cuando coinciden elementos. Ver `State_Pending.md` sección 3.
- **Gameplay Cue de impacto melee (clases 205-206)** — ✅ **implementado en código** (tag `GameplayCue.Melee.Impact`, disparo desde `WeaponTraceComponent`, `GC_MeleeImpact` construido en Blueprint; ver sección 9). **Pendiente de editor:** añadir `/Game/Blueprints/AnimNotifies` a `GameplayCueNotifyPaths` + reiniciar, y asignar los assets (`BloodEffect` Niagara y `ImpactSound` por personaje/montage/arma).

### Otros pendientes de combate
- ~~`GA_Dodge` (dash/esquive)~~ — ✅ **implementado** (Fases 3-5: dash 8 direcciones, i-frames, Perfect Dodge, follow-ups). Ver sección 13. Quedan animaciones definitivas y balance.
- **Blueprints de Stagger** — `GE_Stagger` / `GA_Stagger` / montages (ver sección 8.2).
- **Combos del WarriorBoss** — la IA del boss se migró a **StateTree + BossBrain + BossProfile** (ver `State_AI.md`); el `BP_WarriorBoss` ya ataca en ciclo gobernado por StateTree y su Behavior Tree quedó deshabilitado. Los combos saldrán de las **acciones ponderadas** + `bCanExtendCombo` del `BossProfile` (tarea pendiente de ese sistema), no de un sistema aparte.

### Pendientes de IA / animación de enemigos (debugging)

- **Motion Warping impreciso** en los ataques enemigos — a veces falla el reposicionamiento/giro. Comportamiento conocido, se pulirá en clases posteriores.
- **Stuttering / movimiento entrecortado de enemigos** — se manifiesta en `BP_Boss` **y** `BP_Shaman` (mismo síntoma en ambos). Pendiente de investigar.
- **Transición idle↔run brusca del TestBoss** — `BS_BossMovement` tiene `Smoothing Time = 0`. Subirlo a 0.1 **no** tuvo efecto visible; la causa real está por investigar. Posible relación: `ABP_Boss` usa `TryGetPawnOwner` mientras que `ABP_WarriorBoss` usa `GetOwningActor` — esa diferencia podría afectar cómo el AnimBP lee la velocidad para el BlendSpace.
- **`BP_WarriorBoss` sin ataque ranged** — sigue siendo solo melee. Con el nuevo sistema de IA (`State_AI.md`), añadir ataques a distancia es una entrada `ActionType = Ranged` en el `BossProfile`, no un rediseño; queda como decisión de diseño (si el WarriorBoss será híbrido).
- **Locomoción del boss (Chase/Reposition/GapCloser)** — pendiente, gobernada por StateTree (tarea 7 de `State_AI.md`). Conecta con el stuttering de movimiento de arriba.
- **Dissolve del WarriorBoss al morir** — pendiente (infraestructura lista, faltan materiales). Ver `State_Abilities.md` sección 11.4.
