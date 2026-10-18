"""
Upgrade Shoot system (Guns) :
  - /Game/Core/Upgrades/DA_Upgrade_ShootSystem (UYSShootSystemUpgrade : PewPew +1 / +2 / +3 balles par rafale)
  - ajoutée à la fin de l'UpgradePool de chaque personnage joueur de /Game/Placeable/Character
Ne recrée pas l'asset s'il existe ; ne l'ajoute pas deux fois à un pool.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_shoot_system_upgrade.py"
Rapport : Saved/Logs/setup_shoot_system_upgrade.txt
"""
import os
import unreal

UPGRADES_DIR = "/Game/Core/Upgrades"
NAME = "DA_Upgrade_ShootSystem"
PATH = f"{UPGRADES_DIR}/{NAME}"
CHAR_DIR = "/Game/Placeable/Character"

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_shoot_system_upgrade] {msg}")
    report.append(msg)


try:
    if assets.does_asset_exist(PATH):
        log(f"existe déjà : {PATH}")
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.YSShootSystemUpgrade)
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, UPGRADES_DIR, unreal.YSShootSystemUpgrade, factory)
        assets.save_asset(PATH, only_if_is_dirty=False)
        log(f"créé : {PATH}")
    upgrade = assets.load_asset(PATH)

    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for data in registry.get_assets_by_path(CHAR_DIR, recursive=False):
        if str(data.asset_class_path.asset_name) != "Blueprint":
            continue
        path = str(data.package_name)
        cdo = unreal.get_default_object(assets.load_blueprint_class(path))
        if not isinstance(cdo, unreal.YSPlayerCharacter):
            continue
        pool = list(cdo.get_editor_property("upgrade_pool"))
        if upgrade in pool:
            log(f"{path} : déjà dans le pool (index {pool.index(upgrade)})")
            continue
        pool.append(upgrade)
        cdo.set_editor_property("upgrade_pool", pool)
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"{path} : ajoutée au pool (index {len(pool) - 1})")

    check = assets.load_asset(PATH)
    log(f"relu : {check.get_editor_property('display_name')}, {check.get_editor_property('bronze_value')} / "
        f"{check.get_editor_property('silver_value')} / {check.get_editor_property('gold_value')}, max {check.get_editor_property('max_stacks')}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_shoot_system_upgrade.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
