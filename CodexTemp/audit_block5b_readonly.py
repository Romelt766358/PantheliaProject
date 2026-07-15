import json
import unreal


OUTPUT = unreal.Paths.convert_relative_path_to_full(
    unreal.Paths.project_dir() + "CodexTemp/block5b_audit_evidence.json"
)


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def path(value):
    if value is None:
        return None
    try:
        return value.get_path_name()
    except Exception:
        return str(value)


def text_value(value):
    if value is None:
        return None
    if isinstance(value, (bool, int, float, str)):
        return value
    if hasattr(value, "get_path_name"):
        return path(value)
    return str(value)


def dirty_packages():
    names = []
    for getter in (
        unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages,
        unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages,
    ):
        try:
            for package in getter():
                name = package.get_path_name()
                if name not in names:
                    names.append(name)
        except Exception as exc:
            names.append("<dirty-query-error: %s>" % exc)
    return sorted(names)


def asset_object_path(data):
    return "%s.%s" % (str(data.package_name), str(data.asset_name))


def generated_class(blueprint):
    try:
        return unreal.BlueprintEditorLibrary.generated_class(blueprint)
    except Exception:
        value = prop(blueprint, "generated_class")
        if value:
            return value
        try:
            return blueprint.generated_class()
        except Exception:
            return None


def node_title(node):
    try:
        return str(unreal.BlueprintEditorLibrary.get_node_title(node))
    except Exception:
        return node.get_name()


def pin_record(pin):
    linked = []
    try:
        connected = pin.list_connected_pins()
    except Exception:
        connected = prop(pin, "linked_to", []) or []
    for other in connected:
        try:
            owner = other.get_owning_node()
        except Exception:
            owner = prop(other, "owning_node")
        try:
            other_name = str(other.get_pin_name())
        except Exception:
            other_name = str(prop(other, "pin_name", ""))
        linked.append({
            "pin": other_name,
            "node": node_title(owner) if owner else None,
            "node_class": path(owner.get_class()) if owner else None,
        })
    try:
        name = str(pin.get_pin_name())
        direction = str(pin.get_pin_direction())
        default_value = str(pin.get_pin_value() or "")
    except Exception:
        name = str(prop(pin, "pin_name", ""))
        direction = str(prop(pin, "direction", ""))
        default_value = str(prop(pin, "default_value", "") or "")
    return {
        "name": name,
        "direction": direction,
        "default_value": default_value,
        "default_object": path(prop(pin, "default_object")),
        "linked_to": linked,
    }


INTERESTING_TERMS = (
    "spawn actor", "on weapon equipped", "weapon equipped", "start trace",
    "activate trace", "deactivate trace", "set external weapon trace source",
    "set weapon mesh component", "clear external weapon trace source",
    "set ignore look input", "reset ignore look input", "orient rotation to movement",
    "use controller desired rotation", "target offset", "init overlay", "highlight actor",
    "unhighlight actor", "un highlight actor", "block component", "player actions component",
    "sprint", "walk", "possess", "unpossess", "restart player", "destroy actor",
    "is valid", "equipment component", "equipped weapon",
)


def graph_records(blueprint):
    result = []
    try:
        graphs = unreal.BlueprintEditorLibrary.list_graphs(blueprint)
    except Exception:
        graphs = []
    for graph in graphs:
        graph_name = graph.get_name()
        try:
            graph_editor = unreal.BlueprintGraphEditor.get_graph_editor_by_name(
                blueprint, graph_name
            )
            nodes = graph_editor.list_all_nodes()
        except Exception:
            nodes = prop(graph, "nodes", []) or []
        for node in nodes:
            title = node_title(node)
            node_class = path(node.get_class())
            lower = title.lower()
            if "spawnactor" in node_class.lower() or any(term in lower for term in INTERESTING_TERMS):
                try:
                    pins = unreal.BlueprintEditorLibrary.list_all_pins(node)
                except Exception:
                    pins = []
                result.append({
                    "graph": graph_name,
                    "title": title,
                    "class": node_class,
                    "pins": [pin_record(pin) for pin in pins],
                })
    return result


CHANNELS = {}
for channel_name in (
    "ECC_WORLD_STATIC", "ECC_WORLD_DYNAMIC", "ECC_PAWN", "ECC_PHYSICS_BODY",
    "ECC_VEHICLE", "ECC_DESTRUCTIBLE", "ECC_GAME_TRACE_CHANNEL1",
    "ECC_GAME_TRACE_CHANNEL2", "ECC_GAME_TRACE_CHANNEL3", "ECC_GAME_TRACE_CHANNEL4",
):
    try:
        CHANNELS[channel_name] = getattr(unreal.CollisionChannel, channel_name)
    except Exception:
        pass


def primitive_record(comp):
    responses = {}
    for name, channel in CHANNELS.items():
        try:
            responses[name] = str(comp.get_collision_response_to_channel(channel))
        except Exception as exc:
            responses[name] = "<error: %s>" % exc
    record = {
        "name": comp.get_name(),
        "class": path(comp.get_class()),
        "component_tags": [str(x) for x in (prop(comp, "component_tags", []) or [])],
        "collision_enabled": str(comp.get_collision_enabled()),
        "collision_object_type": str(comp.get_collision_object_type()),
        "collision_profile_name": str(comp.get_collision_profile_name()),
        "generate_overlap_events": prop(comp, "generate_overlap_events"),
        "responses": responses,
    }
    if isinstance(comp, unreal.SkeletalMeshComponent):
        record["skeletal_mesh"] = path(prop(comp, "skeletal_mesh_asset"))
    if isinstance(comp, unreal.StaticMeshComponent):
        record["static_mesh"] = path(prop(comp, "static_mesh"))
    return record


def component_records(cdo):
    result = []
    try:
        components = cdo.get_components_by_class(unreal.ActorComponent)
    except Exception:
        components = []
    for comp in components:
        item = {
            "name": comp.get_name(),
            "class": path(comp.get_class()),
            "component_tags": [str(x) for x in (prop(comp, "component_tags", []) or [])],
        }
        if isinstance(comp, unreal.PrimitiveComponent):
            item = primitive_record(comp)
        for name in (
            "trace_radius", "weapon_base_socket_name", "weapon_tip_socket_name",
            "debug_mode", "log_trace_debug", "weapon_mesh_component",
            "default_weapon_class", "default_weapon_definition", "hand_socket_name",
            "orient_rotation_to_movement", "use_controller_desired_rotation", "target_offset",
        ):
            value = prop(comp, name, "<missing>")
            if value != "<missing>":
                item[name] = text_value(value)
        result.append(item)
    return result


report = {
    "dirty_before": dirty_packages(),
    "collision_channel_enum_names": [x for x in dir(unreal.CollisionChannel) if x.startswith("ECC_")],
    "effect_actors": [],
    "blueprints": [],
    "data_assets": {},
    "referencers": {},
    "errors": [],
    "api_probe": {},
    "template_checks": {},
    "socket_checks": {},
    "target_full_graphs": {},
    "weapon_trace_notifies": [],
}

registry = unreal.AssetRegistryHelpers.get_asset_registry()
registry.wait_for_completion()
all_assets = registry.get_assets_by_path("/Game", recursive=True, include_only_on_disk_assets=False)

effect_data = []
project_blueprint_data = []
for data in all_assets:
    class_path = str(data.asset_class_path)
    object_path = asset_object_path(data)
    if "Blueprint" not in class_path:
        continue
    native_parent = str(data.get_tag_value("NativeParentClass") or "")
    if "PantheliaEffectActor" in native_parent:
        effect_data.append(data)
    if object_path.startswith((
        "/Game/Blueprints/", "/Game/Characters/", "/Game/ThirdPerson/Blueprints/",
        "/Game/Input/", "/Game/UI/", "/Game/Combat/",
    )) and "/ARPG_Pack/" not in object_path and "/TestAssets/" not in object_path \
            and object_path != "/Game/UI/AttributeMenu/Button/WBP_Button.WBP_Button":
        project_blueprint_data.append(data)

effect_paths = set(asset_object_path(data) for data in effect_data)

for data in project_blueprint_data:
    object_path = asset_object_path(data)
    try:
        blueprint = data.get_asset()
        gen_class = generated_class(blueprint)
        if not gen_class:
            continue
        cdo = unreal.get_default_object(gen_class)
        entry = {
            "path": object_path,
            "class": path(gen_class),
            "parent_class": path(unreal.BlueprintEditorLibrary.get_blueprint_parent_class(blueprint)),
            "actor_tags": [str(x) for x in (prop(cdo, "tags", []) or [])],
            "components": component_records(cdo),
            "root_component": path(prop(cdo, "root_component")),
            "graphs": graph_records(blueprint),
        }
        report["blueprints"].append(entry)
        if object_path in effect_paths:
            props = {}
            for name in (
                "instant_gameplay_effect_class", "instant_effect_application_policy",
                "duration_gameplay_effect_class", "duration_effect_application_policy",
                "infinite_gameplay_effect_class", "infinite_effect_application_policy",
                "infinite_effect_removal_policy", "effect_source_policy",
                "destroy_on_effect_application", "destroy_on_effect_removal",
                "apply_effects_to_enemies", "actor_level", "owner", "instigator",
            ):
                props[name] = text_value(prop(cdo, name))
            entry_effect = dict(entry)
            entry_effect["properties"] = props
            report["effect_actors"].append(entry_effect)
    except Exception as exc:
        report["errors"].append({"asset": object_path, "error": str(exc)})


for asset_path in (
    "/Game/Blueprints/AbilitySystem/Data/PlayerWeapons/DA_Sword_Basic.DA_Sword_Basic",
    "/Game/Blueprints/Weapons/BP_Sword_Basic.BP_Sword_Basic",
    "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter",
    "/Game/UI/HUD/BP_PantheliaHUD.BP_PantheliaHUD",
):
    try:
        asset = unreal.EditorAssetLibrary.load_asset(asset_path)
        target = asset
        if "Blueprint" in asset.get_class().get_name():
            target = unreal.get_default_object(generated_class(asset))
        values = {"class": path(target.get_class())}
        for name in (
            "weapon_base_socket_name", "weapon_tip_socket_name", "weapon_definition",
            "overlay_widget_class", "overlay_widget_controller_class",
            "attribute_menu_widget_controller_class",
        ):
            value = prop(target, name, "<missing>")
            if value != "<missing>":
                values[name] = text_value(value)
        report["data_assets"][asset_path] = values
    except Exception as exc:
        report["errors"].append({"asset": asset_path, "error": str(exc)})


for template_path in (
    "/Game/Blueprints/GameplayEffects/BP_FireArea.BP_FireArea_C:Box_GEN_VARIABLE",
    "/Game/Characters/TestBoss/BP_Boss.BP_Boss_C:WeaponTrace_GEN_VARIABLE",
    "/Game/Characters/WarriorBoss/BP_WarriorBoss.BP_WarriorBoss_C:WeaponTrace_GEN_VARIABLE",
    "/Game/Characters/WarriorBoss/BP_WarriorBoss.BP_WarriorBoss_C:SM_Sword_GEN_VARIABLE",
):
    try:
        obj = unreal.load_object(None, template_path)
        if not obj:
            obj = unreal.find_object(None, template_path)
        values = {"object": path(obj), "class": path(obj.get_class()) if obj else None}
        if isinstance(obj, unreal.PrimitiveComponent):
            values.update(primitive_record(obj))
        for name in (
            "trace_radius", "weapon_base_socket_name", "weapon_tip_socket_name",
            "debug_mode", "log_trace_debug", "weapon_mesh_component",
        ):
            value = prop(obj, name, "<missing>") if obj else "<missing>"
            if value != "<missing>":
                values[name] = text_value(value)
        report["template_checks"][template_path] = values
    except Exception as exc:
        report["template_checks"][template_path] = {"error": str(exc)}


try:
    player_bp = unreal.EditorAssetLibrary.load_asset(
        "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter"
    )
    player_cdo = unreal.get_default_object(generated_class(player_bp))
    player_mesh = None
    equipment = None
    for comp in player_cdo.get_components_by_class(unreal.ActorComponent):
        if isinstance(comp, unreal.SkeletalMeshComponent) and comp.get_name() == "CharacterMesh0":
            player_mesh = comp
        if path(comp.get_class()) == "/Script/PantheliaProject.PantheliaEquipmentComponent":
            equipment = comp
    hand_socket = prop(equipment, "hand_socket_name")
    report["socket_checks"]["player_hand"] = {
        "mesh": path(prop(player_mesh, "skeletal_mesh_asset")),
        "socket": str(hand_socket),
        "exists": bool(player_mesh and player_mesh.does_socket_exist(hand_socket)),
    }

    weapon_bp = unreal.EditorAssetLibrary.load_asset(
        "/Game/Blueprints/Weapons/BP_Sword_Basic.BP_Sword_Basic"
    )
    weapon_cdo = unreal.get_default_object(generated_class(weapon_bp))
    definition = unreal.EditorAssetLibrary.load_asset(
        "/Game/Blueprints/AbilitySystem/Data/PlayerWeapons/DA_Sword_Basic.DA_Sword_Basic"
    )
    base_socket = prop(definition, "weapon_base_socket_name")
    tip_socket = prop(definition, "weapon_tip_socket_name")
    active_mesh = None
    try:
        active_mesh = weapon_cdo.get_active_mesh_component()
    except Exception:
        for comp in weapon_cdo.get_components_by_class(unreal.MeshComponent):
            if isinstance(comp, unreal.SkeletalMeshComponent) and prop(comp, "skeletal_mesh_asset"):
                active_mesh = comp
                break
            if isinstance(comp, unreal.StaticMeshComponent) and prop(comp, "static_mesh"):
                active_mesh = comp
    report["socket_checks"]["basic_sword_trace"] = {
        "active_mesh_component": path(active_mesh),
        "active_mesh_asset": path(prop(active_mesh, "skeletal_mesh_asset") or prop(active_mesh, "static_mesh")),
        "base_socket": str(base_socket),
        "tip_socket": str(tip_socket),
        "distinct": base_socket != tip_socket,
        "base_exists": bool(active_mesh and active_mesh.does_socket_exist(base_socket)),
        "tip_exists": bool(active_mesh and active_mesh.does_socket_exist(tip_socket)),
        "actor_root": path(prop(weapon_cdo, "root_component")),
    }

    boss_bp = unreal.EditorAssetLibrary.load_asset(
        "/Game/Characters/TestBoss/BP_Boss.BP_Boss"
    )
    boss_cdo = unreal.get_default_object(generated_class(boss_bp))
    boss_weapon = None
    for comp in boss_cdo.get_components_by_class(unreal.SkeletalMeshComponent):
        if comp.get_name() == "FinalWeaponMesh":
            boss_weapon = comp
            break
    report["socket_checks"]["test_boss_trace"] = {
        "mesh_component": path(boss_weapon),
        "mesh_asset": path(prop(boss_weapon, "skeletal_mesh_asset")),
        "base_socket": "WeaponBase",
        "tip_socket": "WeaponTip",
        "base_exists": bool(boss_weapon and boss_weapon.does_socket_exist("WeaponBase")),
        "tip_exists": bool(boss_weapon and boss_weapon.does_socket_exist("WeaponTip")),
    }

    warrior_weapon = unreal.load_object(
        None,
        "/Game/Characters/WarriorBoss/BP_WarriorBoss.BP_WarriorBoss_C:SM_Sword_GEN_VARIABLE",
    )
    report["socket_checks"]["warrior_boss_trace"] = {
        "mesh_component": path(warrior_weapon),
        "mesh_asset": path(prop(warrior_weapon, "static_mesh")),
        "base_socket": "WeaponBase",
        "tip_socket": "WeaponTip",
        "base_exists": bool(warrior_weapon and warrior_weapon.does_socket_exist("WeaponBase")),
        "tip_exists": bool(warrior_weapon and warrior_weapon.does_socket_exist("WeaponTip")),
    }
except Exception as exc:
    report["socket_checks"]["error"] = str(exc)


try:
    probe_bp = unreal.EditorAssetLibrary.load_asset(
        "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter"
    )
    probe_graphs = unreal.BlueprintEditorLibrary.list_graphs(probe_bp)
    report["api_probe"]["unreal_graph_blueprint_names"] = sorted(
        [name for name in dir(unreal) if "graph" in name.lower() or "blueprint" in name.lower()]
    )
    report["api_probe"]["blueprint_editor_library"] = sorted(
        [name for name in dir(unreal.BlueprintEditorLibrary) if not name.startswith("_")]
    )
    for api_name in (
        "BlueprintGraphEditor", "BlueprintGraphPin", "BlueprintGraphPinLibrary",
        "SubobjectDataBlueprintFunctionLibrary", "EdGraph", "EdGraphNode",
    ):
        api_type = getattr(unreal, api_name, None)
        if api_type:
            report["api_probe"][api_name] = sorted(
                [name for name in dir(api_type) if not name.startswith("_")]
            )
    report["api_probe"]["graph_count"] = len(probe_graphs)
    if probe_graphs:
        probe_graph = probe_graphs[0]
        report["api_probe"]["graph_name"] = probe_graph.get_name()
        report["api_probe"]["graph_class"] = path(probe_graph.get_class())
        report["api_probe"]["graph_dir"] = sorted(
            [name for name in dir(probe_graph) if not name.startswith("_")]
        )
        report["api_probe"]["graph_nodes_property"] = str(prop(probe_graph, "nodes", "<missing>"))
except Exception as exc:
    report["api_probe"]["error"] = str(exc)


for effect_path in sorted(effect_paths):
    package_name = effect_path.split(".")[0]
    try:
        options = unreal.AssetRegistryDependencyOptions()
        for option_name in (
            "include_soft_package_references", "include_hard_package_references",
            "include_searchable_names", "include_soft_management_references",
            "include_hard_management_references",
        ):
            try:
                options.set_editor_property(option_name, True)
            except Exception:
                pass
        refs = registry.get_referencers(package_name, options)
        report["referencers"][effect_path] = sorted([str(x) for x in refs])
    except Exception as exc:
        report["referencers"][effect_path] = ["<error: %s>" % exc]


for data in all_assets:
    object_path = asset_object_path(data)
    if "AnimMontage" not in str(data.asset_class_path):
        continue
    if not object_path.startswith(("/Game/Animations/", "/Game/Characters/")):
        continue
    try:
        montage = data.get_asset()
        for index, notify_event in enumerate(prop(montage, "notifies", []) or []):
            notify_state = prop(notify_event, "notify_state_class")
            if notify_state and "WeaponTraceNotifyState" in path(notify_state.get_class()):
                report["weapon_trace_notifies"].append({
                    "montage": object_path,
                    "index": index,
                    "notify": path(notify_state),
                    "use_radius_override": prop(notify_state, "use_trace_radius_override"),
                    "override_trace_radius": prop(notify_state, "override_trace_radius"),
                })
    except Exception as exc:
        report["errors"].append({"asset": object_path, "error": str(exc)})


full_graph_targets = set(effect_paths)
full_graph_targets.update((
    "/Game/Blueprints/Characters/BP_PantheliaEnemy.BP_PantheliaEnemy",
    "/Game/Blueprints/Characters/TrainingDummy/BP_TrainingDummy.BP_TrainingDummy",
    "/Game/Characters/Shaman/BP_Shaman.BP_Shaman",
    "/Game/Characters/TestBoss/BP_Boss.BP_Boss",
    "/Game/Characters/WarriorBoss/BP_WarriorBoss.BP_WarriorBoss",
    "/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter.BP_ThirdPersonCharacter",
    "/Game/UI/HUD/BP_PantheliaHUD.BP_PantheliaHUD",
))
for target_path in sorted(full_graph_targets):
    try:
        bp = unreal.EditorAssetLibrary.load_asset(target_path)
        graph_items = []
        for graph in unreal.BlueprintEditorLibrary.list_graphs(bp):
            graph_name = graph.get_name()
            try:
                editor = unreal.BlueprintGraphEditor.get_graph_editor_by_name(bp, graph_name)
                nodes = editor.list_all_nodes()
            except Exception:
                nodes = []
            for node in nodes:
                try:
                    pins = unreal.BlueprintEditorLibrary.list_all_pins(node)
                except Exception:
                    pins = []
                graph_items.append({
                    "graph": graph_name,
                    "title": node_title(node),
                    "class": path(node.get_class()),
                    "pins": [pin_record(pin) for pin in pins],
                })
        report["target_full_graphs"][target_path] = graph_items
    except Exception as exc:
        report["target_full_graphs"][target_path] = [{"error": str(exc)}]


report["dirty_after"] = dirty_packages()
with open(OUTPUT, "w", encoding="utf-8") as handle:
    json.dump(report, handle, ensure_ascii=False, indent=2)

unreal.log("BLOCK5B_AUDIT_OUTPUT=" + OUTPUT)
unreal.log("BLOCK5B_DIRTY_BEFORE=" + json.dumps(report["dirty_before"], ensure_ascii=False))
unreal.log("BLOCK5B_DIRTY_AFTER=" + json.dumps(report["dirty_after"], ensure_ascii=False))
