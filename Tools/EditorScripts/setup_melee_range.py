"""
Upgrades du 2026-10-29 :
  - Atk speed et Shoot system ne sont plus jumelées (DA_Up_Shard_AtkSpeed : Twin Shard vidée)
  - nouvelle shard DA_Up_Shard_MeleeRange (« Melee Range » : taille des armes de mêlée, Multiplicative, Base 1, Index = Explosive Index
    du Shoot system (0.065) : x1 à 0 / 30, x2.95 à 30 / 30), jumelle du Shoot system (dans les deux sens)
  - nouvelle upgrade DA_Up_Stats_MeleeRange = copie de DA_Up_Stats_ShootSystem où chaque shard Shoot system devient Melee Range
    (bloquée par Melee Range, même icône), ajoutée à la ligne AllChara de DT_Chara_Upgrades
Ne recrée rien qui existe (relançable).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_melee_range.py"
Rapport : Saved/Logs/setup_melee_range.txt
"""
import os
import unreal

SHARDS_DIR = "/Game/Core/Upgrades/Up_Shards"
STATS_DIR = "/Game/Core/Upgrades/Up_Character"
TABLE_PATH = "/Game/Core/Upgrades/DT_Chara_Upgrades"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
report = []


def log(msg):
    unreal.log(f"[setup_melee_range] {msg}")
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


def short(shard):
    return shard.get_name().replace("DA_Up_Shard_", "") if shard else None


try:
    atk_speed = assets.load_asset(f"{SHARDS_DIR}/DA_Up_Shard_AtkSpeed")
    shoot = assets.load_asset(f"{SHARDS_DIR}/DA_Up_Shard_ShootSystem")

    # 1) shard Melee Range
    melee, created = data_asset(SHARDS_DIR, "DA_Up_Shard_MeleeRange", unreal.YSUpShard)
    if created:
        melee.set_editor_property("display_name", "Melee Range")
        melee.set_editor_property("description", "Agrandit les armes de mêlée (allonge et hitbox)")
        melee.set_editor_property("icon", shoot.get_editor_property("icon"))
        melee.set_editor_property("stat", unreal.YSShardStat.MELEE_RANGE)
        melee.set_editor_property("equation", unreal.YSShardEquation.MULTIPLICATIVE)
        melee.set_editor_property("base", 1.0)
        melee.set_editor_property("index", shoot.get_editor_property("explosive_index"))
        melee.set_editor_property("max_level", shoot.get_editor_property("max_level"))

    # 2) jumelles : Melee Range <-> Shoot system ; Atk speed seule
    atk_speed.set_editor_property("twin_shard", None)
    shoot.set_editor_property("twin_shard", melee)
    melee.set_editor_property("twin_shard", shoot)
    for shard in (atk_speed, shoot, melee):
        assets.save_loaded_asset(shard, only_if_is_dirty=False)
        log(f"{shard.get_name()} : stat {shard.get_editor_property('stat')}, base {shard.get_editor_property('base')}, "
            f"index {shard.get_editor_property('index')}, jumelle {short(shard.get_editor_property('twin_shard'))}")

    # 3) upgrade Melee Range (copie de Shoot system)
    source = assets.load_asset(f"{STATS_DIR}/DA_Up_Stats_ShootSystem")
    stats, created = data_asset(STATS_DIR, "DA_Up_Stats_MeleeRange", unreal.YSUpStats)
    if created:
        stats.set_editor_property("display_name", "Melee Range")
        stats.set_editor_property("description", "Agrandit les armes de mêlée")
        stats.set_editor_property("icon", source.get_editor_property("icon"))
        stats.set_editor_property("offer_weight", source.get_editor_property("offer_weight"))
        stats.set_editor_property("shards", [melee if s == shoot else s for s in source.get_editor_property("shards")])
        blocking = source.get_editor_property("blocking_shard")
        stats.set_editor_property("blocking_shard", melee if blocking == shoot or not blocking else blocking)
        assets.save_loaded_asset(stats, only_if_is_dirty=False)
    log(f"{stats.get_name()} : bloquée par {short(stats.get_editor_property('blocking_shard'))}, "
        f"shards {[short(s) for s in stats.get_editor_property('shards')]} (Shoot system : {[short(s) for s in source.get_editor_property('shards')]})")

    # 4) ligne AllChara
    table = assets.load_asset(TABLE_PATH)
    row = list(unreal.YSEditorScriptLibrary.get_chara_upgrades_row(table, "AllChara"))
    if stats in row:
        log("AllChara contient déjà DA_Up_Stats_MeleeRange")
    else:
        row.append(stats)
        unreal.YSEditorScriptLibrary.set_chara_upgrades_row(table, "AllChara", row)
        assets.save_loaded_asset(table, only_if_is_dirty=False)
        log(f"AllChara : DA_Up_Stats_MeleeRange ajoutée ({len(row)} upgrades)")
    log("OK")
except Exception as error:
    import traceback
    log(f"ERREUR : {error}\n{traceback.format_exc()}")

with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_melee_range.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
