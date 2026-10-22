"""
Objectif du jeu (doublons) et objets interactifs :
  - /Game/Art/Props/BP_Prop                                  : parent de tous les props (YSProp)
  - /Game/Core/UI/WBP_InteractPrompt, WBP_Objective           : Widget Blueprints réglables (catégorie Layout)
  - /Game/Placeable/InteractiveObjects/BP_InteractiveObject   : parent des objets interactifs (YSInteractiveObject)
  - /Game/Placeable/InteractiveObjects/BP_InteractiveObject_Chest : coffre (primitives), 10 doublons
  - /Game/Placeable/NPC/BP_NPC_Brooker                        : Brooker (YSBrookerNPC, primitives)
  - BP_YSPlayerController : ObjectiveWidgetClass = WBP_Objective
  - FoeCharacter_Skull_Elite : DeathLootClass = coffre
  - GameMap : un Brooker et un coffre de test (s'ils n'y sont pas)
Ne recrée pas ce qui existe déjà (les réglages de Paul sont conservés).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_objective.py"
Rapport : Saved/Logs/setup_objective.txt
"""
import os
import unreal

PROPS_DIR = "/Game/Art/Props"
UI_DIR = "/Game/Core/UI"
OBJECTS_DIR = "/Game/Placeable/InteractiveObjects"
NPC_DIR = "/Game/Placeable/NPC"
PC_BP = "/Game/Core/Player/BP_YSPlayerController"
ELITE_BP = "/Game/Placeable/Character/FoeCharacter_Skull_Elite"
LEVEL_PATH = "/Game/Maps/GameMap"
MATERIALS = "/Game/Art/Materials/PH_materials"
CUBE = "/Engine/BasicShapes/Cube.Cube"
CYLINDER = "/Engine/BasicShapes/Cylinder.Cylinder"
SPHERE = "/Engine/BasicShapes/Sphere.Sphere"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_objective] {msg}")
    report.append(msg)


def get_or_create(directory, name, parent, asset_class=unreal.Blueprint, factory_class=unreal.BlueprintFactory):
    path = f"{directory}/{name}"
    created = not assets.does_asset_exist(path)
    if created:
        factory = factory_class()
        factory.set_editor_property("parent_class", parent)
        tools.create_asset(name, directory, asset_class, factory)
        log(f"créé : {path}")
    else:
        log(f"existe déjà : {path}")
    bp = assets.load_asset(path)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    return bp, path, created


def add_shape(bp, name, mesh, material, location, scale):
    """Ajoute une forme (StaticMeshComponent) sous la racine « Root » du Blueprint."""
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    root = next(h for h in handles if str(lib.get_variable_name(lib.get_data(h))) == "Root")
    params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.StaticMeshComponent, blueprint_context=bp)
    handle, fail = subsystem.add_new_subobject(params)
    if not lib.is_handle_valid(handle):
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    subsystem.rename_subobject(handle, unreal.Text(name))
    shape = lib.get_object(lib.get_data(handle))
    shape.set_static_mesh(assets.load_asset(mesh))
    shape.set_material(0, assets.load_asset(f"{MATERIALS}/{material}"))
    shape.set_editor_property("relative_location", location)
    shape.set_editor_property("relative_scale3d", scale)


def spawn_if_missing(actors, cls, location, label):
    if any(a.get_class() == cls for a in actors.get_all_level_actors()):
        log(f"{label} : déjà dans {LEVEL_PATH}")
        return False
    actor = actors.spawn_actor_from_class(cls, location)
    actor.set_actor_label(label)
    log(f"{label} placé à {location}")
    return True


try:
    # props
    _, prop_path, _ = get_or_create(PROPS_DIR, "BP_Prop", unreal.YSProp)
    assets.save_asset(prop_path, only_if_is_dirty=False)

    # widgets réglables
    _, prompt_path, _ = get_or_create(UI_DIR, "WBP_InteractPrompt", unreal.YSInteractPromptWidget, unreal.WidgetBlueprint, unreal.WidgetBlueprintFactory)
    assets.save_asset(prompt_path, only_if_is_dirty=False)
    _, objective_path, _ = get_or_create(UI_DIR, "WBP_Objective", unreal.YSObjectiveWidget, unreal.WidgetBlueprint, unreal.WidgetBlueprintFactory)
    assets.save_asset(objective_path, only_if_is_dirty=False)

    # objet interactif de base : invite = WBP_InteractPrompt
    _, object_path, _ = get_or_create(OBJECTS_DIR, "BP_InteractiveObject", unreal.YSInteractiveObject)
    object_cdo = unreal.get_default_object(assets.load_blueprint_class(object_path))
    object_cdo.set_editor_property("prompt_widget_class", assets.load_blueprint_class(prompt_path))
    assets.save_asset(object_path, only_if_is_dirty=False)

    # coffre : caisse + couvercle + serrure, 10 doublons
    chest_bp, chest_path, chest_created = get_or_create(OBJECTS_DIR, "BP_InteractiveObject_Chest", assets.load_blueprint_class(object_path))
    if chest_created:
        add_shape(chest_bp, "Body", CUBE, "m_PH_monochrome_Brown", unreal.Vector(0, 0, 18), unreal.Vector(0.5, 0.7, 0.36))
        add_shape(chest_bp, "Lid", CUBE, "m_PH_monochrome_Brown", unreal.Vector(0, 0, 43), unreal.Vector(0.54, 0.74, 0.14))
        add_shape(chest_bp, "Lock", CUBE, "m_PH_monochrome_Yellow", unreal.Vector(27, 0, 36), unreal.Vector(0.06, 0.12, 0.12))
        unreal.BlueprintEditorLibrary.compile_blueprint(chest_bp)
        chest_cdo = unreal.get_default_object(assets.load_blueprint_class(chest_path))
        chest_cdo.set_editor_property("display_name", "Coffre")
        chest_cdo.set_editor_property("doubloon_value", 10)
    assets.save_asset(chest_path, only_if_is_dirty=False)
    chest_class = assets.load_blueprint_class(chest_path)

    # Brooker : corps, tête, chapeau
    brooker_bp, brooker_path, brooker_created = get_or_create(NPC_DIR, "BP_NPC_Brooker", unreal.YSBrookerNPC)
    if brooker_created:
        add_shape(brooker_bp, "Body", CYLINDER, "m_PH_monochrome_Sand", unreal.Vector(0, 0, 80), unreal.Vector(0.84, 0.84, 1.6))
        add_shape(brooker_bp, "Head", SPHERE, "m_PH_monochrome_white", unreal.Vector(0, 0, 195), unreal.Vector(0.7, 0.7, 0.7))
        add_shape(brooker_bp, "HatBrim", CYLINDER, "m_PH_monochrome_Black", unreal.Vector(0, 0, 232), unreal.Vector(0.95, 0.95, 0.06))
        add_shape(brooker_bp, "HatTop", CYLINDER, "m_PH_monochrome_Black", unreal.Vector(0, 0, 255), unreal.Vector(0.55, 0.55, 0.42))
        unreal.BlueprintEditorLibrary.compile_blueprint(brooker_bp)
        brooker_cdo = unreal.get_default_object(assets.load_blueprint_class(brooker_path))
        brooker_cdo.set_editor_property("indicator_height", 310.0)
    assets.save_asset(brooker_path, only_if_is_dirty=False)
    brooker_class = assets.load_blueprint_class(brooker_path)

    # HUD : jauge d'objectif
    pc_cdo = unreal.get_default_object(assets.load_blueprint_class(PC_BP))
    pc_cdo.set_editor_property("objective_widget_class", assets.load_blueprint_class(objective_path))
    assets.save_asset(PC_BP, only_if_is_dirty=False)

    # l'élite laisse un coffre à sa mort
    elite_cdo = unreal.get_default_object(assets.load_blueprint_class(ELITE_BP))
    if elite_cdo.get_editor_property("death_loot_class") is None:
        elite_cdo.set_editor_property("death_loot_class", chest_class)
        assets.save_asset(ELITE_BP, only_if_is_dirty=False)

    # GameMap : un Brooker à l'est des points d'apparition, un coffre de test devant eux
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not levels.load_level(LEVEL_PATH):
        raise RuntimeError(f"impossible d'ouvrir {LEVEL_PATH}")
    changed = spawn_if_missing(actors, brooker_class, unreal.Vector(0, 4000, 100), "NPC_Brooker")
    changed |= spawn_if_missing(actors, chest_class, unreal.Vector(450, 0, 0), "Chest_Test")
    if changed:
        levels.save_current_level()

    # relecture
    check_chest = unreal.get_default_object(assets.load_blueprint_class(chest_path))
    check_elite = unreal.get_default_object(assets.load_blueprint_class(ELITE_BP))
    check_pc = unreal.get_default_object(assets.load_blueprint_class(PC_BP))
    log(f"relu : coffre {check_chest.get_editor_property('doubloon_value')} doublons, invite "
        f"{check_chest.get_editor_property('prompt_widget_class').get_name()} ; butin de l'élite "
        f"{check_elite.get_editor_property('death_loot_class').get_name()} ; jauge "
        f"{check_pc.get_editor_property('objective_widget_class').get_name()}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_objective.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
