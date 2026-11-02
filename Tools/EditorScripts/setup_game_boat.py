"""
GameBoat (bateau des joueurs de toute la partie, FlowMod) :
  - /Game/Placeable/Ship/Stations/BP_GameBoatSeat : place d'un joueur (caméra pendant la traversée)
  - /Game/Placeable/Ship/BP_GameBoat_Longboat : copie de BP_Longboat reparentée sur AYSGameBoatLongboat (4 places, caméra de la sélection)
  - /Game/Placeable/Ship/Stations/BP_GameBoatZone : zone de dépôt des objectifs (AYSStartBoat sans visuel), posée où la chaloupe accoste
  - /Game/Placeable/Ship/BP_GameBoat_Sloop : miniature de BP_PirateShip (largeur et longueur x SLOOP_XY, hauteurs gardées), 1 canon à l'avant, core, barre, nid d'pie, chalouperie
  - /Game/Core/GameBoat/DA_GameBoat : étapes [chaloupe, sloop]
  - BP_FlowMod : Game Boat Data ; DA_I_StartIsland : Start Boat Class = la zone (plus de chaloupe de test échouée)
Relançable : ne recrée rien qui existe (retouches de Paul gardées).

Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/setup_game_boat.py"
Rapport : Saved/Logs/setup_game_boat.txt
"""
import os
import unreal

SHIP_DIR = "/Game/Placeable/Ship"
STATION_DIR = f"{SHIP_DIR}/Stations"
LONGBOAT_BP = f"{SHIP_DIR}/BP_Longboat"
PIRATE_BP = f"{SHIP_DIR}/BP_PirateShip"
BOAT_BP = f"{SHIP_DIR}/BP_GameBoat_Longboat"
SLOOP_BP = f"{SHIP_DIR}/BP_GameBoat_Sloop"
SEAT_BP = f"{STATION_DIR}/BP_GameBoatSeat"
ZONE_BP = f"{STATION_DIR}/BP_GameBoatZone"
DATA_DIR = "/Game/Core/GameBoat"
DATA_DA = f"{DATA_DIR}/DA_GameBoat"
FLOW_GM = "/Game/Core/GameModes/BP_FlowMod"
ISLAND_DA = "/Game/Core/Islands/DA_I_StartIsland"
START_BOAT = f"{SHIP_DIR}/BP_StartBoat"

SLOOP_XY = 0.4
# seulement replacés (taille gardée), comme au doublement du bateau
KEEP_SIZE = ("MainMast", "ForeMast", "MizzenMast", "CrowsNest", "Helm", "HelmWheel", "Lantern", "FlagSkull", "Bowsprit")
SKIP_SHAPES = ("Cannon",)  # canons décoratifs : le sloop n'a que son vrai canon

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
copy_props = unreal.YSEditorScriptLibrary.copy_editable_properties
report = []


def log(msg):
    unreal.log(f"[setup_game_boat] {msg}")
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


def create_bp(path, parent_class):
    if assets.does_asset_exist(path):
        log(f"existe déjà : {path}")
        return None
    directory, name = path.rsplit("/", 1)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    bp = tools.create_asset(name, directory, unreal.Blueprint, factory)
    log(f"créé : {path}")
    return bp


def sync_template(comp):
    """Gabarit de l'acteur enfant = réglages du Blueprint du poste"""
    cls = comp.get_editor_property("child_actor_class")
    template = comp.get_editor_property("child_actor_template")
    if cls and template:
        return copy_props(unreal.get_default_object(cls), template)
    return 0


def make_seat():
    bp = create_bp(SEAT_BP, unreal.YSGameBoatSeat)
    if bp:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assets.save_asset(SEAT_BP, only_if_is_dirty=False)


def make_longboat():
    if assets.does_asset_exist(BOAT_BP):
        log(f"existe déjà : {BOAT_BP}")
        return
    assets.duplicate_asset(LONGBOAT_BP, BOAT_BP)
    bp = assets.load_asset(BOAT_BP)
    unreal.BlueprintEditorLibrary.reparent_blueprint(bp, unreal.YSGameBoatLongboat)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    boat = cdo(BOAT_BP)
    seat_class = assets.load_blueprint_class(SEAT_BP)
    for name in ("Seat1", "Seat2", "Seat3", "Seat4"):
        comp = boat.get_editor_property(name)
        comp.set_child_actor_class(seat_class)
        sync_template(comp)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(BOAT_BP, only_if_is_dirty=False)
    log(f"créé : {BOAT_BP} (copie de BP_Longboat)")


def make_zone():
    bp = create_bp(ZONE_BP, unreal.YSStartBoat)
    if not bp:
        return
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    zone = cdo(ZONE_BP)
    # réglages de zone de la chaloupe de test (couleur, matériau, bulle)
    copy_props(cdo(START_BOAT), zone)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(ZONE_BP, only_if_is_dirty=False)


def scaled_shape(name, location, scale):
    """Largeur et longueur x SLOOP_XY, hauteur gardée ; mâts et barre seulement replacés"""
    if name in KEEP_SIZE:
        return v(location.x * SLOOP_XY, location.y * SLOOP_XY, location.z), scale
    if "_Yard" in name or "_Sail" in name:
        # vergues et voiles : plus courtes (axe Z local = le long de Y), même épaisseur
        return v(location.x * SLOOP_XY, location.y * SLOOP_XY, location.z), v(scale.x, scale.y, scale.z * SLOOP_XY)
    return v(location.x * SLOOP_XY, location.y * SLOOP_XY, location.z), v(scale.x * SLOOP_XY, scale.y * SLOOP_XY, scale.z)


def make_sloop():
    bp = create_bp(SLOOP_BP, unreal.YSSloop)
    if not bp:
        return
    source = components(assets.load_asset(PIRATE_BP))
    root = subsystem.k2_gather_subobject_data_for_blueprint(bp)[0]
    copied = 0
    for name, comp in source.items():
        if not isinstance(comp, unreal.StaticMeshComponent) or not comp.get_editor_property("static_mesh") or name.startswith(SKIP_SHAPES):
            continue
        params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.StaticMeshComponent, blueprint_context=bp)
        handle, fail = subsystem.add_new_subobject(params)
        if not lib.is_handle_valid(handle):
            raise RuntimeError(f"ajout de {name} impossible : {fail}")
        subsystem.rename_subobject(handle, unreal.Text(name))
        shape = lib.get_object(lib.get_data(handle))
        shape.set_editor_property("static_mesh", comp.get_editor_property("static_mesh"))
        shape.set_material(0, comp.get_material(0))
        location, scale = scaled_shape(name, comp.get_editor_property("relative_location"), comp.get_editor_property("relative_scale3d"))
        shape.set_editor_property("relative_location", location)
        shape.set_editor_property("relative_rotation", comp.get_editor_property("relative_rotation"))
        shape.set_editor_property("relative_scale3d", scale)
        shape.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        shape.set_collision_profile_name(comp.get_collision_profile_name())
        shape.set_editor_property("generate_overlap_events", False)
        shape.set_editor_property("can_character_step_up_on", comp.get_editor_property("can_character_step_up_on"))
        copied += 1
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    log(f"sloop : {copied} formes copiées de BP_PirateShip")

    pirate = cdo(PIRATE_BP)
    sloop = cdo(SLOOP_BP)
    copy_props(pirate, sloop)
    sloop.set_editor_property("ShipName", unreal.Text("Sloop"))
    sloop.set_editor_property("bObjectiveShip", False)
    sloop.set_editor_property("bDrivable", True)
    sloop.set_editor_property("bPlaytestMode", False)
    sloop.set_editor_property("HullProbePoints", [unreal.Vector2D(p.x * SLOOP_XY, p.y * SLOOP_XY) for p in pirate.get_editor_property("HullProbePoints")])

    # postes : mêmes classes que le bateau pirate, replacés
    for name in ("HelmStation", "CoreStation", "CrowsNestStation", "LongboatStation", "CannonFront", "BoardingAccess"):
        origin = pirate.get_editor_property(name)
        comp = sloop.get_editor_property(name)
        comp.set_child_actor_class(origin.get_editor_property("child_actor_class"))
        location = origin.get_editor_property("relative_location")
        comp.set_editor_property("relative_location", v(location.x * SLOOP_XY, location.y * SLOOP_XY, location.z))
        comp.set_editor_property("relative_rotation", origin.get_editor_property("relative_rotation"))
        sync_template(comp)
        log(f"sloop.{name} en ({location.x * SLOOP_XY:.0f}, {location.y * SLOOP_XY:.0f}, {location.z:.0f})")
    # réglages liés à la taille : barre dézoomée moins loin, zone du core et chaloupe lancée plus près
    sloop.get_editor_property("HelmStation").get_editor_property("child_actor_template").set_editor_property("CameraArmLength", 3600.0)
    sloop.get_editor_property("CoreStation").get_editor_property("child_actor_template").set_editor_property("AcceptRange", 450.0)
    sloop.get_editor_property("LongboatStation").get_editor_property("child_actor_template").set_editor_property("SpawnDistanceBehindHelm", 500.0)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(SLOOP_BP, only_if_is_dirty=False)


def make_data():
    if assets.does_asset_exist(DATA_DA):
        log(f"existe déjà : {DATA_DA}")
        return
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.YSGameBoatData)
    data = tools.create_asset("DA_GameBoat", DATA_DIR, unreal.YSGameBoatData, factory)
    data.set_editor_property("Stages", [assets.load_blueprint_class(BOAT_BP), assets.load_blueprint_class(SLOOP_BP)])
    assets.save_asset(DATA_DA, only_if_is_dirty=False)
    log(f"créé : {DATA_DA}")


def setup_flow():
    gm = cdo(FLOW_GM)
    gm.set_editor_property("GameBoatData", assets.load_asset(DATA_DA))
    assets.save_asset(FLOW_GM, only_if_is_dirty=False)
    island = assets.load_asset(ISLAND_DA)
    island.set_editor_property("start_boat_class", assets.load_blueprint_class(ZONE_BP))
    assets.save_asset(ISLAND_DA, only_if_is_dirty=False)
    log("BP_FlowMod : Game Boat Data ; DA_I_StartIsland : Start Boat Class = BP_GameBoatZone")


def dump_longboat():
    """Formes de la chaloupe (pour placer les places)"""
    for name, comp in components(assets.load_asset(LONGBOAT_BP)).items():
        if isinstance(comp, unreal.StaticMeshComponent):
            loc = comp.get_editor_property("relative_location")
            scale = comp.get_editor_property("relative_scale3d")
            log(f"  chaloupe {name} : ({loc.x:.0f}, {loc.y:.0f}, {loc.z:.0f}) taille ({scale.x * 100:.0f}, {scale.y * 100:.0f}, {scale.z * 100:.0f})")


try:
    dump_longboat()
    make_seat()
    make_longboat()
    make_zone()
    make_sloop()
    make_data()
    setup_flow()
    data = assets.load_asset(DATA_DA)
    log(f"relu DA_GameBoat : {[c.get_name() for c in data.get_editor_property('Stages')]}, départ à {data.get_editor_property('StartDistanceFromShore')}")
    log(f"relu BP_FlowMod : {cdo(FLOW_GM).get_editor_property('GameBoatData').get_name()}")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_game_boat.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
