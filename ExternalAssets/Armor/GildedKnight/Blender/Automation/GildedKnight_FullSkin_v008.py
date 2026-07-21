import bpy
import bmesh
import json
import math
import os
import sys
import time
import traceback

from mathutils import Matrix, Vector
from mathutils.bvhtree import BVHTree


SOURCE = r"C:\Panthelia Project\PantheliaProject\ExternalAssets\Armor\GildedKnight\Blender\Panthelia_GildedKnight_Fit_v006_FullVisualFit.blend"
OUTPUT = r"C:\Panthelia Project\PantheliaProject\ExternalAssets\Armor\GildedKnight\Blender\Panthelia_GildedKnight_Fit_v008_ExecutableSkinned.blend"
DIAG_DIR = r"C:\Panthelia Project\PantheliaProject\ExternalAssets\Armor\GildedKnight\Blender\Diagnostics\v008_ExecutableSkinned"
LOG_PATH = os.path.join(DIAG_DIR, "run.log")
STATUS_PATH = os.path.join(DIAG_DIR, "status.json")

ARM_NAME = "REF_Panthelia_Body_Armature"
BODY_NAME = "REF_Panthelia_Body_LOD0"
GLOBALFIT_NAME = "WORK_GildedKnight_GlobalFit"
PIECES = [
    "WORK_Armor002",
    "WORK_Cuirass",
    "WORK_ArmArmor",
    "WORK_LegArmor",
    "WORK_Sock",
]
BACKUPS = {
    "WORK_Armor002": "BACKUP_V008_Armor002",
    "WORK_Cuirass": "BACKUP_V008_Cuirass",
    "WORK_ArmArmor": "BACKUP_V008_ArmArmor",
    "WORK_LegArmor": "BACKUP_V008_LegArmor",
    "WORK_Sock": "BACKUP_V008_Sock",
}

os.makedirs(DIAG_DIR, exist_ok=True)
_log_handle = open(LOG_PATH, "w", encoding="utf-8", buffering=1)


def log(message):
    line = "[%s] %s" % (time.strftime("%Y-%m-%d %H:%M:%S"), message)
    print(line, flush=True)
    _log_handle.write(line + "\n")
    _log_handle.flush()


def write_status(stage, success=False, **values):
    data = {"stage": stage, "success": bool(success)}
    data.update(values)
    temp_path = STATUS_PATH + ".tmp"
    with open(temp_path, "w", encoding="utf-8") as handle:
        json.dump(data, handle, indent=2, ensure_ascii=False)
    os.replace(temp_path, STATUS_PATH)


def save_output():
    bpy.ops.wm.save_as_mainfile(filepath=OUTPUT, check_existing=False)
    log("Checkpoint saved: %s" % OUTPUT)


def require_object(name, object_type=None):
    obj = bpy.data.objects.get(name)
    if obj is None:
        raise RuntimeError("Required object is missing: %s" % name)
    if object_type and obj.type != object_type:
        raise RuntimeError("%s is %s, expected %s" % (name, obj.type, object_type))
    return obj


def get_or_create_collection(name):
    collection = bpy.data.collections.get(name)
    if collection is None:
        collection = bpy.data.collections.new(name)
        bpy.context.scene.collection.children.link(collection)
    return collection


def bone_points(armature, name, pose=False):
    if pose:
        bone = armature.pose.bones.get(name)
        if bone is None:
            return None
        return armature.matrix_world @ bone.head, armature.matrix_world @ bone.tail
    bone = armature.data.bones.get(name)
    if bone is None:
        return None
    return armature.matrix_world @ bone.head_local, armature.matrix_world @ bone.tail_local


def bone_midpoint(armature, name, pose=False):
    points = bone_points(armature, name, pose=pose)
    if points is None:
        return None
    return (points[0] + points[1]) * 0.5


def anatomical_calf_points(armature, side, pose=False):
    """Return actual knee/ankle joints for this imported MetaHuman skeleton.

    In this asset the Blender display tail of calf points sideways and is not at
    the ankle.  The deform-chain joints are calf.head (knee) and foot.head
    (ankle), which together define the real calf axis requested by the fit.
    """
    calf = bone_points(armature, "calf_" + side, pose=pose)
    foot = bone_points(armature, "foot_" + side, pose=pose)
    if calf is None:
        return None
    knee = calf[0]
    if foot is None:
        ankle = calf[1]
    else:
        ankle = foot[0]
    return knee, ankle


def point_segment_distance(point, start, end):
    segment = end - start
    length_squared = segment.length_squared
    if length_squared <= 1.0e-20:
        return (point - start).length
    factor = max(0.0, min(1.0, (point - start).dot(segment) / length_squared))
    return (point - (start + segment * factor)).length


def world_vertices(obj):
    matrix = obj.matrix_world.copy()
    return [matrix @ vertex.co for vertex in obj.data.vertices]


def suffix_side(name):
    lower = name.lower()
    if lower.endswith("_l") or "_l_" in lower:
        return "l"
    if lower.endswith("_r") or "_r_" in lower:
        return "r"
    return None


def anatomical_side(point, armature, region="leg"):
    if region == "arm":
        left_names = ("clavicle_l", "upperarm_l", "lowerarm_l")
        right_names = ("clavicle_r", "upperarm_r", "lowerarm_r")
    else:
        left_names = ("thigh_l", "calf_l", "foot_l")
        right_names = ("thigh_r", "calf_r", "foot_r")

    def distance_to_side(names):
        distances = []
        for bone_name in names:
            points = bone_points(armature, bone_name)
            if points:
                distances.append(point_segment_distance(point, points[0], points[1]))
        return min(distances) if distances else float("inf")

    return "l" if distance_to_side(left_names) <= distance_to_side(right_names) else "r"


def mesh_topology_signature(obj):
    mesh = obj.data
    return (len(mesh.vertices), len(mesh.edges), len(mesh.polygons), tuple(len(p.vertices) for p in mesh.polygons))


def extract_object_weights(obj):
    group_names = {group.index: group.name for group in obj.vertex_groups}
    result = []
    for vertex in obj.data.vertices:
        weights = {}
        for element in vertex.groups:
            name = group_names.get(element.group)
            if name and element.weight > 0.0:
                weights[name] = float(element.weight)
        result.append(weights)
    return result


def write_object_weights(obj, weights_per_vertex):
    used_names = sorted({name for weights in weights_per_vertex for name, weight in weights.items() if weight > 0.0})
    obj.vertex_groups.clear()
    group_indices = {}
    for name in used_names:
        group_indices[name] = obj.vertex_groups.new(name=name).index

    bm = bmesh.new()
    bm.from_mesh(obj.data)
    deform_layer = bm.verts.layers.deform.verify()
    bm.verts.ensure_lookup_table()
    for index, bm_vertex in enumerate(bm.verts):
        deform_weights = bm_vertex[deform_layer]
        deform_weights.clear()
        for name, weight in weights_per_vertex[index].items():
            if weight > 0.0:
                deform_weights[group_indices[name]] = float(weight)
    bm.to_mesh(obj.data)
    bm.free()
    obj.data.update()


def normalized_weights(weights, maximum=4, threshold=0.005, power=1.0):
    filtered = {
        name: float(weight)
        for name, weight in weights.items()
        if math.isfinite(float(weight)) and float(weight) >= threshold
    }
    if not filtered:
        return {}
    items = sorted(filtered.items(), key=lambda item: item[1], reverse=True)[:maximum]
    if power != 1.0:
        items = [(name, weight ** power) for name, weight in items]
    total = sum(weight for _, weight in items)
    if total <= 1.0e-20:
        return {}
    return {name: weight / total for name, weight in items}


def allowed_for_piece(piece_name, bone_name):
    name = bone_name.lower()
    if piece_name in {"WORK_Sock", "WORK_LegArmor"}:
        return any(token in name for token in ("thigh_", "calf_", "foot_")) and not any(
            token in name for token in ("ball", "toe")
        )
    if piece_name == "WORK_ArmArmor":
        return any(token in name for token in ("clavicle_", "upperarm_", "lowerarm_", "hand_")) and not any(
            token in name for token in ("thumb", "index", "middle", "ring", "pinky")
        )
    if piece_name == "WORK_Cuirass":
        return name in {"pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "clavicle_l", "clavicle_r"}
    if piece_name == "WORK_Armor002":
        exact = {
            "pelvis", "spine_01", "spine_02", "spine_03", "spine_04", "neck_01",
            "clavicle_l", "clavicle_r", "upperarm_l", "upperarm_r", "thigh_l", "thigh_r",
        }
        return name in exact or "upperarm_twist" in name or "thigh_twist" in name
    return False


def nearest_allowed_bone(point, armature, allowed_names, side=None):
    best_name = None
    best_distance = float("inf")
    for name in allowed_names:
        if side and suffix_side(name) not in (None, side):
            continue
        points = bone_points(armature, name)
        if points is None:
            continue
        distance = point_segment_distance(point, points[0], points[1])
        if distance < best_distance:
            best_name = name
            best_distance = distance
    return best_name


def create_backups(pieces):
    diagnostics = get_or_create_collection("03_DIAGNOSTICS")
    for piece in pieces:
        backup_name = BACKUPS[piece.name]
        if bpy.data.objects.get(backup_name) is not None:
            raise RuntimeError("Backup already exists in source file: %s" % backup_name)
        backup = piece.copy()
        backup.data = piece.data.copy()
        backup.name = backup_name
        diagnostics.objects.link(backup)
        for collection in list(backup.users_collection):
            if collection != diagnostics:
                collection.objects.unlink(backup)
        backup.hide_set(True)
        backup.hide_viewport = True
        backup.hide_render = True
        log("Created independent hidden backup: %s" % backup_name)


def neutralize_globalfit(pieces, globalfit, centimeters_per_blender_unit):
    maximum_error_cm = 0.0
    for piece in pieces:
        before = world_vertices(piece)
        matrix_world = piece.matrix_world.copy()
        piece.data.transform(matrix_world)
        piece.data.update()
        piece.parent = None
        piece.matrix_parent_inverse = Matrix.Identity(4)
        piece.matrix_world = Matrix.Identity(4)
        after = world_vertices(piece)
        local_max = max(((a - b).length for a, b in zip(before, after)), default=0.0)
        local_max_cm = local_max * centimeters_per_blender_unit
        maximum_error_cm = max(maximum_error_cm, local_max_cm)
        log("GlobalFit neutralized: %s, maximum error %.10f cm" % (piece.name, local_max_cm))
    globalfit.hide_set(True)
    globalfit.hide_viewport = True
    globalfit.hide_render = True
    if maximum_error_cm >= 0.001:
        raise RuntimeError("GlobalFit neutralization error %.10f cm exceeds 0.001 cm" % maximum_error_cm)
    return maximum_error_cm


def connected_components(mesh):
    count = len(mesh.vertices)
    parent = list(range(count))

    def find(index):
        while parent[index] != index:
            parent[index] = parent[parent[index]]
            index = parent[index]
        return index

    def union(a, b):
        root_a = find(a)
        root_b = find(b)
        if root_a != root_b:
            parent[root_b] = root_a

    for edge in mesh.edges:
        union(edge.vertices[0], edge.vertices[1])
    components = {}
    for index in range(count):
        components.setdefault(find(index), []).append(index)
    return list(components.values())


def raise_legarmor(legarmor, armature, blender_units_per_cm):
    points_before = world_vertices(legarmor)
    components = connected_components(legarmor.data)
    side_indices = {"l": [], "r": []}
    component_summary = []
    for component in components:
        center = sum((points_before[index] for index in component), Vector()) / len(component)
        side = anatomical_side(center, armature, "leg")
        side_indices[side].extend(component)
        component_summary.append((len(component), side))
    log("LegArmor disconnected components: %d; classification=%s" % (len(components), component_summary))

    clear_min = 2.0 * blender_units_per_cm
    clear_target = 3.0 * blender_units_per_cm
    clear_max = 4.0 * blender_units_per_cm
    minimum_required_shift = 8.0 * blender_units_per_cm
    fallback_shift = 15.0 * blender_units_per_cm
    inverse_world = legarmor.matrix_world.inverted_safe()

    data = {}
    for side in ("l", "r"):
        calf = anatomical_calf_points(armature, side)
        if calf is None:
            raise RuntimeError("Missing required calf_%s bone" % side)
        knee, ankle = calf[0], calf[1]
        axis_down = (ankle - knee).normalized()
        axis_up = -axis_down
        leg_length = (ankle - knee).length
        indices = side_indices[side]
        if not indices:
            raise RuntimeError("No LegArmor vertices classified on side %s" % side)
        projected_down = [(points_before[index] - knee).dot(axis_down) for index in indices]
        old_top = min(projected_down)
        old_bottom = max(projected_down)
        old_height = max(old_bottom - old_top, 1.0e-12)
        target_top = clear_target
        target_bottom = leg_length - clear_target
        target_height = max(target_bottom - target_top, 1.0e-12)
        scale = max(0.92, min(1.08, target_height / old_height))
        center_projection = (old_top + old_bottom) * 0.5

        if abs(scale - 1.0) > 1.0e-10:
            for index in indices:
                point = legarmor.matrix_world @ legarmor.data.vertices[index].co
                projection = (point - knee).dot(axis_down)
                scaled = point + axis_down * ((projection - center_projection) * (scale - 1.0))
                legarmor.data.vertices[index].co = inverse_world @ scaled
            legarmor.data.update()

        scaled_points = world_vertices(legarmor)
        scaled_projection = [(scaled_points[index] - knee).dot(axis_down) for index in indices]
        scaled_top = min(scaled_projection)
        scaled_bottom = max(scaled_projection)
        desired_center = (target_top + target_bottom) * 0.5
        current_center = (scaled_top + scaled_bottom) * 0.5
        shift_up = current_center - desired_center
        if shift_up < minimum_required_shift:
            log("LegArmor %s anatomical shift %.4f cm is below 8 cm; applying 15 cm fallback" % (
                side, shift_up / blender_units_per_cm
            ))
            shift_up = fallback_shift

        translation = axis_up * shift_up
        for index in indices:
            point = legarmor.matrix_world @ legarmor.data.vertices[index].co
            legarmor.data.vertices[index].co = inverse_world @ (point + translation)
        legarmor.data.update()
        data[side] = {
            "indices": indices,
            "knee": knee,
            "ankle": ankle,
            "axis_down": axis_down,
            "axis_up": axis_up,
            "old_top": old_top,
            "old_bottom": old_bottom,
            "scale": scale,
            "shift_up": shift_up,
            "translation": translation,
            "leg_length": leg_length,
            "clear_min": clear_min,
            "clear_max": clear_max,
        }

    points_after = world_vertices(legarmor)
    shifts_cm = []
    for side in ("l", "r"):
        info = data[side]
        projections = [(points_after[index] - info["knee"]).dot(info["axis_down"]) for index in info["indices"]]
        new_top = min(projections)
        new_bottom = max(projections)
        knee_distance = new_top
        ankle_distance = info["leg_length"] - new_bottom
        shift_cm = info["shift_up"] / blender_units_per_cm
        shifts_cm.append(shift_cm)
        log("LegArmor %s shift: %.4f cm; vector BU=%s" % (side, shift_cm, tuple(round(v, 6) for v in info["translation"])))
        log("LegArmor %s bounds before (knee-axis BU): top=%.6f bottom=%.6f" % (side, info["old_top"], info["old_bottom"]))
        log("LegArmor %s bounds after  (knee-axis BU): top=%.6f bottom=%.6f" % (side, new_top, new_bottom))
        log("LegArmor %s final distance: knee=%.4f cm ankle=%.4f cm scale=%.6f" % (
            side,
            knee_distance / blender_units_per_cm,
            ankle_distance / blender_units_per_cm,
            info["scale"],
        ))
        if info["shift_up"] < minimum_required_shift - 1.0e-8:
            raise RuntimeError("LegArmor %s was moved less than 8 cm" % side)
        if new_bottom > info["leg_length"] + 1.0e-8:
            raise RuntimeError("LegArmor %s lower edge remains below the ankle" % side)
        if ankle_distance < 0.0:
            raise RuntimeError("LegArmor %s still reaches the foot" % side)
        if not (1.5 <= knee_distance / blender_units_per_cm <= 4.5):
            raise RuntimeError("LegArmor %s top edge is not 2-4 cm below knee" % side)
        if not (1.5 <= ankle_distance / blender_units_per_cm <= 4.5):
            raise RuntimeError("LegArmor %s bottom edge is not 2-4 cm above ankle" % side)

    average_shift_cm = sum(shifts_cm) * 0.5
    average_vector = (data["l"]["translation"] + data["r"]["translation"]) * 0.5
    log("LegArmor final average shift: %.4f cm; average vector BU=%s" % (
        average_shift_cm, tuple(round(value, 6) for value in average_vector)
    ))
    return average_shift_cm, data


def source_weights_by_vertex(body, real_bones):
    index_to_name = {
        group.index: group.name
        for group in body.vertex_groups
        if group.name in real_bones
    }
    result = []
    for vertex in body.data.vertices:
        values = {}
        for element in vertex.groups:
            name = index_to_name.get(element.group)
            if name and element.weight > 0.0:
                values[name] = float(element.weight)
        result.append(values)
    return result


def build_body_surface(body, real_bones):
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = body.evaluated_get(depsgraph)
    evaluated_mesh = evaluated.to_mesh(preserve_all_data_layers=True, depsgraph=depsgraph)
    evaluated_mesh.calc_loop_triangles()
    vertices = [vertex.co.copy() for vertex in evaluated_mesh.vertices]
    triangles = [tuple(triangle.vertices) for triangle in evaluated_mesh.loop_triangles]
    if not triangles:
        evaluated.to_mesh_clear()
        raise RuntimeError("Evaluated body has no triangles")
    if len(vertices) != len(body.data.vertices):
        evaluated.to_mesh_clear()
        raise RuntimeError("Evaluated body changes vertex count; safe weight index mapping is unavailable")
    bvh = BVHTree.FromPolygons(vertices, triangles, all_triangles=True)
    weights = source_weights_by_vertex(body, real_bones)
    matrix_world = evaluated.matrix_world.copy()
    inverse_world = matrix_world.inverted_safe()
    log("Body transfer surface: %d vertices, %d triangles, %d real bone groups" % (
        len(vertices), len(triangles), len({name for vertex in weights for name in vertex})
    ))
    return {
        "evaluated": evaluated,
        "mesh": evaluated_mesh,
        "vertices": vertices,
        "triangles": triangles,
        "bvh": bvh,
        "weights": weights,
        "matrix_world": matrix_world,
        "inverse_world": inverse_world,
    }


def barycentric_coordinates(point, a, b, c):
    edge0 = b - a
    edge1 = c - a
    relative = point - a
    d00 = edge0.dot(edge0)
    d01 = edge0.dot(edge1)
    d11 = edge1.dot(edge1)
    d20 = relative.dot(edge0)
    d21 = relative.dot(edge1)
    denominator = d00 * d11 - d01 * d01
    if abs(denominator) <= 1.0e-20:
        distances = ((point - a).length_squared, (point - b).length_squared, (point - c).length_squared)
        nearest = distances.index(min(distances))
        values = [0.0, 0.0, 0.0]
        values[nearest] = 1.0
        return tuple(values)
    v = (d11 * d20 - d01 * d21) / denominator
    w = (d00 * d21 - d01 * d20) / denominator
    u = 1.0 - v - w
    values = [max(0.0, u), max(0.0, v), max(0.0, w)]
    total = sum(values)
    if total <= 1.0e-20:
        return (1.0, 0.0, 0.0)
    return tuple(value / total for value in values)


def transfer_piece_weights(piece, surface, real_bones):
    result = []
    matrix_world = piece.matrix_world.copy()
    for vertex in piece.data.vertices:
        world_point = matrix_world @ vertex.co
        body_point = surface["inverse_world"] @ world_point
        nearest = surface["bvh"].find_nearest(body_point)
        values = {}
        if nearest is not None:
            location, _normal, triangle_index, _distance = nearest
            triangle = surface["triangles"][triangle_index]
            a, b, c = (surface["vertices"][index] for index in triangle)
            barycentric = barycentric_coordinates(location, a, b, c)
            for source_index, coefficient in zip(triangle, barycentric):
                for name, weight in surface["weights"][source_index].items():
                    if name in real_bones:
                        values[name] = values.get(name, 0.0) + weight * coefficient
            if not values:
                nearest_index = min(triangle, key=lambda index: (surface["vertices"][index] - location).length_squared)
                values = dict(surface["weights"][nearest_index])
        result.append(values)
    log("Nearest-face barycentric transfer complete: %s (%d vertices)" % (piece.name, len(result)))
    return result


def leg_reference(armature, side):
    calf = anatomical_calf_points(armature, side)
    thigh = bone_points(armature, "thigh_" + side)
    knee = calf[0]
    ankle = calf[1]
    axis_up = (knee - ankle).normalized()
    return knee, ankle, axis_up


def clean_piece_weights(piece, transferred, armature, real_bones):
    points = world_vertices(piece)
    allowed = [name for name in real_bones if allowed_for_piece(piece.name, name)]
    if not allowed:
        raise RuntimeError("No allowed real bone groups for %s" % piece.name)
    result = []
    for point, raw_weights in zip(points, transferred):
        region = "arm" if piece.name == "WORK_ArmArmor" else "leg"
        side = anatomical_side(point, armature, region)
        weights = {
            name: weight
            for name, weight in raw_weights.items()
            if name in allowed and suffix_side(name) in (None, side)
        }

        if piece.name == "WORK_LegArmor":
            knee, ankle, axis_up = leg_reference(armature, side)
            length = max((knee - ankle).length, 1.0e-12)
            fraction = (point - ankle).dot(axis_up) / length
            calf_name = "calf_" + side
            thigh_name = "thigh_" + side
            if fraction >= 0.80 and thigh_name in real_bones:
                thigh_weight = min(0.35, max(0.0, (fraction - 0.80) / 0.20 * 0.35))
                weights = {calf_name: 1.0 - thigh_weight, thigh_name: thigh_weight}
            else:
                weights = {calf_name: 1.0}
            weights = normalized_weights(weights, maximum=2, threshold=0.0)

        elif piece.name == "WORK_Sock":
            knee, ankle, axis_up = leg_reference(armature, side)
            length = max((knee - ankle).length, 1.0e-12)
            fraction = (point - ankle).dot(axis_up) / length
            calf_name = "calf_" + side
            thigh_name = "thigh_" + side
            foot_name = "foot_" + side
            if fraction > 0.72 and thigh_name in real_bones:
                blend = min(0.45, max(0.0, (fraction - 0.72) / 0.28 * 0.45))
                weights[thigh_name] = max(weights.get(thigh_name, 0.0), blend)
                weights[calf_name] = max(weights.get(calf_name, 0.0), 1.0 - blend)
            elif fraction < 0.08 and foot_name in real_bones:
                blend = min(0.25, max(0.0, (0.08 - fraction) / 0.08 * 0.25))
                weights[foot_name] = max(weights.get(foot_name, 0.0), blend)
                weights[calf_name] = max(weights.get(calf_name, 0.0), 1.0 - blend)
            weights = normalized_weights(weights, maximum=4, threshold=0.005)

        elif piece.name == "WORK_ArmArmor":
            clavicle_name = "clavicle_" + side
            upperarm_name = "upperarm_" + side
            upperarm_points = bone_points(armature, upperarm_name)
            if upperarm_points:
                upper_length = max((upperarm_points[1] - upperarm_points[0]).length, 1.0e-12)
                if (point - upperarm_points[0]).length <= upper_length * 0.30:
                    weights[upperarm_name] = max(weights.get(upperarm_name, 0.0), 0.68)
                    if clavicle_name in real_bones:
                        weights[clavicle_name] = max(weights.get(clavicle_name, 0.0), 0.32)
            weights = normalized_weights(weights, maximum=2, threshold=0.005, power=2.2)

        elif piece.name == "WORK_Cuirass":
            weights = normalized_weights(weights, maximum=2, threshold=0.005, power=2.8)

        elif piece.name == "WORK_Armor002":
            pelvis = bone_midpoint(armature, "pelvis")
            spine = bone_midpoint(armature, "spine_01")
            thigh_left = bone_midpoint(armature, "thigh_l")
            thigh_right = bone_midpoint(armature, "thigh_r")
            if pelvis and spine:
                torso_up = (spine - pelvis).normalized()
                vertical = (point - pelvis).dot(torso_up)
                pelvis_to_spine = max((spine - pelvis).length, 1.0e-12)
                if vertical < pelvis_to_spine * 0.15:
                    side_thigh = thigh_left if side == "l" else thigh_right
                    opposite_thigh = thigh_right if side == "l" else thigh_left
                    thigh_name = "thigh_" + side
                    if side_thigh and opposite_thigh:
                        center_width = (thigh_left - thigh_right).length * 0.22
                        if (point - pelvis).length > center_width and thigh_name in real_bones:
                            weights = {"pelvis": 0.70, thigh_name: 0.30}
                        else:
                            weights = {"pelvis": 1.0}
            weights = normalized_weights(weights, maximum=2, threshold=0.005, power=2.4)

        if not weights:
            nearest = nearest_allowed_bone(point, armature, allowed, side)
            if nearest is None:
                raise RuntimeError("Could not find fallback bone for %s vertex" % piece.name)
            weights = {nearest: 1.0}
        result.append(normalized_weights(weights, maximum=4, threshold=0.0))

    write_object_weights(piece, result)
    log("Functional cleanup complete: %s; groups=%d" % (piece.name, len(piece.vertex_groups)))


def install_armature_modifier(piece, armature):
    for modifier in list(piece.modifiers):
        piece.modifiers.remove(modifier)
    modifier = piece.modifiers.new(name="Armature_MetaHuman", type="ARMATURE")
    modifier.object = armature
    modifier.use_vertex_groups = True
    modifier.use_bone_envelopes = False
    modifier.use_deform_preserve_volume = piece.name == "WORK_Sock"


def reset_pose(armature):
    armature.data.pose_position = "POSE"
    for pose_bone in armature.pose.bones:
        pose_bone.location = (0.0, 0.0, 0.0)
        pose_bone.scale = (1.0, 1.0, 1.0)
        pose_bone.rotation_mode = "QUATERNION"
        pose_bone.rotation_quaternion = (1.0, 0.0, 0.0, 0.0)
    bpy.context.view_layer.update()


def rotate_pose_bone(armature, name, axis, angle):
    pose_bone = armature.pose.bones.get(name)
    if pose_bone is None:
        return False
    pose_bone.rotation_mode = "XYZ"
    rotation = [0.0, 0.0, 0.0]
    rotation[axis] = angle
    pose_bone.rotation_euler = rotation
    return True


def sampled_world_positions(piece, maximum_samples=256):
    depsgraph = bpy.context.evaluated_depsgraph_get()
    evaluated = piece.evaluated_get(depsgraph)
    mesh = evaluated.to_mesh()
    count = len(mesh.vertices)
    stride = max(1, count // maximum_samples)
    matrix_world = evaluated.matrix_world.copy()
    points = [matrix_world @ mesh.vertices[index].co for index in range(0, count, stride)]
    evaluated.to_mesh_clear()
    return points


def pose_tests(armature, pieces, leg_side_indices, blender_units_per_cm):
    reset_pose(armature)
    baseline = {piece.name: sampled_world_positions(piece) for piece in pieces}
    poses = {
        "arms_down": [("upperarm_l", 2, math.radians(62)), ("upperarm_r", 2, math.radians(-62))],
        "elbows_flexed": [
            ("upperarm_l", 2, math.radians(50)), ("upperarm_r", 2, math.radians(-50)),
            ("lowerarm_l", 2, math.radians(88)), ("lowerarm_r", 2, math.radians(-88)),
        ],
        "wide_step": [
            ("thigh_l", 1, math.radians(-24)), ("thigh_r", 1, math.radians(24)),
            ("calf_l", 1, math.radians(14)), ("calf_r", 1, math.radians(-14)),
        ],
        "knee_left_raised": [("thigh_l", 1, math.radians(-62)), ("calf_l", 1, math.radians(82))],
        "knee_right_raised": [("thigh_r", 1, math.radians(-62)), ("calf_r", 1, math.radians(82))],
        "torso_flexed": [
            ("spine_01", 0, math.radians(8)), ("spine_02", 0, math.radians(9)),
            ("spine_03", 0, math.radians(7)),
        ],
    }
    movement = {}
    for pose_name, operations in poses.items():
        reset_pose(armature)
        used = [rotate_pose_bone(armature, name, axis, angle) for name, axis, angle in operations]
        bpy.context.view_layer.update()
        pose_movement = {}
        for piece in pieces:
            posed = sampled_world_positions(piece)
            rest = baseline[piece.name]
            maximum = max(((a - b).length for a, b in zip(rest, posed)), default=0.0)
            pose_movement[piece.name] = maximum / blender_units_per_cm
        movement[pose_name] = pose_movement
        log("Pose test %s movement_cm=%s bones_found=%s" % (pose_name, pose_movement, used))

        if pose_name in {"knee_left_raised", "knee_right_raised"}:
            side = "l" if "left" in pose_name else "r"
            legarmor = bpy.data.objects["WORK_LegArmor"]
            depsgraph = bpy.context.evaluated_depsgraph_get()
            evaluated = legarmor.evaluated_get(depsgraph)
            mesh = evaluated.to_mesh()
            matrix_world = evaluated.matrix_world.copy()
            calf = anatomical_calf_points(armature, side, pose=True)
            axis_down = (calf[1] - calf[0]).normalized()
            indices = leg_side_indices[side]["indices"]
            bottom = max((matrix_world @ mesh.vertices[index].co - calf[0]).dot(axis_down) for index in indices)
            calf_length = (calf[1] - calf[0]).length
            ankle_clearance_cm = (calf_length - bottom) / blender_units_per_cm
            evaluated.to_mesh_clear()
            log("Pose test %s greave ankle clearance: %.4f cm" % (pose_name, ankle_clearance_cm))
            if ankle_clearance_cm < -0.5:
                raise RuntimeError("Greave reaches below posed ankle in %s" % pose_name)
    reset_pose(armature)
    return movement


def corrective_pass(piece, armature, real_bones):
    allowed = [name for name in real_bones if allowed_for_piece(piece.name, name)]
    region = "arm" if piece.name == "WORK_ArmArmor" else "leg"
    points = world_vertices(piece)
    current = extract_object_weights(piece)
    corrected = []
    for point, weights in zip(points, current):
        side = anatomical_side(point, armature, region)
        values = {
            name: weight
            for name, weight in weights.items()
            if name in allowed and suffix_side(name) in (None, side)
        }
        values = normalized_weights(values, maximum=4, threshold=0.005)
        if not values:
            nearest = nearest_allowed_bone(point, armature, allowed, side)
            if nearest is None:
                raise RuntimeError("Corrective pass found no fallback bone for %s" % piece.name)
            values = {nearest: 1.0}
        corrected.append(values)
    write_object_weights(piece, corrected)


def validate_piece(piece, armature, original_topology, real_bones):
    topology = mesh_topology_signature(piece)
    topology_ok = topology == original_topology
    armature_modifiers = [modifier for modifier in piece.modifiers if modifier.type == "ARMATURE"]
    modifier_ok = len(armature_modifiers) == 1 and armature_modifiers[0].object == armature
    group_names = {group.index: group.name for group in piece.vertex_groups}
    temporary_groups = [name for name in group_names.values() if name.startswith("TEMP_") or name not in real_bones]
    region = "arm" if piece.name == "WORK_ArmArmor" else "leg"
    points = world_vertices(piece)
    unweighted = 0
    bad_sums = 0
    maximum_influences = 0
    crossed = 0
    nan_values = 0
    for point, vertex in zip(points, piece.data.vertices):
        entries = [element for element in vertex.groups if element.weight > 1.0e-12]
        total = sum(element.weight for element in entries)
        if not entries:
            unweighted += 1
        if entries and not (0.999 <= total <= 1.001):
            bad_sums += 1
        maximum_influences = max(maximum_influences, len(entries))
        side = anatomical_side(point, armature, region)
        if any(suffix_side(group_names.get(element.group, "")) not in (None, side) for element in entries):
            crossed += 1
        if any(not math.isfinite(value) for value in (point.x, point.y, point.z)) or any(
            not math.isfinite(element.weight) for element in entries
        ):
            nan_values += 1
    result = {
        "topology_ok": topology_ok,
        "armature_modifier_ok": modifier_ok,
        "unweighted": unweighted,
        "bad_weight_sums": bad_sums,
        "maximum_influences": maximum_influences,
        "temporary_groups": temporary_groups,
        "crossed": crossed,
        "nan_values": nan_values,
    }
    log("Validation %s: %s" % (piece.name, result))
    return result


def clear_animation_and_restore_rest(armature, pieces):
    reset_pose(armature)
    for datablock in [armature] + pieces:
        if datablock.animation_data is not None:
            datablock.animation_data_clear()
    for action in list(bpy.data.actions):
        if action.users == 0:
            bpy.data.actions.remove(action)
    for piece in pieces:
        for modifier in list(piece.modifiers):
            if modifier.type != "ARMATURE" or modifier.object != armature:
                piece.modifiers.remove(modifier)
    reset_pose(armature)
    for pose_bone in armature.pose.bones:
        if pose_bone.location.length > 1.0e-10:
            raise RuntimeError("Pose location was not restored: %s" % pose_bone.name)
        if (Vector(pose_bone.scale) - Vector((1.0, 1.0, 1.0))).length > 1.0e-10:
            raise RuntimeError("Pose scale was not restored: %s" % pose_bone.name)
        quaternion = pose_bone.rotation_quaternion
        if abs(quaternion.w - 1.0) > 1.0e-10 or Vector((quaternion.x, quaternion.y, quaternion.z)).length > 1.0e-10:
            raise RuntimeError("Pose rotation was not restored: %s" % pose_bone.name)


def main():
    normalized_current = os.path.normcase(os.path.abspath(bpy.data.filepath))
    normalized_source = os.path.normcase(os.path.abspath(SOURCE))
    if normalized_current != normalized_source:
        raise RuntimeError("Blender did not open the required v006 source: %s" % bpy.data.filepath)
    log("Stage A/C: Blender %s; safe source opened" % bpy.app.version_string)
    if bpy.app.version[:2] != (5, 2):
        raise RuntimeError("Blender 5.2 required, found %s" % bpy.app.version_string)

    armature = require_object(ARM_NAME, "ARMATURE")
    body = require_object(BODY_NAME, "MESH")
    globalfit = require_object(GLOBALFIT_NAME)
    pieces = [require_object(name, "MESH") for name in PIECES]
    if not (300 <= len(armature.data.bones) <= 350):
        raise RuntimeError("Unexpected armature bone count: %d" % len(armature.data.bones))
    if not (30000 <= len(body.data.vertices) <= 35000):
        raise RuntimeError("Unexpected source body vertex count: %d" % len(body.data.vertices))
    write_status("preflight", False)
    save_output()

    original_topology = {piece.name: mesh_topology_signature(piece) for piece in pieces}
    log("Stage D: creating backups")
    create_backups(pieces)
    save_output()
    write_status("backups_complete", False)

    unit_scale = bpy.context.scene.unit_settings.scale_length or 1.0
    centimeters_per_blender_unit = unit_scale * 100.0
    blender_units_per_cm = 1.0 / centimeters_per_blender_unit

    log("Stage E: neutralizing GlobalFit")
    globalfit_error_cm = neutralize_globalfit(pieces, globalfit, centimeters_per_blender_unit)
    save_output()
    write_status("globalfit_neutralized", False, globalfit_max_error_cm=globalfit_error_cm)

    log("Stage F: raising WORK_LegArmor before skinning")
    average_shift_cm, leg_side_data = raise_legarmor(
        bpy.data.objects["WORK_LegArmor"], armature, blender_units_per_cm
    )
    save_output()
    write_status("legarmor_raised", False, legarmor_raised=True, legarmor_shift_cm=average_shift_cm)

    log("Stage G/H: building evaluated body surface and transferring weights")
    reset_pose(armature)
    if armature.animation_data is not None:
        armature.animation_data_clear()
    real_bones = {bone.name for bone in armature.data.bones}
    surface = build_body_surface(body, real_bones)
    try:
        for piece in pieces:
            transferred = transfer_piece_weights(piece, surface, real_bones)
            clean_piece_weights(piece, transferred, armature, real_bones)
            install_armature_modifier(piece, armature)
            save_output()
    finally:
        surface["evaluated"].to_mesh_clear()
    write_status("skinning_complete", False, legarmor_raised=True, legarmor_shift_cm=average_shift_cm)

    log("Stage J: bounded validation and at most two corrective passes")
    validations = None
    for pass_number in range(3):
        validations = {
            piece.name: validate_piece(piece, armature, original_topology[piece.name], real_bones)
            for piece in pieces
        }
        failed = [
            name for name, values in validations.items()
            if not values["topology_ok"]
            or not values["armature_modifier_ok"]
            or values["unweighted"]
            or values["bad_weight_sums"]
            or values["maximum_influences"] > 4
            or values["temporary_groups"]
            or values["crossed"]
            or values["nan_values"]
        ]
        if not failed:
            break
        if pass_number >= 2:
            raise RuntimeError("Validation failed after two corrective passes: %s" % failed)
        log("Corrective weight pass %d for %s" % (pass_number + 1, failed))
        for name in failed:
            corrective_pass(bpy.data.objects[name], armature, real_bones)

    log("Stage K: temporary pose tests")
    pose_tests(armature, pieces, leg_side_data, blender_units_per_cm)

    log("Stage L: restoring rest pose and cleaning temporary state")
    clear_animation_and_restore_rest(armature, pieces)
    final_validations = {
        piece.name: validate_piece(piece, armature, original_topology[piece.name], real_bones)
        for piece in pieces
    }
    total_unweighted = sum(values["unweighted"] for values in final_validations.values())
    if total_unweighted != 0:
        raise RuntimeError("Final unweighted vertex count is %d" % total_unweighted)
    if any(not values["armature_modifier_ok"] for values in final_validations.values()):
        raise RuntimeError("A final Armature modifier is invalid")
    if any(values["maximum_influences"] > 4 for values in final_validations.values()):
        raise RuntimeError("A final vertex has more than four influences")
    if any(values["crossed"] for values in final_validations.values()):
        raise RuntimeError("Final cross-side weights remain")

    save_output()
    final_status = {
        "stage": "complete",
        "success": True,
        "legarmor_raised": True,
        "legarmor_shift_cm": average_shift_cm,
        "globalfit_neutralized": True,
        "skinned_meshes": 5,
        "unweighted_vertices": 0,
        "rest_pose_restored": True,
        "ready_for_manual_touchup": True,
    }
    write_status(**final_status)
    save_output()
    log("COMPLETE: %s" % final_status)
    return final_status


try:
    result = main()
except Exception as exception:
    error_text = traceback.format_exc()
    log("FAILED: %s" % exception)
    log(error_text)
    write_status("failed", False, error=str(exception), traceback=error_text)
    try:
        if os.path.normcase(os.path.abspath(bpy.data.filepath)) == os.path.normcase(os.path.abspath(OUTPUT)):
            save_output()
    except Exception:
        log("Could not save failed checkpoint:\n%s" % traceback.format_exc())
    _log_handle.close()
    raise
else:
    _log_handle.close()
