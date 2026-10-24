"""
MorningStar : la hitbox du boulet frappe pendant TOUTE l'animation de l'Auto swing (préparation et retour compris, dégâts
secondaires hors coupe) au lieu d'en permanence (la hitbox permanente passe au Déblocage de l'upgrade Yoloooooo).
Rapport des réglages de DA_Upgrade_Yolo (valeurs de taille / longueur / Golden).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_morningstar_whole_anim.py"
Rapport : Saved/Logs/setup_morningstar_whole_anim.txt
"""
import os
import unreal

STAR = "/Game/Placeable/Weapons/Player/BP_MorningStar"
YOLO = "/Game/Core/Upgrades/DA_Upgrade_Yolo"

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_morningstar_whole_anim] {msg}")
    report.append(msg)


try:
    star = assets.load_asset(STAR)
    seen = []
    for h in subsystem.k2_gather_subobject_data_for_blueprint(star):
        data = lib.get_data(h)
        action = lib.get_object(data)
        # Auto swing de l'attaque auto seulement (pas le Bumping Slash, sous-classe en Ability)
        if type(action) == unreal.YSAutoSwingAbility and all(o is not action for o in seen):
            seen.append(action)
            action.set_editor_property("hitbox_during_whole_animation", True)
            log(f"BP_MorningStar.{lib.get_variable_name(data)} : Hitbox During Whole Animation, "
                f"dégâts {action.get_editor_property('damage')}, secondaires x{action.get_editor_property('secondary_damage_multiplier')}")
    unreal.BlueprintEditorLibrary.compile_blueprint(star)
    assets.save_asset(STAR, only_if_is_dirty=False)

    yolo = assets.load_asset(YOLO)
    for prop in ["bonus_silver_value", "bonus_gold_value", "bonus_length_silver_value", "bonus_length_gold_value", "super_atk_speed"]:
        log(f"DA_Upgrade_Yolo : {prop} = {yolo.get_editor_property(prop)}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_morningstar_whole_anim.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
