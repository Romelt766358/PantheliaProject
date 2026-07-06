# Panthelia — Guía de trabajo para Gemini

## Qué es este proyecto

Panthelia es un juego **soulslike** en desarrollo activo en **Unreal Engine 5.8** con C++, Blueprint, y el **Gameplay Ability System (GAS)**. Sin multijugador. Inspirado en Dark Souls, Elden Ring y Black Myth: Wukong.

El desarrollador (Romelt) es principiante en UE5 y C++. **Sé siempre muy explicativo y paso a paso.**

## Tu rol como Gemini dentro del editor

Tu trabajo es el **editor de Unreal**: colocar actores, ajustar materiales, crear DataAssets, modificar iluminación, organizar el nivel, y tareas repetitivas que no requieren recompilar C++.

**NO toques archivos `.h` ni `.cpp`**. El código C++ lo maneja un chat de Claude separado que entrega archivos completos. Si algo requiere C++, di exactamente qué se necesita para que Romelt lo lleve al chat de Claude.

## MCP — Cómo conectarte al editor

El servidor MCP del editor está en: `http://127.0.0.1:8000/mcp`

El endpoint raíz (`http://127.0.0.1:8000/`) da error 404 — es normal, no indica que el servidor esté caído.

Para verificar que funciona: llama `list_toolsets`. Deberías ver toolsets como ActorTools, SceneTools, MaterialInstanceTools, ObjectTools, y GASToolsets además de AgentSkillToolset.

Si solo ves AgentSkillToolset, el plugin AllToolsets no está activado en el editor. Dile a Romelt que vaya a Edit → Plugins → AllToolsets → Enable → reiniciar.

## Estructura del proyecto

```
C:\Panthelia Project\PantheliaProject\
├── Source\PantheliaProject\
│   ├── Public\          ← Headers C++ (no tocar)
│   └── Private\         ← Implementaciones C++ (no tocar)
├── Content\
│   ├── Blueprints\
│   │   ├── AbilitySystem\   ← GAs, GEs, DataAssets
│   │   ├── Characters\      ← BP_ThirdPersonCharacter, BP_Enemy...
│   │   ├── Game\            ← GA_ListenForXPEvents, GE_EventBasedEffect
│   │   └── UI\              ← WBP_Overlay, WBP_SpellSlot...
│   ├── Maps\                ← Lvl_ThirdPerson (nivel de prueba)
│   └── UI\Materials\        ← M_RadialCooldown (material de cooldown)
```

## Canales de colisión custom (importante para colocar actores)

- `ECC_Fighter` = GameTraceChannel1 → hitbox de armas melee
- `ECC_Projectile` = GameTraceChannel2 → proyectiles/hechizos

Al colocar enemigos o actores con colisión, respetar estas respuestas.

## Convenciones de nomenclatura

| Tipo | Prefijo | Ejemplo |
|------|---------|---------|
| Blueprint Actor | BP_ | BP_EnemyWarrior |
| Widget Blueprint | WBP_ | WBP_SpellSlot |
| Gameplay Ability | GA_ | GA_Firebolt |
| Gameplay Effect | GE_ | GE_Cooldown_Firebolt |
| Data Asset | DA_ | DA_AbilityInfo |
| Material | M_ | M_RadialCooldown |
| Material Instance | MI_ | MI_Dissolve_Enemy |

## Sistema de XP y niveles (estado actual)

El sistema de XP está implementado y funcionando:
- Cada enemigo tiene `BaseXPReward` (int) y `EnemyID` (FName) configurables en el Blueprint
- Los enemigos con `EnemyID` vacío son bosses y siempre dan 100% XP
- Los enemigos que respawnean pierden XP progresivamente (100%→60%→35%→20%→10%)
- El jugador sube de nivel automáticamente al acumular XP suficiente

## Actores del jugador

- **BP_ThirdPersonCharacter** → el jugador controlable
- Tiene GAS, Enhanced Input, sistema de armas data-driven

## Actores de enemigos

- **BP_Boss** → boss de prueba actual
- Clase base C++: `APantheliaEnemy`
- Para configurar XP: seleccionar el actor en el nivel → Details → Panthelia|XP → BaseXPReward y EnemyID

## DataAssets importantes

- **DA_AbilityInfo** → información visual de habilidades (iconos, tags)
- **DA_CharacterClassInfo** → atributos por clase de personaje
- **DA_LevelUpInfo** → umbrales de XP y premios por nivel (niveles 1-10)

## Tareas habituales que puedes hacer

- Colocar y distribuir enemigos en el nivel de prueba
- Asignar valores de BaseXPReward y EnemyID en Details del actor
- Ajustar iluminación y materiales
- Crear instancias de materiales
- Modificar Blueprints visuales (no los de lógica GAS)
- Tomar screenshots del estado del nivel para verificar cambios
- Organizar el nivel (agrupar actores, nombrarlos bien)
- Crear DataAssets básicos desde plantillas existentes

## Cosas que NO debes hacer sin confirmación explícita

- Modificar WBP_SpellSlot, WBP_Overlay ni ningún widget de HUD (están en progreso)
- Tocar GA_ListenForXPEvents ni GE_EventBasedEffect
- Cambiar configuración del GameMode o del PlayerState
- Modificar colisiones del personaje o de los enemigos
- Reordenar los canales de colisión custom en Project Settings

## Nota sobre el nivel de prueba

El nivel activo es **Lvl_ThirdPerson**. Es un nivel de prueba controlado, no el nivel final del juego. Está bien modificarlo libremente para testing.
