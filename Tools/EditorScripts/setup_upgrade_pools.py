"""
Upgrades en Data Assets (/Game/Core/Upgrades), créés s'ils n'existent pas (réglages par défaut de la classe C++,
ensuite à régler dans l'asset) :
  - DA_Upgrade_AtkBonus        (UYSHandAtkBonusUpgrade : Right / Left hand Atk bonus)
  - DA_Upgrade_MoveSpeedBonus  (UYSMoveSpeedBonusUpgrade)
  - DA_Upgrade_HpsBonus        (UYSHealthBonusUpgrade)
  - DA_Upgrade_HpsRegen        (UYSHealthRegenUpgrade)
  - DA_Upgrade_Armor           (UYSArmorUpgrade)
  - DA_Upgrade_CDReduction     (UYSHandCooldownReductionUpgrade : Right / Left hand CD reduction)
  - DA_Upgrade_Luck            (UYSLuckUpgrade : Silver uniquement)
  - DA_Upgrade_Ricochet        (UYSRicochetUpgrade : upgrade d'arme, Coup d'Petoir)
  - DA_ClassUpgrade_Mouss2Sabre / DA_ClassUpgrade_Mouss2Pistol (UYSClassUpgrade : Mouss -> 2 sabres / 2 pétoirs,
    classe source / cible et description réglées par ce script)
Sélection des personnages (BP_TestGameMode) : Mouss seul (les 2 autres s'obtiennent par l'upgrade de classe).
Upgrade Pool des 3 personnages jouables = ces assets
(les enfants reçoivent la même valeur que le parent : ils en héritent à nouveau).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_upgrade_pools.py"
Rapport : Saved/Logs/setup_upgrade_pools.txt
"""
import os
import unreal

UPGRADE_DIR = "/Game/Core/Upgrades"
UPGRADES = [
    ("DA_Upgrade_AtkBonus", unreal.YSHandAtkBonusUpgrade),
    ("DA_Upgrade_MoveSpeedBonus", unreal.YSMoveSpeedBonusUpgrade),
    ("DA_Upgrade_HpsBonus", unreal.YSHealthBonusUpgrade),
    ("DA_Upgrade_HpsRegen", unreal.YSHealthRegenUpgrade),
    ("DA_Upgrade_Armor", unreal.YSArmorUpgrade),
    ("DA_Upgrade_CDReduction", unreal.YSHandCooldownReductionUpgrade),
    ("DA_Upgrade_Luck", unreal.YSLuckUpgrade),
    ("DA_Upgrade_Ricochet", unreal.YSRicochetUpgrade),
]
MOUSS = "/Game/Placeable/Character/PlayerCharacter_mouss"
CLASS_UPGRADES = [
    # (asset, classe source, classe cible, description)
    ("DA_ClassUpgrade_Mouss2Sabre", MOUSS, "/Game/Placeable/Character/PlayerCharacter_mouss2sabre", "Un sabre dans chaque main"),
    ("DA_ClassUpgrade_Mouss2Pistol", MOUSS, "/Game/Placeable/Character/PlayerCharacter_mouss2pistol", "Un pétoir dans chaque main"),
]
GAME_MODE_BP = "/Game/Core/GameModes/BP_TestGameMode"
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


def describe_settings(upgrade):
    """Réglages principaux (selon le type d'upgrade)"""
    values = []
    for prop in ("offer_weight", "max_stacks", "bronze_value", "silver_value", "gold_value",
                 "unlock_weight", "bonus_weight", "super_weight", "max_bonus_stacks", "bonus_silver_value", "bonus_gold_value"):
        try:
            value = upgrade.get_editor_property(prop)
            values.append(f"{prop} {value:.3f}" if isinstance(value, float) else f"{prop} {value}")
        except Exception:
            pass
    return ", ".join(values)


def describe(pool):
    return [u.get_name() if u else None for u in pool]


try:
    pool = []
    for name, cls in UPGRADES:
        path = f"{UPGRADE_DIR}/{name}"
        if assets.does_asset_exist(path):
            upgrade = assets.load_asset(path)
            log(f"existe déjà : {path}")
        else:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property("data_asset_class", cls)
            upgrade = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, UPGRADE_DIR, cls, factory)
            assets.save_asset(path, only_if_is_dirty=False)
            log(f"créé : {path}")
        log(f"  {upgrade.get_editor_property('display_name')} ({upgrade.get_class().get_name()}) : {describe_settings(upgrade)}")
        pool.append(upgrade)

    # upgrades de classe : source / cible / description imposées
    for name, source, target, description in CLASS_UPGRADES:
        path = f"{UPGRADE_DIR}/{name}"
        if assets.does_asset_exist(path):
            upgrade = assets.load_asset(path)
            log(f"existe déjà : {path}")
        else:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property("data_asset_class", unreal.YSClassUpgrade)
            upgrade = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, UPGRADE_DIR, unreal.YSClassUpgrade, factory)
            log(f"créé : {path}")
        upgrade.set_editor_property("source_class", assets.load_blueprint_class(source))
        upgrade.set_editor_property("target_class", assets.load_blueprint_class(target))
        upgrade.set_editor_property("description", description)
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"  {name} : {upgrade.get_editor_property('source_class').get_name()} -> {upgrade.get_editor_property('target_class').get_name()}")
        pool.append(upgrade)

    # sélection : Mouss seul
    gm = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP))
    log(f"sélection avant : {[c.get_name() for c in gm.get_editor_property('selectable_characters') if c]}")
    gm.set_editor_property("selectable_characters", [assets.load_blueprint_class(MOUSS)])
    assets.save_asset(GAME_MODE_BP, only_if_is_dirty=False)
    check_gm = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP))
    log(f"sélection relue : {[c.get_name() for c in check_gm.get_editor_property('selectable_characters') if c]}, "
        f"classe au niveau {check_gm.get_editor_property('class_upgrade_level')}")

    for path in CHARACTERS:
        cdo = unreal.get_default_object(assets.load_blueprint_class(path))
        log(f"{path} : avant {describe(cdo.get_editor_property('upgrade_pool'))}")
        cdo.set_editor_property("upgrade_pool", pool)
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
