"""
Caméra de la barre proportionnelle à la taille du bateau (demande de Paul du 2026-11-09), relançable :
  BP_GameBoat_Brigantin = celle du sloop x1.25, BP_GameBoat_Galion = celle du sloop x1.5 (Camera Arm Length du poste HelmStation)

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="<chemin absolu>/Tools/EditorScripts/setup_helm_camera.py"
Rapport : Saved/Logs/setup_helm_camera.txt
"""
import os
import unreal

SHIP_DIR = "/Game/Placeable/Ship"
SLOOP = f"{SHIP_DIR}/BP_GameBoat_Sloop"
SHIPS = [(f"{SHIP_DIR}/BP_GameBoat_Brigantin", 1.25), (f"{SHIP_DIR}/BP_GameBoat_Galion", 1.5)]

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_helm_camera] {msg}")
    report.append(str(msg))


def helm_template(path):
    ship = unreal.get_default_object(assets.load_blueprint_class(path))
    return ship.get_editor_property("HelmStation").get_editor_property("child_actor_template")


try:
    base = helm_template(SLOOP).get_editor_property("CameraArmLength")
    log(f"sloop : {base:.0f}")
    for path, ratio in SHIPS:
        template = helm_template(path)
        before = template.get_editor_property("CameraArmLength")
        template.set_editor_property("CameraArmLength", base * ratio)
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"{path.rsplit('/', 1)[1]} : {before:.0f} -> {helm_template(path).get_editor_property('CameraArmLength'):.0f} (x{ratio})")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_helm_camera.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
