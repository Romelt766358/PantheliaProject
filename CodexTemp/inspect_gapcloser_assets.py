import json
import os
import unreal


def obj_path(obj):
    return obj.get_path_name() if obj else None


def tag_to_string(tag):
    try:
        return str(tag)
    except Exception:
        return None


def get_prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def generated_class(bp):
    cls = get_prop(bp, "generated_class")
    if cls:
        return cls
    try:
        return bp.generated_class()
    except Exception:
        return None


def dump_notify_event(event):
    notify = get_prop(event, "notify")
    notify_state = get_prop(event, "notify_state_class")
    linked = get_prop(event, "link_value", None)
    duration = get_prop(event, "duration", None)
    trigger_time_offset = get_prop(event, "trigger_time_offset", None)
    end_trigger_time_offset = get_prop(event, "end_trigger_time_offset", None)
    out = {
        "event_name": str(get_prop(event, "notify_name", "")),
        "time": linked,
        "duration": duration,
        "trigger_time_offset": trigger_time_offset,
        "end_trigger_time_offset": end_trigger_time_offset,
        "notify_class": notify.get_class().get_name() if notify else None,
        "notify_state_class": notify_state.get_class().get_name() if notify_state else None,
        "notify_state_path": obj_path(notify_state),
    }
    if notify_state:
        for name in [
            "bUseTraceRadiusOverride",
            "override_trace_radius",
            "OverrideTraceRadius",
            "montage_tag",
            "MontageTag",
            "socket_base",
            "SocketBase",
            "socket_tip",
            "SocketTip",
        ]:
            value = get_prop(notify_state, name, None)
            if value is not None:
                out[name] = tag_to_string(value)
    return out


def run():
    paths = {
        "profile": "/Game/Blueprints/AI/Bosses/WarriorBoss/DA_WarriorBoss_Profile",
        "boss_bp": "/Game/Characters/WarriorBoss/BP_WarriorBoss",
        "montage": "/Game/Characters/WarriorBoss/Animations/AM_WarriorBoss_GapCloser",
        "ability": "/Game/Characters/WarriorBoss/Abilities/GA_WarriorBoss_GapCloser",
        "sword": "/Game/Characters/WarriorBoss/Mesh/SM_Sword",
        "eqs_pawn": "/Game/Blueprints/AI/EQS/BP_EQSTestingPawn",
    }
    out = {}

    montage = unreal.EditorAssetLibrary.load_asset(paths["montage"])
    out["montage_class"] = montage.get_class().get_name() if montage else None
    out["montage_length"] = get_prop(montage, "sequence_length")
    out["montage_notifies"] = []
    if montage:
        for event in get_prop(montage, "notifies", []) or []:
            out["montage_notifies"].append(dump_notify_event(event))

    bp = unreal.EditorAssetLibrary.load_asset(paths["boss_bp"])
    out["bp_class"] = bp.get_class().get_name() if bp else None
    bp_class = generated_class(bp) if bp else None
    cdo = unreal.get_default_object(bp_class) if bp_class else None
    components = []
    if cdo:
        for comp in cdo.get_components_by_class(unreal.ActorComponent):
            info = {
                "name": comp.get_name(),
                "class": comp.get_class().get_name(),
                "tags": [str(t) for t in get_prop(comp, "component_tags", []) or []],
            }
            if comp.get_class().get_name().endswith("WeaponTraceComponent"):
                info["weapon_mesh_component"] = obj_path(get_prop(comp, "weapon_mesh_component"))
                info["trace_radius"] = get_prop(comp, "trace_radius")
                info["base_socket"] = str(get_prop(comp, "weapon_base_socket_name"))
                info["tip_socket"] = str(get_prop(comp, "weapon_tip_socket_name"))
                info["debug"] = get_prop(comp, "b_debug_mode")
                info["log_trace_debug"] = get_prop(comp, "b_log_trace_debug")
            if isinstance(comp, unreal.StaticMeshComponent):
                info["static_mesh"] = obj_path(comp.static_mesh)
            components.append(info)
    out["boss_components"] = components
    scs_components = []
    if bp:
        scs = get_prop(bp, "simple_construction_script")
        nodes = []
        if scs:
            try:
                nodes = scs.get_all_nodes()
            except Exception:
                nodes = []
        for node in nodes or []:
            template = None
            try:
                template = node.get_editor_property("component_template")
            except Exception:
                pass
            info = {
                "node_name": str(get_prop(node, "variable_name", "")),
                "template_path": obj_path(template),
                "class": template.get_class().get_name() if template else None,
                "tags": [str(t) for t in get_prop(template, "component_tags", []) or []] if template else [],
            }
            if template and template.get_class().get_name().endswith("WeaponTraceComponent"):
                info["weapon_mesh_component"] = obj_path(get_prop(template, "weapon_mesh_component"))
                info["trace_radius"] = get_prop(template, "trace_radius")
                info["base_socket"] = str(get_prop(template, "weapon_base_socket_name"))
                info["tip_socket"] = str(get_prop(template, "weapon_tip_socket_name"))
                info["debug"] = get_prop(template, "b_debug_mode")
                info["log_trace_debug"] = get_prop(template, "b_log_trace_debug")
            if template and isinstance(template, unreal.StaticMeshComponent):
                info["static_mesh"] = obj_path(template.static_mesh)
            scs_components.append(info)
    out["boss_scs_components"] = scs_components

    sword = unreal.EditorAssetLibrary.load_asset(paths["sword"])
    out["sword_class"] = sword.get_class().get_name() if sword else None
    sword_sockets = []
    if sword:
        for socket in get_prop(sword, "sockets", []) or []:
            name = get_prop(socket, "socket_name", None)
            sword_sockets.append(str(name))
    out["sword_sockets"] = sword_sockets

    ability_bp = unreal.EditorAssetLibrary.load_asset(paths["ability"])
    out["ability_class"] = ability_bp.get_class().get_name() if ability_bp else None
    ability_class = generated_class(ability_bp) if ability_bp else None
    if ability_class:
        ability_cdo = unreal.get_default_object(ability_class)
        ability_props = {}
        for prop in [
            "damage_effect_class",
            "DamageEffectClass",
            "gap_closer_montages",
            "GapCloserMontages",
            "startup_montage",
            "StartupMontage",
            "attack_montage",
            "AttackMontage",
        ]:
            val = get_prop(ability_cdo, prop, None)
            if val is not None:
                ability_props[prop] = str(val)
        out["ability_cdo_props"] = ability_props

    eqs_bp = unreal.EditorAssetLibrary.load_asset(paths["eqs_pawn"])
    out["eqs_parent"] = eqs_bp.parent_class.get_path_name() if eqs_bp and hasattr(eqs_bp, "parent_class") else None
    eqs_class = generated_class(eqs_bp) if eqs_bp else None
    if eqs_class:
        eqs_cdo = unreal.get_default_object(eqs_class)
        out["eqs_components"] = [
            {"name": c.get_name(), "class": c.get_class().get_name()}
            for c in eqs_cdo.get_components_by_class(unreal.ActorComponent)
        ]

    text = json.dumps(out, indent=2)
    print(text)
    output_path = os.path.join(unreal.Paths.project_saved_dir(), "CodexGapCloserInspect.json")
    with open(output_path, "w", encoding="utf-8") as f:
        f.write(text)


run()
