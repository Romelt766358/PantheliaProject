# State_Validation.md — Validación de Data Assets

> Data Validation de los Data Assets principales (Bloque 4B de la campaña de hardening, 2026-07-16). El objetivo es que un asset mal configurado **falle en el editor con un mensaje claro**, no en runtime de forma silenciosa. Encaja con la prioridad de escalabilidad: cuanto más crezca el árbol y el contenido data-driven, más barato es que el editor avise antes de jugar.

---

## 1. Familias validadas

Se implementaron validadores (`IsDataValid` / utilidades en `PantheliaDataValidationUtils`) para **siete** familias de Data Assets:

1. **Ability Info** (`UPantheliaAbilityInfo`)
2. **Elemental Status Config** (`UPantheliaElementalStatusConfig`)
3. **Level Up Info** (`UPantheliaLevelUpInfo`)
4. **Skill Tree Info** (`UPantheliaSkillTreeInfo`)
5. **Boss Profile** (`UPantheliaBossProfile`)
6. **Weapon Definition** (`UPantheliaWeaponDefinition`)
7. **Input Config** (`UPantheliaInputConfig`)

Cada validador comprueba lo propio de su dominio (tags requeridos no vacíos, referencias no nulas, coherencia de índices/rangos, unicidad de IDs, etc.) y reporta errores **bloqueantes** y warnings **no bloqueantes** de forma diferenciada.

---

## 2. Detección de ciclos en el árbol (DFS)

El árbol de habilidades no puede tener ciclos en sus prerequisitos. El validador del Skill Tree hace un **DFS de detección de ciclos**. Se corrigió un bug sutil: el DFS **retenía referencias a entradas del `TMap`** que podían invalidarse por **rehash** al modificar el contenedor durante el recorrido; ahora no se retienen esas referencias.

**Pruebas confirmadas:**
- Cadena **válida de 8 nodos** → aceptada.
- **Ciclo de 8 nodos** → rechazado.
- Assets temporales de prueba → retirados.
- Assets productivos → válidos, salvo warnings no bloqueantes registrados (abajo).

---

## 3. Warnings vivos (no bloqueantes)

Configuración incompleta que el validador reporta pero **no** bloquea, pendiente de rellenar como contenido:
- `DA_Sword_Basic.WeaponName` — vacío.
- **Firebolt** `BackgroundMaterial` — vacío.

Estos warnings son deliberadamente no bloqueantes: son campos de presentación/contenido, no de integridad estructural.

---

## 4. Contrato para contenido nuevo

Al crear un Data Asset de cualquiera de las 7 familias, ejecutar la validación del editor antes de darlo por bueno. Si se añade una **familia nueva** de Data Asset con relaciones internas (referencias, índices, grafos), conviene darle su propio validador siguiendo el patrón de `PantheliaDataValidationUtils` — especialmente si puede formar grafos (reusar el DFS de ciclos ya corregido).

---

## 5. Validación de cook/package — migración MetaHuman (Fases 4G-4I)

Matriz de la validación por build cocinada (complementa la validación en PIE de las fases funcionales).

- **Fase 4G** — primer package funcional; smoke del jugador MetaHuman.
- **Fase 4H** — auditoría **solo lectura**: `errors=[]`, `dirty_before=[]`, `dirty_after=[]`. **Sin cambios C++, ni de abilities/montages/timings/costes/tags/balance. Sin PIE separado.**
- **Fase 4I** — 2 assets eliminados (`BP_BaseKatana`, `BP_ThirdPersonGameMode`); 185 archivos iguales, 0 modificados; package posterior PASS; smoke posterior PASS.

**Datos finales del package:**
```
UnrealBuildTool ExitCode=0 · UnrealEditor-Cmd ExitCode=0 · AutomationTool ExitCode=0
0 cook errors · 29 cook warnings · 1258 cooked packages · 1265 descubiertos
```

**Smoke cocinado final (PASS):** mapa, MetaHuman, Body/Face/grooms, movimiento/cámara, ataque/daño, dodge, guardia/parry, muerte multipart.

> **Desviación documentada:** no hubo PIE separado durante 4H/4I. Justificación del cierre: cero assets productivos modificados, solo dos eliminaciones legacy, package completo y smoke cocinado exitoso. Las fases funcionales previas sí se validaron en PIE.
