"""
Upgrades en Data Assets :
  - crée /Game/Core/Upgrades/DA_Upgrade_AtkBonus (UYSHandAtkBonusUpgrade : Right / Left hand Atk bonus) s'il n'existe pas
    (réglages par défaut de la classe C++ ; ensuite à régler dans l'asset)
  - Upgrade Pool des 3 personnages jouables = [DA_Upgrade_AtkBonus]
    (les enfants reçoivent la même valeur que le parent : ils en héritent à nouveau)

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_upgrade_pools.py"
Rapport : Saved/Logs/setup_upgrade_pools.txt
"""
import os
import unreal

UPGRADE_DIR = "/Game/Core/Upgrades"
ATK_BONUS = f"{UPGRADE_DIR}/DA_Upgrade_AtkBonus"
CHARACTERS = [
    "/Game/Placeable/Character/PlayerCharacter_mouss",  # parent d'abord
    "/Game/Placeable/Character/PlayerCharacter_mouss2sabre",
    "/Game/Placeable/Character/PlayerCharacter_mouss2pistol",
]

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_upgrade_pools] {msg}")
    report.append(msg)


def describe(pool):
    return [u.get_path_name() if u else None for u in pool]


try:
    if assets.does_asset_exist(ATK_BONUS):
        atk = assets.load_asset(ATK_BONUS)
        log(f"existe déjà : {ATK_BONUS}")
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.YSHandAtkBonusUpgrade)
        atk = unreal.AssetToolsHelpers.get_asset_tools().create_asset("DA_Upgrade_AtkBonus", UPGRADE_DIR, unreal.YSHandAtkBonusUpgrade, factory)
        assets.save_asset(ATK_BONUS, only_if_is_dirty=False)
        log(f"créé : {ATK_BONUS}")
    log(f"  {atk.get_editor_property('right_hand_name')} / {atk.get_editor_property('left_hand_name')}, "
        f"max {atk.get_editor_property('max_stacks')}, valeurs {atk.get_editor_property('bronze_value')} / "
        f"{atk.get_editor_property('silver_value')} / {atk.get_editor_property('gold_value')}")

    for path in CHARACTERS:
        cdo = unreal.get_default_object(assets.load_blueprint_class(path))
        log(f"{path} : avant {describe(cdo.get_editor_property('upgrade_pool'))}")
        cdo.set_editor_property("upgrade_pool", [atk])
        assets.save_asset(path, only_if_is_dirty=False)

    for path in CHARACTERS:
        check = unreal.get_default_object(assets.load_blueprint_class(path)).get_editor_property("upgrade_pool")
        log(f"{path} : relu {describe(check)}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_upgrade_pools.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
