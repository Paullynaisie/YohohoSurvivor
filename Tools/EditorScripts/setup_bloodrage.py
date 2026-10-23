"""
BloodRage (upgrade d'arme de la GreatAxe, à la place du Bloodlust) :
  - /Game/Core/Upgrades/DA_Upgrade_BloodRage (UYSBloodRageUpgrade) : Auto swing en Auto atk de BP_GreatAxe seulement
    (Required Weapon Classes) ; poids des paliers copiés du Bloodlust
  - DA_Upgrade_Bloodlust : BP_GreatAxe dans Excluded Weapon Classes (plus proposé au Quartermaster)
  - ajoutée à la FIN du pool du Quartermaster
Relançable (ne recrée pas l'asset, n'ajoute rien en double).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_bloodrage.py"
Rapport : Saved/Logs/setup_bloodrage.txt
"""
import os
import unreal

UPGRADES_DIR = "/Game/Core/Upgrades"
NAME = "DA_Upgrade_BloodRage"
PATH = f"{UPGRADES_DIR}/{NAME}"
BLOODLUST = f"{UPGRADES_DIR}/DA_Upgrade_Bloodlust"
GREAT_AXE = "/Game/Placeable/Weapons/Player/BP_GreatAxe"
QUARTERMASTER = "/Game/Placeable/Character/Player/PlayerCharacter_Quartermaster"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
report = []


def log(msg):
    unreal.log(f"[setup_bloodrage] {msg}")
    report.append(msg)


try:
    axe_class = assets.load_blueprint_class(GREAT_AXE)
    bloodlust = assets.load_asset(BLOODLUST)

    if not assets.does_asset_exist(PATH):
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.YSBloodRageUpgrade)
        rage = tools.create_asset(NAME, UPGRADES_DIR, unreal.YSBloodRageUpgrade, factory)
        for prop in ("unlock_weight", "bonus_weight", "super_weight", "max_bonus_stacks", "offer_weight"):
            rage.set_editor_property(prop, bloodlust.get_editor_property(prop))
        rage.set_editor_property("required_weapon_classes", [axe_class])
        assets.save_asset(PATH, only_if_is_dirty=False)
        log(f"créé : {PATH} (GreatAxe seulement)")
    else:
        log(f"existe déjà : {PATH}")

    excluded = list(bloodlust.get_editor_property("excluded_weapon_classes"))
    if axe_class not in excluded:
        bloodlust.set_editor_property("excluded_weapon_classes", excluded + [axe_class])
        assets.save_asset(BLOODLUST, only_if_is_dirty=False)
        log("Bloodlust : GreatAxe exclue")

    character = unreal.get_default_object(assets.load_blueprint_class(QUARTERMASTER))
    pool = list(character.get_editor_property("upgrade_pool"))
    rage = assets.load_asset(PATH)
    if rage not in pool:
        pool.append(rage)
        character.set_editor_property("upgrade_pool", pool)
        assets.save_asset(QUARTERMASTER, only_if_is_dirty=False)
    log(f"Quartermaster : BloodRage en index {pool.index(rage)} du pool ({len(pool)} upgrades)")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_bloodrage.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
