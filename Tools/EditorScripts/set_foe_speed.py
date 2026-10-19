"""
Vitesse de déplacement des NME (MoveSpeed, catégorie Foe|Movement) : FoeCharacter_Skull et FoeCharacter_Skull_Elite.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/set_foe_speed.py"
Rapport : Saved/Logs/set_foe_speed.txt
"""
import os
import unreal

FOE_SPEED = 300.0
FOES = ["/Game/Placeable/Character/FoeCharacter_Skull", "/Game/Placeable/Character/FoeCharacter_Skull_Elite"]

report = []
try:
    for path in FOES:
        cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path))
        before = cdo.get_editor_property("move_speed")
        cdo.set_editor_property("move_speed", FOE_SPEED)
        unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
        after = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(path)).get_editor_property("move_speed")
        report.append(f"{path} : {before} -> {after}")
    report.append("OK")
except Exception as e:
    report.append(f"ERREUR : {e!r}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "set_foe_speed.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
