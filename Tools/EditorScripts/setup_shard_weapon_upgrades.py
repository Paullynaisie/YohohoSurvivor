"""
Upgrades d'arme réhabilitées dans le système de shards (2026-10-25, demande de Paul) :
  - /Game/Core/Upgrades/Up_Character/Up_Weapons/DA_Up_Weapon_* : copies des anciennes upgrades d'arme (Old_Upgrades : réglages et
    icônes gardés) + DA_Up_Weapon_MarquageCritique (nouvelle) ; Shard System coché, Unlock / Bonus Shards = [Random, Random],
    5 bonus max à valeur fixe (Bonus Gold Value), poids 1 par palier (comme les DA_Up_Stats), jumelles deux à deux
  - pools des personnages : chaque ancienne upgrade d'arme remplacée par la nouvelle (même index), Marquage Critique ajouté au Captain
Ne recrée pas un asset qui existe déjà (les réglages de Paul sont gardés) ; les pools sont mis à jour à chaque lancement.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="<chemin absolu>/Tools/EditorScripts/setup_shard_weapon_upgrades.py"
Rapport : Saved/Logs/setup_shard_weapon_upgrades.txt
"""
import os
import unreal

UPGRADES_DIR = "/Game/Core/Upgrades"
WEAPONS_DIR = f"{UPGRADES_DIR}/Up_Character/Up_Weapons"
RANDOM_SHARD = f"{UPGRADES_DIR}/Up_Shards/DA_Up_Shard_Random"
CHARACTERS_DIR = "/Game/Placeable/Character/Player"
CHARACTERS = ["PlayerCharacter_mouss", "PlayerCharacter_JohnnyPlaytest", "PlayerCharacter_Cannoneer", "PlayerCharacter_Quartermaster", "PlayerCharacter_Captain"]

# nouveau nom : (ancien asset, valeur fixe d'un bonus, jumelle)
UPGRADES = {
    "Ricochet": ("DA_Upgrade_Ricochet", 1.0, "Bloodlust"),
    "Bloodlust": ("DA_Upgrade_Bloodlust", 0.02, "Ricochet"),
    "Mine": ("DA_Upgrade_Mine", 0.2, "MineThrower"),
    "MineThrower": ("DA_Upgrade_MineThrower", 0.2, "Mine"),
    "BloodRage": ("DA_Upgrade_BloodRage", None, "Yolo"),            # None : valeur Gold de l'ancien asset
    "Yolo": ("DA_Upgrade_Yolo", None, "BloodRage"),
    "AllMyMinions": ("DA_Upgrade_AllMyMinions", None, "MarquageCritique"),
    "MarquageCritique": (None, 0.1, "AllMyMinions"),
}

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
registry = unreal.AssetRegistryHelpers.get_asset_registry()
report = []


def log(msg):
    unreal.log(f"[setup_shard_weapon_upgrades] {msg}")
    report.append(msg)


def find_path(name, root):
    for data in registry.get_assets_by_path(root, recursive=True):
        if str(data.asset_name) == name:
            return str(data.package_name)
    return None


def find_bp_class(name):
    path = find_path(name, "/Game/Placeable")
    if not path:
        raise RuntimeError(f"Blueprint introuvable : {name}")
    return assets.load_blueprint_class(path)


def class_names(classes):
    return [c.get_name() for c in classes if c]


try:
    random_shard = assets.load_asset(RANDOM_SHARD)
    if not random_shard:
        raise RuntimeError(f"shard introuvable : {RANDOM_SHARD}")

    upgrades = {}
    old_assets = {}
    for key, (old_name, fixed, twin) in UPGRADES.items():
        path = f"{WEAPONS_DIR}/DA_Up_Weapon_{key}"
        old_path = find_path(old_name, UPGRADES_DIR) if old_name else None
        if old_path:
            old_assets[old_path] = key
        if assets.does_asset_exist(path):
            upgrades[key] = assets.load_asset(path)
            log(f"existe déjà : {path}")
            continue

        if old_path:
            asset = assets.duplicate_asset(old_path, path)
            log(f"créé : {path} (copie de {old_path})")
        else:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property("data_asset_class", unreal.YSMarkCriticalUpgrade)
            asset = tools.create_asset(f"DA_Up_Weapon_{key}", WEAPONS_DIR, unreal.YSMarkCriticalUpgrade, factory)
            log(f"créé : {path}")

        value = fixed if fixed is not None else asset.get_editor_property("bonus_gold_value")
        asset.set_editor_property("shard_system", True)
        asset.set_editor_property("unlock_shards", [random_shard, random_shard])
        asset.set_editor_property("bonus_shards", [random_shard, random_shard])
        asset.set_editor_property("max_bonus_stacks", 5)
        asset.set_editor_property("bonus_silver_value", value)
        asset.set_editor_property("bonus_gold_value", value)
        for weight in ("unlock_weight", "bonus_weight", "super_weight"):
            asset.set_editor_property(weight, 1.0)

        # comportements changés (textes copiés des anciens assets)
        if key == "Ricochet":
            asset.set_editor_property("super_extra_ricochets", 0)
            asset.set_editor_property("super_damage_bonus", 0.25)
            asset.set_editor_property("bonus_name", "Ricochet ++")
            asset.set_editor_property("bonus_description", "La balle ricoche une fois de plus")
            asset.set_editor_property("bonus_stat_label", "Ricochets")
            asset.set_editor_property("super_description", "+25 % de dégâts à chaque ricochet")
        elif key == "Bloodlust":
            asset.set_editor_property("required_weapon_classes", [find_bp_class("BP_BattleSword")])
        elif key in ("Mine", "MineThrower"):
            asset.set_editor_property("bonus_name", "Une Mine ! ++")
            asset.set_editor_property("bonus_description", "Les mines explosent plus loin")
            asset.set_editor_property("bonus_stat_label", "Rayon d'explosion des mines (+%)")
        elif key == "BloodRage":
            # le Boulet et Chaîne (MorningStar) a maintenant Yoloooooo (jumelle)
            morning_star = find_bp_class("BP_MorningStar")
            kept = [c for c in asset.get_editor_property("required_weapon_classes") if c != morning_star]
            asset.set_editor_property("required_weapon_classes", kept)
        elif key == "Yolo":
            length = asset.get_editor_property("bonus_length_gold_value")
            asset.set_editor_property("bonus_length_silver_value", length)
        upgrades[key] = asset

    # jumelles, puis sauvegarde
    for key, (_, _, twin) in UPGRADES.items():
        asset = upgrades[key]
        # toujours la nouvelle jumelle (une copie garde celle de l'ancien asset)
        asset.set_editor_property("twin_upgrade", upgrades[twin])
        assets.save_loaded_asset(asset, only_if_is_dirty=False)
        log(f"{asset.get_name()} : bonus {asset.get_editor_property('bonus_gold_value')} x{asset.get_editor_property('max_bonus_stacks')}, "
            f"jumelle {asset.get_editor_property('twin_upgrade').get_name()}, armes {class_names(asset.get_editor_property('required_weapon_classes'))}, "
            f"shards {[s.get_name() if s else None for s in asset.get_editor_property('unlock_shards')]} / {[s.get_name() if s else None for s in asset.get_editor_property('bonus_shards')]}")

    # pools des personnages
    for name in CHARACTERS:
        path = f"{CHARACTERS_DIR}/{name}"
        cdo = unreal.get_default_object(assets.load_blueprint_class(path))
        pool = list(cdo.get_editor_property("upgrade_pool"))
        replaced = []
        for index, upgrade in enumerate(pool):
            key = old_assets.get(upgrade.get_path_name().split(".")[0]) if upgrade else None
            if key:
                pool[index] = upgrades[key]
                replaced.append(f"{index}:{key}")
        if name == "PlayerCharacter_Captain" and upgrades["MarquageCritique"] not in pool:
            pool.append(upgrades["MarquageCritique"])
            replaced.append(f"{len(pool) - 1}:MarquageCritique (ajouté)")
        cdo.set_editor_property("upgrade_pool", pool)
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"{name} : {replaced}")
    log("OK")
except Exception as error:
    import traceback
    log(f"ERREUR : {error}\n{traceback.format_exc()}")

with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_shard_weapon_upgrades.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
