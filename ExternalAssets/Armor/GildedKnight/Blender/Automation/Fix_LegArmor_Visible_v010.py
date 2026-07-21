import bpy
import os
import json
from mathutils import Vector

SOURCE = r"C:\Panthelia Project\PantheliaProject\ExternalAssets\Armor\GildedKnight\Blender\Panthelia_GildedKnight_Fit_v008_ExecutableSkinned.blend"
OUTPUT = r"C:\Panthelia Project\PantheliaProject\ExternalAssets\Armor\GildedKnight\Blender\Panthelia_GildedKnight_Fit_v010_LegArmor_VisibleFix.blend"
REPORT = r"C:\Panthelia Project\PantheliaProject\ExternalAssets\Armor\GildedKnight\Blender\Diagnostics\v010_LegArmor_VisibleFix.json"

bpy.ops.wm.open_mainfile(filepath=SOURCE)

obj = bpy.data.objects.get("WORK_LegArmor")
arm = bpy.data.objects.get("REF_Panthelia_Body_Armature")

if obj is None or obj.type != "MESH":
    raise RuntimeError("No existe WORK_LegArmor")

if arm is None or arm.type != "ARMATURE":
    raise RuntimeError("No existe REF_Panthelia_Body_Armature")

if len(obj.data.vertices) != 2608:
    raise RuntimeError(
        f"Conteo inesperado: {len(obj.data.vertices)} vértices"
    )

# Rest pose real.
arm.data.pose_position = "REST"

for pose_bone in arm.pose.bones:
    pose_bone.matrix_basis.identity()

bpy.context.view_layer.update()

# Ocultar originales, backups y duplicados para que no tapen visualmente
# el resultado corregido.
for collection_name in (
    "01_SOURCE_ARMOR_ORIGINAL",
    "03_DIAGNOSTICS",
):
    collection = bpy.data.collections.get(collection_name)

    if collection:
        collection.hide_viewport = True
        collection.hide_render = True

for candidate in bpy.data.objects:
    if candidate == obj:
        continue

    lower_name = candidate.name.lower()

    if (
        lower_name.startswith("backup_")
        or lower_name.startswith("tmp_")
        or lower_name == "leg armor"
        or "legarmor_pre" in lower_name
    ):
        candidate.hide_set(True)
        candidate.hide_render = True

# Backup interno.
backup = obj.copy()
backup.data = obj.data.copy()
backup.name = "BACKUP_V010_LegArmor_PreVisibleFix"

diagnostics = bpy.data.collections.get("03_DIAGNOSTICS")

if diagnostics is None:
    diagnostics = bpy.data.collections.new("03_DIAGNOSTICS")
    bpy.context.scene.collection.children.link(diagnostics)

diagnostics.objects.link(backup)
backup.hide_set(True)
backup.hide_render = True

mesh = obj.data

# Encontrar las dos islas sin separar el objeto.
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
        f"Se esperaban 2 islas; se encontraron {len(components)}"
    )

# Centros X para asociar cada isla con el lado anatómico correcto.
component_data = []

for component in components:
    points = [
        obj.matrix_world @ mesh.vertices[index].co
        for index in component
    ]

    center = sum(points, Vector()) / len(points)

    component_data.append({
        "indices": component,
        "center_x": center.x,
    })

component_data.sort(key=lambda item: item["center_x"])

# No usar bone.tail. Los landmarks anatómicos serán:
# - rodilla: head de calf
# - tobillo: head de foot
joint_data = []

for suffix in ("l", "r"):
    calf = arm.data.bones.get(f"calf_{suffix}")
    foot = arm.data.bones.get(f"foot_{suffix}")

    if calf is None:
        raise RuntimeError(f"No existe calf_{suffix}")

    if foot is None:
        raise RuntimeError(f"No existe foot_{suffix}")

    knee = arm.matrix_world @ calf.head_local
    ankle = arm.matrix_world @ foot.head_local

    if knee.z <= ankle.z:
        raise RuntimeError(
            f"Landmarks invertidos para lado {suffix}: "
            f"knee Z={knee.z}, ankle Z={ankle.z}"
        )

    joint_data.append({
        "side": suffix,
        "knee": knee,
        "ankle": ankle,
        "center_x": (knee.x + ankle.x) * 0.5,
    })

joint_data.sort(key=lambda item: item["center_x"])

world_to_local = obj.matrix_world.inverted()

# Objetivo visual directo:
# - borde inferior 6 cm por encima del tobillo;
# - borde superior 3 cm por debajo de la rodilla.
#
# Esto evita que la geometría vuelva a cubrir el pie.
BOTTOM_ABOVE_ANKLE_CM = 6.0
TOP_BELOW_KNEE_CM = 3.0

results = []

for component, joints in zip(component_data, joint_data):
    indices = component["indices"]

    points_before = [
        obj.matrix_world @ mesh.vertices[index].co
        for index in indices
    ]

    current_min_z = min(point.z for point in points_before)
    current_max_z = max(point.z for point in points_before)
    current_height = current_max_z - current_min_z

    target_min_z = joints["ankle"].z + BOTTOM_ABOVE_ANKLE_CM
    target_max_z = joints["knee"].z - TOP_BELOW_KNEE_CM
    target_height = target_max_z - target_min_z

    if current_height <= 1.0:
        raise RuntimeError(
            f"Altura actual inválida en lado {joints['side']}"
        )

    if target_height <= 10.0:
        raise RuntimeError(
            f"Altura objetivo inválida en lado {joints['side']}: "
            f"{target_height}"
        )

    z_scale = target_height / current_height

    # Cambio obligatorio y visible.
    bottom_lift = target_min_z - current_min_z

    if bottom_lift < 5.0:
        target_min_z += 8.0 - bottom_lift
        target_height = target_max_z - target_min_z
        z_scale = target_height / current_height
        bottom_lift = target_min_z - current_min_z

    before_positions = {}

    for index in indices:
        world = obj.matrix_world @ mesh.vertices[index].co
        before_positions[index] = world.copy()

        normalized_z = (world.z - current_min_z) / current_height
        new_z = target_min_z + normalized_z * target_height

        # Mantener X/Y exactamente.
        new_world = Vector((world.x, world.y, new_z))
        mesh.vertices[index].co = world_to_local @ new_world

    mesh.update()
    bpy.context.view_layer.update()

    displacement_values = []

    for index in indices:
        after_world = obj.matrix_world @ mesh.vertices[index].co
        displacement_values.append(
            (after_world - before_positions[index]).length
        )

    results.append({
        "side": joints["side"],
        "knee_z": float(joints["knee"].z),
        "ankle_z": float(joints["ankle"].z),
        "before_min_z": float(current_min_z),
        "before_max_z": float(current_max_z),
        "target_min_z": float(target_min_z),
        "target_max_z": float(target_max_z),
        "bottom_lift_cm": float(bottom_lift),
        "z_scale": float(z_scale),
        "mean_vertex_displacement_cm": float(
            sum(displacement_values) / len(displacement_values)
        ),
        "max_vertex_displacement_cm": float(max(displacement_values)),
    })

mesh.update()
bpy.context.view_layer.update()

# Validar la malla evaluada después del Armature Modifier.
depsgraph = bpy.context.evaluated_depsgraph_get()
evaluated_obj = obj.evaluated_get(depsgraph)
evaluated_mesh = evaluated_obj.to_mesh()

try:
    validation = []

    for component, joints in zip(component_data, joint_data):
        points = [
            evaluated_obj.matrix_world @ evaluated_mesh.vertices[index].co
            for index in component["indices"]
        ]

        final_min_z = min(point.z for point in points)
        final_max_z = max(point.z for point in points)

        distance_above_ankle = final_min_z - joints["ankle"].z
        distance_below_knee = joints["knee"].z - final_max_z

        validation.append({
            "side": joints["side"],
            "final_min_z": float(final_min_z),
            "final_max_z": float(final_max_z),
            "distance_above_ankle_cm": float(distance_above_ankle),
            "distance_below_knee_cm": float(distance_below_knee),
        })

        if distance_above_ankle < 5.0:
            raise RuntimeError(
                f"Lado {joints['side']}: la geometría evaluada continúa "
                f"demasiado baja. Distancia sobre tobillo: "
                f"{distance_above_ankle:.3f} cm"
            )

        if distance_below_knee < 2.0:
            raise RuntimeError(
                f"Lado {joints['side']}: la geometría evaluada invade "
                f"la rodilla. Distancia: {distance_below_knee:.3f} cm"
            )

finally:
    evaluated_obj.to_mesh_clear()

# Validar que el cambio fue realmente visible.
for result in results:
    if result["mean_vertex_displacement_cm"] < 4.0:
        raise RuntimeError(
            f"Cambio visual insuficiente en lado {result['side']}: "
            f"{result['mean_vertex_displacement_cm']:.3f} cm"
        )

# Seleccionar únicamente WORK_LegArmor.
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
    "method": "direct_world_z_remap_using_calf_head_and_foot_head",
    "results": results,
    "evaluated_validation": validation,
    "duplicates_hidden": True,
    "weights_preserved": True,
    "armature_modifier_preserved": True,
    "topology_modified": False,
}

with open(REPORT, "w", encoding="utf-8") as handle:
    json.dump(report, handle, indent=2, ensure_ascii=False)

bpy.ops.wm.save_as_mainfile(filepath=OUTPUT)

print(json.dumps(report, indent=2, ensure_ascii=False))
