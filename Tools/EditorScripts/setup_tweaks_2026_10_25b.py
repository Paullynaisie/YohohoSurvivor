"""
Réglages du 2026-10-25 (après la build 0.009) :
  - Base Health Regen = 0 sur tous les personnages joueurs (et minions)
  - Foe Attraction Multiplier = 2 sur les objets portés (coffres, carte au trésor...)
  - Minion Target Distance Multiplier = 2 sur tous les NME
  - Carte au trésor : un segment de plus (Segment Count + 1, seulement si le BP est encore à la valeur vue : 3)
  - Shard DA_Up_Shard_SummonSystem (Summon system, incrémentale tronquée, Base 0, Index 0.3) et upgrade
    DA_Up_Stats_Summoning (bloquée par Summon system ; Summon system, Summon system, Ability damage, Summon system,
    Ability damage), ajoutée à la ligne Captain de DT_Chara_Upgrades (les upgrades déjà dans la ligne sont gardées)
Relançable (ne recrée rien qui existe).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_tweaks_2026_10_25b.py"
Rapport : Saved/Logs/setup_tweaks_2026_10_25b.txt
"""
import json
import os
import unreal

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
registry = unreal.AssetRegistryHelpers.get_asset_registry()
report = []

UPGRADES_DIR = "/Game/Core/Upgrades"
SHARDS_DIR = f"{UPGRADES_DIR}/Up_Shards"
STATS_DIR = f"{UPGRADES_DIR}/Up_Character"
TABLE_PATH = f"{UPGRADES_DIR}/DT_Chara_Upgrades"
CAPTAIN_ROW = "Captain"
OLD_SUMMON_UPGRADE = "DA_Upgrade_SummonSystem"
TREASURE_MAP_BP = "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_TreasureMap"


def log(msg):
    unreal.log(f"[setup_tweaks_2026_10_25b] {msg}")
    report.append(msg)


def blueprints_of(native_class, root="/Game"):
    """(chemin, CDO) de tous les Blueprints dont la classe générée hérite de native_class"""
    result = []
    for data in registry.get_assets_by_path(root, recursive=True):
        if str(data.asset_class_path.asset_name) != "Blueprint":
            continue
        path = str(data.package_name)
        if path.startswith("/Game/PresetProject"):
            continue
        cls = assets.load_blueprint_class(path)
        if cls and unreal.MathLibrary.class_is_child_of(cls, native_class):
            result.append((path, unreal.get_default_object(cls)))
    return result


def set_on_blueprints(native_class, prop, value):
    for path, cdo in blueprints_of(native_class):
        old = cdo.get_editor_property(prop)
        if abs(old - value) > 1e-4:
            cdo.set_editor_property(prop, value)
            assets.save_asset(path, only_if_is_dirty=False)
        log(f"{path.split('/')[-1]} : {prop} {old} -> {value}")


def find_asset(name):
    for data in registry.get_assets_by_path(UPGRADES_DIR, recursive=True):
        if str(data.asset_name) == name:
            return assets.load_asset(str(data.package_name))
    return None


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
    # 1) regen de base, attraction des objets portés, minions moins visés
    set_on_blueprints(unreal.YSPlayerCharacter, "base_health_regen", 0.0)
    set_on_blueprints(unreal.YSInteractiveObject, "foe_attraction_multiplier", 2.0)
    set_on_blueprints(unreal.YSFoeCharacter, "minion_target_distance_multiplier", 2.0)

    # 2) carte au trésor : un segment de plus
    map_cdo = unreal.get_default_object(assets.load_blueprint_class(TREASURE_MAP_BP))
    segments = map_cdo.get_editor_property("segment_count")
    if segments == 3:
        map_cdo.set_editor_property("segment_count", 4)
        assets.save_asset(TREASURE_MAP_BP, only_if_is_dirty=False)
    log(f"Carte au trésor : segment_count {segments} -> {map_cdo.get_editor_property('segment_count')}")

    # 3) shard Summon system
    summon, created = data_asset(SHARDS_DIR, "DA_Up_Shard_SummonSystem", unreal.YSUpShard)
    if created:
        summon.set_editor_property("display_name", "Summon system")
        summon.set_editor_property("description", "Minions max des armes d'invocation")
        summon.set_editor_property("stat", unreal.YSShardStat.SUMMON_SYSTEM)
        summon.set_editor_property("equation", unreal.YSShardEquation.TRUNCATED)
        summon.set_editor_property("base", 0.0)
        summon.set_editor_property("index", 0.3)
        summon.set_editor_property("max_level", 30)
        assets.save_loaded_asset(summon, only_if_is_dirty=False)
    ability_damage = assets.load_asset(f"{SHARDS_DIR}/DA_Up_Shard_AbilityDamage")

    # 4) upgrade Summoning
    summoning, created = data_asset(STATS_DIR, "DA_Up_Stats_Summoning", unreal.YSUpStats)
    if created:
        summoning.set_editor_property("display_name", "Summoning")
        summoning.set_editor_property("description", "Plus de minions et des minions plus forts")
        summoning.set_editor_property("shards", [summon, summon, ability_damage, summon, ability_damage])
        summoning.set_editor_property("blocking_shard", summon)
        old = find_asset(OLD_SUMMON_UPGRADE)
        if old:
            summoning.set_editor_property("icon", old.get_editor_property("icon"))
        assets.save_loaded_asset(summoning, only_if_is_dirty=False)
    log(f"DA_Up_Stats_Summoning : bloquée par {summoning.get_editor_property('blocking_shard').get_name()}, shards "
        f"{[s.get_name().replace('DA_Up_Shard_', '') for s in summoning.get_editor_property('shards')]}")

    # 5) ligne Captain de DT_Chara_Upgrades (upgrades existantes gardées)
    table = assets.load_asset(TABLE_PATH)
    rows = json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    current = []
    for row in rows:
        if row.get("Name") == CAPTAIN_ROW:
            for ref in row.get("Upgrades", []):
                if ref and ref != "None":
                    loaded = unreal.load_asset(ref.split("'")[1] if "'" in ref else ref)
                    if loaded:
                        current.append(loaded)
    if summoning.get_path_name() not in [u.get_path_name() for u in current]:
        current.append(summoning)
        unreal.YSEditorScriptLibrary.set_chara_upgrades_row(table, CAPTAIN_ROW, current)
        assets.save_loaded_asset(table, only_if_is_dirty=False)
    log(f"Ligne {CAPTAIN_ROW} : {[u.get_name() for u in current]}")
    log("OK")
except Exception as error:
    import traceback
    log(f"ERREUR : {error}\n{traceback.format_exc()}")

with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_tweaks_2026_10_25b.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
