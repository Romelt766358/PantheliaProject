# State_Abilities — Sistema de Gameplay Abilities

> **Propósito:** Estado del sistema de Gameplay Abilities. Cubre las clases base, el sistema de input-to-ability, hechizos de proyectil, el pipeline de animación, el sistema de daño (ExecCalc con elementos + escalado), arquetipos de enemigos, hit react/muerte y la barra de vida de bosses. Lee `State_Overview.md` y `State_GAS.md` primero.

---

## 1. UPantheliaGameplayAbility — ✅ Funcional

Hereda de `UGameplayAbility`. Base de TODAS las abilities del juego.

**Variables:**
- `StartupInputTag` (FGameplayTag, EditDefaultsOnly, Category="Input") — el InputTag con el que esta ability empieza el juego asociada. Se lee UNA VEZ en `AddCharacterAbilities()` para agregarlo a `DynamicAbilityTags` de la spec. Después, el tag editable es el de la spec (permite remapeo en runtime).

**Regla:** Nunca heredar directo de `UGameplayAbility`. Siempre de `UPantheliaGameplayAbility` (o una subclase).

---

## 2. UPantheliaDamageGameplayAbility — ✅ Funcional

> **Abilities productivas del jugador MetaHuman (validadas en build cocinada).** Las abilities productivas concedidas por `startupAbilities` son las versiones de Production: **dodge productivo** (`GA_PantheliaPlayer_Dodge`) y **parry físico productivo** (`GA_PantheliaPlayer_ParryPhysical`); los ataques (`GA_PlayerLightAttack`/`GA_PlayerHeavyAttack`) permanecen **compartidos** y reciben sus montages productivos desde el Data Asset (`DA_PantheliaPlayer_Sword_Basic`); `GA_Stagger` sigue **compartida**. **No se duplican clases GAS comunes por variante visual.** La build Development cocinada confirmó activación y ejecución correctas (ver `State_Combat.md` sección 10 y `State_Validation.md`).
> **Deuda editorial (no bloqueante):** quedan **33 secuencias `MetaHumanMigration` en disco / 25 cocinadas** — **no** implican dependencia funcional del Pawn de migración y **no** autorizan reconstruir montages a ciegas; revisar solo con una futura herramienta de auditoría/resave. `UnexpectedLoad` no bloqueante de `GC_MeleeImpact` y `GC_Dodge_Perfect`.

Hereda de `UPantheliaGameplayAbility`. Base de toda ability que inflige daño. Centraliza la configuración de daño que antes vivía dispersa.

**Jerarquía completa:**

```
UGameplayAbility
└── UPantheliaGameplayAbility            (StartupInputTag)
    ├── UPantheliaParryAbility            (EParryType, guardia/bloqueo/parry)        → defensa del jugador
    ├── UPantheliaPlayerDodgeAbility      (dash 8 dir, i-frames, Perfect, follow-ups) → evasión del jugador
    └── UPantheliaDamageGameplayAbility   (DamageTypes, AttributeScalings, PoiseDamage, BuildupAmounts, DamageEffectClass)
        ├── UPantheliaProjectileSpell      (ProjectileClass, SpawnProjectile)   → hechizos y ranged enemigo
        └── UPantheliaPlayerAttackAbility  (AttackType, combo con buffer, coste por arma) → melee del jugador
```

> `UPantheliaParryAbility` hereda directamente de `UPantheliaGameplayAbility` (**no** de la rama de daño — el parry no inflige daño por scaling, aunque sí daña postura). Maneja parry físico y mágico con un enum `EParryType`, la guardia sostenida upper-body, el knockback y los Gameplay Cues. Se documenta en detalle en `State_Combat.md` sección 11.

> `UPantheliaPlayerDodgeAbility` (`GA_PlayerDodge`) también cuelga directo de la base (no inflige daño). Responsable de: movimiento del dash, coste, i-frames, ventana perfecta, detección del golpe evitado y emisión de `Event.Dodge.Perfect`; más la ventana de follow-up y el buffer del ataque post-dodge. El **Perfect Dodge no es una ability separada**, es un resultado de esta. Detalle en `State_Combat.md` sección 13.

> `UPantheliaPlayerAttackAbility` maneja el combo melee del jugador en C++ (sistema de armas data-driven + buffer de input estilo Dark Souls). Se documenta en detalle en `State_Combat.md` sección 10; aquí solo consta su lugar en la jerarquía (reutiliza `MakeDamageSpec`/`ApplyDamageScalingToSpec` de la clase base). **Fixes v2.1:** `ApplyWeaponDamageDataToAbility(WeaponDef)` es ahora el **único punto de copia** de los datos del arma a la ability (`DamageTypes`, `AttributeScalings`, `PoiseDamage` y **`BuildupAmounts`**) — la asignación del `TMap` es completa, así que cambiar a un arma sin buildup **reemplaza** el mapa por uno vacío en vez de conservar el anterior. Además sobrescribe `CheckCost`/`ApplyCost` para el **coste de estamina por arma y por golpe** (ver `State_GAS.md`), selecciona el **montage opener** cuando el contexto de entrada es `DodgeFollowup`, y valida el montage **antes** de cobrar. El heavy (`UPantheliaPlayerHeavyAttackAbility`) hereda el sistema sin implementación paralela. **Nada de esto afecta a dodge/parry/hechizos**, que tienen sus propios costes.

**Campos:**
- `DamageTypes` (`TMap<FGameplayTag, FScalableFloat>`) — reemplaza al antiguo `Damage` (FScalableFloat único). Mapea tipo de daño (`Damage.Physical.Ice`, `Damage.Magical.Fire`, etc.) → curva de daño base por nivel. Permite que una habilidad declare varios tipos de daño a la vez.
- `AttributeScalings` (`TArray<FAbilityAttributeScaling>`, máximo 2 por diseño) — escalado estilo LoL con atributos secundarios/vitales del caster.
- `PoiseDamage` (FScalableFloat) — daño a postura. 0 = no aplica.
- `DamageEffectClass` (TSubclassOf<UGameplayEffect>) — el GE de daño (`GE_Damage`). Movido aquí desde `UPantheliaProjectileSpell`.
- **`BuildupAmounts` (`TMap<FGameplayTag, FScalableFloat>`)** — la **identidad de acumulación** del ataque para los estados elementales (rediseño Sección 25). Clave = **tipo de daño** (`Damage.Magical.Fire`), **no** un tag de buildup. La ability colapsa esos tipos a los 4 elementos vía `DamageTypeToElement` (dos tipos del mismo elemento se **suman**) y `ApplyDamageScalingToSpec()` los emite por SetByCaller como `CombatTricks.Buildup.Fire/Storm/Water/Nature` (cero si no hay). **El daño/duración/frecuencia del estado NO viven en la ability** — están en `DA_ElementalStatusConfig`. `BuildupAmounts` sí, porque describe *qué tan rápido* llena la barra este ataque (dos ataques de Fuego pueden llenar a ritmos distintos). Ver `State_Combat.md` sección 12.2.
- **Heridas Graves directas opcionales:** `GrievousWoundsPercent` / `GrievousWoundsDuration` — antiheal inmediato que la ability puede aplicar en el golpe (sin barra), transportado por SetByCaller. Compite con el antiheal On-Hit del atacante: **gana la fuente de mayor %** (con su duración; ver `State_GAS.md`). No confundir con el antiheal ligado al Veneno.

> **Nota (rediseño):** los antiguos parámetros `DebuffChance`/`DebuffDamage`/`DebuffFrequency`/`DebuffDuration` de la ability **se eliminaron**. El estado ya no es aleatorio ni lo define el último golpe. `MagicResistance`/`MagicPenetration` ya mitigan el daño mágico directo en el `ExecCalc`.

**Struct `FAbilityAttributeScaling`** (en `PantheliaGameplayAbility.h`):

```cpp
USTRUCT(BlueprintType)
struct FAbilityAttributeScaling
{
    GENERATED_BODY()

    UPROPERTY(EditDefaultsOnly)
    FGameplayTag AttributeTag;  // ej: Attributes.Secondary.MagicDamage

    UPROPERTY(EditDefaultsOnly)
    float Ratio = 0.f;          // 0.5 = +50% del atributo al daño
};
```

**Fórmula de escalado:** `DañoFinal = Base + Σ(Ratio × Atributo)`, distribuido proporcionalmente entre los tipos de daño según el peso de su base. Se calcula **en la ability** (antes del SetByCaller); el ExecCalc recibe valores ya escalados y no sabe del escalado.

- Atributos elegibles: **secundarios y vitales** del caster (no primarios). Incluye ofensivos (`PhysicalDamage`, `MagicDamage`), defensivos, de recursos (`MaxMana`, etc.) y vitales actuales (`Health`, `Mana`, `Stamina`).
- Ratios fijos (no escalan por nivel — la curva por nivel ya está en `DamageTypes`).

**Asimetría `PhysicalDamage` vs `MagicDamage` (modelo LoL, decisión cerrada):**
- `PhysicalDamage` = **AD**: el ExecCalc lo captura del source y lo suma como sumando genérico a TODOS los tipos físicos antes de Armor. Si además se usa como ratio en `AttributeScalings`, cuenta doble (intencional).
- `MagicDamage` = **AP**: el ExecCalc NO lo captura. Solo existe como ratio en `AttributeScalings` de abilities y armas. Nunca se suma automáticamente.

### Métodos de daño centralizados — ✅ (hallazgo D1 resuelto)

Toda la lógica de construir el daño (base + escalado por atributos) vive ahora en la clase base, y las tres rutas de daño la comparten:

- `ApplyDamageScalingToSpec(FGameplayEffectSpecHandle&, UAbilitySystemComponent* SourceASC)` (protegido) — aplica los **5 pasos del escalado** sobre un spec ya creado: (1) daño base por tipo (`DamageTypes` × nivel), (2) escalado por atributos del caster vía `TagsToAttributes`, (3) multiplicador proporcional, (4) `SetByCaller` por cada tipo de daño, (5) daño de postura. Es el único sitio donde vive esta lógica.
- `MakeDamageSpec()` — crea el spec desde `DamageEffectClass`, le aplica `ApplyDamageScalingToSpec` y lo **devuelve sin aplicarlo**. Lo usan los sistemas que entregan el spec de forma diferida (proyectiles, Weapon Trace).
- `CauseDamage(AActor* TargetActor)` — construye el spec con `MakeDamageSpec()` y lo **aplica** al target. Antes del refactor **no** aplicaba el escalado por atributos (solo el daño base); ahora sí, porque reutiliza `MakeDamageSpec`.
- `GetRandomTaggedMontageFromArray(const TArray<FTaggedMontage>&)` const, BlueprintPure — devuelve un `FTaggedMontage` al azar del array, o un struct vacío si está vacío (`PlayMontageAndWait` maneja un montage null sin crashear). Reemplaza los ~7 nodos manuales de random que tenía `GA_MeleeAttack` (Array Length → Subtract → RandomIntegerInRange → Get + branch de validación) por un único nodo. Requiere `#include "Interfaces/CombatInterface.h"` en el `.h` (por `FTaggedMontage`). `GA_RangedAttack` **no** la usa — usa `CastMontage` directo, no un array.

**Consecuencia (D1 cerrado):** melee (`CauseDamage`), proyectiles (`SpawnProjectile`) y Weapon Trace (`MakeDamageSpec` + `SetDamageSpec`) comparten exactamente la misma lógica de escalado. Ya no hay rutas divergentes.

> **`FDamageEffectParams` + `ApplyDamageEffect` — infraestructura reservada (Sección 25, clase 305).** El curso reconstruye el proyectil para usar `FDamageEffectParams` (struct con `WorldContextObject`, GE class, ASCs, `AbilityLevel`, un `DamageType` **singular**, `BaseDamage`, y los 4 campos de debuff) + `UPantheliaAbilitySystemLibrary::ApplyDamageEffect(const FDamageEffectParams&)` como el **único** mecanismo de daño. Panthelia lo implementó pero **no lo usa en el golpe principal** — su pipeline multi-tipo (`MakeDamageSpec`) sigue siendo la vía de las 3 rutas reales. Queda como infraestructura para **daño secundario de un solo elemento** (el struct solo admite uno). Deliberadamente **no** se implementó `MakeDamageEffectParamsFromClassDefaults()` (elegir cuál de los varios tipos de daño copiar sería inventar una regla). `ApplyDamageEffect` incluye una mejora de robustez sobre el curso: `if (!TargetActor)` hace log + return en vez de crashear.

---

## 3. UPantheliaProjectileSpell — ✅ Funcional

Hereda de `UPantheliaDamageGameplayAbility`. Base para hechizos que disparan proyectiles.

**Variables propias:**
- `ProjectileClass` (TSubclassOf<APantheliaProjectile>) — la clase de proyectil a spawnear
- `CastMontage` (TObjectPtr<UAnimMontage>, EditDefaultsOnly) — montage de lanzamiento del hechizo, **separado** del array `AttackMontages`. **CRÍTICO:** no añadir `CastMontage` a `AttackMontages`, porque `GA_MeleeAttack` elige de ese array al azar y reproduciría animaciones de casting en ataques melee.
- `SocketTag` (FGameplayTag, EditDefaultsOnly) — desde qué socket spawna el proyectil (`Weapon` / `RightHand` / `LeftHand`), con fallback a `Weapon` si es inválido (compatibilidad con el ability del jugador). El graph del ability también lee `SocketTag` para el `EventTag` del `WaitGameplayEvent`, de modo que los Blueprints hijos configuran un único valor.
- (el `DamageEffectClass` ahora vive en la clase padre)

**`SpawnProjectile()` (BlueprintCallable):**
0. Guards (C3, ya resueltos): si `ProjectileClass` es null o `SpawnActorDeferred` devuelve null, sale con `UE_LOG` de warning en vez de crashear.
1. Construye el spec de daño llamando a `MakeDamageSpec()` de la clase base (que ya aplica el escalado por atributos y el daño de postura — ver sección 2). El refactor de D1 reemplazó las ~70 líneas de escalado que antes vivían aquí por esta única llamada; el comportamiento de los proyectiles no cambió.
2. `SpawnActorDeferred` del proyectil
3. Le asigna el spec a `Projectile->DamageEffectSpecHandle`
4. Calcula la rotación (pitch = 0) y hace `FinishSpawning`

- `GetFacingTargetLocation() const` — posición del lock-on target si existe, o un punto al frente del personaje si no.

**Dirección del proyectil:**
- Con lock-on: apunta a `LockonComponent->CurrentTargetActor->GetActorLocation()`
- Sin lock-on: `GetActorForwardVector() * 2000.f`
- Pitch = 0 para vuelo horizontal

---

## 4. APantheliaProjectile — ✅ Funcional

**Archivo:** `Actor/PantheliaProjectile.h/.cpp`

Clase base para todos los proyectiles del juego.

**Componentes:**
- `USphereComponent` (private) — colisión, QueryOnly
  - Object Type: `ECC_Projectile` (canal custom definido en `PantheliaProject.h`, ver `State_GAS.md`)
  - **Perfiles de colisión (hardening 2026-07-16):** cápsula de personajes → **Ignore**; mesh de personajes → **Overlap**; mundo/paredes → **Block**. Ver `State_Hardening.md` sección 1.
- `UProjectileMovementComponent` (public) — velocidad 550, sin gravedad (vuelo recto)

**Consumo (outcomes):** el proyectil se **consume** en un hit real, un block o un parry; se consume/bloquea contra el mundo según su config; **atraviesa** un contacto negado por i-frames de dodge (no se gasta contra un jugador invulnerable) y **no** se niega si el golpe está marcado `Unavoidable`. El fireball normal del Shaman está clasificado `Dodgeable`. Ver la esquivabilidad en `State_GAS.md`.

**Efectos (EditDefaultsOnly, Category="Effects"):**
- `ImpactEffect` (UNiagaraSystem) — Niagara al impactar
- `ImpactSound` (USoundBase) — sonido único al impactar
- `LoopingSound` (USoundBase) — sonido en loop durante el vuelo

**Nota de implementación del sonido:** El LoopingSound se spawna en C++ (`BeginPlay → SpawnSoundAttached`) y se detiene en `OnSphereOverlap`. El `LoopingSoundComponent` se guarda como `TObjectPtr<UAudioComponent>` para poder detenerlo.

**Daño:**
- `DamageEffectSpecHandle` (FGameplayEffectSpecHandle, BlueprintReadWrite, ExposeOnSpawn) — el spec del GE de daño, seteado desde `SpawnProjectile()` antes de `FinishSpawning`
- En `OnSphereOverlap`: aplica el spec al ASC del target con `ApplyGameplayEffectSpecToSelf` (solo con `HasAuthority`)
- **Filtro de bando (clase 196):** tras los guards existentes, comprueba `IsNotFriend(GetInstigator(), OtherActor)` antes de aplicar daño — un proyectil enemigo ya no daña a otros enemigos (mismo Actor Tag = amigos). Requiere `#include "AbilitySystem/PantheliaAbilitySystemLibrary.h"`.
- **Anti doble-disparo (clase 210):** flag `bHit` (default false). Tras los checks de validez/friendly-fire, si `bHit` ya es true → `return`; si no, se pone true y se procesa. Evita que el `ImpactSound`/`ImpactEffect` se reproduzcan dos veces si el overlap dispara varias veces en un frame. Protege hechizos del jugador y proyectiles enemigos.

**Flags:**
- `bReplicates = true` — buena práctica aunque no haya multiplayer
- `PrimaryActorTick.bCanEverTick = false`
- `Lifespan = 15.f` (EditDefaultsOnly) — se destruye automáticamente si no impacta

**Colisión de personajes (configurada en APantheliaCharacterBase):**
- `GetMesh()->SetCollisionResponseToChannel(ECC_Projectile, ECR_Overlap)`
- `GetMesh()->SetGenerateOverlapEvents(true)` — solo el mesh genera overlaps
- `GetCapsuleComponent()->SetGenerateOverlapEvents(false)` — evita doble detección de daño

---

## 5. StartupAbilities — ✅ Funcional

**En APantheliaCharacterBase:**
- `TArray<TSubclassOf<UGameplayAbility>> StartupAbilities` (EditAnywhere, Category="Abilities")
- `AddCharacterAbilities()` — protected, llamado desde `AMainCharacter::PossessedBy()` (solo servidor)

**Flujo de otorgamiento:**

```
PossessedBy → AddCharacterAbilities() → ASC::AddCharacterAbilities() → por cada ability:
  - Crear FGameplayAbilitySpec(AbilityClass, Level=1)
  - Cast a UPantheliaGameplayAbility → agregar StartupInputTag a DynamicAbilityTags
  - GiveAbility(spec)
```

**ICombatInterface — GetCombatSocketLocation() — ✅ centralizado en C++ (C1 resuelto)**

`FVector GetCombatSocketLocation(const FGameplayTag& MontageTag)` — BlueprintNativeEvent. Devuelve la posición del socket desde el que se origina un ataque (proyectil, traza, etc.) según el tag del montage.

- Implementación **única en `APantheliaCharacterBase`**, que ramifica por `MontageTag` con `MatchesTagExact`:
  - `Montage.Attack.Weapon` → `FinalWeaponMesh` + `WeaponTipSocketName` (comportamiento original intacto — el Weapon Trace System no se ve afectado)
  - `Montage.Attack.RightHand` / `.LeftHand` / `.RightFoot` / `.LeftFoot` / `.Mouth` → `GetMesh()->GetSocketLocation(<nombre correspondiente>)`
  - Fallback: si el tag no se reconoce, `UE_LOG` de warning + `GetMesh()->GetComponentLocation()` (evita spawns en 0,0,0)
- **5 nombres de socket** configurables (UPROPERTY, Category="Combat|Sockets") con defaults estandarizados: `RightHandSocketName="RightHandSocket"`, `LeftHandSocketName="LeftHandSocket"`, `RightFootSocketName="RightFootSocket"`, `LeftFootSocketName="LeftFootSocket"`, `MouthSocketName="MouthSocket"`. Cada mesh usa estos nombres o sobrescribe el FName en su Blueprint.
- **Consecuencia de arquitectura:** la función está **centralizada en C++**; ningún enemigo necesita implementarla en Blueprint. Cualquier implementación Blueprint previa (p. ej. la de `BP_Boss`) debe **borrarse** para que use la versión de C++.
- Afecta al jugador (hereda de `CharacterBase`), pero sin romper nada: el jugador solo usa `Montage.Attack.Weapon`, que quedó idéntico. `WeaponTipSocketName` debe seguir configurado en `BP_ThirdPersonCharacter`.
- **Nota UHT crítica:** `ICombatInterface` SOLO en `APantheliaCharacterBase`. UHT no permite declararla también en `AMainCharacter` o `APantheliaEnemy` (herencia duplicada → error de compilación).

> **⚠️ Lección (gotcha de UE) — UPROPERTY con default en clase base + BP hijos preexistentes:** al añadir las 5 UPROPERTY de socket a `APantheliaCharacterBase`, los Blueprints que ya existían **no** recogieron los defaults y quedaron con los campos **vacíos** (`NAME_None`). Síntoma: el proyectil no aparecía, porque `GetSocketLocation(NAME_None)` devuelve el origen del actor. Fix: rellenar a mano los campos de `Combat|Sockets` en cada BP preexistente (p. ej. `BP_Boss → LeftHandSocketName = LeftHandSocket`). Regla general: al añadir una UPROPERTY con default a una clase base, revisar los BP hijos ya creados.

> **Overrides Blueprint eliminados:** `BP_Boss` y `BP_Shaman` tenían implementaciones Blueprint de `GetCombatSocketLocation` (entry → result vacío que devolvía 0,0,0) que **sobrescribían** la versión centralizada de C++. Ya **borradas**, así que ambos caen a la implementación de `APantheliaCharacterBase`. Detalle: la función aparecía bajo "Functions" en `BP_Shaman` y bajo "Interfaces" en `BP_Boss` — en ambos casos hay que borrarla.

---

## 6. Pipeline de Animación para Hechizos — ✅ Funcional

### Retargeting de Mixamo

- IK Rig para Mixamo: `IKRig_Mixamo` (esqueleto X-Bot de Mixamo)
- IK Rig para Mannequin: `IKRig_Mannequin` (esqueleto SK_Mannequin del personaje)
- Retargeter: `RTG_Mixamo_To_Mannequin`
- Proceso: importar animación con SK_Mixamo → click derecho → Retarget Animation Assets → seleccionar retargeter

> **⚠️ Lección — traslación en la raíz de animaciones Mixamo:** algunas animaciones de Mixamo (p. ej. el walk usado al principio en `BP_Shaman`) llevan traslación en el hueso raíz y se desplazan en preview pese a `bEnableRootMotion = false`. Activar **Force Root Lock** las rompe (el personaje "flota"). Solución aplicada: reemplazar por una animación equivalente que ya funcione en el proyecto sobre el mismo esqueleto (el walk de `BP_WarriorBoss`). Tenerlo en cuenta al traer animaciones nuevas de Mixamo.
>
> Relacionado (animación de locomoción de enemigos): un AnimBP de enemigo necesita alimentar su `Speed` con `Cast to Character → Get Velocity → VectorLength (VSize) → Set Speed`; si falta, `Speed = 0` siempre y el blend space se queda en idle permanente (bug que tuvo `ABP_Shaman`).

### Montage actual

- `AM_Standing_ProjectileMagic_Montage` — animación de casteo de hechizo de pie
- Slot: DefaultGroup.DefaultSlot
- Blend In: 0.0, Blend Out: 0.05
- AnimNotify que envía el GameplayEvent `Event.Montage.SpawnProjectile` en frame 15 (punto de lanzamiento de mano izquierda)

### Socket de spawn

- Nombre: `LeftHandSocket`
- Ubicación: hueso `hand_l` del SK_Mannequin
- Configurado en BP del personaje: `WeaponTipSocketName = "LeftHandSocket"`

### Blueprint GA_Firebolt (GA_Spell_1)

- Hereda de `UPantheliaProjectileSpell`
- StartupInputTag: `InputTag.Spell.1`
- ProjectileClass: `BP_FireboltProjectile`
- DamageEffectClass: `GE_Damage`
- DamageTypes: `{ Damage.Magical.Fire: CT_Damage[Abilities.Firebolt] }`
- Event Graph: `PlayMontageAndWait` + `WaitGameplayEvent` (escuchando `Event.Montage.SpawnProjectile`)
  - Bloquea movimiento al inicio de la ability
  - Cuando llega el evento del montage → llama `SpawnProjectile()` → restaura el movimiento
  - `EndAbility` en Completed / Interrupted / Cancelled
- **EndAbility es obligatorio** en soulslike: sin él la ability nunca termina y no puede reactivarse (`AbilityInputTagHeld` verifica `IsActive()`)

**Por qué WaitGameplayEvent:** sincroniza el spawn del proyectil con el frame exacto de la animación, en lugar de spawnearlo al activar la ability. El AnimNotify del montage emite el evento; la ability lo espera.

---

## 7. Sistema de Daño — ✅ Funcional (ExecCalc con elementos y escalado)

`GE_Damage`: GameplayEffect Instant **sin modifiers**. Toda la lógica de daño vive en `UExecCalc_Damage`, registrado en el array Executions del GE.

### Pipeline de daño completo

La **construcción** del daño es única (clase base); cambia solo cómo se **entrega** el spec según la fuente:

```
UPantheliaDamageGameplayAbility::MakeDamageSpec()      (construcción ÚNICA)
  → ApplyDamageScalingToSpec():
       daño base por tipo (DamageTypes × nivel)
       + escalado (Σ Ratio × Atributo del caster, vía AttributeScalings)
       + multiplicador proporcional entre tipos
       + SetByCaller por cada tipo de daño (valor ya escalado)
       + [opcional] SetByCaller(Damage.Poise, PoiseDamage)

  ── ENTREGA del spec (según la fuente) ────────────────────────────
  · Melee directo:  CauseDamage()      → ApplyGameplayEffectSpecToSelf al target
  · Proyectil:      SpawnProjectile()  → Projectile->DamageEffectSpecHandle → OnSphereOverlap → ApplyGameplayEffectSpecToSelf
  · Weapon Trace:   SetDamageSpec()    → UWeaponTraceComponent (sweep) → ApplyGameplayEffectSpecToSelf
  ──────────────────────────────────────────────────────────────────

  → UExecCalc_Damage::Execute_Implementation()
  → AddOutputModifier(IncomingDamage, daño calculado)
  → [si PoiseDamage > 0] AddOutputModifier(IncomingPoiseDamage)
  → UPantheliaAttributeSet::PostGameplayEffectExecute()
       · rama IncomingDamage:      aplica daño a Health; bFatal → ICombatInterface::Die()
       · rama IncomingPoiseDamage: FlinchThreshold → HitReact / Stagger / ResetPoiseRegen
```

El Weapon Trace (melee de enemigos) se documenta en `State_Combat.md` sección 9.

### UExecCalc_Damage — loop por tipo de daño

Itera sobre el mapa `DamageTypesToResistances` (ver `State_GAS.md` sección 4). Por cada tipo presente en el spec:

1. Leer el daño del SetByCaller (ya escalado desde la ability)
2. Si es **físico**: sumar `PhysicalDamage` del source como sumando genérico → mitigar con `EffectiveArmor`
3. Si es **mágico**: mitigar con `MagicResistance` (**⚠️ PENDIENTE** — chat de matemáticas). `MagicDamage` NO se suma (asimetría AP, ver sección 2)
4. **Tabla de afinidades ±15%** (elemento del atacante × `GetDefensiveElement()` del target)
5. **Resistencia elemental** del target (`FireResistance`, etc. — % de reducción del elemento)
6. Acumular en `TotalDamage`

Sobre el total: aplicar **Crítico** → escribir `IncomingDamage`. Si hubo crítico, marca el flag en el contexto (`SetIsCriticalHit`, ver `State_GAS.md` Custom Effect Context).

**Atributos capturados por el ExecCalc:**
- Target: `Armor`, `MagicResistance`, + las 4 resistencias elementales
- Source: `PhysicalDamage`, `CritChance`, `CritDamage`, `ArmorPenetration`, `MagicPenetration` (**NO** `MagicDamage`)

### Armor + ArmorPenetration (paso 2)

```
EffectiveArmor = Armor × (100 - ArmorPen × ArmorPenCoeff) / 100
Damage        *= (100 - EffectiveArmor × EffectiveArmorCoeff) / 100
```

Coeficientes desde `CT_DamageCalculationCoefficients` al nivel del personaje. Interpolación Constant.

### Tabla de afinidades (paso 4, ±15%)

Función estática `GetTypeChartMultiplier()` en el ExecCalc. Modificador al daño del atacante según el elemento defensivo del target:

| Atacante ↓ \ Defensor → | Fuego | Agua | Tormenta | Naturaleza |
|---|---|---|---|---|
| **Fuego** | — | −15% | +15% | neutro |
| **Agua** | +15% | — | −15% | neutro |
| **Tormenta** | −15% | +15% | — | **−15%** |
| **Naturaleza** | neutro | neutro | neutro | — |

- Triángulo: Fuego vence a Tormenta, Tormenta vence a Agua, Agua vence a Fuego.
- Naturaleza es puramente defensivo: resiste Tormenta entera (Aire y Rayo le hacen −15%), no hace daño extra a nadie.
- `Damage.Physical` genérico y atacante `None` → neutro.
- El elemento del atacante se obtiene del mapa `DamageTypeToElement`; el del defensor de `ICombatInterface::GetDefensiveElement()`.

### Golpe Crítico (sobre el total)

```
bCrit  = FMath::RandRange(1, 100) <= CritChance
Damage = bCrit ? 1.5 × Damage + CritDamage : Damage
```

Multiplicador base **1.5×** (moderado por diseño — para escalar más, el jugador invierte en `CritDamage`). **No existe `CritResistance`** — los críticos se evitan activamente (ver `State_Overview.md` sección 2.8). Si hay crítico, se marca el flag en el Custom Effect Context.

### Tipos de UE: PantheliaElementTypes.h

**Archivo nuevo `Public/PantheliaElementTypes.h`** (header manual, no generado por el wizard). Contiene `EPantheliaElement : uint8` (`None`, `Fire`, `Water`, `Storm`, `Nature`). Incluido en `PantheliaGameplayTags.h` y `CombatInterface.h`.

### Curve Tables de daño

**`CT_DamageCalculationCoefficients`** (en `Content/Blueprints/AbilitySystem/Data/`):
- Curva `ArmorPenetration`: 1→0.25, 10→0.15, 20→0.085, 40→0.035
- Curva `EffectiveArmor`: 1→0.333, 10→0.25, 20→0.15, 40→0.085
- Interpolación: Constant; asignada en `DA_CharacterClassInfo → Damage → DamageCalculationCoefficients`

**`CT_Damage`** (daño por hechizo por nivel):
- Curva `Abilities.Firebolt`: 1→5, 5→10, 10→16, 15→27, 20→41, 40→120
- Curva `Abilities.Enemy.Ranged`: 1→7.5, 40→35 (usada por `GA_RangedAttack` enemigo — ver sección 8)
- Asignada en `GA_Firebolt → DamageTypes → Damage.Magical.Fire → CT_Damage`, fila `Abilities.Firebolt`

---

## 8. Sistema de Arquetipos de Enemigos — ✅ Funcional

**`EPantheliaCharacterClass`** (enum): `Warrior`, `Ranger`, `Elementalist`. Define el perfil de atributos del enemigo. **El jugador no tiene arquetipo** — sus stats vienen del equipamiento.

### UPantheliaCharacterClassInfo (Data Asset)

Ubicación: `AbilitySystem/Data/`. Contiene:
- `CharacterClassInformation` — TMap arquetipo → `FCharacterClassDefaultInfo` (incluye el GE de atributos primarios de ese arquetipo)
- `DefaultSecondaryAttributes` — GE compartido entre todos los arquetipos (valores calculados por MMCs)
- `DefaultVitalAttributes` — GE compartido (llena Health/Mana/Stamina/Poise a sus máximos)
- `CommonAbilities` — TArray de abilities otorgadas a TODOS los enemigos al inicializarse (`GA_HitReact`, `GA_Stagger`, etc.)
- `DamageCalculationCoefficients` — UCurveTable con los coeficientes de daño escalables por nivel

**`DA_CharacterClassInfo`** — instancia única del Data Asset, asignada en `BP_PantheliaGameMode`.

### Curve Tables de atributos primarios

`CT_PrimaryAttributes_Warrior` / `_Ranger` / `_Elementalist`:
- Una Curve Table por arquetipo, interpolación Cubic o importadas desde CSV
- Filas: `Attributes.Primary.Hardness`, `Resonance`, `Resilience`, `Endurance`, `Spirit`
- El GE de primarios de cada arquetipo usa ScalableFloat apuntando a su CT correspondiente

### APantheliaEnemy — propiedades del arquetipo

- `CharacterClass` (EPantheliaCharacterClass, EditAnywhere, Category="Panthelia|Combat") — arquetipo del enemigo
- `Level` (int32) — nivel del enemigo, determina qué fila de la CT se usa
- `bHitReacting` (bool, BlueprintReadOnly) — true mientras el tag `Effects.HitReact` está activo
- `BaseWalkSpeed` (float) — velocidad base; se pone a 0 durante hit react, se restaura al terminar
- `Lifespan` (float, default = 5.0) — segundos antes de destruir el actor tras la muerte
- `bCanRangedAttack` (bool, EditAnywhere, default false) — habilita el ataque a distancia. **Reemplaza** la lógica anterior `RangedAttacker = (CharacterClass != Warrior)`: ahora se decide por Blueprint, lo que permite enemigos híbridos independientes del arquetipo de atributos. En `PossessedBy` escribe la key `RangedAttacker` del Blackboard desde este bool (la key del BB conserva su nombre).
- `CombatAbilities` (TArray<TSubclassOf<UGameplayAbility>>, EditAnywhere) — abilities específicas por enemigo (p. ej. `GA_RangedAttack`). Se conceden en `BeginPlay` al nivel del personaje, **después** de las abilities de clase de `DA_CharacterClassInfo`. Permite dar abilities a un enemigo concreto sin meterlas en la clase compartida.
- (propiedades de postura y elemento defensivo en `State_Combat.md`)

La inicialización de atributos del enemigo se hace vía `UPantheliaAbilitySystemLibrary::InitializeDefaultAttributes()`, que lee el arquetipo y nivel y aplica los GEs correspondientes del Data Asset.

### Sistema de ataque enemigo: melee / ranged / híbrido — ✅ Funcional

Los enemigos pueden atacar cuerpo a cuerpo, a distancia, o ambos (híbrido). En los **enemigos normales** la coordinación la hace el **Behavior Tree** activando abilities por tag. El **WarriorBoss** ya no usa Behavior Tree: su coordinación la lleva el sistema **StateTree + BossBrain + BossProfile** (ver `State_AI.md`), que igualmente activa las abilities por `AbilityTag` — la capa de ejecución (GAS + estos tags) es la misma para ambos.

**Tags de activación** (ver `State_GAS.md` sección 4): `Abilities.Attack.Melee` (→ `GA_MeleeAttack`) y `Abilities.Attack.Ranged` (→ `GA_RangedAttack`). Separados para que un híbrido no active ambas a la vez con un único `TryActivateAbilitiesByTag`.

**`GA_RangedAttack`** (Blueprint, hereda de `UPantheliaProjectileSpell`, en `AbilitySystem/Enemy/Abilities/`):
- AbilityTag: `Abilities.Attack.Ranged`
- Graph: `ActivateAbility` → Cast `CombatInterface` → `GetCombatTarget` → `GetActorLocation` → `UpdateFacingTarget` → `PlayMontageAndWait(CastMontage)` → `WaitGameplayEvent(EventTag = SocketTag, OnlyMatchExact = true, OnlyTriggerOnce = true)` → `EventReceived` → `SpawnProjectile`. `OnCompleted/Interrupted/Cancelled` → `EndAbility`.
- Usa **Blueprints hijos por enemigo** para variar `CastMontage` y `SocketTag` (p. ej. el hijo del TestBoss usa `LeftHand`).
- Daño configurado: `Damage.Magical.Fire`, curva `Abilities.Enemy.Ranged` en `CT_Damage` (nivel 1 = 7.5, nivel 40 = 35).

**Socket de lanzamiento:** lo resuelve `GetCombatSocketLocation(MontageTag)`, ahora **centralizado en C++** (ver sección 5). El `GA_RangedAttack` pasa su `SocketTag` y la función devuelve el socket correspondiente del mesh. Ya **no** se implementa por Blueprint en cada enemigo; si un boss antiguo (p. ej. `BP_Boss`) tenía una implementación Blueprint de esta función, hay que borrarla para que use la de C++. Históricamente, un enemigo sin la función implementada devolvía 0,0,0 y el proyectil spawneaba en el origen del mundo — el fallback en C++ ahora evita ese síntoma.

**Bandas de distancia en `BT_PantheliaEnemy`** (sistema híbrido):
- Rama Ranged: decorators de distancia `DistanceToTarget >= 250` **y** `< 600` (Observer Aborts = Self) → la rama ranged opera en la banda **250–600**.
- Resultado por distancia: `< 250` → melee, `250–600` → ranged, `500–4000` → aproximarse. En la zona de solape (250–500) la rama ranged gana sobre melee por ser el primer hijo del Selector.
- `BTT_Attack` melee usa `AttackTag = Abilities.Attack.Melee`; el ranged usa `Abilities.Attack.Ranged`. Ambos con `CombatTargetSelector = TargetToFollow`.

**Configuración por enemigo (Blueprint):** `bCanRangedAttack = true`, añadir `GA_RangedAttack` (o su hijo) a `CombatAbilities`, y `GA_MeleeAttack` con el tag `Abilities.Attack.Melee`.

**Ejemplo — `BP_Shaman`** (Sección 18, enemigo ranged puro): hijo de `BP_PantheliaEnemy`, mesh Mannequin retargeteado, `ABP_Shaman` propio (state machine Main → IdleWalk con `BS_Shaman_IdleWalk`), `CharacterClass = Elementalist`. Config: `bCanRangedAttack = true`, `CombatAbilities = [GA_RangedAttack_Shaman]`, `BehaviorTree = BT_PantheliaEnemy`, Actor Tag `Enemy`, lock-on heredado del padre. `GA_RangedAttack_Shaman` (hijo de `GA_RangedAttack`): `CastMontage = AM_Attack_Shaman`, `SocketTag = Montage.Attack.RightHand`, `ProjectileClass = BP_FireboltProjectile`, daño `Damage.Magical.Fire` con curva `Abilities.Enemy.Ranged`. Lanza desde la mano derecha (no bastón); no usa `AttackMontages` ni `WeaponTipSocketName`. `AM_Attack_Shaman` lleva tracks de MotionWarping (Warp Target = FacingTarget, solo rotación) y un `AN_MontageEvent` con tag `Montage.Attack.RightHand`. **Pendiente:** darle la ability en el flujo del BT y probar el disparo (clase 201).

> **Fix:** se añadió `GA_HitReact` al array `StartupAbilities` del arquetipo **Elementalist** en `DA_CharacterClassInfo` (faltaba, así que los Elementalist no reaccionaban a los golpes).

---

## 9. Sistema de Hit React y Muerte — ✅ Funcional

### Hit React

`GE_HitReact`: GE Infinite con componente "Grant Tags to Target Actor" que otorga `Effects.HitReact`.

`GA_HitReact` (Blueprint ability, Instancing Policy: Instance Per Actor):
- Se activa vía `TryActivateAbilitiesByTag(Effects.HitReact)` desde `PostGameplayEffectExecute`
- Aplica `GE_HitReact` al owner (guarda el handle en `ActiveGEHitReact`)
- Obtiene el montage vía `ICombatInterface::GetHitReactMontage()`
- `PlayMontageAndWait` → en OnCompleted / OnInterrupted / OnCancelled: `RemoveGameplayEffectFromOwner(ActiveGEHitReact)` + `EndAbility`
- Debe tener `Effects.HitReact` en sus **Ability Tags**
- Añadida al array `CommonAbilities` de `DA_CharacterClassInfo`

`APantheliaEnemy` se suscribe con `RegisterGameplayTagEvent(Effects.HitReact, NewOrRemoved)` y llama a `HitReactTagChanged()`, que actualiza `bHitReacting` y la `MaxWalkSpeed`.

> El hit react también puede dispararse por daño de postura cuando un golpe supera el `FlinchThreshold` (ver `State_Combat.md`, sistema de Postura). El stagger usa una ability hermana, `GA_Stagger`.

### Muerte

`ICombatInterface::Die()` — pure virtual. Llamada desde `PostGameplayEffectExecute` cuando `bFatal = true`.

**`APantheliaCharacterBase::Die()`:**
1. Desacopla el arma (`DetachFromComponent(KeepWorld)`)
2. Llama a `MulticastHandleDeath()`, que reproduce el `DeathSound` (`PlaySoundAtLocation`, al inicio — antes del dissolve), activa ragdoll en mesh y arma, desactiva la colisión del capsule y llama a `Dissolve()`. `DeathSound` (`USoundBase`, Combat|Impact) es un campo de `CharacterBase` heredado por todos; single-player, directo sin Cue (clase 208).

**`APantheliaCharacterBase::Dissolve()`:**
- Crea un `UMaterialInstanceDynamic` desde `DissolveMaterialInstance` y `WeaponDissolveMaterialInstance`
- Los asigna al mesh y al arma
- Llama a `StartDissolveTimeline()` y `StartWeaponDissolveTimeline()` (BlueprintImplementableEvents)
- **⚠️ PENDIENTE DIRECCIÓN ARTÍSTICA** — el código está completo pero los materiales de dissolve no están asignados todavía. Ver sección 11.4.

**`APantheliaEnemy::Die()`:**
1. Cancela el lockon si este enemigo era el target del jugador (ver `State_Combat.md` sección 3, "Fix lockon")
2. Setea la key `Dead = true` en el Blackboard (clase 202), para que el Behavior Tree deje de ejecutar lógica de combate. El BT usa un decorator "Am I Alive?" (comprueba `Dead`) en las ramas relevantes.
3. `SetLifeSpan(Lifespan)` — destruye el actor automáticamente
4. `Super::Die()` — ejecuta el ragdoll

---

## 10. Sistema de Boss Health Bar — ✅ Funcional

### ABossCharacter

Hereda de `APantheliaEnemy`.

**Variables:**
- `BossName` (FText, EditAnywhere, BlueprintReadOnly) — nombre visible en la barra
- `OnBossHealthChanged` (FBossAttributeChangedSignature, BlueprintAssignable) — delegate float
- `OnBossMaxHealthChanged` (FBossAttributeChangedSignature, BlueprintAssignable) — delegate float

**Delegate:** `DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FBossAttributeChangedSignature, float, NewValue)`

**Funciones:**
- `BeginPlay()` — bindea lambdas a `GetGameplayAttributeValueChangeDelegate` del ASC para Health y MaxHealth
- `BroadcastInitialValues()` (BlueprintCallable) — broadcastea MaxHealth y luego Health. Se llama desde el Level Blueprint **después** de crear el widget, para sincronizar valores iniciales (el `BeginPlay` del boss ocurre mucho antes de que el jugador entre al área).

### WBP_BossHealthBar

Hereda de `UPantheliaUserWidget`. Contiene:
- `WBP_HealthBar` (instancia de `PantheliaProgressBar`) — con ghost bar funcionando
- `TextBlock` para el nombre del boss

**Variables:** `CurrentHealth` (float), `CurrentMaxHealth` (float)

**Event Graph** — `Event WidgetControllerSet` → `Sequence` de 3 pines:
- Pin 0: Nombre del boss (Cast to BossCharacter → Get Boss Name → Set Text)
- Pin 1: Bind a `OnBossHealthChanged` → Set CurrentHealth → `SetProgressBarPercent`
- Pin 2: Bind a `OnBossMaxHealthChanged` → Set CurrentMaxHealth → `SetMaxValue` → `UpdateBoxSize` → `ForceSetPercent`

### Level Blueprint

Trigger Box en el área del boss:
- `On Actor Begin Overlap` → Cast to MainCharacter → Create Widget (WBP_BossHealthBar) → Cast to PantheliaUserWidget → Set Widget Controller (el boss) → Add to Viewport → `BroadcastInitialValues` (boss)
- El widget se guarda en la variable `BossHealthBarWidget` para poder ocultarlo más tarde

---

## 11. Pendientes del sistema de Abilities

### 11.1 Hechizos adicionales — ⚠️ Sin implementar

`GA_Spell_2` a `GA_Spell_5` y `GA_Spell_Ultimate`. Esperan a que el sistema de elementos esté definido (ver `State_Pending.md`). Las habilidades existentes aún no tienen `AttributeScalings` configurados — eso llega con el playtesting/balance.

### 11.2 Pipeline de daño — ✅ Implementado, con pendientes residuales

El ExecCalc cubre Armor/ArmorPenetration, tabla de afinidades elementales, resistencias elementales (placeholder) y Golpe Crítico. **Pendiente:**
- MagicResistance / MagicPenetration (paso 3 del ExecCalc) — se resolverá en el chat de matemáticas
- MMCs de `PhysicalDamage` y `MagicDamage` (ver `State_GAS.md` sección 7.4)
- Resistencias elementales en placeholder (derivadas de Resilience — ver `State_GAS.md` sección 1 y `State_Pending.md`)

### 11.3 Pendientes del boss health bar

- Ocultar la barra al salir del área (`On Actor End Overlap` → `Remove from Parent`)
- Ocultar la barra al morir el boss

### 11.4 Dissolve materials — ⚠️ Pendiente dirección artística

La infraestructura C++ está lista (`Dissolve()`, `StartDissolveTimeline()`, `StartWeaponDissolveTimeline()`). Falta:
- Crear los materiales de dissolve (Blend Mode: Masked + nodos de dissolve noise + parámetro escalar `Dissolve`)
- Asignarlos en cada BP de enemigo (`DissolveMaterialInstance`, `WeaponDissolveMaterialInstance`) — incluido el **WarriorBoss** (aún no los tiene) y el **Shaman** (clase 203: instrucciones dadas, pendiente confirmar que funciona)

### 11.5 VFX/SFX de crítico — ⚠️ Pendiente

El flag `bIsCriticalHit` ya viaja por el Custom Effect Context (ver `State_GAS.md`). Falta disparar el VFX/SFX cuando se detecta un crítico en el target.
