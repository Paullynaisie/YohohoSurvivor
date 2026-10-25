"""
Système d'upgrades à shards (2026-10-25) :
  - /Game/Core/Upgrades/Up_Shards/DA_Up_Shard_*     : les shards (valeurs améliorées, 0 à 30, équation + Base / Index)
  - /Game/Core/Upgrades/Up_Character/DA_Up_Stats_*  : les upgrades (5 shards, Blocking Shard), dont les 3 de remplacement
                                                     (Roadmap = Max stats, CatchUp = Min stats, FullRandom = Random)
  - /Game/Core/Upgrades/DT_Chara_Upgrades           : ligne AllChara (toutes les upgrades de test) + une ligne vide par personnage
                                                     (nom = Character Name : upgrades exclusives à remplir par Paul)
  - BP_TestGameMode : Chara Upgrades Table + Replacement Up Stats
Ne recrée pas ce qui existe déjà (les réglages de Paul sont conservés) ; une ligne de table existante n'est pas touchée.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_shard_upgrades.py"
Rapport : Saved/Logs/setup_shard_upgrades.txt
"""
import os
import unreal

UPGRADES_DIR = "/Game/Core/Upgrades"
SHARDS_DIR = f"{UPGRADES_DIR}/Up_Shards"
STATS_DIR = f"{UPGRADES_DIR}/Up_Character"
OLD_DIR = f"{UPGRADES_DIR}/Old_Upgrades"
TABLE_NAME = "DT_Chara_Upgrades"
TABLE_PATH = f"{UPGRADES_DIR}/{TABLE_NAME}"
GAME_MODE = "/Game/Core/GameModes/BP_TestGameMode"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
report = []

Stat = unreal.YSShardStat
Eq = unreal.YSShardEquation

# nom, (nom affiché, description, stat, équation, Base, Index)
SHARDS = {
    "AutoAtkDamage": ("Auto atk damage", "Dégâts de l'attaque auto", Stat.AUTO_ATK_DAMAGE, Eq.MULTIPLICATIVE, 1.0, 0.135),
    "AbilityDamage": ("Ability damage", "Dégâts de l'Ability", Stat.ABILITY_DAMAGE, Eq.MULTIPLICATIVE, 1.0, 0.135),
    "CritDamage": ("Crit damage", "Dégâts des coups critiques (attaque auto et Ability)", Stat.CRIT_DAMAGE, Eq.MULTIPLICATIVE, 1.5, 0.28),
    "HpsBonus": ("Hps bonus", "PV max", Stat.HPS_BONUS, Eq.MULTIPLICATIVE, 1.0, 0.135),
    "MoveSpeed": ("Move speed", "Vitesse de déplacement", Stat.MOVE_SPEED, Eq.MULTIPLICATIVE, 1.0, 0.025),
    "RegenHps": ("Regen Hps", "PV rendus par seconde sans subir de dégât", Stat.REGEN_HPS, Eq.INCREMENTAL, 0.0, 1.65),
    "Armor": ("Armor", "Dégâts retirés de chaque coup subi", Stat.ARMOR, Eq.INCREMENTAL, 0.0, 3.5),
    "CdReduction": ("Cd reduction", "Recharge de l'Ability", Stat.CD_REDUCTION, Eq.DIVISIBLE, 1.0, 0.025),
    "AtkSpeed": ("Atk speed", "Vitesse de l'attaque auto", Stat.ATK_SPEED, Eq.DIVISIBLE, 1.0, 0.025),
    "ShootSystem": ("Shoot system", "PewPew : balles par rafale ; Shotgun : AMMO par tir ; Sniper : NME traversés", Stat.SHOOT_SYSTEM, Eq.TRUNCATED, 0.0, 0.34),
    "Luck": ("Luck", "Chances de rareté : moins de Bronze / Silver, plus de Gold / Mythique", Stat.LUCK, Eq.INCREMENTAL, 0.0, 0.0),
    "MaxStats": ("Max stats", "La shard la plus haute déjà possédée (pas au maximum)", Stat.MAX_STATS, Eq.INCREMENTAL, 0.0, 0.0),
    "MinStats": ("Min stats", "La shard la plus basse déjà possédée", Stat.MIN_STATS, Eq.INCREMENTAL, 0.0, 0.0),
    "Random": ("Random", "Une shard au hasard (pas au maximum)", Stat.RANDOM, Eq.INCREMENTAL, 0.0, 0.0),
}

# nom, (nom affiché, description, shard bloquante, 5 shards, icône de l'ancienne upgrade)
STATS = {
    "Degat": ("Dégât", "Plus de dégâts", "AutoAtkDamage", ["AutoAtkDamage", "AbilityDamage", "AutoAtkDamage", "AbilityDamage", "MaxStats"], "DA_Upgrade_AtkBonus"),
    "AtkSpeed": ("Atk speed", "Attaques auto plus rapides", "AtkSpeed", ["AtkSpeed", "AtkSpeed", "AutoAtkDamage", "AtkSpeed", "AutoAtkDamage"], "DA_Upgrade_AtkSpeed"),
    "ShootSystem": ("Shoot system", "Améliore le tir de l'attaque auto", "ShootSystem", ["ShootSystem", "ShootSystem", "AutoAtkDamage", "ShootSystem", "AutoAtkDamage"], "DA_Upgrade_ShootSystem"),
    "AbilityReduction": ("Ability reduction", "Ability plus souvent et plus forte", "CdReduction", ["CdReduction", "CdReduction", "AbilityDamage", "CdReduction", "AbilityDamage"], "DA_Upgrade_CDReduction"),
    "BulkUp": ("Bulk up", "Plus solide", "HpsBonus", ["HpsBonus", "RegenHps", "Armor", "Random", "Random"], "DA_Upgrade_HpsBonus"),
    "Vitality": ("Vitality", "Plus de PV", "HpsBonus", ["HpsBonus", "HpsBonus", "HpsBonus", "MinStats", "MinStats"], "DA_Upgrade_HpsBonus"),
    "Tankiness": ("Tankiness", "Plus d'armure", "Armor", ["Armor", "Armor", "Armor", "MinStats", "MinStats"], "DA_Upgrade_Armor"),
    "Regeneration": ("Regeneration", "Plus de regen", "RegenHps", ["RegenHps", "RegenHps", "RegenHps", "Random", "Random"], "DA_Upgrade_HpsRegen"),
    "Critical": ("Critical", "Coups critiques plus forts", "CritDamage", ["CritDamage", "CritDamage", "AutoAtkDamage", "AbilityDamage", "CritDamage"], "DA_Upgrade_CritDamage"),
    "FastnFurious": ("Fast n Furious", "Plus rapide", "MoveSpeed", ["MoveSpeed", "MoveSpeed", "CdReduction", "MaxStats", "MaxStats"], "DA_Upgrade_MoveSpeedBonus"),
    "Lucky": ("Lucky", "Plus de chance", "Luck", ["Luck", "Luck", "Luck", "Luck", "Luck"], "DA_Upgrade_Luck"),
}
REPLACEMENTS = {
    "Roadmap": ("Roadmap", "Renforce tes meilleures stats", None, ["MaxStats"] * 5, None),
    "CatchUp": ("CatchUp", "Rattrape tes stats les plus basses", None, ["MinStats"] * 5, None),
    "FullRandom": ("FullRandom", "Tout au hasard", None, ["Random"] * 5, None),
}


def log(msg):
    unreal.log(f"[setup_shard_upgrades] {msg}")
    report.append(msg)


def data_asset(directory, name, cls):
    path = f"{directory}/{name}"
    if assets.does_asset_exist(path):
        return assets.load_asset(path), False
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    asset = tools.create_asset(name, directory, cls, factory)
    log(f"créé : {path}")
    return asset, True


try:
    # 1) shards
    shards = {}
    for key, (display, desc, stat, equation, base, index) in SHARDS.items():
        shard, created = data_asset(SHARDS_DIR, f"DA_Up_Shard_{key}", unreal.YSUpShard)
        if created:
            shard.set_editor_property("display_name", display)
            shard.set_editor_property("description", desc)
            shard.set_editor_property("stat", stat)
            shard.set_editor_property("equation", equation)
            shard.set_editor_property("base", base)
            shard.set_editor_property("index", index)
            shard.set_editor_property("max_level", 30)
        shards[key] = shard
    # jumelles : Atk speed <-> Shoot system (conversion au changement de set d'arme)
    for a, b in (("AtkSpeed", "ShootSystem"), ("ShootSystem", "AtkSpeed")):
        if not shards[a].get_editor_property("twin_shard"):
            shards[a].set_editor_property("twin_shard", shards[b])
    for shard in shards.values():
        assets.save_loaded_asset(shard, only_if_is_dirty=False)

    # 2) upgrades
    def make_stats(definitions):
        result = []
        for key, (display, desc, blocking, entries, old_icon) in definitions.items():
            stats, created = data_asset(STATS_DIR, f"DA_Up_Stats_{key}", unreal.YSUpStats)
            if created:
                stats.set_editor_property("display_name", display)
                stats.set_editor_property("description", desc)
                stats.set_editor_property("shards", [shards[e] for e in entries])
                stats.set_editor_property("blocking_shard", shards[blocking] if blocking else None)
                old_path = f"{OLD_DIR}/{old_icon}" if old_icon else None
                if old_path and assets.does_asset_exist(old_path):
                    stats.set_editor_property("icon", assets.load_asset(old_path).get_editor_property("icon"))
                assets.save_loaded_asset(stats, only_if_is_dirty=False)
            result.append(stats)
            log(f"{stats.get_name()} : bloquée par {stats.get_editor_property('blocking_shard').get_name() if stats.get_editor_property('blocking_shard') else '-'}, "
                f"shards {[s.get_name().replace('DA_Up_Shard_', '') if s else None for s in stats.get_editor_property('shards')]}")
        return result

    upgrades = make_stats(STATS)
    replacements = make_stats(REPLACEMENTS)

    # 3) table : AllChara + une ligne par personnage sélectionnable
    if assets.does_asset_exist(TABLE_PATH):
        table = assets.load_asset(TABLE_PATH)
    else:
        factory = unreal.DataTableFactory()
        factory.set_editor_property("struct", unreal.YSCharaUpgradesRow.static_struct())
        table = tools.create_asset(TABLE_NAME, UPGRADES_DIR, unreal.DataTable, factory)
        log(f"créée : {TABLE_PATH}")
    existing = [str(n) for n in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)]
    gm_class = assets.load_blueprint_class(GAME_MODE)
    gm_cdo = unreal.get_default_object(gm_class)
    rows = ["AllChara"]
    for character_class in gm_cdo.get_editor_property("selectable_characters"):
        cdo = unreal.get_default_object(character_class)
        row = str(cdo.get_editor_property("upgrade_table_row"))
        if row in ("", "None"):
            row = str(cdo.get_editor_property("character_name"))
        rows.append(row)
    for row in rows:
        if row in existing:
            log(f"ligne déjà présente : {row}")
            continue
        unreal.YSEditorScriptLibrary.set_chara_upgrades_row(table, row, upgrades if row == "AllChara" else [])
        log(f"ligne ajoutée : {row} ({len(upgrades) if row == 'AllChara' else 0} upgrade(s))")
    assets.save_loaded_asset(table, only_if_is_dirty=False)

    # 4) GameMode
    if not gm_cdo.get_editor_property("chara_upgrades_table"):
        gm_cdo.set_editor_property("chara_upgrades_table", table)
    if len(gm_cdo.get_editor_property("replacement_up_stats")) == 0:
        gm_cdo.set_editor_property("replacement_up_stats", replacements)
    assets.save_asset(GAME_MODE, only_if_is_dirty=False)
    log(f"BP_TestGameMode : table {gm_cdo.get_editor_property('chara_upgrades_table').get_name()}, remplacements "
        f"{[r.get_name() for r in gm_cdo.get_editor_property('replacement_up_stats')]}, shards {gm_cdo.get_editor_property('bShardUpgrades')}")
    log("OK")
except Exception as error:
    import traceback
    log(f"ERREUR : {error}\n{traceback.format_exc()}")

with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_shard_upgrades.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
