import bpy
import os
import json
from mathutils import Vector

SOURCE = r"C:\Panthelia Project\PantheliaProject\ExternalAssets\Armor\GildedKnight\Blender\Panthelia_GildedKnight_Fit_v008_ExecutableSkinned.blend"
OUTPUT = r"C:\Panthelia Project\PantheliaProject\ExternalAssets\Armor\GildedKnight\Blender\Panthelia_GildedKnight_Fit_v009_LegArmorRealFix.blend"
REPORT = r"C:\Panthelia Project\PantheliaProject\ExternalAssets\Armor\GildedKnight\Blender\Diagnostics\v009_LegArmorRealFix.json"

bpy.ops.wm.open_mainfile(filepath=SOURCE)

obj = bpy.data.objects.get("WORK_LegArmor")
arm = bpy.data.objects.get("REF_Panthelia_Body_Armature")

if obj is None or obj.type != "MESH":
    raise RuntimeError("No existe WORK_LegArmor como objeto Mesh")

if arm is None or arm.type != "ARMATURE":
    raise RuntimeError("No existe REF_Panthelia_Body_Armature")

if len(obj.data.vertices) != 2608:
    raise RuntimeError(
        f"Conteo inesperado de WORK_LegArmor: {len(obj.data.vertices)}"
    )

# Trabajar y validar en rest pose.
arm.data.pose_position = "REST"

for pose_bone in arm.pose.bones:
    pose_bone.matrix_basis.identity()

bpy.context.view_layer.update()

# Backup interno real antes de tocar geometría.
backup = obj.copy()
backup.data = obj.data.copy()
backup.name = "BACKUP_V009_LegArmor_PreRealFix"

diagnostics = bpy.data.collections.get("03_DIAGNOSTICS")

if diagnostics is None:
    diagnostics = bpy.data.collections.new("03_DIAGNOSTICS")
    bpy.context.scene.collection.children.link(diagnostics)

diagnostics.objects.link(backup)
backup.hide_set(True)
backup.hide_render = True

# Encontrar las dos islas desconectadas.
mesh = obj.data
adjacency = [set() for _ in mesh.vertices]

for edge in mesh.edges:
    a, b = edge.vertices
    adjacency[a].add(b)
    adjacency[b].add(a)

visited = set()
components = []

for start in range(len(mesh.vertices)):
    if start in visited:
        continue

    stack = [start]
    visited.add(start)
    component = []

    while stack:
        current = stack.pop()
        component.append(current)

        for neighbor in adjacency[current]:
            if neighbor not in visited:
                visited.add(neighbor)
                stack.append(neighbor)

    components.append(component)

components.sort(key=len, reverse=True)

if len(components) != 2:
    raise RuntimeError(
        f"WORK_LegArmor debía tener 2 islas; encontradas: {len(components)}"
    )

# Obtener centros mundiales de las islas para emparejar L/R.
component_data = []

for component in components:
    points = [
        obj.matrix_world @ mesh.vertices[index].co
        for index in component
    ]

    center = sum(points, Vector()) / len(points)

    component_data.append({
        "indices": component,
        "center": center,
    })

component_data.sort(key=lambda item: item["center"].x)

# Obtener los ejes anatómicos calf en rest pose.
bone_data = []

for bone_name in ("calf_l", "calf_r"):
    bone = arm.data.bones.get(bone_name)

    if bone is None:
        raise RuntimeError(f"No existe el hueso {bone_name}")

    a = arm.matrix_world @ bone.head_local
    b = arm.matrix_world @ bone.tail_local

    if a.z >= b.z:
        knee = a
        ankle = b
    else:
        knee = b
        ankle = a

    axis = knee - ankle
    length = axis.length

    if length < 5.0:
        raise RuntimeError(
            f"Longitud de {bone_name} inválida: {length}"
        )

    axis.normalize()

    bone_data.append({
        "name": bone_name,
        "knee": knee,
        "ankle": ankle,
        "axis": axis,
        "length": length,
        "center_x": ((knee + ankle) * 0.5).x,
    })

bone_data.sort(key=lambda item: item["center_x"])

world_to_local = obj.matrix_world.inverted()
results = []

# La greave final debe ocupar el segmento calf, no llegar al pie.
TARGET_ABOVE_ANKLE = 3.0
TARGET_BELOW_KNEE = 3.0

for component, bone in zip(component_data, bone_data):
    axis = bone["axis"]
    ankle = bone["ankle"]
    knee = bone["knee"]
    bone_length = bone["length"]

    target_min = TARGET_ABOVE_ANKLE
    target_max = bone_length - TARGET_BELOW_KNEE

    if target_max <= target_min:
        raise RuntimeError(
            f"Intervalo anatómico inválido para {bone['name']}"
        )

    world_points_before = [
        obj.matrix_world @ mesh.vertices[index].co
        for index in component["indices"]
    ]

    projections_before = [
        (point - ankle).dot(axis)
        for point in world_points_before
    ]

    current_min = min(projections_before)
    current_max = max(projections_before)
    current_range = current_max - current_min

    if current_range < 1.0:
        raise RuntimeError(
            f"Rango longitudinal inválido para {bone['name']}"
        )

    target_range = target_max - target_min
    longitudinal_scale = target_range / current_range

    # Transformación afín longitudinal:
    # - el borde superior termina 3 cm bajo rodilla;
    # - el borde inferior termina 3 cm sobre tobillo;
    # - la distancia radial al eje no cambia.
    for index in component["indices"]:
        world = obj.matrix_world @ mesh.vertices[index].co

        projection = (world - ankle).dot(axis)
        radial = world - (ankle + axis * projection)

        normalized = (projection - current_min) / current_range
        new_projection = target_min + normalized * target_range

        new_world = ankle + axis * new_projection + radial
        mesh.vertices[index].co = world_to_local @ new_world

    results.append({
        "bone": bone["name"],
        "before_min": float(current_min),
        "before_max": float(current_max),
        "before_length": float(current_range),
        "target_min": float(target_min),
        "target_max": float(target_max),
        "target_length": float(target_range),
        "longitudinal_scale": float(longitudinal_scale),
        "bottom_lift_cm": float(target_min - current_min),
        "top_shift_cm": float(target_max - current_max),
    })

mesh.update()
bpy.context.view_layer.update()

# Validar la malla FINAL evaluada con el Armature Modifier activo.
depsgraph = bpy.context.evaluated_depsgraph_get()
evaluated_obj = obj.evaluated_get(depsgraph)
evaluated_mesh = evaluated_obj.to_mesh()

try:
    validation = []

    for component, bone in zip(component_data, bone_data):
        axis = bone["axis"]
        ankle = bone["ankle"]
        bone_length = bone["length"]

        evaluated_points = [
            evaluated_obj.matrix_world @ evaluated_mesh.vertices[index].co
            for index in component["indices"]
        ]

        projections = [
            (point - ankle).dot(axis)
            for point in evaluated_points
        ]

        final_min = min(projections)
        final_max = max(projections)

        distance_above_ankle = final_min
        distance_below_knee = bone_length - final_max

        validation.append({
            "bone": bone["name"],
            "final_min": float(final_min),
            "final_max": float(final_max),
            "distance_above_ankle_cm": float(distance_above_ankle),
            "distance_below_knee_cm": float(distance_below_knee),
        })

        # No aceptar un resultado invisible o compensado por el modifier.
        if distance_above_ankle < 2.4:
            raise RuntimeError(
                f"{bone['name']}: la malla evaluada todavía llega al tobillo "
                f"({distance_above_ankle:.3f} cm)"
            )

        if distance_below_knee < 2.4:
            raise RuntimeError(
                f"{bone['name']}: la malla evaluada invade la rodilla "
                f"({distance_below_knee:.3f} cm)"
            )

        if distance_above_ankle > 4.0:
            raise RuntimeError(
                f"{bone['name']}: la greave terminó excesivamente alta sobre "
                f"el tobillo ({distance_above_ankle:.3f} cm)"
            )

        if distance_below_knee > 4.0:
            raise RuntimeError(
                f"{bone['name']}: la greave terminó excesivamente baja bajo "
                f"la rodilla ({distance_below_knee:.3f} cm)"
            )

finally:
    evaluated_obj.to_mesh_clear()

# Seleccionar únicamente el resultado corregido.
bpy.ops.object.select_all(action="DESELECT")
obj.hide_set(False)
obj.hide_render = False
obj.select_set(True)
bpy.context.view_layer.objects.active = obj

os.makedirs(os.path.dirname(REPORT), exist_ok=True)

report = {
    "success": True,
    "source": SOURCE,
    "output": OUTPUT,
    "object": obj.name,
    "vertex_count": len(mesh.vertices),
    "component_count": len(components),
    "transform_type": "longitudinal_affine_remap_anchored_between_knee_and_ankle",
    "results": results,
    "evaluated_mesh_validation": validation,
    "armature_modifier_preserved": True,
    "weights_preserved": True,
    "topology_modified": False,
    "rest_pose_modified": False,
}

with open(REPORT, "w", encoding="utf-8") as handle:
    json.dump(report, handle, indent=2, ensure_ascii=False)

bpy.ops.wm.save_as_mainfile(filepath=OUTPUT)

print(json.dumps(report, indent=2, ensure_ascii=False))
