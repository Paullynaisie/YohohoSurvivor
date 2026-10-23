"""
« Une Mine ! » (upgrade d'arme du Cannoneer) :
  - /Game/Placeable/AMMO/BP_CannoneerMine : enfant de BP_CannoneerBomb (tous ses réglages), mode mine, corps en cube
    (rayon = rayon de la bombe / Mine Radius Divisor 1.5, calculé en C++)
  - Mine Class = BP_CannoneerMine sur la Roll and Bomb de BP_Bomb et BP_Shotgun et sur le BombThrower de BP_Sniper
  - /Game/Core/Upgrades/DA_Upgrade_Mine (Roll and Bomb) et DA_Upgrade_MineThrower (BombThrower), jumeaux
  - ajoutés à la fin de l'Upgrade Pool du Cannoneer (les index existants ne bougent pas)
Ne recrée pas ce qui existe déjà (les réglages de Paul sont conservés).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_mine.py"
Rapport : Saved/Logs/setup_mine.txt
"""
import os
import unreal

AMMO_DIR = "/Game/Placeable/AMMO"
BOMB_BP = f"{AMMO_DIR}/BP_CannoneerBomb"
MINE_NAME = "BP_CannoneerMine"
MINE_BP = f"{AMMO_DIR}/{MINE_NAME}"
CUBE = "/Engine/BasicShapes/Cube"
WEAPONS_DIR = "/Game/Placeable/Weapons/Player"
UPGRADES_DIR = "/Game/Core/Upgrades"
MINE_UPGRADE = f"{UPGRADES_DIR}/DA_Upgrade_Mine"
MINE_THROWER_UPGRADE = f"{UPGRADES_DIR}/DA_Upgrade_MineThrower"
CANNONEER_BP = "/Game/Placeable/Character/Player/PlayerCharacter_Cannoneer"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_mine] {msg}")
    report.append(msg)


def templates(bp):
    result = []
    for h in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(h))
        if obj and obj not in result:
            result.append(obj)
    return result


def upgrade_asset(path, name):
    if assets.does_asset_exist(path):
        log(f"existe déjà : {path}")
        return assets.load_asset(path), False
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.YSMineUpgrade)
    asset = tools.create_asset(name, UPGRADES_DIR, unreal.YSMineUpgrade, factory)
    log(f"créé : {path}")
    return asset, True


try:
    # 1) la mine : enfant de la bombe
    if assets.does_asset_exist(MINE_BP):
        log(f"existe déjà : {MINE_BP}")
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", assets.load_blueprint_class(BOMB_BP))
        tools.create_asset(MINE_NAME, AMMO_DIR, unreal.Blueprint, factory)
        log(f"créé : {MINE_BP}")
    mine_cdo = unreal.get_default_object(assets.load_blueprint_class(MINE_BP))
    if not mine_cdo.get_editor_property("bMine"):
        # première configuration seulement (ensuite : réglages de Paul)
        mine_cdo.set_editor_property("bMine", True)
        mine_cdo.get_editor_property("mesh").set_editor_property("static_mesh", assets.load_asset(CUBE))
        unreal.BlueprintEditorLibrary.compile_blueprint(assets.load_asset(MINE_BP))
        assets.save_asset(MINE_BP, only_if_is_dirty=False)
    mine_class = assets.load_blueprint_class(MINE_BP)
    mine_cdo = unreal.get_default_object(mine_class)
    bomb_cdo = unreal.get_default_object(assets.load_blueprint_class(BOMB_BP))
    log(f"mine : {mine_cdo.get_editor_property('bMine')}, corps {mine_cdo.get_editor_property('mesh').get_editor_property('static_mesh').get_name()}, "
        f"rayon {mine_cdo.get_editor_property('explosion_radius') / mine_cdo.get_editor_property('mine_radius_divisor')} (bombe {bomb_cdo.get_editor_property('explosion_radius')}), mèche {mine_cdo.get_editor_property('fuse_duration')} s")

    # 2) Mine Class des actions
    for weapon, cls in (("BP_Bomb", unreal.YSRollAndBombAbility), ("BP_Shotgun", unreal.YSRollAndBombAbility), ("BP_Sniper", unreal.YSBombThrowerAbility)):
        path = f"{WEAPONS_DIR}/{weapon}"
        actions = [t for t in templates(assets.load_asset(path)) if isinstance(t, cls)]
        for action in actions:
            action.set_editor_property("mine_class", mine_class)
            log(f"{weapon}.{action.get_name()} : Mine Class = {MINE_NAME} (bombe {action.get_editor_property('bomb_class').get_name() if action.get_editor_property('bomb_class') else None})")
        if not actions:
            log(f"ATTENTION : {weapon} sans {cls.__name__}")
        assets.save_asset(path, only_if_is_dirty=False)

    # 3) upgrades jumelles
    mine, _ = upgrade_asset(MINE_UPGRADE, "DA_Upgrade_Mine")
    thrower, created = upgrade_asset(MINE_THROWER_UPGRADE, "DA_Upgrade_MineThrower")
    if created:
        thrower.set_editor_property("required_action_class", unreal.YSBombThrowerAbility)
    mine.set_editor_property("twin_upgrade", thrower)
    thrower.set_editor_property("twin_upgrade", mine)
    assets.save_asset(MINE_UPGRADE, only_if_is_dirty=False)
    assets.save_asset(MINE_THROWER_UPGRADE, only_if_is_dirty=False)

    # 4) pool du Cannoneer
    cannoneer = unreal.get_default_object(assets.load_blueprint_class(CANNONEER_BP))
    pool = list(cannoneer.get_editor_property("upgrade_pool"))
    for upgrade in (mine, thrower):
        if upgrade not in pool:
            pool.append(upgrade)
    cannoneer.set_editor_property("upgrade_pool", pool)
    assets.save_asset(CANNONEER_BP, only_if_is_dirty=False)
    log(f"pool du Cannoneer : Mine index {pool.index(mine)}, MineThrower index {pool.index(thrower)} ({len(pool)} upgrades)")

    for path in (MINE_UPGRADE, MINE_THROWER_UPGRADE):
        a = assets.load_asset(path)
        log(f"relu {a.get_name()} : action {a.get_editor_property('required_action_class').get_name()}, jumeau {a.get_editor_property('twin_upgrade').get_name()}, "
            f"bonus {a.get_editor_property('bonus_silver_value')} / {a.get_editor_property('bonus_gold_value')}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_mine.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
