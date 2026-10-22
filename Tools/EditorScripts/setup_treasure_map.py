"""
Carte au trésor :
  - /Game/Placeable/InteractiveObjects/BP_InteractiveObject_TreasureChest : coffre au trésor (enfant du coffre), 30 doublons
  - /Game/Placeable/InteractiveObjects/BP_InteractiveObject_TreasureMap   : carte (YSTreasureMap, primitives), récompense = coffre au trésor
  - BP_TestGameMode : TreasureMapClass = la carte, zone d'apparition = sol de GameMap (moins une marge)
Ne recrée pas ce qui existe déjà (les réglages de Paul sont conservés).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_treasure_map.py"
Rapport : Saved/Logs/setup_treasure_map.txt
"""
import os
import unreal

OBJECTS_DIR = "/Game/Placeable/InteractiveObjects"
CHEST_BP = f"{OBJECTS_DIR}/BP_InteractiveObject_Chest"
PROMPT_WBP = "/Game/Core/UI/WBP_InteractPrompt"
GAME_MODE_BP = "/Game/Core/GameModes/BP_TestGameMode"
LEVEL_PATH = "/Game/Maps/GameMap"
MATERIALS = "/Game/Art/Materials/PH_materials"
CUBE = "/Engine/BasicShapes/Cube.Cube"
CYLINDER = "/Engine/BasicShapes/Cylinder.Cylinder"
AREA_MARGIN = 500.0

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_treasure_map] {msg}")
    report.append(msg)


def get_or_create(directory, name, parent):
    path = f"{directory}/{name}"
    created = not assets.does_asset_exist(path)
    if created:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        tools.create_asset(name, directory, unreal.Blueprint, factory)
        log(f"créé : {path}")
    else:
        log(f"existe déjà : {path}")
    bp = assets.load_asset(path)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    return bp, path, created


def add_shape(bp, name, mesh, material, location, scale, rotation=None):
    """Ajoute une forme (StaticMeshComponent) sous la racine « Root » du Blueprint (héritée ou non)."""
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
    shape.set_collision_profile_name("NoCollision")
    shape.set_editor_property("relative_location", location)
    shape.set_editor_property("relative_scale3d", scale)
    if rotation is not None:
        shape.set_editor_property("relative_rotation", rotation)


def ground_bounds():
    """Sol le plus large de GameMap : (centre, demi-largeur X, demi-largeur Y, dessus)."""
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not levels.load_level(LEVEL_PATH):
        raise RuntimeError(f"impossible d'ouvrir {LEVEL_PATH}")
    best = None
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.StaticMeshActor):
            origin, extent = actor.get_actor_bounds(False)
            if best is None or extent.x * extent.y > best[1].x * best[1].y:
                best = (origin, extent)
    if best is None:
        raise RuntimeError("aucun sol trouvé dans GameMap")
    origin, extent = best
    return origin, extent.x, extent.y, origin.z + extent.z


try:
    prompt_class = assets.load_blueprint_class(PROMPT_WBP)

    # coffre au trésor : le coffre avec des bandes dorées, 30 doublons
    treasure_bp, treasure_path, treasure_created = get_or_create(OBJECTS_DIR, "BP_InteractiveObject_TreasureChest", assets.load_blueprint_class(CHEST_BP))
    if treasure_created:
        add_shape(treasure_bp, "GoldBandFront", CUBE, "m_PH_monochrome_Yellow", unreal.Vector(0, -22, 26), unreal.Vector(0.56, 0.06, 0.5))
        add_shape(treasure_bp, "GoldBandBack", CUBE, "m_PH_monochrome_Yellow", unreal.Vector(0, 22, 26), unreal.Vector(0.56, 0.06, 0.5))
        unreal.BlueprintEditorLibrary.compile_blueprint(treasure_bp)
        treasure_cdo = unreal.get_default_object(assets.load_blueprint_class(treasure_path))
        treasure_cdo.set_editor_property("display_name", "Coffre au trésor")
        treasure_cdo.set_editor_property("doubloon_value", 30)
    assets.save_asset(treasure_path, only_if_is_dirty=False)
    treasure_class = assets.load_blueprint_class(treasure_path)

    # carte : parchemin + deux rouleaux + croix rouge ; tenue à deux mains par les rouleaux
    map_bp, map_path, map_created = get_or_create(OBJECTS_DIR, "BP_InteractiveObject_TreasureMap", unreal.YSTreasureMap)
    if map_created:
        add_shape(map_bp, "Parchment", CUBE, "m_PH_monochrome_Sand", unreal.Vector(0, 0, 3), unreal.Vector(0.4, 0.5, 0.02))
        add_shape(map_bp, "RollLeft", CYLINDER, "m_PH_monochrome_Brown", unreal.Vector(0, -26, 4), unreal.Vector(0.07, 0.07, 0.44), unreal.Rotator(roll=0, pitch=90, yaw=0))
        add_shape(map_bp, "RollRight", CYLINDER, "m_PH_monochrome_Brown", unreal.Vector(0, 26, 4), unreal.Vector(0.07, 0.07, 0.44), unreal.Rotator(roll=0, pitch=90, yaw=0))
        add_shape(map_bp, "CrossA", CUBE, "m_PH_monochrome_Red", unreal.Vector(0, 0, 4.2), unreal.Vector(0.18, 0.03, 0.005), unreal.Rotator(roll=0, pitch=0, yaw=45))
        add_shape(map_bp, "CrossB", CUBE, "m_PH_monochrome_Red", unreal.Vector(0, 0, 4.2), unreal.Vector(0.18, 0.03, 0.005), unreal.Rotator(roll=0, pitch=0, yaw=-45))
        unreal.BlueprintEditorLibrary.compile_blueprint(map_bp)
        map_cdo = unreal.get_default_object(assets.load_blueprint_class(map_path))
        map_cdo.set_editor_property("prompt_widget_class", prompt_class)
        map_cdo.set_editor_property("carry_location", unreal.Vector(60, 0, -5))
        map_cdo.set_editor_property("carry_rotation", unreal.Rotator(roll=0, pitch=25, yaw=0))
        map_cdo.get_editor_property("LeftHandPoint").set_editor_property("relative_location", unreal.Vector(0, -30, 4))
        map_cdo.get_editor_property("RightHandPoint").set_editor_property("relative_location", unreal.Vector(0, 30, 4))
        map_cdo.get_editor_property("InteractPrompt").set_editor_property("relative_location", unreal.Vector(0, 0, 80))
    map_cdo = unreal.get_default_object(assets.load_blueprint_class(map_path))
    if map_cdo.get_editor_property("reward_class") is None:
        map_cdo.set_editor_property("reward_class", treasure_class)
    assets.save_asset(map_path, only_if_is_dirty=False)
    map_class = assets.load_blueprint_class(map_path)

    # GameMode : une carte par jour, dans la zone du sol de GameMap
    center, half_x, half_y, top = ground_bounds()
    gm_cdo = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP))
    if gm_cdo.get_editor_property("treasure_map_class") is None:
        gm_cdo.set_editor_property("treasure_map_class", map_class)
        gm_cdo.set_editor_property("treasure_map_area_center", unreal.Vector(center.x, center.y, top))
        gm_cdo.set_editor_property("treasure_map_area_extent", unreal.Vector2D(max(half_x - AREA_MARGIN, 0.0), max(half_y - AREA_MARGIN, 0.0)))
        assets.save_asset(GAME_MODE_BP, only_if_is_dirty=False)
        log("BP_TestGameMode : carte au trésor réglée")

    # relecture
    check_gm = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP))
    check_map = unreal.get_default_object(assets.load_blueprint_class(map_path))
    check_chest = unreal.get_default_object(treasure_class)
    log(f"relu : GameMode carte {check_gm.get_editor_property('treasure_map_class').get_name()}, zone "
        f"{check_gm.get_editor_property('treasure_map_area_center')} ± {check_gm.get_editor_property('treasure_map_area_extent')} ; "
        f"récompense {check_map.get_editor_property('reward_class').get_name()} ({check_chest.get_editor_property('doubloon_value')} doublons)")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_treasure_map.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
