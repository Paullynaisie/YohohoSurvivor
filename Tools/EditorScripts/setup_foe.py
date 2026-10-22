"""
Crée le Blueprint d'ennemi de base et en place quelques-uns dans GameMap pour les tests :
  - /Game/Placeable/Character/FoeCharacter_Skull  (Blueprint, parent AYSFoeCharacter)
  - 3 FoeCharacter_Skull dans /Game/Maps/GameMap (seulement s'il n'y en a aucun)

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_foe.py"
Rapport : Saved/Logs/setup_foe.txt
"""
import os
import unreal

FOE_DIR, FOE_NAME = "/Game/Placeable/Character", "FoeCharacter_Skull"
LEVEL_PATH = "/Game/Maps/GameMap"
TEST_SPAWNS = [(1500, 0), (-1200, 1200), (0, -1800)]
MESH_MATERIAL = "/Game/PresetProject/Characters/Mannequins/Materials/Manny/MI_Manny_01_New"

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_foe] {msg}")
    report.append(msg)


def create_foe_blueprint():
    path = f"{FOE_DIR}/{FOE_NAME}"
    if assets.does_asset_exist(path):
        log(f"existe déjà : {path}")
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.YSFoeCharacter)
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(FOE_NAME, FOE_DIR, unreal.Blueprint, factory)
        log(f"créé : {path}")
    bp = assets.load_asset(path)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(path, only_if_is_dirty=False)
    return assets.load_blueprint_class(path)


def check_color_parameter():
    material = assets.load_asset(MESH_MATERIAL)
    names = [str(n) for n in unreal.MaterialEditingLibrary.get_vector_parameter_names(material)]
    log(f"paramètres couleur de {MESH_MATERIAL} : {names}")


def place_test_foes(foe_class):
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not levels.load_level(LEVEL_PATH):
        log(f"ERREUR : impossible d'ouvrir {LEVEL_PATH}")
        return
    existing = [a for a in actors.get_all_level_actors() if a.get_class() == foe_class]
    if existing:
        log(f"{len(existing)} ennemis déjà présents, rien ajouté")
        return
    for i, (x, y) in enumerate(TEST_SPAWNS):
        foe = actors.spawn_actor_from_class(foe_class, unreal.Vector(x, y, 100))
        foe.set_actor_label(f"{FOE_NAME}_{i + 1}")
    levels.save_current_level()
    log(f"{len(TEST_SPAWNS)} ennemis placés dans {LEVEL_PATH}")


try:
    check_color_parameter()
    place_test_foes(create_foe_blueprint())
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_foe.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
