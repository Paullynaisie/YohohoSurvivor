"""
Menu principal :
  - crée /Game/Maps/MainMenu (map vide, sans World Partition)
  - World Settings : GameMode Override = AYSMenuGameMode (affiche le menu Héberger / Rejoindre)

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_main_menu.py"
Rapport : Saved/Logs/setup_main_menu.txt
"""
import os
import unreal

MENU_MAP = "/Game/Maps/MainMenu"

assets = unreal.EditorAssetLibrary
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
report = []


def log(msg):
    unreal.log(f"[setup_main_menu] {msg}")
    report.append(msg)


try:
    if assets.does_asset_exist(MENU_MAP):
        levels.load_level(MENU_MAP)
        log(f"map existante chargée : {MENU_MAP}")
    else:
        if not levels.new_level(MENU_MAP, False):
            raise RuntimeError(f"new_level a échoué pour {MENU_MAP}")
        log(f"map créée : {MENU_MAP}")

    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    settings = world.get_world_settings()
    settings.set_editor_property("default_game_mode", unreal.YSMenuGameMode.static_class())
    log(f"GameMode Override : {settings.get_editor_property('default_game_mode').get_name()}")

    if not levels.save_current_level():
        raise RuntimeError("save_current_level a échoué")
    log("sauvegardé")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_main_menu.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
