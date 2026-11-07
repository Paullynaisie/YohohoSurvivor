"""
Batailles navales, îles prégénérées, Port Town, bastingages bas et second accès (2026-11-07, demandes de Paul). Relançable :
  - /Game/Core/NavalBattles/DT_NavalBattles (lignes FYSNavalBattleRow) avec une ligne « Base » (Galion + 3 Jolly Ships, renforts de 3 toutes
    les 30 s, activation à 75 m, Galion qui fonce après 60 s) ; une table qui existe déjà n'est jamais touchée (réglages de Paul).
  - BP_FlowMod : Naval Battle Table, 2 batailles « Base », plus de vagues de bateaux de NME (Jolly Ship / Galion / go naviguer).
  - DA_I_P1_objectifIsland (Port Town) : ponton 3x plus large et collé à la falaise, 2x plus d'accès depuis l'eau.
  - Tous les bateaux : bastingages (composants Rail* / CastleRail*) abaissés à RAIL_HEIGHT pour sauter à l'eau de partout ;
    second accès BoardingAccessStarboard = miroir de BoardingAccess (côté tribord).
  - BP_NME_Galion : matériau des planches d'abordage.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_naval_battles.py"
Rapport : Saved/Logs/setup_naval_battles.txt
"""
import json
import os
import unreal

PH = "/Game/Art/Materials/PH_materials/m_PH_"
TABLE_DIR = "/Game/Core/NavalBattles"
TABLE_NAME = "DT_NavalBattles"
TABLE_PATH = f"{TABLE_DIR}/{TABLE_NAME}"
FLOW_MOD_BP = "/Game/Core/GameModes/BP_FlowMod"
OBJECTIVE_DA = "/Game/Core/Islands/DA_I_P1_objectifIsland"
GALION_BP = "/Game/Placeable/Ship/BP_NME_Galion"
JOLLY_BP = "/Game/Placeable/Ship/BP_NME_JollyShip"
SHIP_BPS = [
    "/Game/Placeable/Ship/BP_PirateShip",
    "/Game/Placeable/Ship/BP_NME_Galion",
    "/Game/Placeable/Ship/BP_GameBoat_Sloop",
    "/Game/Placeable/Ship/BP_Longboat",
    "/Game/Placeable/Ship/BP_GameBoat_Longboat",
    "/Game/Placeable/Ship/BP_NME_JollyShip",
]

RAIL_HEIGHT = 30.0          # sous la hauteur de marche des personnages (45) : on l'enjambe
OLD_PIER_WIDTH = 420.0
PIER_WIDTH_MULTIPLIER = 3.0
OLD_WATER_ACCESS_SPACING = 3000.0

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_naval_battles] {msg}")
    report.append(msg)


def save(path):
    assets.save_asset(path, only_if_is_dirty=False)


def class_path(bp_path):
    name = bp_path.split("/")[-1]
    return f"{bp_path}.{name}_C"


def components(bp):
    """(nom de variable, gabarit propre à ce Blueprint) de tous ses composants, hérités compris"""
    result = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(handle)
        obj = lib.get_object_for_blueprint(data, bp)
        if obj:
            result.append((str(lib.get_variable_name(data)), obj))
    return result


def setup_table():
    if assets.does_asset_exist(TABLE_PATH):
        log(f"{TABLE_PATH} existe déjà : pas touchée")
        return assets.load_asset(TABLE_PATH)
    factory = unreal.DataTableFactory()
    factory.struct = unreal.YSNavalBattleRow.static_struct()
    table = tools.create_asset(TABLE_NAME, TABLE_DIR, unreal.DataTable, factory)
    rows = [{
        "Name": "Base",
        "GalionClass": class_path(GALION_BP),
        "GalionCount": 1,
        "JollyShipClass": class_path(JOLLY_BP),
        "InitialJollyShips": 3,
        "ReinforcementJollyShips": 3,
        "ReinforcementInterval": 30.0,
        "bStopReinforcementsWhenGalionsDefeated": True,
        "ActivationDistance": 7500.0,
        "SpawnRadius": 4000.0,
        "GalionRamDelay": 60.0,
    }]
    ok = unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(rows))
    save(TABLE_PATH)
    log(f"{TABLE_PATH} créée (ligne Base, import {'ok' if ok else 'ÉCHEC'})")
    return table


def setup_flow_mod(table):
    cdo = unreal.get_default_object(assets.load_blueprint_class(FLOW_MOD_BP))
    cdo.set_editor_property("NavalBattleTable", table)
    if len(cdo.get_editor_property("NavalBattleRows")) == 0:
        cdo.set_editor_property("NavalBattleRows", ["Base", "Base"])
    cdo.set_editor_property("NavigationWaves", [])
    cdo.set_editor_property("bJollyShipWaves", False)
    cdo.set_editor_property("bGalionWaves", False)
    save(FLOW_MOD_BP)
    log(f"BP_FlowMod : batailles {list(cdo.get_editor_property('NavalBattleRows'))}, plus de vagues de bateaux de NME")


def setup_port_town():
    data = assets.load_asset(OBJECTIVE_DA)
    rules = data.get_editor_property("generation")
    width = rules.get_editor_property("PierWidth")
    if abs(width - OLD_PIER_WIDTH) < 1.0:
        rules.set_editor_property("PierWidth", OLD_PIER_WIDTH * PIER_WIDTH_MULTIPLIER)
    rules.set_editor_property("PierCoastOffset", 0.0)
    spacing = rules.get_editor_property("PierWaterAccessSpacing")
    if abs(spacing - OLD_WATER_ACCESS_SPACING) < 1.0:
        rules.set_editor_property("PierWaterAccessSpacing", OLD_WATER_ACCESS_SPACING * 0.5)
    data.set_editor_property("generation", rules)
    save(OBJECTIVE_DA)
    log(f"{OBJECTIVE_DA} : ponton {rules.get_editor_property('PierWidth'):.0f} cm collé à la falaise, rampes {rules.get_editor_property('PierRampWidth'):.0f} cm, "
        f"accès depuis l'eau tous les {rules.get_editor_property('PierWaterAccessSpacing'):.0f} cm, {rules.get_editor_property('TownTreeChance'):.2f} de palmiers en ville")


def setup_ship(path):
    bp = assets.load_asset(path)
    comps = components(bp)
    lowered = 0
    for name, comp in comps:
        if not isinstance(comp, unreal.StaticMeshComponent) or not (name.startswith("Rail") or name.startswith("CastleRail")):
            continue
        scale = comp.get_editor_property("relative_scale3d")
        height = scale.z * 100.0
        if height <= RAIL_HEIGHT + 5.0:
            continue
        location = comp.get_editor_property("relative_location")
        comp.set_editor_property("relative_location", unreal.Vector(location.x, location.y, location.z - height * 0.5 + RAIL_HEIGHT * 0.5))
        comp.set_editor_property("relative_scale3d", unreal.Vector(scale.x, scale.y, RAIL_HEIGHT / 100.0))
        lowered += 1

    by_name = dict(comps)
    port = by_name.get("BoardingAccess")
    starboard = by_name.get("BoardingAccessStarboard")
    mirrored = ""
    if isinstance(port, unreal.ChildActorComponent) and isinstance(starboard, unreal.ChildActorComponent):
        location = port.get_editor_property("relative_location")
        rotation = port.get_editor_property("relative_rotation")
        child_class = port.get_editor_property("child_actor_class")
        if child_class:
            starboard.set_child_actor_class(child_class)
        starboard.set_editor_property("relative_location", unreal.Vector(location.x, -location.y, location.z))
        starboard.set_editor_property("relative_rotation", unreal.Rotator(roll=-rotation.roll, pitch=rotation.pitch, yaw=-rotation.yaw))
        source = port.get_editor_property("child_actor_template")
        target = starboard.get_editor_property("child_actor_template")
        copied = unreal.YSEditorScriptLibrary.copy_editable_properties(source, target) if source and target else 0
        mirrored = f", second accès en ({location.x:.0f}, {-location.y:.0f}, {location.z:.0f}) yaw {-rotation.yaw:.0f} ({copied} réglage(s) copiés)"

    if path == GALION_BP:
        cdo = unreal.get_default_object(assets.load_blueprint_class(path))
        cdo.set_editor_property("BoardingPlankMaterial", assets.load_asset(f"{PH}monochrome_Brown"))

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(path)
    log(f"{path} : {lowered} bastingage(s) abaissé(s) à {RAIL_HEIGHT:.0f} cm{mirrored}")


try:
    table = setup_table()
    setup_flow_mod(table)
    setup_port_town()
    for ship in SHIP_BPS:
        if assets.does_asset_exist(ship):
            setup_ship(ship)
    log("Terminé")
except Exception as error:
    log(f"ERREUR : {error}")
    raise
finally:
    path = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_naval_battles.txt")
    with open(path, "w", encoding="utf-8") as file:
        file.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
