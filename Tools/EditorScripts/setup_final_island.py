"""
Île finale, seconde planche d'embarquement (2026-11-07, demandes de Paul). Relançable, ne recrée rien qui existe :
  - BP_PirateShip, BP_NME_Galion, BP_GameBoat_Sloop : seconde planche d'embarquement GangplankStarboard = miroir de Gangplank (tribord, +Y),
    et ouverture du bastingage tribord en face (RailStarboard = miroir de RailPortFore, RailStarboardAft = miroir de RailPortAft).
  - /Game/Core/Islands/DA_I_FinalIsland : copie de DA_I_P1_objectifIsland, tous les biomes (tiré au hasard), rayon 150 m, jours infinis, sans
    objectif principal ; nuits : 2 élites (jours impairs) / 1 boss (jours pairs) jusqu'au jour 19, puis 2 boss chaque nuit à partir du jour 20.
    Un asset qui existe déjà n'est pas touché (réglages de Paul).
  - BP_FlowMod : Final Island = DA_I_FinalIsland.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_final_island.py"
Rapport : Saved/Logs/setup_final_island.txt
"""
import os
import unreal

ISLAND_DIR = "/Game/Core/Islands"
SOURCE_DA = f"{ISLAND_DIR}/DA_I_P1_objectifIsland"
FINAL_NAME = "DA_I_FinalIsland"
FINAL_DA = f"{ISLAND_DIR}/{FINAL_NAME}"
FLOW_MOD_BP = "/Game/Core/GameModes/BP_FlowMod"
SHIP_BPS = [
    "/Game/Placeable/Ship/BP_PirateShip",
    "/Game/Placeable/Ship/BP_NME_Galion",
    "/Game/Placeable/Ship/BP_GameBoat_Sloop",
]
FINAL_RADIUS = 15000.0
BOSS_EVERY_NIGHT_FROM_DAY = 20

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_final_island] {msg}")
    report.append(msg)


def save(path):
    assets.save_asset(path, only_if_is_dirty=False)


def components(bp):
    """{nom de variable: (handle, gabarit de ce Blueprint)}"""
    result = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(handle)
        obj = lib.get_object_for_blueprint(data, bp)
        if obj:
            result[str(lib.get_variable_name(data))] = (handle, obj)
    return result


def mirror_rotation(rotation):
    # symétrie par rapport au plan XZ (Y -> -Y)
    return unreal.Rotator(roll=-rotation.roll, pitch=rotation.pitch, yaw=-rotation.yaw)


def mirror_onto(source, target):
    location = source.get_editor_property("relative_location")
    target.set_editor_property("static_mesh", source.get_editor_property("static_mesh"))
    target.set_material(0, source.get_material(0))
    target.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    target.set_editor_property("relative_location", unreal.Vector(location.x, -location.y, location.z))
    target.set_editor_property("relative_rotation", mirror_rotation(source.get_editor_property("relative_rotation")))
    target.set_editor_property("relative_scale3d", source.get_editor_property("relative_scale3d"))
    target.set_collision_profile_name(source.get_collision_profile_name())
    target.set_editor_property("generate_overlap_events", source.get_editor_property("generate_overlap_events"))
    target.set_editor_property("can_character_step_up_on", source.get_editor_property("can_character_step_up_on"))
    target.set_editor_property("cast_shadow", source.get_editor_property("cast_shadow"))


def add_mirror(bp, comps, source_name, new_name):
    if new_name in comps:
        return f"{new_name} existe déjà"
    if source_name not in comps:
        return f"pas de {source_name}"
    source_handle, source = comps[source_name]
    parent = lib.get_data(source_handle)
    root = subsystem.k2_gather_subobject_data_for_blueprint(bp)[0]
    params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.StaticMeshComponent, blueprint_context=bp)
    handle, fail = subsystem.add_new_subobject(params)
    if not lib.is_handle_valid(handle):
        return f"ajout de {new_name} impossible : {fail}"
    subsystem.rename_subobject(handle, unreal.Text(new_name))
    target = lib.get_object(lib.get_data(handle))
    mirror_onto(source, target)
    return f"{new_name} = miroir de {source_name}"


def setup_ship(path):
    bp = assets.load_asset(path)
    comps = components(bp)
    if "GangplankStarboard" in comps:
        log(f"{path} : seconde planche déjà là")
        return
    done = [add_mirror(bp, comps, "Gangplank", "GangplankStarboard")]
    # ouverture du bastingage tribord en face de la planche
    if "RailPortFore" in comps and "RailPortAft" in comps and "RailStarboard" in comps:
        mirror_onto(comps["RailPortFore"][1], comps["RailStarboard"][1])
        done.append("RailStarboard = miroir de RailPortFore")
        done.append(add_mirror(bp, comps, "RailPortAft", "RailStarboardAft"))
    else:
        done.append("bastingage tribord pas trouvé (RailPortFore / RailPortAft / RailStarboard)")
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(path)
    log(f"{path} : " + ", ".join(done))


def classes_of(event):
    result = []
    for foe in event.get_editor_property("Foes"):
        if foe.get_editor_property("FoeClass"):
            result.append(foe.get_editor_property("FoeClass"))
        result.extend(foe.get_editor_property("RandomFoeClasses"))
    return result


def center_event(day, period, day_max, classes, count):
    event = unreal.YSIslandSpawnEvent()
    event.set_editor_property("Time", 0.0)
    event.set_editor_property("IslandDay", day)
    event.set_editor_property("IslandDayPeriod", period)
    event.set_editor_property("IslandDayMax", day_max)
    event.set_editor_property("RepeatCount", 1)
    event.set_editor_property("Shape", unreal.YSIslandSpawnShape.CENTER)
    event.set_editor_property("bScaleWithPlayers", False)
    entry = unreal.YSIslandFoeEntry()
    entry.set_editor_property("RandomFoeClasses", classes)
    entry.set_editor_property("Count", count)
    event.set_editor_property("Foes", [entry])
    return event


def setup_final_island():
    if assets.does_asset_exist(FINAL_DA):
        log(f"{FINAL_DA} existe déjà : pas touchée")
        return assets.load_asset(FINAL_DA)
    data = assets.duplicate_asset(SOURCE_DA, FINAL_DA)
    data.set_editor_property("IslandName", unreal.Text("Île finale"))
    data.set_editor_property("RandomGenerationTypes", [unreal.YSIslandGeneration.TROPICAL, unreal.YSIslandGeneration.DESERT, unreal.YSIslandGeneration.PORT_TOWN])
    data.set_editor_property("bRandomRotation", True)
    rules = data.get_editor_property("Generation")
    rules.set_editor_property("Radius", FINAL_RADIUS)
    data.set_editor_property("Generation", rules)
    data.set_editor_property("bInfiniteDays", True)
    data.set_editor_property("MainObjectives", [])

    # élites / boss de la copie (nuit 1 : élite Skull + élite Gun, nuit 2 : Boss Skull ou Boss Gun) remplacés par le cycle infini
    elites, bosses, kept = [], [], []
    for event in data.get_editor_property("NightEvents"):
        day = event.get_editor_property("IslandDay")
        if event.get_editor_property("Shape") == unreal.YSIslandSpawnShape.CENTER and day == 1:
            elites.extend(classes_of(event))
        elif event.get_editor_property("Shape") == unreal.YSIslandSpawnShape.CENTER and day == 2:
            bosses.extend(classes_of(event))
        else:
            kept.append(event)
    last = BOSS_EVERY_NIGHT_FROM_DAY - 1
    night = [
        center_event(1, 2, last, elites, 2),                                 # jours 1, 3, 5... 19 : 2 élites
        center_event(2, 2, last, bosses, 1),                                 # jours 2, 4, 6... 18 : 1 boss
        center_event(BOSS_EVERY_NIGHT_FROM_DAY, 1, 0, bosses, 2),            # à partir du jour 20 : 2 boss
    ] + kept
    data.set_editor_property("NightEvents", night)
    save(FINAL_DA)
    log(f"{FINAL_DA} créée : rayon {FINAL_RADIUS / 100:.0f} m, biomes au hasard, jours infinis ; élites {[c.get_name() for c in elites]}, "
        f"boss {[c.get_name() for c in bosses]}, {len(kept)} autre(s) vague(s) de nuit gardée(s), {len(data.get_editor_property('DayEvents'))} vague(s) de jour, "
        f"{len(data.get_editor_property('SecondaryObjectives'))} objectif(s) secondaire(s)")
    return data


def setup_flow_mod(final):
    cdo = unreal.get_default_object(assets.load_blueprint_class(FLOW_MOD_BP))
    if not cdo.get_editor_property("FinalIsland"):
        cdo.set_editor_property("FinalIsland", final)
        save(FLOW_MOD_BP)
    log(f"BP_FlowMod : Final Island = {cdo.get_editor_property('FinalIsland').get_name()}")


try:
    for ship in SHIP_BPS:
        if assets.does_asset_exist(ship):
            setup_ship(ship)
    setup_flow_mod(setup_final_island())
    log("Terminé")
except Exception as error:
    log(f"ERREUR : {error}")
    raise
finally:
    path = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_final_island.txt")
    with open(path, "w", encoding="utf-8") as file:
        file.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
