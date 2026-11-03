"""
Lecture seule : réglages utiles aux îles P1 (DA_I_StartIsland, BP_FlowMod, FlowMap, Galion, props, coffres).
  UnrealEditor-Cmd.exe YohohoSurvivor.uproject -run=pythonscript -script="Tools/EditorScripts/inspect_flow_islands.py" -unattended
Rapport : Saved/Logs/inspect_flow_islands.txt
"""
import os
import unreal

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    report.append(str(msg))


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def find_asset(name, root="/Game"):
    for path in assets.list_assets(root, recursive=True):
        package = path.split(".")[0]
        if package.rsplit("/", 1)[1] == name:
            return package
    return None


def components(path):
    out = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(assets.load_asset(path)):
        obj = lib.get_object(lib.get_data(handle))
        if obj:
            out.append(obj)
    return out


try:
    data = assets.load_asset("/Game/Core/Islands/DA_I_StartIsland")
    for p in ["max_days", "day_duration", "sink_delay", "sink_duration", "sink_depth", "boat_zone_radius", "fixed_seed"]:
        log(f"start.{p} = {data.get_editor_property(p)}")
    rules = data.get_editor_property("generation")
    for p in ["radius", "coast_noise", "plateau_height", "beach_depth", "massif_count_min", "water_access_count", "water_access_players", "massif_layouts", "massif_attempts", "rim_width"]:
        log(f"start.gen.{p} = {rules.get_editor_property(p)}")
    for kind in ["main_objectives", "secondary_objectives"]:
        for o in data.get_editor_property(kind):
            log(f"start.{kind}: {o.get_editor_property('display_name')} item {o.get_editor_property('item_class')} delivery {o.get_editor_property('delivery_class')} dist {o.get_editor_property('distance_from_boat')}")
    for kind in ["day_events", "night_events", "sink_events"]:
        for ev in data.get_editor_property(kind):
            foes = [(f.get_editor_property("foe_class").get_name() if f.get_editor_property("foe_class") else None, f.get_editor_property("count")) for f in ev.get_editor_property("foes")]
            log(f"start.{kind}: t={ev.get_editor_property('time')} rep={ev.get_editor_property('repeat_count')}/{ev.get_editor_property('repeat_interval')} {ev.get_editor_property('shape')} {foes} scale={ev.get_editor_property('scale_with_players')} ot={ev.get_editor_property('overtime_level')}")

    gm = cdo("/Game/Core/GameModes/BP_FlowMod")
    for p in ["day_night_cycle", "jolly_ship_waves", "galion_waves", "ship_arrives_at_doubloon_goal", "jolly_ship_class", "galion_class", "galion_min_spawn_distance", "galion_max_spawn_distance",
              "jolly_ship_min_spawn_distance", "jolly_ship_max_spawn_distance", "overtime", "spawn_rules"]:
        try:
            log(f"flow.{p} = {gm.get_editor_property(p)}")
        except Exception as e:
            log(f"flow.{p} : {e}")

    galion = find_asset("BP_NME_Galion")
    log(f"galion {galion} captain {cdo(galion).get_editor_property('CaptainClass')}")

    for name in ["BP_Prop_PalmTree", "BP_Prop_Bush", "BP_Prop_Pier", "BP_Prop"]:
        path = find_asset(name, "/Game/Art/Props")
        log(f"== {name} {path} parent {unreal.SystemLibrary.get_class_display_name(assets.load_blueprint_class(path).get_super_class()) if hasattr(assets.load_blueprint_class(path), 'get_super_class') else ''}")
        for comp in components(path):
            line = f"  {comp.get_name()} {comp.get_class().get_name()}"
            if isinstance(comp, unreal.StaticMeshComponent):
                line += f" mesh {comp.get_editor_property('static_mesh').get_name() if comp.get_editor_property('static_mesh') else None} mat {comp.get_material(0).get_path_name() if comp.get_material(0) else None} loc {comp.get_editor_property('relative_location')} scale {comp.get_editor_property('relative_scale3d')} coll {comp.get_collision_profile_name()}"
            if isinstance(comp, unreal.PrimitiveComponent) and not isinstance(comp, unreal.StaticMeshComponent):
                line += f" coll {comp.get_collision_profile_name()}"
            log(line)

    for name in ["BP_InteractiveObject_Chest", "BP_InteractiveObject_TreasureChest"]:
        path = find_asset(name, "/Game/Placeable")
        o = cdo(path)
        log(f"{name} {path} doublons {o.get_editor_property('doubloon_value')} plumes {o.get_editor_property('phoenix_feather_value')}")
    log(f"medic {find_asset('BP_NPC_Medic')}, carpenter npc {find_asset('BP_NPC_Carpenter')}")
    for name in ["FoeCharacter_Skull_Boss", "FoeCharacter_Skull_Gun_Boss", "FoeCharacter_Skull_Elite", "FoeCharacter_Skull_Gun_Elite"]:
        log(f"{name} loot {cdo(find_asset(name)).get_editor_property('death_loot_class')}")

    # FlowMap : mer et deadzone
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).load_level("/Game/Maps/FlowMap")
    for actor in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
        cls = actor.get_class().get_name()
        if "Ocean" in cls or "Light" in cls or "Fog" in cls or "PostProcess" in cls or "Sky" in cls:
            log(f"flowmap {actor.get_name()} {cls} loc {actor.get_actor_location()} scale {actor.get_actor_scale3d()}")
    ws = unreal.EditorLevelLibrary.get_editor_world().get_world_settings()
    log(f"flowmap killz {ws.get_editor_property('kill_z')}")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "inspect_flow_islands.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
