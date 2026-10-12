"""
Met en place le Coup d'Petoir :
  - socket PistolSocket_L (os hand_l) sur le squelette du mannequin
  - /Game/Placeable/AMMO/PetoirAMMO : Blueprint du projectile (parent AYSProjectile)
  - PlayerCharacter_mouss.CoupDPetoir.ProjectileClass = PetoirAMMO

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_attack.py"
Rapport : Saved/Logs/setup_attack.txt
"""
import os
import unreal

SKELETON = "/Game/PresetProject/Characters/Mannequins/Meshes/SK_Mannequin"
SOCKET_NAME, SOCKET_BONE = "PistolSocket_L", "hand_l"
AMMO_DIR, AMMO_NAME = "/Game/Placeable/AMMO", "PetoirAMMO"
PLAYER_BP = "/Game/Placeable/Character/PlayerCharacter_mouss"
SHAPE_MATERIAL = "/Engine/BasicShapes/BasicShapeMaterial"

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_attack] {msg}")
    report.append(msg)


def load(path):
    asset = assets.load_asset(path)
    if asset is None:
        raise RuntimeError(f"asset introuvable : {path}")
    return asset


def add_socket():
    skeleton = load(SKELETON)
    ok = unreal.YSEditorScriptLibrary.set_skeleton_socket(
        skeleton, SOCKET_NAME, SOCKET_BONE, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    if not ok:
        raise RuntimeError(f"os {SOCKET_BONE} introuvable sur {SKELETON}")
    assets.save_asset(SKELETON, only_if_is_dirty=False)
    log(f"socket {SOCKET_NAME} sur {SOCKET_BONE}")


def create_ammo_blueprint():
    path = f"{AMMO_DIR}/{AMMO_NAME}"
    if assets.does_asset_exist(path):
        log(f"existe déjà : {path}")
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.YSProjectile)
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(AMMO_NAME, AMMO_DIR, unreal.Blueprint, factory)
        log(f"créé : {path}")
    unreal.BlueprintEditorLibrary.compile_blueprint(load(path))
    assets.save_asset(path, only_if_is_dirty=False)
    return assets.load_blueprint_class(path)


def link_projectile(ammo_class):
    bp = load(PLAYER_BP)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(assets.load_blueprint_class(PLAYER_BP))
    ability = cdo.get_editor_property("CoupDPetoir")
    ability.set_editor_property("projectile_class", ammo_class)
    assets.save_asset(PLAYER_BP, only_if_is_dirty=False)
    log(f"CoupDPetoir.ProjectileClass = {ammo_class.get_name()}")


def check_shape_material():
    names = [str(n) for n in unreal.MaterialEditingLibrary.get_vector_parameter_names(load(SHAPE_MATERIAL))]
    log(f"paramètres couleur de BasicShapeMaterial : {names}")


try:
    check_shape_material()
    add_socket()
    link_projectile(create_ammo_blueprint())
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_attack.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
