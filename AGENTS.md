# PantheliaProject - AI Agent Instructions

## General

- Read the entire file before modifying it.
- Understand the existing architecture before changing anything.
- Never assume. Ask if something is unclear.
- Preserve compilation after every task.

## Architecture

- Scalability is the highest priority.
- Modularity is the highest priority.
- Prefer Components over inheritance.
- Prefer Data Assets over hardcoded values.
- Prefer Gameplay Tags over booleans.
- Prefer reusable systems over boss-specific code.

## Unreal

- Unreal Engine 5.8.
- Gameplay Ability System (GAS) is the gameplay backbone.
- Single-player only.
- Never make architecture decisions for multiplayer.

## C++

- Do not rename classes unless requested.
- Do not refactor unrelated code.
- Do not remove existing comments from .h/.cpp files unless the associated code is removed.
- Make the smallest correct change.

## Boss AI

The architecture is:

StateTree
→ BossBrainComponent
→ BossProfile Data Asset
→ Gameplay Ability System

Responsibilities:

StateTree:
- Controls high-level states.
- Never chooses attacks.

BossBrain:
- Reads BossProfile.
- Chooses actions.
- Handles cooldowns.
- Activates GameplayAbilities.

BossProfile:
- Contains configuration only.
- No gameplay logic.

Gameplay Abilities:
- Execute attacks.

## Workflow

Before changing anything:

1. Read the relevant code.
2. Explain the plan.
3. List every file that will be modified.
4. Wait for confirmation if the architecture changes.