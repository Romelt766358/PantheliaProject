# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project overview

Panthelia is a single-player soulslike action RPG built in **Unreal Engine 5.8**, using C++, Blueprint, and the **Gameplay Ability System (GAS)**. The developer (Romelt) is new to UE5/C++, so explanations should be step-by-step and explicit — this is reflected throughout the codebase in unusually long, teaching-style Spanish comments explaining *why* something was built a certain way (not just what it does). Preserve that commenting style when editing files that already use it.

There is no multiplayer support and none should be assumed — never make architecture decisions for multiplayer (no need to account for replication correctness beyond what already exists).

## Working agreements (from AGENTS.md)

- Read a file completely before modifying it. Understand the existing architecture before changing anything. Ask rather than assume.
- Preserve compilation after every task — this is a C++ UE project; a broken build blocks the editor for everyone.
- Do not rename classes unless requested. Do not refactor unrelated code. Do not remove existing comments from `.h`/`.cpp` files unless the associated code is removed. Make the smallest correct change.
- Prefer Components over inheritance, Data Assets over hardcoded values, Gameplay Tags over booleans, and reusable systems over boss/enemy-specific one-offs.
- Before changing anything non-trivial: read the relevant code, explain the plan, and list every file that will be modified. Wait for confirmation if the change touches architecture.
- A separate Gemini session handles in-editor tasks (placing actors, materials, DataAsset instances, lighting) and is instructed never to touch `Source/**`. C++ changes always go through this Claude session.

## Build

This is a standard Unreal Engine 5.8 C++ project — there is no CLI test/lint/build script; builds go through the engine toolchain:

- **Regenerate project files** (after adding/removing `.cpp`/`.h` files): right-click `PantheliaProject.uproject` → "Generate Visual Studio project files", or run the engine's `GenerateProjectFiles` batch script against the `.uproject`.
- **Compile**: open `PantheliaProject.sln` in Visual Studio (or the `.slnx` variant) and build the `PantheliaProjectEditor` target (Development Editor / Win64), or use "Live Coding" from within the running editor for fast iteration.
- **Editor target vs game target**: `Source/PantheliaProject.Target.cs` (Game) and `Source/PantheliaProjectEditor.Target.cs` (Editor) both build the single module `PantheliaProject`. `PantheliaProject.Build.cs` lists the module dependencies — check it before assuming a UE module (e.g. `Niagara`, `UMG`, `GameplayStateTreeModule`, `AIModule`) is available.
- There are no automated C++ unit tests (`IMPLEMENT_*_AUTOMATION_TEST`) in this codebase. Verification is done by compiling and by Play-In-Editor (PIE) testing, including through the `unreal-mcp` MCP server (`.mcp.json`) connected to a running editor instance for driving/inspecting PIE sessions.
- Editor plugin `AllToolsets`/`MCPClientToolset`/`ModelContextProtocol` expose the editor over MCP at `http://127.0.0.1:8000/mcp` — useful for placing actors, checking Blueprint state, or running PIE without leaving the chat.

## Architecture

### Layering: C++ base classes + Blueprint configuration

Nearly everything gameplay-relevant is a C++ class exposing `UPROPERTY`/`UFUNCTION` surface that a matching Blueprint (`BP_*`) configures — sockets, montages, Data Asset references, tuning floats. When changing a C++ class's behavior, check whether the actual values live in a Blueprint before assuming a bug is in C++.

- `APantheliaCharacterBase` (`Characters/PantheliaCharacterBase.h`) — shared base for player and enemies. Owns poise/flinch, elemental buildup decay, death shutdown sequencing (`ShutdownGameplayForDeath` → `OnDeathGameplayShutdown` per-subclass → `MulticastHandleDeath` → dissolve), and the debuff Niagara components (Burn/Shock/Saturation/Poison).
  - `AMainCharacter` (player) and `APantheliaEnemy` (all enemies, including `ABossCharacter`) both derive from it.
  - The player's `UAbilitySystemComponent` lives on `APantheliaPlayerState` (survives Pawn respawn); enemies own their ASC directly on the character. `InitAbilityActorInfo` is deliberately *not* called via `Super()` — each subclass sets its own ASC pointer first, then broadcasts `OnASCRegistered` itself.
- **Interfaces over hard casts**: `ICombatInterface` (`Interfaces/CombatInterface.h`) is the contract nearly all combat code depends on instead of concrete classes — montage lookup, socket resolution (`GetCombatSocketLocation`, keyed by `Montage.Attack.*` tags: Weapon/RightHand/LeftHand/RightFoot/LeftFoot/Mouth), death handling, and the `FOnASCRegistered`/`FOnDeath` delegates that let reusable components (e.g. debuff Niagara) react without knowing about `APantheliaCharacterBase` concretely. Other small interfaces: `IEnemy` (lock-on/highlight), `IMainPlayer`, `IPantheliaPlayerInterface`.

### Gameplay Ability System

- `PantheliaGameplayTags.h/.cpp` — a singleton (`FPantheliaGameplayTags::Get()`) of native gameplay tags, initialized once by `UPantheliaAssetManager::StartInitialLoading()`. This is the vocabulary the whole combat/AI/UI system is built on: input tags, damage type tags, ability tags, boss action/state tags, status-effect ("buildup") tags, cooldown tags, etc. It also holds the cross-reference maps (`DamageTypesToResistances`, `DamageTypeToElement`, `ElementToDebuff`, `ElementToResistance`, `ElementToStatusPower`) that `ExecCalc_Damage` uses to resolve elemental interactions. Read the extensive header comments before adding a new tag family — the hierarchy conventions (empty parent tags for hierarchical queries, leaf tags for concrete grants) are intentional and consistent throughout.
- `UPantheliaAbilitySystemLibrary` — the shared static toolbox for reading/writing the custom `FPantheliaGameplayEffectContext` (crit, parry/block, dodge response, defense attack type, hit outcome, debuff results, knockback/launch/death-impulse vectors), applying secondary damage effects (`FDamageEffectParams`/`ApplyDamageEffect`), granting temporary tags, and XP diminishing-returns math (`GetXPMultiplierForKillCount`).
- `FPantheliaGameplayEffectContext` (`AbilitySystem/PantheliaAbilityTypes.h`) — custom `FGameplayEffectContext` subclass carrying crit/parry/block/debuff/knockback/launch results between `ExecCalc_Damage` and `PostGameplayEffectExecute`. Any new per-hit result should be added here (with getter/setter pairs) rather than invented as a separate side channel.
- Combat rules of note baked into the tag/context design: no RNG on status effects — elemental "debuffs" (Burn/Shock/Saturation/Poison) trigger deterministically off a 0–100 buildup bar per element (Elden Ring/Lies of P style), not a chance roll (`Debuff_Chance` was deliberately removed). Crit is the only random roll in combat.
- Damage abilities derive from `UPantheliaDamageGameplayAbility`; player-specific ones (`PantheliaPlayerAttackAbility`, `PantheliaPlayerHeavyAttackAbility`, `PantheliaPlayerDodgeAbility`) and boss/enemy ones (`PantheliaMeleeAbility`, `PantheliaParryAbility`, spells under `AbilitySystem/Abilities/`) branch from there.

### Boss AI

Documented explicitly in `AGENTS.md` — keep this separation when touching boss logic:

```
StateTree → UPantheliaBossBrainComponent → UPantheliaBossProfile (Data Asset) → Gameplay Ability System
```

- **StateTree** controls high-level state only; it never chooses attacks.
- **`UPantheliaBossBrainComponent`** (`AI/PantheliaBossBrainComponent.h`) reads the `UPantheliaBossProfile`, selects/weights actions (with cooldowns and short-term anti-repetition memory), tracks phase transitions off health thresholds, and activates the chosen `GameplayAbility` by tag. It exposes a small state machine (`EPantheliaBossActionRuntimeState`: None → Selected → Starting → Running → Finished/Failed/Interrupted) that StateTree polls.
- **`UPantheliaBossProfile`** is pure configuration (actions, phases, weights, stats presets) — no gameplay logic belongs there.
- **Gameplay Abilities** execute the actual attacks.
- Regular (non-boss) enemies instead use `APantheliaAIController` + a Behavior Tree (`APantheliaEnemy::BehaviorTree`), with `bCanRangedAttack` toggling the ranged branch independent of character class.

### Equipment & weapons

- `UPantheliaEquipmentComponent` (player only — enemies use a fixed weapon mesh, not this component) manages a transactional equip/unequip flow (`TryEquipWeapon`): validate owner/socket/class/definition → spawn into a temp variable → validate mesh/sockets/attachment → only then publish and destroy the old weapon. A failed equip never mutates the previously-equipped weapon.
- `UPantheliaWeaponDefinition` (Data Asset) carries per-weapon moveset/damage/stamina cost; `APantheliaWeapon` is the spawned actor; `EWeaponType` is the broad family (movement/idle set), currently just `Sword`.
- `UWeaponTraceComponent` resolves the actual weapon mesh at `BeginPlay` (by actor tag `"Weapon"`, or first static mesh found) independent of whether the character was built in C++ or Blueprint — this is why `APantheliaCharacterBase::ResolveDeathWeaponMesh()` exists: `FinalWeaponMesh` (a C++-created skeletal mesh component on the base class) is often unused by real enemies, whose actual weapon is a separate Blueprint-added static mesh component that only `UWeaponTraceComponent` knows how to find.

### UI / Widget Controllers

MVC-ish split: `UPantheliaWidgetController` (base) is constructed with `FWidgetControllerParams` (PlayerController, PlayerState, ASC, AttributeSet) via `UPantheliaAbilitySystemLibrary::GetOverlayWidgetController`/`GetAttributeMenuWidgetController`. Subclasses (`OverlayWidgetController`, `AttributeMenuWidgetController`) bind to ASC/attribute delegates once (`bCallbacksBound` guards against duplicate binds from Blueprint) and broadcast initial values to UMG widgets (`PantheliaUserWidget`, `PantheliaProgressBar`).

### Asset Manager & tags bootstrapping

`UPantheliaAssetManager::StartInitialLoading()` is the entry point that calls `FPantheliaGameplayTags::InitializeNativeGameplayTags()` before anything else loads — if a new native tag isn't showing up at runtime, confirm it was added to both the header declaration and the `.cpp` initializer.

### Custom collision channels

- `ECC_Fighter` = `GameTraceChannel1` — melee weapon hitboxes.
- `ECC_Projectile` = `GameTraceChannel2` — projectiles/spells.

### Input

`UPantheliaInputConfig` (Data Asset) maps `UInputAction` (Enhanced Input) → semantic `FGameplayTag` (`InputTag.LightAttack`, etc.), so abilities key off tags rather than raw input actions and keybinds can change without touching ability code. `UPantheliaInputComponent` binds through this config.

## Naming conventions (asset/Blueprint side)

| Type | Prefix | Example |
|---|---|---|
| Blueprint Actor | `BP_` | `BP_EnemyWarrior` |
| Widget Blueprint | `WBP_` | `WBP_SpellSlot` |
| Gameplay Ability | `GA_` | `GA_Firebolt` |
| Gameplay Effect | `GE_` | `GE_Cooldown_Firebolt` |
| Data Asset | `DA_` | `DA_AbilityInfo` |
| Material | `M_` | `M_RadialCooldown` |
| Material Instance | `MI_` | `MI_Dissolve_Enemy` |

## Logging

Use `LogPanthelia` (declared in `PantheliaLogChannels.h`), not `LogTemp`, so messages are filterable in the Output Log: `UE_LOG(LogPanthelia, Log/Warning/Error, TEXT("..."));`.

## Content layout (high level)

- `Content/Blueprints/AbilitySystem` — GAs, GEs, Data Assets.
- `Content/Blueprints/Characters` — `BP_ThirdPersonCharacter` (player), `BP_Enemy*`, `BP_Boss`.
- `Content/Blueprints/Game`, `Content/Blueprints/UI` — event/XP glue Blueprints and HUD widgets.
- `Content/Maps/Lvl_ThirdPerson` — the active test level; safe to modify freely, not the final game level.
- `CodexTemp/` — scratch/history artifacts from prior agent sessions (audit reports, PIE test scripts); not part of the shipped project.
