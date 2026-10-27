"""
Postes du bateau pirate (2026-10-27) :
  - Blueprints des postes dans /Game/Placeable/Ship/Stations (réglages de chaque module dans ses Class Defaults) :
    BP_ShipCore (core : stats du bateau, zone Brooker + Medic), BP_CrowsNest (nid d'pie), BP_LongboatLauncher (chalouperie),
    BP_ShipCannon (canonnerie) ; créés s'ils n'existent pas (sinon gardés tels quels)
  - /Game/Placeable/Ship/BP_Longboat : chaloupe en primitives (enfant de AYSLongboat), créée si elle n'existe pas
  - /Game/Placeable/AMMO/CanonAMMO : enfant de PetoirAMMO (noire), AMMO des canonneries ; créée si elle n'existe pas
  - BP_PirateShip : composants des postes placés sur les formes du bateau (grand mât, porte de la cabine, pont, proue) et
    branchés sur les Blueprints ci-dessus ; nid d'pie à la hauteur du « CrowsNest »
  - GameMap : le bateau objectif devient pilotable ; BP_TestGameMode : plus de victoire en montant à bord (bShipBoardingVictory)
Relançable.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_ship_stations.py"
Rapport : Saved/Logs/setup_ship_stations.txt
"""
import os
import unreal

SHIP_BP = "/Game/Placeable/Ship/BP_PirateShip"
STATIONS_DIR = "/Game/Placeable/Ship/Stations"
LONGBOAT_DIR = "/Game/Placeable/Ship"
LONGBOAT_BP = f"{LONGBOAT_DIR}/BP_Longboat"
AMMO_DIR = "/Game/Placeable/AMMO"
CANON_AMMO = f"{AMMO_DIR}/CanonAMMO"
PETOIR_AMMO = f"{AMMO_DIR}/PetoirAMMO"
SHOTGUN_BP = "/Game/Placeable/Weapons/Player/Wp_Guns/BP_Shotgun"
GAME_MAP = "/Game/Maps/GameMap"
TEST_GAME_MODE = "/Game/Core/GameModes/BP_TestGameMode"
MAT = "/Game/Art/Materials/PH_materials"
BROWN = f"{MAT}/m_PH_monochrome_Brown"
SAND = f"{MAT}/m_PH_monochrome_Sand"
DARK = f"{MAT}/m_PH_monochrome_LightBlack"
CUBE = "/Engine/BasicShapes/Cube"
CYLINDER = "/Engine/BasicShapes/Cylinder"

# composant du bateau -> (Blueprint du poste, classe C++ parente)
STATION_BPS = {
    "CoreStation": ("BP_ShipCore", unreal.YSShipCore),
    "CrowsNestStation": ("BP_CrowsNest", unreal.YSCrowsNest),
    "LongboatStation": ("BP_LongboatLauncher", unreal.YSLongboatLauncher),
    "CannonFront": ("BP_ShipCannon", unreal.YSShipCannon),
    "CannonLeft": ("BP_ShipCannon", unreal.YSShipCannon),
    "CannonRight": ("BP_ShipCannon", unreal.YSShipCannon),
}

DECK = 210.0

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
report = []


def log(msg):
    unreal.log(f"[setup_ship_stations] {msg}")
    report.append(msg)


def v(x, y, z):
    return unreal.Vector(x, y, z)


def rot(yaw=0.0, pitch=0.0, roll=0.0):
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


def create_bp(path, parent):
    if assets.does_asset_exist(path):
        log(f"existe déjà : {path}")
        return assets.load_asset(path), False
    folder, name = path.rsplit("/", 1)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent)
    bp = tools.create_asset(name, folder, unreal.Blueprint, factory)
    log(f"créé : {path}")
    return bp, True


def components(bp):
    out = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if obj:
            out[obj.get_name().replace("_GEN_VARIABLE", "")] = obj
    return out


def add_shape(bp, name, mesh, material, location, size, rotation=None, solid=True):
    """Forme de taille size (cm ; cube et cylindre moteur = 100 cm) centrée en location"""
    root = subsystem.k2_gather_subobject_data_for_blueprint(bp)[0]
    params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.StaticMeshComponent, blueprint_context=bp)
    handle, fail = subsystem.add_new_subobject(params)
    if not lib.is_handle_valid(handle):
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    subsystem.rename_subobject(handle, unreal.Text(name))
    comp = lib.get_object(lib.get_data(handle))
    comp.set_editor_property("static_mesh", assets.load_asset(mesh))
    comp.set_material(0, assets.load_asset(material))
    comp.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    comp.set_editor_property("relative_location", location)
    comp.set_editor_property("relative_rotation", rotation or rot())
    comp.set_editor_property("relative_scale3d", v(size.x / 100.0, size.y / 100.0, size.z / 100.0))
    comp.set_collision_profile_name("BlockAll" if solid else "NoCollision")
    return comp


def box(bp, name, material, x0, x1, y0, y1, z0, z1, solid=True):
    return add_shape(bp, name, CUBE, material, v((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), v(abs(x1 - x0), abs(y1 - y0), abs(z1 - z0)), None, solid)


def build_longboat(bp):
    """Chaloupe : coque de -220 à 230 (proue arrondie à partir de 150), 180 de large, pont à +30 au-dessus de la mer"""
    box(bp, "Hull", BROWN, -220, 150, -90, 90, -50, 30)
    add_shape(bp, "HullBow", CYLINDER, BROWN, v(150, 0, -10), v(160, 180, 80))
    box(bp, "Deck", SAND, -215, 150, -82, 82, 28, 31, solid=False)
    # bordés (décor) et bancs
    box(bp, "SidePort", DARK, -220, 150, -90, -80, 30, 70, solid=False)
    box(bp, "SideStarboard", DARK, -220, 150, 80, 90, 30, 70, solid=False)
    box(bp, "SideStern", DARK, -220, -210, -90, 90, 30, 70, solid=False)
    box(bp, "BenchFront", BROWN, 40, 70, -80, 80, 30, 55, solid=False)
    box(bp, "BenchBack", BROWN, -110, -80, -80, 80, 30, 55, solid=False)
    # barre franche du pilote (arrière)
    add_shape(bp, "Tiller", CYLINDER, BROWN, v(-170, 0, 95), v(12, 12, 90), rot(pitch=60.0), solid=False)


try:
    # --- CanonAMMO
    if not assets.does_asset_exist(CANON_AMMO):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", assets.load_blueprint_class(PETOIR_AMMO))
        tools.create_asset("CanonAMMO", AMMO_DIR, unreal.Blueprint, factory)
        ammo_cdo = unreal.get_default_object(assets.load_blueprint_class(CANON_AMMO))
        try:
            ammo_cdo.set_editor_property("Color", unreal.LinearColor(0.02, 0.02, 0.02, 1.0))
        except Exception as e:
            log(f"CanonAMMO : couleur non réglée ({e!r})")
        assets.save_asset(CANON_AMMO, only_if_is_dirty=False)
        log(f"créé : {CANON_AMMO} (enfant de PetoirAMMO)")
    else:
        log(f"existe déjà : {CANON_AMMO}")
    canon_ammo = assets.load_blueprint_class(CANON_AMMO)

    # icône du tir de canon : celle de l'Auto shot du Shotgun
    cannon_icon = None
    if assets.does_asset_exist(SHOTGUN_BP):
        auto_shot = components(assets.load_asset(SHOTGUN_BP)).get("AutoShot")
        cannon_icon = auto_shot.get_editor_property("Icon") if auto_shot else None
        log(f"icône du tir de canon : {cannon_icon.get_name() if cannon_icon else 'aucune (AutoShot du Shotgun sans icône)'}")

    # --- géométrie du bateau
    ship_bp = assets.load_asset(SHIP_BP)
    comps = components(ship_bp)

    def loc(name, default):
        comp = comps.get(name)
        return comp.get_editor_property("relative_location") if comp else default

    def scale(name, default):
        comp = comps.get(name)
        return comp.get_editor_property("relative_scale3d") if comp else default

    mast = loc("MainMast", v(100, 0, DECK + 850))
    nest = loc("CrowsNest", v(100, 0, DECK + 1530))
    nest_scale = scale("CrowsNest", v(1.7, 1.7, 0.6))
    door = loc("CastleDoor", v(-698.5, -180, DECK + 100))
    bow = loc("HullBow", v(1200, 0, 0))
    bow_radius = scale("HullBow", v(10, 10, 1)).x * 50.0
    castle = loc("Castle", v(-950, 0, 300))
    castle_scale = scale("Castle", v(5, 10, 2.6))
    castle_front = castle.x + castle_scale.x * 50.0
    nest_top = nest.z + nest_scale.z * 50.0
    log(f"bateau : grand mât ({mast.x:.0f}, {mast.y:.0f}), nid à {nest_top:.0f}, porte ({door.x:.0f}, {door.y:.0f}), avant du gaillard {castle_front:.0f}, proue {bow.x:.0f} r {bow_radius:.0f}")

    placements = {
        # milieu du pont, entre le gaillard et le grand mât
        "CoreStation": (v((castle_front + mast.x) / 2.0, 0.0, DECK), 0.0),
        # pied du grand mât
        "CrowsNestStation": (v(mast.x, mast.y, DECK), 0.0),
        # devant la porte de la cabine, tourné vers elle
        "LongboatStation": (v(castle_front + 45.0, door.y, DECK), 180.0),
        # avant (proue), bâbord (-Y), tribord (+Y) ; +X du poste = direction de tir
        "CannonFront": (v(bow.x + bow_radius - 260.0, 0.0, DECK), 0.0),
        "CannonLeft": (v(mast.x + 350.0, -380.0, DECK), -90.0),
        "CannonRight": (v(mast.x + 350.0, 380.0, DECK), 90.0),
    }

    # --- Blueprints des postes
    station_classes = {}
    for comp_name, (bp_name, parent) in STATION_BPS.items():
        path = f"{STATIONS_DIR}/{bp_name}"
        if bp_name in station_classes:
            continue
        bp, created = create_bp(path, parent)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        cls = assets.load_blueprint_class(path)
        station_classes[bp_name] = cls
        # icône du canon : remplie si elle est encore vide
        if bp_name == "BP_ShipCannon" and cannon_icon and not unreal.get_default_object(cls).get_editor_property("Icon"):
            unreal.get_default_object(cls).set_editor_property("Icon", cannon_icon)
            assets.save_asset(path, only_if_is_dirty=False)
            log("BP_ShipCannon : icône réglée")
        if created:
            cdo = unreal.get_default_object(cls)
            if bp_name == "BP_ShipCannon":
                cdo.set_editor_property("ProjectileClass", canon_ammo)
                if cannon_icon:
                    cdo.set_editor_property("Icon", cannon_icon)
            if bp_name == "BP_CrowsNest":
                # joueur sur le nid (à côté du mât), plancher juste dessous
                height = nest_top - DECK
                cdo.get_editor_property("OccupantPoint").set_editor_property("relative_location", v(-100.0, 0.0, height + 5.0))
                cdo.get_editor_property("NestFloor").set_editor_property("relative_location", v(-60.0, 0.0, height))
                log(f"BP_CrowsNest : nid à +{height:.0f} au-dessus du pont")
            assets.save_asset(path, only_if_is_dirty=False)

    # --- composants du bateau : position + Blueprint du poste
    ship_cdo = unreal.get_default_object(assets.load_blueprint_class(SHIP_BP))
    for comp_name, (bp_name, _) in STATION_BPS.items():
        comp = ship_cdo.get_editor_property(comp_name)
        if comp is None:
            log(f"BP_PirateShip : pas de composant {comp_name}")
            continue
        location, yaw = placements[comp_name]
        comp.set_editor_property("relative_location", location)
        comp.set_editor_property("relative_rotation", rot(yaw=yaw))
        comp.set_child_actor_class(station_classes[bp_name])
        # gabarit de l'acteur enfant : mêmes réglages que le Blueprint du poste (sinon il garde des valeurs à part, et les
        # réglages du Blueprint ne s'appliquent plus à ce poste)
        template = comp.get_editor_property("child_actor_template")
        copied = unreal.YSEditorScriptLibrary.copy_editable_properties(unreal.get_default_object(station_classes[bp_name]), template) if template else 0
        log(f"BP_PirateShip.{comp_name} -> {bp_name} en ({location.x:.0f}, {location.y:.0f}, {location.z:.0f}) yaw {yaw:.0f} ({copied} réglages du Blueprint)")
    unreal.BlueprintEditorLibrary.compile_blueprint(ship_bp)
    assets.save_asset(SHIP_BP, only_if_is_dirty=False)

    # --- chaloupe
    longboat_bp, created = create_bp(LONGBOAT_BP, unreal.YSLongboat)
    if created:
        build_longboat(longboat_bp)
        unreal.BlueprintEditorLibrary.compile_blueprint(longboat_bp)
        assets.save_asset(LONGBOAT_BP, only_if_is_dirty=False)

    # --- fin de jeu : le bateau ne termine plus la partie, il est pilotable
    game_mode_cdo = unreal.get_default_object(assets.load_blueprint_class(TEST_GAME_MODE))
    game_mode_cdo.set_editor_property("bShipBoardingVictory", False)
    assets.save_asset(TEST_GAME_MODE, only_if_is_dirty=False)
    log("BP_TestGameMode : bShipBoardingVictory décoché (plus de victoire en montant à bord)")

    levels.load_level(GAME_MAP)
    ships = [a for a in actors_sub.get_all_level_actors() if isinstance(a, unreal.YSPirateShip)]
    for ship in ships:
        ship.set_editor_property("bDrivable", True)
        log(f"GameMap : {ship.get_actor_label()} pilotable (objectif : {ship.get_editor_property('bObjectiveShip')})")
    levels.save_current_level()
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_ship_stations.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
