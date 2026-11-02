"""
Mise en scène des aperçus (Showcase des sets d'arme, 2026-11-02) : valeurs de départ = la pose de jeu actuelle des mains
(réglage de l'arme, sinon celui du personnage) pour que l'aperçu ne change pas avant les retouches de Paul.
Ne touche qu'aux sets encore aux valeurs par défaut (une retouche n'est jamais écrasée). Sets de NME : ignorés.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_showcase.py"
Rapport : Saved/Logs/setup_showcase.txt
"""
import os
import unreal

SET_DIR = "/Game/Core/WeaponSets"
REFERENCE_CHARACTER = "/Game/Placeable/Character/Player/PlayerCharacter_mouss"
DEFAULT_RIGHT = unreal.Vector(0.0, 55.0, -10.0)
DEFAULT_LEFT = unreal.Vector(0.0, -55.0, -10.0)

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_showcase] {msg}")
    report.append(msg)


def same(a, b):
    return abs(a.x - b.x) < 0.01 and abs(a.y - b.y) < 0.01 and abs(a.z - b.z) < 0.01


def hand_pose(weapon_class, character):
    """Pose de jeu de la main droite (l'arme peut l'imposer)"""
    location = character.get_editor_property("hand_location")
    rotation = character.get_editor_property("hand_rotation")
    if weapon_class:
        weapon = unreal.get_default_object(weapon_class)
        if weapon.get_editor_property("override_hand_location"):
            location = weapon.get_editor_property("hand_location")
        if weapon.get_editor_property("override_hand_rotation"):
            rotation = weapon.get_editor_property("hand_rotation")
    return location, rotation


try:
    character = unreal.get_default_object(assets.load_blueprint_class(REFERENCE_CHARACTER))
    for path in assets.list_assets(SET_DIR, recursive=False):
        name = path.split("/")[-1].split(".")[0]
        if "_NME_" in name:
            continue
        weapon_set = assets.load_asset(path)
        if not same(weapon_set.get_editor_property("showcase_right_hand_location"), DEFAULT_RIGHT) \
                or not same(weapon_set.get_editor_property("showcase_left_hand_location"), DEFAULT_LEFT):
            log(f"{name} : déjà retouché, gardé")
            continue

        right_location, right_rotation = hand_pose(weapon_set.get_editor_property("right_hand_weapon"), character)
        left_location, left_rotation = hand_pose(weapon_set.get_editor_property("left_hand_weapon"), character)
        weapon_set.set_editor_property("showcase_right_hand_location", right_location)
        weapon_set.set_editor_property("showcase_right_hand_rotation", right_rotation)
        # symétrie de la main gauche : Y, Yaw et Roll inversés
        weapon_set.set_editor_property("showcase_left_hand_location", unreal.Vector(left_location.x, -left_location.y, left_location.z))
        weapon_set.set_editor_property("showcase_left_hand_rotation", unreal.Rotator(roll=-left_rotation.roll, pitch=left_rotation.pitch, yaw=-left_rotation.yaw))
        assets.save_asset(path.split(".")[0], only_if_is_dirty=False)
        log(f"{name} : droite {right_location} {right_rotation}, gauche {left_location} {left_rotation}")
    log("OK")
except Exception as error:
    import traceback
    log(f"ERREUR : {error}\n{traceback.format_exc()}")

with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_showcase.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
