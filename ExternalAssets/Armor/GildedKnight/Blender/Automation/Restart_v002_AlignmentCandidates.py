import bpy
import hashlib
import json
import math
import sys
import traceback
from pathlib import Path

import numpy as np


SOURCE_BLEND = Path(r"C:\Panthelia Project\PantheliaProject\ExternalAssets\Armor\GildedKnight\Blender\Restart\Panthelia_GildedKnight_RESTART_v001_TexturedRigid.blend")
OUTPUT_DIR = Path(r"C:\Panthelia Project\PantheliaProject\ExternalAssets\Armor\GildedKnight\Blender\Restart\AlignmentCandidates")
DIAG_DIR = Path(r"C:\Panthelia Project\PantheliaProject\ExternalAssets\Armor\GildedKnight\Blender\Diagnostics\Restart_v002_AlignmentCandidates")

VARIANTS = {
    "A": {
        "chest_cm": 5.0,
        "legs_cm": 3.0,
        "blend": "Panthelia_GildedKnight_RESTART_v002_A_Chest5_Legs3.blend",
    },
    "B": {
        "chest_cm": 8.0,
        "legs_cm": 5.0,
        "blend": "Panthelia_GildedKnight_RESTART_v002_B_Chest8_Legs5.blend",
    },
    "C": {
        "chest_cm": 11.0,
        "legs_cm": 7.0,
        "blend": "Panthelia_GildedKnight_RESTART_v002_C_Chest11_Legs7.blend",
    },
}

CAMERAS = {
    "front": "CAM_Front",
    "perspective": "CAM_Perspective",
    "chest": "CAM_ChestCloseup",
    "legs": "CAM_LegsCloseup",
}

WORK_NAMES = (
    "WORK_Armor",
    "WORK_Cuirass",
    "WORK_ArmArmor",
    "WORK_LegArmor",
    "WORK_Sock",
)

PROTECTED_TRANSFORM_NAMES = (
    "WORK_Armor",
    "WORK_ArmArmor",
    "WORK_Sock",
    "WORK_Armor_RigidRoot",
    "REF_Panthelia_Body_LOD0",
    "REF_Panthelia_Body_Armature",
    "CAM_Front",
    "CAM_Perspective",
    "CAM_ChestCloseup",
    "CAM_LegsCloseup",
)


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def matrix_values(matrix):
    return tuple(float(value) for row in matrix for value in row)


def matrices_close(first, second, tolerance=1.0e-6):
    return all(abs(a - b) <= tolerance for a, b in zip(matrix_values(first), matrix_values(second)))


def hash_update_array(digest, values, dtype):
    array = np.asarray(values, dtype=dtype)
    digest.update(array.tobytes(order="C"))


def mesh_fingerprint(obj):
    mesh = obj.data
    mesh.calc_loop_triangles()
    digest = hashlib.sha256()
    vertices = np.empty(len(mesh.vertices) * 3, dtype=np.float32)
    mesh.vertices.foreach_get("co", vertices)
    hash_update_array(digest, vertices, np.float32)

    triangles = np.asarray([tuple(triangle.vertices) for triangle in mesh.loop_triangles], dtype=np.int32)
    hash_update_array(digest, triangles, np.int32)

    uv_summary = []
    for layer in mesh.uv_layers:
        uvs = np.empty(len(layer.data) * 2, dtype=np.float32)
        layer.data.foreach_get("uv", uvs)
        digest.update(layer.name.encode("utf-8"))
        hash_update_array(digest, uvs, np.float32)
        uv_summary.append({"name": layer.name, "loop_count": len(layer.data)})

    return {
        "vertex_count": len(mesh.vertices),
        "triangle_count": len(mesh.loop_triangles),
        "uv_layers": uv_summary,
        "geometry_uv_sha256": digest.hexdigest(),
    }


def serializable_value(value):
    if isinstance(value, (str, int, float, bool)) or value is None:
        return value
    try:
        return [float(component) for component in value]
    except (TypeError, ValueError):
        return str(value)


def material_fingerprint(material):
    digest = hashlib.sha256()
    digest.update(material.name.encode("utf-8"))
    digest.update(str(material.diffuse_color[:]).encode("utf-8"))
    digest.update(str(material.metallic).encode("utf-8"))
    digest.update(str(material.roughness).encode("utf-8"))
    if material.node_tree is not None:
        nodes = []
        for node in material.node_tree.nodes:
            node_record = {
                "name": node.name,
                "type": node.bl_idname,
                "mute": node.mute,
                "inputs": [],
            }
            if hasattr(node, "image") and node.image is not None:
                node_record["image"] = str(Path(bpy.path.abspath(node.image.filepath)).resolve())
                node_record["color_space"] = node.image.colorspace_settings.name
            for socket in node.inputs:
                if hasattr(socket, "default_value"):
                    node_record["inputs"].append((socket.name, serializable_value(socket.default_value)))
            nodes.append(node_record)
        links = sorted(
            (
                link.from_node.name,
                link.from_socket.name,
                link.to_node.name,
                link.to_socket.name,
            )
            for link in material.node_tree.links
        )
        digest.update(json.dumps(sorted(nodes, key=lambda item: item["name"]), sort_keys=True).encode("utf-8"))
        digest.update(json.dumps(links).encode("utf-8"))
    return digest.hexdigest()


def object_material_state(obj):
    return [
        {
            "name": material.name if material else None,
            "pointer": material.as_pointer() if material else None,
            "fingerprint": material_fingerprint(material) if material else None,
        }
        for material in obj.data.materials
    ]


def armature_fingerprint(obj):
    digest = hashlib.sha256()
    for bone in obj.data.bones:
        digest.update(bone.name.encode("utf-8"))
        hash_update_array(digest, matrix_values(bone.matrix_local), np.float64)
        hash_update_array(digest, tuple(bone.head_local) + tuple(bone.tail_local), np.float64)
    return digest.hexdigest()


def capture_baseline():
    required = set(WORK_NAMES) | set(PROTECTED_TRANSFORM_NAMES) | set(CAMERAS.values())
    missing = sorted(name for name in required if name not in bpy.data.objects)
    require(not missing, f"Missing required objects: {missing}")

    mesh_states = {name: mesh_fingerprint(bpy.data.objects[name]) for name in WORK_NAMES}
    material_states = {name: object_material_state(bpy.data.objects[name]) for name in WORK_NAMES}
    protected_matrices = {
        name: bpy.data.objects[name].matrix_world.copy() for name in PROTECTED_TRANSFORM_NAMES
    }
    camera_states = {
        name: {
            "matrix_world": bpy.data.objects[name].matrix_world.copy(),
            "lens": bpy.data.objects[name].data.lens,
            "sensor_width": bpy.data.objects[name].data.sensor_width,
            "clip_start": bpy.data.objects[name].data.clip_start,
            "clip_end": bpy.data.objects[name].data.clip_end,
        }
        for name in CAMERAS.values()
    }
    body = bpy.data.objects["REF_Panthelia_Body_LOD0"]
    armature = bpy.data.objects["REF_Panthelia_Body_Armature"]
    return {
        "mesh_states": mesh_states,
        "material_states": material_states,
        "protected_matrices": protected_matrices,
        "camera_states": camera_states,
        "body_mesh": mesh_fingerprint(body),
        "body_materials": object_material_state(body),
        "body_modifiers": [(modifier.name, modifier.type) for modifier in body.modifiers],
        "body_vertex_groups": tuple(group.name for group in body.vertex_groups),
        "armature_fingerprint": armature_fingerprint(armature),
        "scene_camera": bpy.context.scene.camera,
        "render_filepath": bpy.context.scene.render.filepath,
    }


def validate_invariants(baseline):
    for name in WORK_NAMES:
        obj = bpy.data.objects[name]
        require(mesh_fingerprint(obj) == baseline["mesh_states"][name], f"Mesh/triangles/UVs changed: {name}")
        require(object_material_state(obj) == baseline["material_states"][name], f"Materials changed: {name}")
        require(len(obj.vertex_groups) == 0, f"Vertex groups found on {name}")
        require(
            not any(modifier.type == "ARMATURE" for modifier in obj.modifiers),
            f"Armature modifier found on {name}",
        )

    for name, original_matrix in baseline["protected_matrices"].items():
        require(
            matrices_close(bpy.data.objects[name].matrix_world, original_matrix, 1.0e-7),
            f"Protected transform changed: {name}",
        )

    for name, original in baseline["camera_states"].items():
        camera = bpy.data.objects[name]
        require(matrices_close(camera.matrix_world, original["matrix_world"], 1.0e-7), f"Camera moved: {name}")
        require(camera.data.lens == original["lens"], f"Camera lens changed: {name}")
        require(camera.data.sensor_width == original["sensor_width"], f"Camera sensor changed: {name}")
        require(camera.data.clip_start == original["clip_start"], f"Camera clip_start changed: {name}")
        require(camera.data.clip_end == original["clip_end"], f"Camera clip_end changed: {name}")

    body = bpy.data.objects["REF_Panthelia_Body_LOD0"]
    armature = bpy.data.objects["REF_Panthelia_Body_Armature"]
    require(mesh_fingerprint(body) == baseline["body_mesh"], "Body geometry/triangles/UVs changed")
    require(object_material_state(body) == baseline["body_materials"], "Body materials changed")
    require([(modifier.name, modifier.type) for modifier in body.modifiers] == baseline["body_modifiers"], "Body modifiers changed")
    require(tuple(group.name for group in body.vertex_groups) == baseline["body_vertex_groups"], "Body vertex groups changed")
    require(armature_fingerprint(armature) == baseline["armature_fingerprint"], "Armature/rest data changed")


def restore_original_matrices(cuirass, legs, original_cuirass, original_legs):
    cuirass.matrix_world = original_cuirass.copy()
    legs.matrix_world = original_legs.copy()
    bpy.context.view_layer.update()
    require(matrices_close(cuirass.matrix_world, original_cuirass, 1.0e-6), "Failed to restore WORK_Cuirass")
    require(matrices_close(legs.matrix_world, original_legs, 1.0e-6), "Failed to restore WORK_LegArmor")


def apply_world_z_offset(obj, original_matrix, displacement_cm):
    matrix = original_matrix.copy()
    matrix.translation.z += displacement_cm
    obj.matrix_world = matrix
    bpy.context.view_layer.update()


def verify_world_offset(obj, original_matrix, displacement_cm):
    current = obj.matrix_world
    actual = current.translation.z - original_matrix.translation.z
    require(abs(actual - displacement_cm) <= 1.0e-5, f"Incorrect world Z displacement on {obj.name}: {actual}")
    for row in range(3):
        for column in range(3):
            require(
                abs(current[row][column] - original_matrix[row][column]) <= 1.0e-6,
                f"Rotation/scale changed on {obj.name}",
            )
    require(abs(current.translation.x - original_matrix.translation.x) <= 1.0e-6, f"World X changed on {obj.name}")
    require(abs(current.translation.y - original_matrix.translation.y) <= 1.0e-6, f"World Y changed on {obj.name}")
    return actual


def render_view(camera_name, output_path):
    scene = bpy.context.scene
    scene.camera = bpy.data.objects[camera_name]
    scene.render.filepath = str(output_path)
    result = bpy.ops.render.render(write_still=True)
    require("FINISHED" in result, f"Render failed: {output_path}")
    require(output_path.is_file(), f"Render missing: {output_path}")
    require(output_path.stat().st_size > 50 * 1024, f"Render is too small: {output_path}")
    print(f"RENDERED {output_path}")


def load_rgb_pixels(path):
    image = bpy.data.images.load(str(path), check_existing=False)
    width, height = image.size
    require(width > 0 and height > 0, f"Invalid image dimensions: {path}")
    pixels = np.empty(width * height * 4, dtype=np.float32)
    image.pixels.foreach_get(pixels)
    pixels = pixels.reshape((height, width, 4))[:, :, :3].copy()
    bpy.data.images.remove(image)
    return pixels


def visible_difference(original_path, variant_path):
    original = load_rgb_pixels(original_path)
    variant = load_rgb_pixels(variant_path)
    require(original.shape == variant.shape, f"Image dimensions differ: {variant_path}")
    difference = np.abs(original - variant)
    changed_pixels = np.any(difference > (1.0 / 255.0), axis=2)
    changed_fraction = float(np.mean(changed_pixels))
    mean_absolute_difference = float(np.mean(difference))
    maximum_difference = float(np.max(difference))
    require(changed_fraction > 0.0001, f"No real visible pixel difference: {variant_path}")
    require(maximum_difference > (2.0 / 255.0), f"Pixel difference is below visibility threshold: {variant_path}")
    return {
        "original": str(original_path),
        "variant": str(variant_path),
        "changed_pixel_fraction": changed_fraction,
        "mean_absolute_rgb_difference": mean_absolute_difference,
        "maximum_rgb_difference": maximum_difference,
        "visible_difference_confirmed": True,
        "aesthetic_quality_assessed": False,
    }


def main():
    require(SOURCE_BLEND.is_file(), f"Missing source blend: {SOURCE_BLEND}")
    require(Path(bpy.data.filepath).resolve() == SOURCE_BLEND.resolve(), f"Unexpected open blend: {bpy.data.filepath}")
    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    DIAG_DIR.mkdir(parents=True, exist_ok=True)

    scene = bpy.context.scene
    baseline = capture_baseline()
    cuirass = bpy.data.objects["WORK_Cuirass"]
    legs = bpy.data.objects["WORK_LegArmor"]
    original_cuirass = cuirass.matrix_world.copy()
    original_legs = legs.matrix_world.copy()

    original_render_paths = {}
    for view, camera_name in CAMERAS.items():
        output_path = DIAG_DIR / f"Original_{view}.png"
        render_view(camera_name, output_path)
        original_render_paths[view] = output_path

    results = {
        "source_blend": str(SOURCE_BLEND),
        "geometry_modified": False,
        "materials_modified": False,
        "skinning_performed": False,
        "awaiting_human_visual_approval": True,
        "variants": {},
    }

    for label, settings in VARIANTS.items():
        restore_original_matrices(cuirass, legs, original_cuirass, original_legs)
        apply_world_z_offset(cuirass, original_cuirass, settings["chest_cm"])
        apply_world_z_offset(legs, original_legs, settings["legs_cm"])

        actual_chest = verify_world_offset(cuirass, original_cuirass, settings["chest_cm"])
        actual_legs = verify_world_offset(legs, original_legs, settings["legs_cm"])
        validate_invariants(baseline)

        render_paths = {}
        differences = {}
        for view, camera_name in CAMERAS.items():
            output_path = DIAG_DIR / f"{label}_{view}.png"
            render_view(camera_name, output_path)
            render_paths[view] = output_path
            differences[view] = visible_difference(original_render_paths[view], output_path)

        scene.camera = baseline["scene_camera"]
        scene.render.filepath = baseline["render_filepath"]
        validate_invariants(baseline)
        verify_world_offset(cuirass, original_cuirass, settings["chest_cm"])
        verify_world_offset(legs, original_legs, settings["legs_cm"])

        blend_path = OUTPUT_DIR / settings["blend"]
        bpy.ops.wm.save_as_mainfile(filepath=str(blend_path), check_existing=False)
        require(blend_path.is_file(), f"Candidate blend was not saved: {blend_path}")
        results["variants"][label] = {
            "blend": str(blend_path),
            "requested_world_z_cm": {
                "WORK_Cuirass": settings["chest_cm"],
                "WORK_LegArmor": settings["legs_cm"],
            },
            "actual_world_z_cm": {
                "WORK_Cuirass": actual_chest,
                "WORK_LegArmor": actual_legs,
            },
            "renders": {view: str(path) for view, path in render_paths.items()},
            "visible_differences": differences,
            "local_mesh_coordinates_unchanged": True,
            "vertex_triangle_counts_unchanged": True,
            "uvs_unchanged": True,
            "materials_unchanged": True,
            "armature_modifier_count": 0,
            "vertex_group_count": 0,
            "root_unchanged": True,
            "body_unchanged": True,
        }

    restore_original_matrices(cuirass, legs, original_cuirass, original_legs)
    validate_invariants(baseline)
    scene.camera = baseline["scene_camera"]
    scene.render.filepath = baseline["render_filepath"]

    validation_path = DIAG_DIR / "alignment_candidates_validation.json"
    with validation_path.open("w", encoding="utf-8") as handle:
        json.dump(results, handle, indent=2, ensure_ascii=False)

    print("GEOMETRY_MODIFIED=false")
    print("MATERIALS_MODIFIED=false")
    print("SKINNING_PERFORMED=false")
    print("AWAITING_HUMAN_VISUAL_APPROVAL=true")


if __name__ == "__main__":
    try:
        main()
    except Exception:
        traceback.print_exc()
        sys.exit(1)
