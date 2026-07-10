import json
import os
import unreal


PROFILE = "/Game/Blueprints/AI/Bosses/WarriorBoss/DA_WarriorBoss_Profile"
BOSS_BP = "/Game/Characters/WarriorBoss/BP_WarriorBoss"
SWORD = "/Game/Characters/WarriorBoss/Mesh/SM_Sword"
MONTAGE = "/Game/Characters/WarriorBoss/Animations/AM_WarriorBoss_GapCloser"
ABILITY = "/Game/Characters/WarriorBoss/Abilities/GA_WarriorBoss_GapCloser"
GE_DAMAGE = "/Game/Blueprints/AbilitySystem/GameplayEffects/GE_Damage"


def prop(obj, name, default=None):
    try:
        return obj.get_editor_property(name)
    except Exception:
        return default


def set_prop(obj, name, value):
    obj.set_editor_property(name, value)


def set_first_prop(obj, names, value):
    last_error = None
    for name in names:
        try:
            obj.set_editor_property(name, value)
            return name
        except Exception as exc:
            last_error = exc
    raise last_error


def obj_path(obj):
    return obj.get_path_name() if obj else None


def load(path):
    asset = unreal.EditorAssetLibrary.load_asset(path)
    if not asset:
        raise RuntimeError("Failed to load " + path)
    return asset


def generated_class(bp):
    cls = prop(bp, "generated_class")
    if cls:
        return cls
    try:
        return bp.generated_class()
    except Exception:
        return None


def get_cdo(bp):
    cls = generated_class(bp)
    return unreal.get_default_object(cls) if cls else None


def get_component(cdo, name):
    for comp in cdo.get_components_by_class(unreal.ActorComponent):
        if comp.get_name() == name:
            return comp
    return None


def find_component_by_class_suffix(cdo, suffix):
    for comp in cdo.get_components_by_class(unreal.ActorComponent):
        if comp.get_class().get_name().endswith(suffix):
            return comp
    return None


def update_profile(profile, mode):
    actions = list(prop(profile, "actions", []))
    before = []
    after = []
    for action in actions:
        tag = str(prop(prop(action, "action_tag"), "tag_name", ""))
        row_before = {
            "tag": tag,
            "weight": prop(action, "weight"),
            "cooldown": prop(action, "cooldown"),
            "min": prop(action, "min_distance"),
            "max": prop(action, "max_distance"),
        }
        before.append(row_before)
        if mode == "isolate":
            if tag == "Boss.Action.GapCloser.Leap":
                set_prop(action, "weight", 1000.0)
                set_prop(action, "cooldown", 0.5)
                set_prop(action, "min_distance", 300.0)
                set_prop(action, "max_distance", 900.0)
            elif tag in ["Boss.Action.Melee.ShortSlash", "Boss.Action.Melee.Heavy", "Boss.Action.Melee.WideSlash"]:
                set_prop(action, "weight", 0.0)
        elif mode == "restore":
            if tag == "Boss.Action.GapCloser.Leap":
                set_prop(action, "weight", 80.0)
                set_prop(action, "cooldown", 3.0)
                set_prop(action, "min_distance", 300.0)
                set_prop(action, "max_distance", 900.0)
            elif tag == "Boss.Action.Melee.ShortSlash":
                set_prop(action, "weight", 60.0)
            elif tag == "Boss.Action.Melee.Heavy":
                set_prop(action, "weight", 25.0)
            elif tag == "Boss.Action.Melee.WideSlash":
                set_prop(action, "weight", 15.0)
        after.append({
            "tag": tag,
            "weight": prop(action, "weight"),
            "cooldown": prop(action, "cooldown"),
            "min": prop(action, "min_distance"),
            "max": prop(action, "max_distance"),
        })
    set_prop(profile, "actions", actions)
    return {"before": before, "after": after}


def ensure_sword_component(bp):
    cdo = get_cdo(bp)
    trace = find_component_by_class_suffix(cdo, "WeaponTraceComponent")
    sword_comp = get_component(cdo, "SM_Sword_GEN_VARIABLE")
    if not trace or not sword_comp:
        raise RuntimeError("Missing WeaponTrace or SM_Sword component")

    set_prop(trace, "weapon_mesh_component", sword_comp)

    tags = list(prop(sword_comp, "component_tags", []) or [])
    if unreal.Name("Weapon") not in tags:
        tags.append(unreal.Name("Weapon"))
        set_prop(sword_comp, "component_tags", tags)

    return {
        "trace": obj_path(trace),
        "weapon_mesh_component": obj_path(prop(trace, "weapon_mesh_component")),
        "sword_component": obj_path(sword_comp),
        "sword_tags": [str(t) for t in prop(sword_comp, "component_tags", [])],
    }


def get_static_mesh_socket_names(mesh):
    return [str(prop(socket, "socket_name")) for socket in prop(mesh, "sockets", []) or []]


def ensure_static_mesh_sockets(mesh):
    sockets = list(prop(mesh, "sockets", []) or [])
    existing = {str(prop(socket, "socket_name")): socket for socket in sockets}

    # SM_Sword local bounds from inspection:
    # X roughly centered, Y runs from tip (-101.5) to hilt/base (20.8), Z centered.
    desired = {
        "WeaponBase": unreal.Vector(0.0, 20.0, 0.0),
        "WeaponTip": unreal.Vector(0.0, -101.0, 0.0),
    }

    added = []
    for name, location in desired.items():
        socket = existing.get(name)
        if not socket:
            socket = unreal.StaticMeshSocket(mesh)
            set_prop(socket, "socket_name", unreal.Name(name))
            if hasattr(mesh, "add_socket"):
                mesh.add_socket(socket)
            else:
                sockets.append(socket)
            added.append(name)
        set_prop(socket, "relative_location", location)
        set_prop(socket, "relative_rotation", unreal.Rotator(0.0, 0.0, 0.0))
        set_prop(socket, "relative_scale", unreal.Vector(1.0, 1.0, 1.0))

    if added and not hasattr(mesh, "add_socket"):
        set_prop(mesh, "sockets", sockets)
    mesh.modify()
    try:
        mesh.post_edit_change()
    except Exception:
        pass
    return {"added": added, "sockets": get_static_mesh_socket_names(mesh)}


def ensure_gapcloser_notify(montage):
    notify_cls = unreal.load_class(None, "/Script/PantheliaProject.WeaponTraceNotifyState")
    if not notify_cls:
        raise RuntimeError("Could not load WeaponTraceNotifyState class")

    notifies = list(prop(montage, "notifies", []) or [])
    existing = []
    for event in notifies:
        state = prop(event, "notify_state_class")
        if state and state.get_class() == notify_cls:
            existing.append((event, state))

    if existing:
        event, state = existing[0]
        action = "updated"
    else:
        track_name = unreal.Name("WeaponTrace")
        if not unreal.AnimationLibrary.is_valid_anim_notify_track_name(montage, track_name):
            unreal.AnimationLibrary.add_animation_notify_track(montage, track_name)
        state = unreal.AnimationLibrary.add_animation_notify_state_event(montage, track_name, 0.28, 0.24, notify_cls)
        event = None
        action = "added"

    radius_bool_prop = set_first_prop(state, ["bUseTraceRadiusOverride", "b_use_trace_radius_override"], True)
    radius_prop = set_first_prop(state, ["OverrideTraceRadius", "override_trace_radius"], 40.0)

    # Keep the timing deterministic even if the event existed at a different range.
    if event:
        try:
            event.set_editor_property("link_value", 0.28)
            event.set_editor_property("duration", 0.24)
        except Exception:
            pass

    montage.modify()
    return {
        "action": action,
        "count_before": len(notifies),
        "count_after": len(list(prop(montage, "notifies", []) or [])),
        "state": obj_path(state),
        "radius_bool_prop": radius_bool_prop,
        "radius_prop": radius_prop,
        "bUseTraceRadiusOverride": prop(state, "bUseTraceRadiusOverride"),
        "OverrideTraceRadius": prop(state, "OverrideTraceRadius"),
    }


def update_gapcloser_ability(ability_bp):
    cdo = get_cdo(ability_bp)
    ge_bp = load(GE_DAMAGE)
    ge_class = generated_class(ge_bp) if hasattr(ge_bp, "get_editor_property") else None
    if ge_class:
        set_first_prop(cdo, ["DamageEffectClass", "damage_effect_class"], ge_class)

    montage_prop_name = None
    montages = None
    for candidate in ["GapCloserMontages", "gap_closer_montages"]:
        value = prop(cdo, candidate, None)
        if value is not None:
            montage_prop_name = candidate
            montages = list(value or [])
            break
    if montages is None:
        return {
            "damage_effect_class": str(prop(cdo, "DamageEffectClass", prop(cdo, "damage_effect_class"))),
            "gap_closer_montages": "not exposed on CDO",
        }

    tag = unreal.GameplayTag()
    try:
        tag.set_editor_property("tag_name", unreal.Name("Montage.Attack.Weapon"))
    except Exception:
        tag = None
    for entry in montages:
        montage_asset = prop(entry, "montage")
        if tag and montage_asset and montage_asset.get_path_name().startswith(MONTAGE + "."):
            set_prop(entry, "montage_tag", tag)
    set_prop(cdo, montage_prop_name, montages)
    ability_bp.modify()
    return {
        "damage_effect_class": str(prop(cdo, "DamageEffectClass", prop(cdo, "damage_effect_class"))),
        "gap_closer_montages": str(prop(cdo, montage_prop_name)),
    }


def main():
    results = {}
    profile = load(PROFILE)
    bp = load(BOSS_BP)
    sword = load(SWORD)
    montage = load(MONTAGE)
    ability = load(ABILITY)

    mode = os.environ.get("CODEX_GAPCLOSER_PROFILE_MODE", "skip")
    if mode in ("isolate", "restore"):
        results["profile"] = update_profile(profile, mode)
    else:
        results["profile"] = {"skipped": True}
    results["boss_bp"] = {"skipped": True}
    results["sword"] = ensure_static_mesh_sockets(sword)
    results["montage"] = ensure_gapcloser_notify(montage)
    results["ability"] = update_gapcloser_ability(ability)

    results["compile"] = {"skipped": "KismetEditorUtilities is not exposed in this commandlet"}

    saved = []
    for asset in [profile, bp, sword, montage, ability]:
        unreal.EditorAssetLibrary.save_loaded_asset(asset)
        saved.append(asset.get_path_name())
    results["saved"] = saved

    text = json.dumps(results, indent=2, default=str)
    print(text)
    with open(os.path.join(unreal.Paths.project_saved_dir(), "CodexGapCloserFix.json"), "w", encoding="utf-8") as f:
        f.write(text)


main()
