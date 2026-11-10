"""
Étapes du GameBoat (demande de Paul, 2026-11-08) :
  - BP_GameBoat_Sloop : le canon centré remplacé par 2 canons à l'avant (Cannon Left / Cannon Right), écartés et tournés de 15° vers l'extérieur
  - /Game/Placeable/Ship/BP_GameBoat_Slooplus (AYSSlooplus) : BP_PirateShip en largeur et longueur x SLOOPLUS_XY, 1 canon avant centré, 1 bâbord, 1 tribord
  - /Game/Placeable/Ship/BP_GameBoat_Brigantin (AYSBrigantin) : x BRIGANTIN_XY, 2 canons à l'avant à 45° (Cannon Left 2 / Right 2), 1 bâbord, 1 tribord
  - /Game/Placeable/Ship/BP_GameBoat_Galion : enfant de BP_PirateShip (suit ses retouches), pas bateau objectif, pilotable
  - DA_GameBoat : étapes [chaloupe, sloop, slooplus, brigantin, galion]
  - BP_InteractiveObject_BoatCore : Upgrades Game Boat (chaque core livré fait passer à l'étape suivante)
Relançable : les Blueprints qui existent ne sont pas recréés (retouches de Paul gardées) ; les canons du sloop sont replacés à chaque fois.

Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/setup_game_boat_stages.py"
Rapport : Saved/Logs/setup_game_boat_stages.txt
"""
import math
import os
import unreal

SHIP_DIR = "/Game/Placeable/Ship"
PIRATE_BP = f"{SHIP_DIR}/BP_PirateShip"
LONGBOAT_BP = f"{SHIP_DIR}/BP_GameBoat_Longboat"
SLOOP_BP = f"{SHIP_DIR}/BP_GameBoat_Sloop"
SLOOPLUS_BP = f"{SHIP_DIR}/BP_GameBoat_Slooplus"
BRIGANTIN_BP = f"{SHIP_DIR}/BP_GameBoat_Brigantin"
GALION_BP = f"{SHIP_DIR}/BP_GameBoat_Galion"
DATA_DA = "/Game/Core/GameBoat/DA_GameBoat"
FLOW_GM = "/Game/Core/GameModes/BP_FlowMod"
BOAT_CORE = "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_BoatCore"

SLOOP_XY = 0.4
SLOOPLUS_XY = 0.6
BRIGANTIN_XY = 0.7  # 0.8 jusqu au 2026-11-10 (ramené par setup_boat_crew.py : moyenne du sloop et du galion)
# canons avant écartés : part de la demi-largeur du bateau
FRONT_PAIR_SPREAD = 0.3
SLOOP_FRONT_YAW = 15.0
BRIGANTIN_FRONT_YAW = 45.0

# seulement replacés (taille gardée), comme pour le sloop
KEEP_SIZE = ("MainMast", "ForeMast", "MizzenMast", "CrowsNest", "Helm", "HelmWheel", "Lantern", "FlagSkull", "Bowsprit")
SKIP_SHAPES = ("Cannon",)  # canons décoratifs : les vrais canons sont des postes
STATIONS = ("HelmStation", "CoreStation", "CrowsNestStation", "LongboatStation", "BoardingAccess", "BoardingAccessStarboard")

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
copy_props = unreal.YSEditorScriptLibrary.copy_editable_properties
report = []


def log(msg):
    unreal.log(f"[setup_game_boat_stages] {msg}")
    report.append(msg)


def v(x, y, z):
    return unreal.Vector(x, y, z)


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def components(bp):
    out = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if obj:
            out[obj.get_name().replace("_GEN_VARIABLE", "")] = obj
    return out


def lerp_size(xy, small, big):
    """Réglage lié à la taille : interpolé entre le sloop (x0.4) et le galion (x1)"""
    t = (xy - SLOOP_XY) / (1.0 - SLOOP_XY)
    return small + (big - small) * t


def scaled_shape(name, location, scale, xy):
    if name in KEEP_SIZE:
        return v(location.x * xy, location.y * xy, location.z), scale
    if "_Yard" in name or "_Sail" in name:
        return v(location.x * xy, location.y * xy, location.z), v(scale.x, scale.y, scale.z * xy)
    return v(location.x * xy, location.y * xy, location.z), v(scale.x * xy, scale.y * xy, scale.z)


def place_station(pirate, ship, name, location, rotation, source_name=None):
    """Poste name du bateau : classe et gabarit du poste source_name de BP_PirateShip"""
    origin = pirate.get_editor_property(source_name or name)
    comp = ship.get_editor_property(name)
    comp.set_child_actor_class(origin.get_editor_property("child_actor_class"))
    comp.set_editor_property("relative_location", location)
    comp.set_editor_property("relative_rotation", rotation)
    source_template = origin.get_editor_property("child_actor_template")
    template = comp.get_editor_property("child_actor_template")
    if source_template and template:
        copy_props(source_template, template)
    log(f"  {name} ({source_name or name}) en ({location.x:.0f}, {location.y:.0f}, {location.z:.0f}), yaw {rotation.yaw:.0f}")
    return comp


def scaled(pirate, name, xy):
    loc = pirate.get_editor_property(name).get_editor_property("relative_location")
    return v(loc.x * xy, loc.y * xy, loc.z)


def front_pair(pirate, ship, xy, left, right, yaw):
    """Deux canons à l'avant : place du canon avant de BP_PirateShip, écartés vers les flancs, tournés de yaw vers l'extérieur"""
    front = scaled(pirate, "CannonFront", xy)
    half_width = max(abs(p.y) for p in pirate.get_editor_property("HullProbePoints")) * xy
    spread = half_width * FRONT_PAIR_SPREAD
    base = pirate.get_editor_property("CannonFront").get_editor_property("relative_rotation")
    # un peu en arrière du canon avant : la proue s'affine
    x = front.x - spread * 1.5
    place_station(pirate, ship, left, v(x, front.y - spread, front.z), unreal.Rotator(roll=base.roll, pitch=base.pitch, yaw=base.yaw - yaw), "CannonFront")
    place_station(pirate, ship, right, v(x, front.y + spread, front.z), unreal.Rotator(roll=base.roll, pitch=base.pitch, yaw=base.yaw + yaw), "CannonFront")


def tune_size(ship, xy):
    """Réglages liés à la taille, entre ceux du sloop et du galion"""
    ship.get_editor_property("HelmStation").get_editor_property("child_actor_template").set_editor_property("CameraArmLength", lerp_size(xy, 3600.0, 5400.0))
    ship.get_editor_property("CoreStation").get_editor_property("child_actor_template").set_editor_property("AcceptRange", lerp_size(xy, 450.0, 900.0))
    ship.get_editor_property("LongboatStation").get_editor_property("child_actor_template").set_editor_property("SpawnDistanceBehindHelm", lerp_size(xy, 500.0, 800.0))


def make_scaled_ship(path, parent_class, xy, ship_name, cannons):
    if assets.does_asset_exist(path):
        log(f"existe déjà : {path} (gardé)")
        return False
    directory, name = path.rsplit("/", 1)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    bp = tools.create_asset(name, directory, unreal.Blueprint, factory)
    log(f"créé : {path} (x{xy})")

    source = components(assets.load_asset(PIRATE_BP))
    root = subsystem.k2_gather_subobject_data_for_blueprint(bp)[0]
    copied = 0
    for shape_name, comp in source.items():
        if not isinstance(comp, unreal.StaticMeshComponent) or not comp.get_editor_property("static_mesh") or shape_name.startswith(SKIP_SHAPES):
            continue
        params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.StaticMeshComponent, blueprint_context=bp)
        handle, fail = subsystem.add_new_subobject(params)
        if not lib.is_handle_valid(handle):
            raise RuntimeError(f"ajout de {shape_name} impossible : {fail}")
        subsystem.rename_subobject(handle, unreal.Text(shape_name))
        shape = lib.get_object(lib.get_data(handle))
        shape.set_editor_property("static_mesh", comp.get_editor_property("static_mesh"))
        shape.set_material(0, comp.get_material(0))
        location, scale = scaled_shape(shape_name, comp.get_editor_property("relative_location"), comp.get_editor_property("relative_scale3d"), xy)
        shape.set_editor_property("relative_location", location)
        shape.set_editor_property("relative_rotation", comp.get_editor_property("relative_rotation"))
        shape.set_editor_property("relative_scale3d", scale)
        shape.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        shape.set_collision_profile_name(comp.get_collision_profile_name())
        shape.set_editor_property("generate_overlap_events", False)
        shape.set_editor_property("can_character_step_up_on", comp.get_editor_property("can_character_step_up_on"))
        copied += 1
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    log(f"  {copied} formes copiées de BP_PirateShip")

    pirate = cdo(PIRATE_BP)
    ship = cdo(path)
    copy_props(pirate, ship)
    ship.set_editor_property("ShipName", unreal.Text(ship_name))
    ship.set_editor_property("bObjectiveShip", False)
    ship.set_editor_property("bDrivable", True)
    ship.set_editor_property("bPlaytestMode", False)
    ship.set_editor_property("HullProbePoints", [unreal.Vector2D(p.x * xy, p.y * xy) for p in pirate.get_editor_property("HullProbePoints")])
    for station in STATIONS:
        origin = pirate.get_editor_property(station)
        place_station(pirate, ship, station, scaled(pirate, station, xy), origin.get_editor_property("relative_rotation"))
    cannons(pirate, ship, xy)
    tune_size(ship, xy)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(path, only_if_is_dirty=False)
    return True


def slooplus_cannons(pirate, ship, xy):
    for name in ("CannonFront", "CannonLeft", "CannonRight"):
        place_station(pirate, ship, name, scaled(pirate, name, xy), pirate.get_editor_property(name).get_editor_property("relative_rotation"))


def brigantin_cannons(pirate, ship, xy):
    front_pair(pirate, ship, xy, "CannonLeft2", "CannonRight2", BRIGANTIN_FRONT_YAW)
    for name in ("CannonLeft", "CannonRight"):
        place_station(pirate, ship, name, scaled(pirate, name, xy), pirate.get_editor_property(name).get_editor_property("relative_rotation"))


def update_sloop():
    """Sloop : 2 canons à l'avant à 15° à la place du canon centré"""
    bp = assets.load_asset(SLOOP_BP)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    pirate = cdo(PIRATE_BP)
    sloop = cdo(SLOOP_BP)
    log("sloop :")
    front_pair(pirate, sloop, SLOOP_XY, "CannonLeft", "CannonRight", SLOOP_FRONT_YAW)
    # ancien canon centré : plus de poste (le composant reste dans les données du Blueprint)
    try:
        front = sloop.get_editor_property("CannonFront")
    except Exception:
        front = None
    if front:
        front.set_child_actor_class(None)
        log("  CannonFront vidé")
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(SLOOP_BP, only_if_is_dirty=False)


def update_brigantin_front():
    """Brigantin : 2 canons avant replacés à chaque fois (comme ceux du sloop)"""
    bp = assets.load_asset(BRIGANTIN_BP)
    log("brigantin :")
    front_pair(cdo(PIRATE_BP), cdo(BRIGANTIN_BP), BRIGANTIN_XY, "CannonLeft2", "CannonRight2", BRIGANTIN_FRONT_YAW)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(BRIGANTIN_BP, only_if_is_dirty=False)


def make_galion():
    if assets.does_asset_exist(GALION_BP):
        log(f"existe déjà : {GALION_BP} (gardé)")
    else:
        directory, name = GALION_BP.rsplit("/", 1)
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", assets.load_blueprint_class(PIRATE_BP))
        bp = tools.create_asset(name, directory, unreal.Blueprint, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        log(f"créé : {GALION_BP} (enfant de BP_PirateShip)")
    galion = cdo(GALION_BP)
    galion.set_editor_property("ShipName", unreal.Text("Galion"))
    galion.set_editor_property("bObjectiveShip", False)
    galion.set_editor_property("bDrivable", True)
    galion.set_editor_property("bPlaytestMode", False)
    unreal.BlueprintEditorLibrary.compile_blueprint(assets.load_asset(GALION_BP))
    assets.save_asset(GALION_BP, only_if_is_dirty=False)


def setup_data():
    data = assets.load_asset(DATA_DA)
    stages = [assets.load_blueprint_class(p) for p in (LONGBOAT_BP, SLOOP_BP, SLOOPLUS_BP, BRIGANTIN_BP, GALION_BP)]
    data.set_editor_property("Stages", stages)
    assets.save_asset(DATA_DA, only_if_is_dirty=False)
    log(f"DA_GameBoat : {[c.get_name() for c in data.get_editor_property('Stages')]}")


def setup_core():
    core = cdo(BOAT_CORE)
    core.set_editor_property("bUpgradesGameBoat", True)
    assets.save_asset(BOAT_CORE, only_if_is_dirty=False)
    log("BP_InteractiveObject_BoatCore : Upgrades Game Boat")


def dump_flow():
    gm = cdo(FLOW_GM)
    for prop in ("DoubloonGoal", "DoubloonGoalPerShipLevel", "bShipArrivesAtDoubloonGoal", "NextIslandsGap", "NextIslandsDistanceMultiplier", "FinalIslandGap",
                 "bDoubloonRewards", "bShardUpgrades", "ClassUpgradeLevel", "UpgradeChoiceCount"):
        try:
            log(f"BP_FlowMod.{prop} = {gm.get_editor_property(prop)}")
        except Exception as e:
            log(f"BP_FlowMod.{prop} : {e!r}")


try:
    dump_flow()
    update_sloop()
    make_scaled_ship(SLOOPLUS_BP, unreal.YSSlooplus, SLOOPLUS_XY, "Slooplus", slooplus_cannons)
    make_scaled_ship(BRIGANTIN_BP, unreal.YSBrigantin, BRIGANTIN_XY, "Brigantin", brigantin_cannons)
    update_brigantin_front()
    make_galion()
    setup_data()
    setup_core()
    for path in (SLOOP_BP, SLOOPLUS_BP, BRIGANTIN_BP, GALION_BP):
        ship = cdo(path)
        cannons = []
        for name in ("CannonFront", "CannonLeft", "CannonRight", "CannonLeft2", "CannonRight2"):
            try:
                comp = ship.get_editor_property(name)
            except Exception:
                comp = None
            if comp and comp.get_editor_property("child_actor_class"):
                loc = comp.get_editor_property("relative_location")
                cannons.append(f"{name} ({loc.x:.0f}, {loc.y:.0f}) yaw {comp.get_editor_property('relative_rotation').yaw:.0f}")
        hull = ship.get_editor_property("HullProbePoints")
        log(f"relu {path.rsplit('/', 1)[1]} : {ship.get_editor_property('ShipName')}, coque X {min(p.x for p in hull):.0f}..{max(p.x for p in hull):.0f}, "
            f"Y +/-{max(abs(p.y) for p in hull):.0f}, objectif {ship.get_editor_property('bObjectiveShip')}, canons : {cannons}")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_game_boat_stages.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
