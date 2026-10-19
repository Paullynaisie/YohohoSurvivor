"""
Applique les poses du Auto swing demandées par Paul dans BP_Epee (main droite ; la gauche est symétrique) :
  - préparation : (-20, 90, 0), rotation X -90 / Y 180 / Z -115
  - fin de coupe : (40, -65, 0), rotation X -90 / Y 180 / Z -400 (arc de cercle)
Option : argument « left=<chemin BP> » pour changer l'arme de la main gauche du joueur (tests akimbo).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/set_sabre_poses.py [left=/Game/...]"
Rapport : Saved/Logs/set_sabre_poses.txt
"""
import os
import sys
import unreal

SWORD_BP = "/Game/Placeable/Weapons/BP_Epee"
PLAYER_BP = "/Game/Placeable/Character/PlayerCharacter_mouss"

assets = unreal.EditorAssetLibrary
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[set_sabre_poses] {msg}")
    report.append(msg)


try:
    sword = assets.load_asset(SWORD_BP)
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    for handle in sds.k2_gather_subobject_data_for_blueprint(sword):
        sabre = lib.get_object(lib.get_data(handle))
        if isinstance(sabre, unreal.YSAutoSwingAbility):
            sabre.set_editor_property("windup_hand_location", unreal.Vector(-20, 90, 0))
            sabre.set_editor_property("windup_hand_rotation", unreal.Rotator(roll=-90, pitch=180, yaw=-115))
            sabre.set_editor_property("swing_end_hand_location", unreal.Vector(40, -65, 0))
            sabre.set_editor_property("swing_end_hand_rotation", unreal.Rotator(roll=-90, pitch=180, yaw=-400))
            sabre.set_editor_property("bCircularSwing", True)
            log("poses du AutoSwing mises à jour")
    unreal.BlueprintEditorLibrary.compile_blueprint(sword)
    assets.save_asset(SWORD_BP, only_if_is_dirty=False)

    left = next((a.split("=", 1)[1] for a in sys.argv[1:] if a.startswith("left=")), None)
    if left:
        player = assets.load_asset(PLAYER_BP)
        unreal.BlueprintEditorLibrary.compile_blueprint(player)
        cdo = unreal.get_default_object(assets.load_blueprint_class(PLAYER_BP))
        cdo.set_editor_property("left_hand_weapon_class", assets.load_blueprint_class(left))
        assets.save_asset(PLAYER_BP, only_if_is_dirty=False)
        log(f"main gauche du joueur = {left}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "set_sabre_poses.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
