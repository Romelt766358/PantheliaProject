import unreal


EXPECTED_LEVEL = "/Game/ThirdPerson/Lvl_ThirdPerson"
HEALTH_POTION_CLASS = "/Game/Blueprints/Actor/BP_HealthPotion.BP_HealthPotion_C"


world = unreal.EditorLevelLibrary.get_editor_world()
if not world:
    raise RuntimeError("No editor world is loaded")

world_package = world.get_outermost().get_name()
if world_package != EXPECTED_LEVEL:
    raise RuntimeError(
        "Refusing to edit unexpected level: %s" % world_package
    )

actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
targets = [
    actor
    for actor in actor_subsystem.get_all_level_actors()
    if actor.get_class().get_path_name() == HEALTH_POTION_CLASS
]

if len(targets) != 1:
    raise RuntimeError(
        "Expected exactly one BP_HealthPotion instance, found %d" % len(targets)
    )

target_path = targets[0].get_path_name()
if not actor_subsystem.destroy_actor(targets[0]):
    raise RuntimeError("EditorActorSubsystem failed to destroy %s" % target_path)

if not unreal.EditorLevelLibrary.save_current_level():
    raise RuntimeError("Failed to save current level %s" % EXPECTED_LEVEL)

unreal.log("BLOCK5C_REMOVED_HEALTH_POTION=" + target_path)
unreal.log("BLOCK5C_SAVED_LEVEL=" + EXPECTED_LEVEL)
