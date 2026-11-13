# Crée /Game/Placeable/Weapons/Player/BP_SpinningSword (enfant de AYSSpinningSword : réglages de la Spinning Sword).
# Relançable : ne touche pas un Blueprint qui existe déjà.
import os
import unreal

WEAPON_DIR = "/Game/Placeable/Weapons/Player"
NAME = "BP_SpinningSword"
BP_PATH = f"{WEAPON_DIR}/{NAME}"

report = []


def log(message):
    report.append(message)
    unreal.log(f"[setup_spinning_sword] {message}")


try:
    assets = unreal.EditorAssetLibrary
    if assets.does_asset_exist(BP_PATH):
        log(f"existe déjà : {BP_PATH}")
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.YSSpinningSword)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, WEAPON_DIR, unreal.Blueprint, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assets.save_asset(BP_PATH, only_if_is_dirty=False)
        log(f"créé : {BP_PATH}")

    cdo = unreal.get_default_object(assets.load_blueprint_class(BP_PATH))
    for prop in ("sword_visual_class", "damage", "max_level", "level_duration", "rotation_speed", "height", "sword_location", "sword_rotation"):
        log(f"relu : {prop} = {cdo.get_editor_property(prop)}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_spinning_sword.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
