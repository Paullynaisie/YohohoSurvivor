"""
Lecture seule : arborescence et réglages des Blueprints utilisés pour Davy Jones et les cartes au trésor
(Captain, Boss Skull, son set, crochet, Galion, carte au trésor, super carte, FlowMod, arène du Kraken).

Lancement :
  UnrealEditor-Cmd.exe YohohoSurvivor.uproject -run=pythonscript -script="Tools/EditorScripts/inspect_davy_jones.py" -unattended
Rapport : Saved/Logs/inspect_davy_jones.txt
"""
import os
import unreal

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
report = []


def log(msg):
    report.append(str(msg))


def find_path(name):
    for data in registry.get_assets_by_path("/Game", recursive=True):
        if str(data.asset_name) == name:
            return str(data.package_name)
    return None


def safe(obj, prop):
    try:
        return obj.get_editor_property(prop)
    except Exception as exc:  # noqa
        return f"<{exc}>"


def tree(name, props=()):
    path = find_path(name)
    log(f"=== {name} : {path}")
    if not path:
        return
    bp = assets.load_asset(path)
    try:
        log(f"  parent : {unreal.BlueprintEditorLibrary.get_blueprint_parent_class(bp).get_name() if hasattr(unreal.BlueprintEditorLibrary, 'get_blueprint_parent_class') else bp.get_editor_property('parent_class').get_name()}")
    except Exception as exc:  # noqa
        log(f"  parent : ? {exc}")
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    for h in handles:
        data = lib.get_data(h)
        obj = lib.get_object(data)
        var = lib.get_variable_name(data)
        if obj is None:
            continue
        line = f"  - {var} [{obj.get_class().get_name()}]"
        parent = lib.get_data(lib.get_parent_handle(data)) if hasattr(lib, "get_parent_handle") else None
        if parent is not None:
            pobj = lib.get_object(parent)
            line += f" parent={lib.get_variable_name(parent)}"
        if isinstance(obj, unreal.SceneComponent):
            line += f" loc={obj.get_editor_property('relative_location')} rot={obj.get_editor_property('relative_rotation')} scale={obj.get_editor_property('relative_scale3d')}"
            att = obj.get_attach_parent()
            if att:
                line += f" attach={att.get_name()}"
        if isinstance(obj, unreal.StaticMeshComponent):
            mesh = obj.get_editor_property("static_mesh")
            mats = obj.get_editor_property("override_materials")
            line += f" mesh={mesh.get_name() if mesh else None} mats={[m.get_path_name() if m else None for m in mats]}"
        if isinstance(obj, unreal.ChildActorComponent):
            cls = obj.get_editor_property("child_actor_class")
            line += f" child={cls.get_name() if cls else None}"
        log(line)
    cdo = unreal.get_default_object(assets.load_blueprint_class(path))
    for prop in props:
        value = safe(cdo, prop)
        if hasattr(value, "get_path_name"):
            value = value.get_path_name()
        log(f"  {prop} = {value}")


try:
    tree("PlayerCharacter_Captain", ["customization_color_component_name"])
    tree("FoeCharacter_Skull_Boss", ["weapon_set", "base_health", "health_per_level", "health_multiplier", "size_multiplier", "foe_name", "death_loot_class",
                                     "boss_shield", "max_rage", "is_elite", "numpad_spawn_key", "b_is_elite", "level_from_day_offset", "b_hud_health_bar"])
    boss_set = find_path("DA_WeaponSet_NME_EliteGreatSword")
    for set_name in ["DA_WeaponSet_NME_EliteGreatSword", "DA_WeaponSet_NME_GreatSword", "DA_WeaponSet_NME_Sword", "DA_WeaponSet_Captain"]:
        p = find_path(set_name)
        if p:
            ws = assets.load_asset(p)
            r = ws.get_editor_property("right_hand_weapon")
            l = ws.get_editor_property("left_hand_weapon")
            log(f"=== {set_name} : droite {r.get_name() if r else None}, gauche {l.get_name() if l else None}")
    tree("BP_NME_EliteGreatSword")
    tree("BP_NME_Sword")
    tree("BP_CaptainHook")
    tree("BP_NME_Galion", ["crew_groups", "captain_class", "captain_classes"])
    tree("BP_InteractiveObject_TreasureMap", ["dash_color", "dash_material", "reward_classes", "segment_count"])
    tree("BP_InteractiveObject_SuperTreasureMap", ["display_name"])
    tree("BP_FlowMod", ["compass_item_class", "super_treasure_map_class", "kraken_arena_class", "kraken_arena_distance", "boat_core_item_class", "crew_item_classes",
                        "galion_class", "start_island", "next_islands"])
    tree("BP_KrakenArena", ["tentacle_range_class", "stage1_range_tentacles"])
    for data in registry.get_assets_by_path("/Game", recursive=True):
        n = str(data.asset_name)
        if any(k in n.lower() for k in ["crew", "coreboat", "boatcore", "skull_bomb", "skull_gun", "skull_elite", "foecharacter_skull"]):
            log(f"asset : {data.package_name}")
except Exception as exc:  # noqa
    import traceback
    log(f"ERREUR : {exc}\n{traceback.format_exc()}")

out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "inspect_davy_jones.txt")
with open(out, "w", encoding="utf-8") as f:
    f.write("\n".join(report))
