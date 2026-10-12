"""
Crée (ou met à jour) les assets de la boucle de test multijoueur :
  - /Game/Placeable/Character/PlayerCharacter_mouss  (Blueprint, parent AYSPlayerCharacter)
  - /Game/Core/GameModes/BP_TestGameMode               (Blueprint GameMode, pawn = PlayerCharacter_mouss)
  - /Game/Maps/TestMaps/TestGround                     (Level : sol grille, lumière, 4 PlayerStart)

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_testground.py"
Le script ferme l'éditeur à la fin et écrit un rapport dans Saved/Logs/setup_testground.txt.
"""
import os
import unreal

CHAR_DIR, CHAR_NAME = "/Game/Placeable/Character", "PlayerCharacter_mouss"
GM_DIR, GM_NAME = "/Game/Core/GameModes", "BP_TestGameMode"
LEVEL_PATH = "/Game/Maps/TestMaps/TestGround"
GRID_MATERIAL = "/Engine/EngineMaterials/WorldGridMaterial"
PLANE_MESH = "/Engine/BasicShapes/Plane"

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_testground] {msg}")
    report.append(msg)


def get_or_create_blueprint(folder, name, parent_class):
    path = f"{folder}/{name}"
    if assets.does_asset_exist(path):
        log(f"existe déjà : {path}")
        return assets.load_asset(path)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, folder, unreal.Blueprint, factory)
    log(f"créé : {path}")
    return bp


def build_blueprints():
    char_bp = get_or_create_blueprint(CHAR_DIR, CHAR_NAME, unreal.YSPlayerCharacter)
    unreal.BlueprintEditorLibrary.compile_blueprint(char_bp)
    assets.save_asset(f"{CHAR_DIR}/{CHAR_NAME}", only_if_is_dirty=False)
    char_class = assets.load_blueprint_class(f"{CHAR_DIR}/{CHAR_NAME}")

    gm_bp = get_or_create_blueprint(GM_DIR, GM_NAME, unreal.GameModeBase)
    unreal.BlueprintEditorLibrary.compile_blueprint(gm_bp)
    gm_class = assets.load_blueprint_class(f"{GM_DIR}/{GM_NAME}")
    unreal.get_default_object(gm_class).set_editor_property("default_pawn_class", char_class)
    assets.save_asset(f"{GM_DIR}/{GM_NAME}", only_if_is_dirty=False)
    log(f"GameMode : DefaultPawnClass = {char_class.get_name()}")
    return gm_class


def spawn(actor_class, location, rotation=unreal.Rotator(0, 0, 0), label=None):
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    actor = actors.spawn_actor_from_class(actor_class, location, rotation)
    if label:
        actor.set_actor_label(label)
    return actor


def build_level(gm_class):
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if assets.does_asset_exist(LEVEL_PATH):
        log(f"level existe déjà, non modifié : {LEVEL_PATH}")
        return
    if not levels.new_level(LEVEL_PATH):
        log(f"ERREUR : impossible de créer {LEVEL_PATH}")
        return

    # sol : plane 100 m x 100 m avec la grille de prototypage
    ground = spawn(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), label="Ground")
    mesh_comp = ground.static_mesh_component
    mesh_comp.set_static_mesh(assets.load_asset(PLANE_MESH))
    mesh_comp.set_material(0, assets.load_asset(GRID_MATERIAL))
    ground.set_actor_scale3d(unreal.Vector(100, 100, 1))

    # éclairage minimal (sans lui, la scène est noire)
    sun = spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(0, -50, 30), "Sun")
    sun.light_component.set_editor_property("atmosphere_sun_light", True)
    sky = spawn(unreal.SkyLight, unreal.Vector(0, 0, 800), label="SkyLight")
    sky.light_component.set_editor_property("real_time_capture", True)
    spawn(unreal.SkyAtmosphere, unreal.Vector(0, 0, 0), label="SkyAtmosphere")

    # un point d'apparition par joueur (4 max)
    for i, (x, y) in enumerate([(-200, -200), (-200, 200), (200, -200), (200, 200)]):
        spawn(unreal.PlayerStart, unreal.Vector(x, y, 100), label=f"PlayerStart_{i + 1}")

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    world.get_world_settings().set_editor_property("default_game_mode", gm_class)

    levels.save_current_level()
    log(f"level créé : {LEVEL_PATH} (GameMode override = {GM_NAME})")


try:
    build_level(build_blueprints())
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_testground.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
