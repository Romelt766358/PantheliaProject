import bpy
import json
import math
import os
import sys
import traceback
from pathlib import Path

import numpy as np
from mathutils import Vector
from bpy_extras.object_utils import world_to_camera_view


BODY_FBX = Path(r"C:\Panthelia Project\PantheliaProject\Saved\Exports\Armor\GildedKnight\SKM_PantheliaPlayer_Body_LOD0.fbx")
ARMOR_FBX = Path(r"C:\Panthelia Project\PantheliaProject\ExternalAssets\Armor\GildedKnight\Original\Extracted\source\FemaleArmor.fbx")
TEXTURE_DIR = Path(r"C:\Panthelia Project\PantheliaProject\ExternalAssets\Armor\GildedKnight\Original\Extracted\textures")
BLEND_PATH = Path(r"C:\Panthelia Project\PantheliaProject\ExternalAssets\Armor\GildedKnight\Blender\Restart\Panthelia_GildedKnight_RESTART_v001_TexturedRigid.blend")
DIAG_DIR = Path(r"C:\Panthelia Project\PantheliaProject\ExternalAssets\Armor\GildedKnight\Blender\Diagnostics\Restart_v001")
MANIFEST_PATH = DIAG_DIR / "materials_manifest.json"

COLLECTION_NAMES = (
    "00_REFERENCE_BODY",
    "01_ARMOR_SOURCE_ORIGINAL",
    "02_ARMOR_WORK_RIGID",
    "03_LIGHTS",
    "04_CAMERAS",
    "05_DIAGNOSTICS",
)

PIECES = (
    ("Armor", "WORK_Armor", "Armor", "MAT_GildedKnight_Armor"),
    ("cuirass", "WORK_Cuirass", "cuirass", "MAT_GildedKnight_Cuirass"),
    ("Arm Armor", "WORK_ArmArmor", "Arm_Armor", "MAT_GildedKnight_ArmArmor"),
    ("leg armor", "WORK_LegArmor", "leg_armor", "MAT_GildedKnight_LegArmor"),
    ("sock", "WORK_Sock", "sock", "MAT_GildedKnight_Sock"),
)

ROLE_SUFFIX = {
    "BaseColor": "Base_color",
    "Metallic": "Metallic",
    "Roughness": "Roughness",
    "Normal_DirectX": "Normal_DirectX",
    "Mixed_AO": "Mixed_AO",
    "Translucency": "Translucency",
}

REQUIRED_ROLES = ("BaseColor", "Metallic", "Roughness", "Normal_DirectX")


def require(condition, message):
    if not condition:
        raise RuntimeError(message)


def ensure_inputs():
    require(BODY_FBX.is_file(), f"Missing body FBX: {BODY_FBX}")
    require(ARMOR_FBX.is_file(), f"Missing armor FBX: {ARMOR_FBX}")
    require(TEXTURE_DIR.is_dir(), f"Missing texture directory: {TEXTURE_DIR}")
    require(not BLEND_PATH.exists(), f"Refusing to overwrite existing blend: {BLEND_PATH}")
    BLEND_PATH.parent.mkdir(parents=True, exist_ok=True)
    DIAG_DIR.mkdir(parents=True, exist_ok=True)


def move_to_collection(obj, collection):
    for current in tuple(obj.users_collection):
        current.objects.unlink(obj)
    collection.objects.link(obj)


def import_fbx(path):
    before = set(bpy.data.objects)
    result = bpy.ops.import_scene.fbx(
        filepath=str(path),
        use_anim=False,
        automatic_bone_orientation=False,
        ignore_leaf_bones=True,
        use_custom_normals=True,
        use_image_search=False,
    )
    require("FINISHED" in result, f"FBX import failed: {path}")
    return [obj for obj in bpy.data.objects if obj not in before]


def configure_scene():
    bpy.ops.wm.read_factory_settings(use_empty=True)
    scene = bpy.context.scene
    scene.unit_settings.system = "METRIC"
    scene.unit_settings.scale_length = 0.01
    scene.unit_settings.length_unit = "CENTIMETERS"
    scene.render.engine = "BLENDER_EEVEE"
    scene.render.resolution_x = 1024
    scene.render.resolution_y = 1024
    scene.render.resolution_percentage = 100
    scene.render.image_settings.file_format = "PNG"
    scene.render.image_settings.color_mode = "RGBA"
    scene.render.film_transparent = False
    scene.render.image_settings.color_depth = "8"
    scene.render.resolution_percentage = 100
    scene.view_settings.view_transform = "AgX"
    scene.view_settings.look = "AgX - Medium High Contrast"
    bpy.context.preferences.system.gl_texture_limit = "CLAMP_2048"
    if scene.world is None:
        scene.world = bpy.data.worlds.new("World")
    scene.world.color = (0.025, 0.025, 0.025)
    scene.world.use_nodes = True
    background = scene.world.node_tree.nodes.get("Background")
    background.inputs["Color"].default_value = (0.025, 0.025, 0.025, 1.0)
    background.inputs["Strength"].default_value = 0.22

    collections = {}
    for name in COLLECTION_NAMES:
        collection = bpy.data.collections.new(name)
        scene.collection.children.link(collection)
        collections[name] = collection
    return scene, collections


def create_body(scene, collections):
    imported = import_fbx(BODY_FBX)
    for obj in imported:
        move_to_collection(obj, collections["00_REFERENCE_BODY"])

    armatures = [obj for obj in imported if obj.type == "ARMATURE"]
    meshes = [obj for obj in imported if obj.type == "MESH"]
    require(len(armatures) == 1, f"Expected one body armature, found {len(armatures)}")
    require(len(meshes) == 1, f"Expected one body mesh, found {len(meshes)}")
    armatures[0].name = "REF_Panthelia_Body_Armature"
    body_mesh = meshes[0]
    body_mesh.name = "REF_Panthelia_Body_LOD0"

    material = bpy.data.materials.new("MAT_Diagnostic_BodyGray")
    material.use_nodes = True
    bsdf = material.node_tree.nodes.get("Principled BSDF")
    bsdf.inputs["Base Color"].default_value = (0.24, 0.24, 0.24, 1.0)
    bsdf.inputs["Metallic"].default_value = 0.0
    bsdf.inputs["Roughness"].default_value = 0.82
    body_mesh.data.materials.clear()
    body_mesh.data.materials.append(material)
    return body_mesh, armatures[0], imported


def mesh_signature(obj):
    mesh = obj.data
    mesh.calc_loop_triangles()
    vertices = tuple(tuple(v.co) for v in mesh.vertices)
    triangles = tuple(tuple(tri.vertices) for tri in mesh.loop_triangles)
    uv_layers = []
    for layer in mesh.uv_layers:
        uv_layers.append((layer.name, tuple(tuple(loop.uv) for loop in layer.data)))
    return len(mesh.vertices), vertices, triangles, tuple(uv_layers)


def create_armor_copies(collections):
    imported = import_fbx(ARMOR_FBX)
    for obj in imported:
        move_to_collection(obj, collections["01_ARMOR_SOURCE_ORIGINAL"])

    source_meshes = {obj.name: obj for obj in imported if obj.type == "MESH"}
    expected = {piece[0] for piece in PIECES}
    require(set(source_meshes) == expected, f"Armor meshes differ from expected: {sorted(source_meshes)}")

    work_objects = {}
    comparisons = {}
    for source_name, work_name, _, _ in PIECES:
        source = source_meshes[source_name]
        source_signature = mesh_signature(source)
        work = source.copy()
        work.data = source.data.copy()
        work.name = work_name
        collections["02_ARMOR_WORK_RIGID"].objects.link(work)
        work.matrix_world = source.matrix_world.copy()
        copied_signature = mesh_signature(work)
        require(
            source_signature == copied_signature,
            f"Exact geometry/triangle/UV verification failed before parenting: {source_name}",
        )
        for modifier in tuple(work.modifiers):
            work.modifiers.remove(modifier)
        work.vertex_groups.clear()
        comparisons[work_name] = {
            "source": source_name,
            "vertex_count": source_signature[0],
            "triangle_count": len(source_signature[2]),
            "uv_layer_count": len(source_signature[3]),
            "local_coordinates_exact": True,
            "triangles_exact": True,
            "uvs_exact": True,
        }
        work_objects[source_name] = work

    root = bpy.data.objects.new("WORK_Armor_RigidRoot", None)
    root.empty_display_type = "PLAIN_AXES"
    root.empty_display_size = 20.0
    collections["02_ARMOR_WORK_RIGID"].objects.link(root)
    for work in work_objects.values():
        world = work.matrix_world.copy()
        work.parent = root
        work.matrix_world = world

    root.location = (0.0, -8.22, 85.35)
    root.rotation_euler = (0.0, 0.0, 0.0)
    root.scale = (0.689, 0.689, 0.689)

    source_collection = collections["01_ARMOR_SOURCE_ORIGINAL"]
    source_collection.hide_viewport = True
    source_collection.hide_render = True
    return work_objects, root, comparisons


def load_texture(path, color_space):
    image = bpy.data.images.load(str(path), check_existing=True)
    image.colorspace_settings.name = color_space
    return image


def texture_node(nodes, image, role, location):
    node = nodes.new("ShaderNodeTexImage")
    node.name = f"TEX_{role}"
    node.label = role
    node.image = image
    node.location = location
    return node


def create_material(material_name, files):
    material = bpy.data.materials.new(material_name)
    material.use_nodes = True
    material.surface_render_method = "DITHERED"
    nodes = material.node_tree.nodes
    links = material.node_tree.links
    nodes.clear()

    output = nodes.new("ShaderNodeOutputMaterial")
    output.name = "Material Output"
    output.location = (900, 100)
    bsdf = nodes.new("ShaderNodeBsdfPrincipled")
    bsdf.name = "Principled BSDF"
    bsdf.location = (620, 100)
    bsdf.inputs["Alpha"].default_value = 1.0
    links.new(bsdf.outputs["BSDF"], output.inputs["Surface"])

    manifest_textures = {}
    images = {}
    for role, path in files.items():
        if path is None:
            manifest_textures[role] = {
                "found": False,
                "path": None,
                "resolution": None,
                "color_space": None,
                "connected": False,
                "destination_nodes": [],
            }
            continue
        color_space = "sRGB" if role == "BaseColor" else "Non-Color"
        images[role] = load_texture(path, color_space)
        manifest_textures[role] = {
            "found": True,
            "path": str(path),
            "resolution": [int(images[role].size[0]), int(images[role].size[1])],
            "color_space": color_space,
            "connected": False,
            "destination_nodes": [],
        }

    base_node = texture_node(nodes, images["BaseColor"], "BaseColor", (-900, 430))
    metallic_node = texture_node(nodes, images["Metallic"], "Metallic", (-280, 120))
    roughness_node = texture_node(nodes, images["Roughness"], "Roughness", (-280, -40))
    normal_node = texture_node(nodes, images["Normal_DirectX"], "Normal_DirectX", (-900, -360))

    links.new(metallic_node.outputs["Color"], bsdf.inputs["Metallic"])
    links.new(roughness_node.outputs["Color"], bsdf.inputs["Roughness"])
    manifest_textures["Metallic"]["connected"] = True
    manifest_textures["Metallic"]["destination_nodes"] = ["Principled BSDF.Metallic"]
    manifest_textures["Roughness"]["connected"] = True
    manifest_textures["Roughness"]["destination_nodes"] = ["Principled BSDF.Roughness"]

    separate = nodes.new("ShaderNodeSeparateColor")
    separate.name = "Separate_Normal_RGB"
    separate.mode = "RGB"
    separate.location = (-620, -360)
    invert_green = nodes.new("ShaderNodeMath")
    invert_green.name = "Invert_DirectX_Green"
    invert_green.operation = "SUBTRACT"
    invert_green.inputs[0].default_value = 1.0
    invert_green.location = (-390, -350)
    combine = nodes.new("ShaderNodeCombineColor")
    combine.name = "Combine_OpenGL_Normal"
    combine.mode = "RGB"
    combine.location = (-150, -330)
    normal_map = nodes.new("ShaderNodeNormalMap")
    normal_map.name = "Normal Map"
    normal_map.space = "TANGENT"
    normal_map.location = (260, -250)
    links.new(normal_node.outputs["Color"], separate.inputs["Color"])
    links.new(separate.outputs["Red"], combine.inputs["Red"])
    links.new(separate.outputs["Green"], invert_green.inputs[1])
    links.new(invert_green.outputs[0], combine.inputs["Green"])
    links.new(separate.outputs["Blue"], combine.inputs["Blue"])
    links.new(combine.outputs["Color"], normal_map.inputs["Color"])
    links.new(normal_map.outputs["Normal"], bsdf.inputs["Normal"])
    manifest_textures["Normal_DirectX"]["connected"] = True
    manifest_textures["Normal_DirectX"]["destination_nodes"] = [
        "Separate_Normal_RGB",
        "Invert_DirectX_Green",
        "Combine_OpenGL_Normal",
        "Normal Map.Color",
        "Principled BSDF.Normal",
    ]

    if files["Mixed_AO"] is not None:
        ao_node = texture_node(nodes, images["Mixed_AO"], "Mixed_AO", (-900, 180))
        multiply = nodes.new("ShaderNodeMixRGB")
        multiply.name = "Multiply_BaseColor_AO"
        multiply.blend_type = "MULTIPLY"
        multiply.inputs[0].default_value = 1.0
        multiply.location = (250, 360)
        links.new(base_node.outputs["Color"], multiply.inputs[1])
        links.new(ao_node.outputs["Color"], multiply.inputs[2])
        links.new(multiply.outputs["Color"], bsdf.inputs["Base Color"])
        manifest_textures["BaseColor"]["connected"] = True
        manifest_textures["BaseColor"]["destination_nodes"] = [
            "Multiply_BaseColor_AO.Color1",
            "Principled BSDF.Base Color",
        ]
        manifest_textures["Mixed_AO"]["connected"] = True
        manifest_textures["Mixed_AO"]["destination_nodes"] = [
            "Multiply_BaseColor_AO.Color2",
            "Principled BSDF.Base Color",
        ]
    else:
        links.new(base_node.outputs["Color"], bsdf.inputs["Base Color"])
        manifest_textures["BaseColor"]["connected"] = True
        manifest_textures["BaseColor"]["destination_nodes"] = ["Principled BSDF.Base Color"]

    if files["Translucency"] is not None:
        texture_node(nodes, images["Translucency"], "Translucency_Unconnected", (-900, 680))
        manifest_textures["Translucency"]["connected"] = False
        manifest_textures["Translucency"]["destination_nodes"] = []
        manifest_textures["Translucency"]["note"] = "Loaded for documentation; intentionally unconnected because its application is ambiguous."

    return material, manifest_textures


def rebuild_materials(work_objects, comparisons):
    png_files = sorted(
        (path for path in TEXTURE_DIR.iterdir() if path.is_file() and path.suffix.casefold() == ".png"),
        key=lambda path: path.name.casefold(),
    )
    require(len(png_files) == 25, f"Expected exactly 25 PNG textures, found {len(png_files)}")
    by_name = {path.name.casefold(): path for path in png_files}
    require(len(by_name) == 25, "Case-insensitive texture names are not unique")

    manifest = {
        "texture_directory": str(TEXTURE_DIR),
        "png_count": len(png_files),
        "rigid_alignment": {
            "root": "WORK_Armor_RigidRoot",
            "location": [0.0, -8.22, 85.35],
            "rotation_degrees": [0.0, 0.0, 0.0],
            "scale": [0.689, 0.689, 0.689],
            "transforms_applied": False,
        },
        "geometry_verification": comparisons,
        "materials": [],
    }
    connected_count = 0
    created_names = []

    for source_name, work_name, prefix, material_name in PIECES:
        files = {}
        for role, suffix in ROLE_SUFFIX.items():
            expected_name = f"{prefix}_{suffix}.png".casefold()
            files[role] = by_name.get(expected_name)
        missing_required = [role for role in REQUIRED_ROLES if files[role] is None]
        require(not missing_required, f"Missing required textures for {source_name}: {missing_required}")
        material, texture_manifest = create_material(material_name, files)
        work = work_objects[source_name]
        work.data.materials.clear()
        work.data.materials.append(material)
        missing = [role for role, path in files.items() if path is None]
        connected_count += sum(1 for info in texture_manifest.values() if info["connected"])
        created_names.append(material_name)
        manifest["materials"].append(
            {
                "object": work_name,
                "material": material_name,
                "textures": texture_manifest,
                "missing_textures": missing,
            }
        )

    manifest["connected_texture_count"] = connected_count
    manifest["created_materials"] = created_names
    with MANIFEST_PATH.open("w", encoding="utf-8") as handle:
        json.dump(manifest, handle, indent=2, ensure_ascii=False)
    return connected_count, created_names


def evaluated_bounds(objects):
    depsgraph = bpy.context.evaluated_depsgraph_get()
    points = []
    for obj in objects:
        if obj.type != "MESH":
            continue
        evaluated = obj.evaluated_get(depsgraph)
        matrix = evaluated.matrix_world
        points.extend(matrix @ Vector(corner) for corner in evaluated.bound_box)
    require(points, "No mesh bounds available")
    minimum = Vector((min(p.x for p in points), min(p.y for p in points), min(p.z for p in points)))
    maximum = Vector((max(p.x for p in points), max(p.y for p in points), max(p.z for p in points)))
    return points, minimum, maximum


def box_points(minimum, maximum):
    return [
        Vector((x, y, z))
        for x in (minimum.x, maximum.x)
        for y in (minimum.y, maximum.y)
        for z in (minimum.z, maximum.z)
    ]


def aim_camera(camera, target, direction, framing_points, margin=1.14):
    direction = Vector(direction).normalized()
    camera.rotation_euler = direction.to_track_quat("Z", "Y").to_euler()
    rotation = camera.rotation_euler.to_matrix()
    right = rotation @ Vector((1.0, 0.0, 0.0))
    up = rotation @ Vector((0.0, 1.0, 0.0))
    horizontal = max(abs((point - target).dot(right)) for point in framing_points)
    vertical = max(abs((point - target).dot(up)) for point in framing_points)
    depth = max(abs((point - target).dot(direction)) for point in framing_points)
    fov = 2.0 * math.atan(camera.data.sensor_width / (2.0 * camera.data.lens))
    distance = max(horizontal / math.tan(fov * 0.5), vertical / math.tan(fov * 0.5)) * margin + depth
    distance = max(distance, 1.0)
    camera.location = target + direction * distance
    camera.data.clip_start = max(0.1, distance / 1000.0)
    camera.data.clip_end = max(10000.0, distance * 10.0)


def create_camera(name, lens, target, direction, framing_points, collection, margin=1.14):
    data = bpy.data.cameras.new(name)
    data.lens = lens
    data.sensor_width = 36.0
    camera = bpy.data.objects.new(name, data)
    collection.objects.link(camera)
    aim_camera(camera, target, direction, framing_points, margin)
    return camera


def create_cameras(scene, collections, body_mesh, work_objects):
    all_objects = [body_mesh, *work_objects.values()]
    points, minimum, maximum = evaluated_bounds(all_objects)
    center = (minimum + maximum) * 0.5
    height = maximum.z - minimum.z

    cameras = {}
    specs = {
        "CAM_Front": (78.0, (0.0, -1.0, 0.0)),
        "CAM_Back": (78.0, (0.0, 1.0, 0.0)),
        "CAM_Left": (78.0, (-1.0, 0.0, 0.0)),
        "CAM_Right": (78.0, (1.0, 0.0, 0.0)),
        "CAM_Perspective": (82.0, (-1.15, -1.55, 0.62)),
    }
    for name, (lens, direction) in specs.items():
        cameras[name] = create_camera(
            name, lens, center, direction, points, collections["04_CAMERAS"], margin=1.16
        )

    chest_target = Vector((center.x, center.y, minimum.z + height * 0.71))
    chest_min = Vector((minimum.x, minimum.y, minimum.z + height * 0.51))
    chest_max = Vector((maximum.x, maximum.y, minimum.z + height * 0.91))
    cameras["CAM_ChestCloseup"] = create_camera(
        "CAM_ChestCloseup",
        100.0,
        chest_target,
        (0.0, -1.0, 0.0),
        box_points(chest_min, chest_max),
        collections["04_CAMERAS"],
        margin=1.10,
    )

    legs_target = Vector((center.x, center.y, minimum.z + height * 0.29))
    legs_min = Vector((minimum.x, minimum.y, minimum.z + height * 0.03))
    legs_max = Vector((maximum.x, maximum.y, minimum.z + height * 0.55))
    cameras["CAM_LegsCloseup"] = create_camera(
        "CAM_LegsCloseup",
        96.0,
        legs_target,
        (0.0, -1.0, 0.0),
        box_points(legs_min, legs_max),
        collections["04_CAMERAS"],
        margin=1.08,
    )
    scene.camera = cameras["CAM_Perspective"]
    return cameras, minimum, maximum


def add_area_light(name, location, target, energy, size, color, collection):
    data = bpy.data.lights.new(name, "AREA")
    data.energy = energy
    data.shape = "DISK"
    data.size = size
    data.color = color
    light = bpy.data.objects.new(name, data)
    collection.objects.link(light)
    light.location = location
    light.rotation_euler = (Vector(target) - Vector(location)).to_track_quat("-Z", "Y").to_euler()
    return light


def create_lighting(collections, minimum, maximum):
    center = (minimum + maximum) * 0.5
    span = max(maximum.x - minimum.x, maximum.y - minimum.y, maximum.z - minimum.z)
    distance = span * 1.35
    size = span * 0.60
    add_area_light(
        "LIGHT_Key",
        center + Vector((-0.75 * distance, -0.95 * distance, 0.55 * distance)),
        center,
        170000.0,
        size,
        (1.0, 0.82, 0.62),
        collections["03_LIGHTS"],
    )
    add_area_light(
        "LIGHT_Fill",
        center + Vector((0.82 * distance, -0.65 * distance, 0.20 * distance)),
        center,
        70000.0,
        size * 0.85,
        (0.66, 0.78, 1.0),
        collections["03_LIGHTS"],
    )
    add_area_light(
        "LIGHT_Rim",
        center + Vector((0.25 * distance, 1.05 * distance, 0.75 * distance)),
        center,
        130000.0,
        size * 0.70,
        (1.0, 0.87, 0.72),
        collections["03_LIGHTS"],
    )


def armor_intersects_camera_frame(scene, camera, work_objects):
    depsgraph = bpy.context.evaluated_depsgraph_get()
    projected = []
    for work in work_objects.values():
        evaluated = work.evaluated_get(depsgraph)
        for corner in evaluated.bound_box:
            point = evaluated.matrix_world @ Vector(corner)
            projected.append(world_to_camera_view(scene, camera, point))
    return any(0.0 <= p.x <= 1.0 and 0.0 <= p.y <= 1.0 and p.z > 0.0 for p in projected)


def render_view(scene, camera, filename, work_objects):
    require(armor_intersects_camera_frame(scene, camera, work_objects), f"Armor is outside camera frame: {camera.name}")
    scene.camera = camera
    output = DIAG_DIR / filename
    scene.render.filepath = str(output)
    result = bpy.ops.render.render(write_still=True)
    require("FINISHED" in result, f"Render failed: {filename}")
    require(output.is_file(), f"Render was not created: {output}")
    print(f"RENDERED {output}")
    return output


def create_contact_sheet(paths, output):
    tile_width = 512
    tile_height = 512
    canvas = np.zeros((tile_height * 2, tile_width * 3, 4), dtype=np.float32)
    canvas[:, :, 3] = 1.0
    for index, path in enumerate(paths):
        image = bpy.data.images.load(str(path), check_existing=False)
        width, height = image.size
        require(width == 1024 and height == 1024, f"Unexpected render dimensions: {path} -> {width}x{height}")
        pixels = np.empty(width * height * 4, dtype=np.float32)
        image.pixels.foreach_get(pixels)
        pixels = pixels.reshape((height, width, 4))[::2, ::2, :]
        row = index // 3
        column = index % 3
        y0 = (1 - row) * tile_height
        x0 = column * tile_width
        canvas[y0:y0 + tile_height, x0:x0 + tile_width, :] = pixels
        bpy.data.images.remove(image)
    sheet = bpy.data.images.new("Restart_v001_ContactSheet", width=1536, height=1024, alpha=False)
    sheet.file_format = "PNG"
    sheet.pixels.foreach_set(canvas.ravel())
    sheet.filepath_raw = str(output)
    sheet.save()
    require(output.is_file(), f"Contact sheet was not created: {output}")


def validate_png(path, source_armor=False):
    require(path.is_file(), f"Missing PNG: {path}")
    require(path.stat().st_size > 50 * 1024, f"PNG is not larger than 50 KB: {path}")
    image = bpy.data.images.load(str(path), check_existing=False)
    width, height = image.size
    pixels = np.empty(width * height * 4, dtype=np.float32)
    image.pixels.foreach_get(pixels)
    pixels = pixels.reshape((-1, 4))
    sample = pixels[::max(1, len(pixels) // 250000)]
    require(np.max(sample[:, 3]) > 0.05, f"PNG is completely transparent: {path}")
    require(np.max(sample[:, :3]) > 0.03, f"PNG is completely black: {path}")
    quantized = np.clip(sample[:, :3] * 255.0, 0, 255).astype(np.uint8)
    unique_colors = len(np.unique(quantized, axis=0))
    require(unique_colors > 100, f"PNG has only {unique_colors} sampled colors: {path}")
    if source_armor:
        chroma = np.max(quantized, axis=1) - np.min(quantized, axis=1)
        require(np.max(chroma) >= 4, f"Source armor render is completely gray: {path}")
    bpy.data.images.remove(image)
    return {
        "path": str(path),
        "bytes": path.stat().st_size,
        "dimensions": [int(width), int(height)],
        "sampled_unique_colors": unique_colors,
        "not_black": True,
        "not_transparent": True,
        "armor_in_frame": True,
        "not_completely_gray": True if source_armor else None,
    }


def main():
    ensure_inputs()
    scene, collections = configure_scene()
    body_mesh, _, body_imported = create_body(scene, collections)
    work_objects, root, comparisons = create_armor_copies(collections)
    connected_count, material_names = rebuild_materials(work_objects, comparisons)
    cameras, minimum, maximum = create_cameras(scene, collections, body_mesh, work_objects)
    create_lighting(collections, minimum, maximum)

    for obj in body_imported:
        obj.hide_render = True
    source_render = render_view(
        scene,
        cameras["CAM_Front"],
        "source_armor_textured_front.png",
        work_objects,
    )
    for obj in body_imported:
        obj.hide_render = False

    render_specs = (
        ("aligned_front.png", "CAM_Front"),
        ("aligned_back.png", "CAM_Back"),
        ("aligned_left.png", "CAM_Left"),
        ("aligned_right.png", "CAM_Right"),
        ("aligned_perspective.png", "CAM_Perspective"),
        ("aligned_chest_closeup.png", "CAM_ChestCloseup"),
        ("aligned_legs_closeup.png", "CAM_LegsCloseup"),
    )
    rendered = {"source_armor_textured_front.png": source_render}
    for filename, camera_name in render_specs:
        rendered[filename] = render_view(scene, cameras[camera_name], filename, work_objects)

    contact_path = DIAG_DIR / "contact_sheet.png"
    create_contact_sheet(
        [
            rendered["aligned_front.png"],
            rendered["aligned_back.png"],
            rendered["aligned_left.png"],
            rendered["aligned_perspective.png"],
            rendered["aligned_chest_closeup.png"],
            rendered["aligned_legs_closeup.png"],
        ],
        contact_path,
    )

    validations = []
    for filename, path in rendered.items():
        validations.append(validate_png(path, source_armor=filename == "source_armor_textured_front.png"))
    validations.append(validate_png(contact_path))
    require(len(validations) == 9, f"Expected nine validated PNGs, got {len(validations)}")

    for work in work_objects.values():
        require(not work.modifiers, f"Unexpected modifier on {work.name}")
        require(not work.vertex_groups, f"Unexpected vertex group on {work.name}")
        require(work.parent == root, f"Unexpected parent on {work.name}")

    validation_path = DIAG_DIR / "validation.json"
    with validation_path.open("w", encoding="utf-8") as handle:
        json.dump(
            {
                "png_count": 9,
                "files": validations,
                "geometry_modified": False,
                "skinning_performed": False,
                "awaiting_human_visual_approval": True,
            },
            handle,
            indent=2,
            ensure_ascii=False,
        )

    scene.camera = cameras["CAM_Perspective"]
    scene.render.filepath = str(rendered["aligned_perspective.png"])
    bpy.context.view_layer.objects.active = root
    root.select_set(True)
    bpy.ops.wm.save_as_mainfile(filepath=str(BLEND_PATH), check_existing=False)
    require(BLEND_PATH.is_file(), f"Blend file was not saved: {BLEND_PATH}")
    print(f"CONNECTED_TEXTURE_COUNT={connected_count}")
    print(f"CREATED_MATERIALS={json.dumps(material_names)}")
    print("GEOMETRY_MODIFIED=false")
    print("SKINNING_PERFORMED=false")
    print("AWAITING_HUMAN_VISUAL_APPROVAL=true")


if __name__ == "__main__":
    try:
        main()
    except Exception:
        traceback.print_exc()
        sys.exit(1)
