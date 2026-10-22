"""
Bateau pirate : /Game/Placeable/Ship/BP_PirateShip, enfant de AYSPirateShip (Ship/YSPirateShip), assemblage de primitives
(visuel provisoire, à retoucher par Paul). Origine = ligne de flottaison, +X = proue, pont à +210 cm.
  - coque (cube + proue arrondie en cylindre), pont en planches claires, bastingage (avec une ouverture à bâbord, -Y),
    gaillard arrière surélevé avec escalier, 3 mâts, vergues et voiles carguées (fines, pour ne pas cacher le pont
    vu de dessus), nid de pie, drapeau, beaupré, canons, planche d'embarquement à bâbord
  - collision : coque, pont, bastingage, gaillard, escalier, mâts, canons, planche = WorldStatic bloquant (sol pour
    les persos, les traces de sol et les balles) ; voiles, vergues, drapeau, cordages sans collision ; tout en Movable
  - GameMap : un bateau amarré au bout du ponton (planche d'embarquement contre le ponton), s'il n'y en a pas déjà
Ne recrée pas le Blueprint s'il existe (le visuel retouché par Paul est conservé).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_pirate_ship.py"
Rapport : Saved/Logs/setup_pirate_ship.txt
"""
import math
import os
import unreal

SHIP_DIR = "/Game/Placeable/Ship"
NAME = "BP_PirateShip"
SHIP_BP = f"{SHIP_DIR}/{NAME}"
LEVEL = "/Game/Maps/GameMap"
MAT = "/Game/Art/Materials/PH_materials"
BROWN = f"{MAT}/m_PH_monochrome_Brown"
SAND = f"{MAT}/m_PH_monochrome_Sand"
BLACK = f"{MAT}/m_PH_monochrome_Black"
DARK = f"{MAT}/m_PH_monochrome_LightBlack"
WHITE = f"{MAT}/m_PH_monochrome_white"
RED = f"{MAT}/m_PH_monochrome_Red"
CUBE = "/Engine/BasicShapes/Cube"
CYLINDER = "/Engine/BasicShapes/Cylinder"
SPHERE = "/Engine/BasicShapes/Sphere"

# dimensions (cm)
HULL_LENGTH = 2400.0     # partie droite de la coque, centrée en X = 0 ; la proue arrondie dépasse de BOW_RADIUS
WIDTH = 1000.0
BOW_RADIUS = WIDTH / 2.0
HULL_BOTTOM = -200.0     # sous la ligne de flottaison
DECK = 210.0             # dessus du pont
RAIL_HEIGHT = 100.0
CASTLE_LENGTH = 500.0    # gaillard arrière
STEP_RISE = 42.0         # marches franchissables (step height du CharacterMovement : 45)
STEPS = 6
CASTLE_TOP = DECK + STEP_RISE * STEPS
GAP_HALF = 200.0         # ouverture du bastingage à bâbord (-Y), centrée en X = 0

# amarrage dans GameMap (bout du ponton BP_Prop_Pier, qui part vers -X)
PIER_LABEL = "Pier"
PIER_LENGTH = 1500.0
DOCK_GAP = 0.0

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
report = []
count = {"solid": 0, "decor": 0}


def log(msg):
    unreal.log(f"[setup_pirate_ship] {msg}")
    report.append(msg)


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
    comp.set_editor_property("relative_rotation", rotation or unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    comp.set_editor_property("relative_scale3d", unreal.Vector(size.x / 100.0, size.y / 100.0, size.z / 100.0))
    if solid:
        comp.set_collision_profile_name("BlockAll")
        count["solid"] += 1
    else:
        comp.set_collision_profile_name("NoCollision")
        comp.set_editor_property("cast_shadow", True)
        count["decor"] += 1
    return comp


def v(x, y, z):
    return unreal.Vector(x, y, z)


def box(bp, name, material, x0, x1, y0, y1, z0, z1, solid=True, yaw=0.0):
    return add_shape(bp, name, CUBE, material, v((x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), v(abs(x1 - x0), abs(y1 - y0), abs(z1 - z0)),
                     unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw), solid)


def mast(bp, name, x, base_z, height, yard_widths, radius=35.0):
    """Mât + vergues horizontales (le long de Y) avec voiles carguées blanches, sans collision au-dessus du pont"""
    add_shape(bp, f"{name}", CYLINDER, BROWN, v(x, 0, base_z + height / 2), v(radius * 2, radius * 2, height))
    for index, (ratio, width) in enumerate(yard_widths):
        z = base_z + height * ratio
        along_y = unreal.Rotator(roll=90.0, pitch=0.0, yaw=0.0)
        add_shape(bp, f"{name}_Yard{index}", CYLINDER, BROWN, v(x, 0, z), v(18, 18, width), along_y, solid=False)
        add_shape(bp, f"{name}_Sail{index}", CYLINDER, WHITE, v(x + 10, 0, z - 22), v(45, 45, width * 0.9), along_y, solid=False)


def build_ship(bp):
    half_l, half_w = HULL_LENGTH / 2, WIDTH / 2
    stern = -half_l

    # coque et proue arrondie
    box(bp, "Hull", BROWN, -half_l, half_l, -half_w, half_w, HULL_BOTTOM, DECK - 10)
    add_shape(bp, "HullBow", CYLINDER, BROWN, v(half_l, 0, (HULL_BOTTOM + DECK - 10) / 2), v(WIDTH, WIDTH, DECK - 10 - HULL_BOTTOM))
    box(bp, "HullStripe", BLACK, -half_l - 2, half_l, -half_w - 2, half_w + 2, 40, 70, solid=False)

    # pont (planches claires)
    box(bp, "Deck", SAND, -half_l, half_l, -half_w + 10, half_w - 10, DECK - 10, DECK)
    add_shape(bp, "DeckBow", CYLINDER, SAND, v(half_l, 0, DECK - 5), v(WIDTH - 20, WIDTH - 20, 10))

    # bastingage : flancs (ouverture à bâbord au milieu), proue en segments, gaillard
    rail_z0, rail_z1 = DECK, DECK + RAIL_HEIGHT
    castle_front = stern + CASTLE_LENGTH
    box(bp, "RailStarboard", DARK, castle_front, half_l, half_w - 20, half_w, rail_z0, rail_z1)
    box(bp, "RailPortAft", DARK, castle_front, -GAP_HALF, -half_w, -half_w + 20, rail_z0, rail_z1)
    box(bp, "RailPortFore", DARK, GAP_HALF, half_l, -half_w, -half_w + 20, rail_z0, rail_z1)
    segments = 8
    for i in range(segments):
        a0 = -math.pi / 2 + math.pi * i / segments
        a1 = -math.pi / 2 + math.pi * (i + 1) / segments
        mid = (a0 + a1) / 2
        r = BOW_RADIUS - 10
        length = 2 * r * math.sin((a1 - a0) / 2) + 8
        add_shape(bp, f"RailBow{i}", CUBE, DARK, v(half_l + r * math.cos(mid), r * math.sin(mid), (rail_z0 + rail_z1) / 2),
                  v(20, length, RAIL_HEIGHT), unreal.Rotator(roll=0.0, pitch=0.0, yaw=math.degrees(mid)))

    # gaillard arrière surélevé + escalier (tribord), bastingage du gaillard
    box(bp, "Castle", BROWN, stern, castle_front, -half_w, half_w, DECK - 10, CASTLE_TOP)
    box(bp, "CastleDeck", SAND, stern, castle_front, -half_w + 10, half_w - 10, CASTLE_TOP - 1, CASTLE_TOP)
    box(bp, "CastleDoor", BLACK, castle_front, castle_front + 3, -260, -100, DECK, DECK + 200, solid=False)
    for k in range(1, STEPS + 1):
        x1 = castle_front + (STEPS - k + 1) * 50
        box(bp, f"Step{k}", BROWN, x1 - 50, x1, 80, 380, DECK - 10, DECK + k * STEP_RISE)
    c0, c1 = CASTLE_TOP, CASTLE_TOP + RAIL_HEIGHT
    box(bp, "CastleRailStern", DARK, stern, stern + 20, -half_w, half_w, c0, c1)
    box(bp, "CastleRailStarboard", DARK, stern, castle_front, half_w - 20, half_w, c0, c1)
    box(bp, "CastleRailPort", DARK, stern, castle_front, -half_w, -half_w + 20, c0, c1)
    box(bp, "CastleRailFront", DARK, castle_front - 20, castle_front, -half_w, 60, c0, c1)  # ouverture au-dessus de l'escalier
    box(bp, "Helm", BROWN, stern + 150, stern + 170, -60, 60, CASTLE_TOP, CASTLE_TOP + 120)
    add_shape(bp, "HelmWheel", CYLINDER, BROWN, v(stern + 180, 0, CASTLE_TOP + 120), v(110, 110, 10),
              unreal.Rotator(roll=0.0, pitch=90.0, yaw=0.0), solid=False)
    add_shape(bp, "Lantern", SPHERE, RED, v(stern - 10, 0, c1 + 60), v(50, 50, 70), solid=False)

    # mâts : misaine, grand mât, artimon (sur le gaillard)
    mast(bp, "ForeMast", half_l - 300, DECK, 1300, [(0.55, 700), (0.85, 500)])
    mast(bp, "MainMast", 100, DECK, 1700, [(0.5, 900), (0.8, 650)], radius=40)
    mast(bp, "MizzenMast", stern + 300, CASTLE_TOP, 1000, [(0.6, 550)])
    add_shape(bp, "CrowsNest", CYLINDER, BROWN, v(100, 0, DECK + 1700 * 0.9), v(170, 170, 60), solid=False)
    box(bp, "Flag", BLACK, 100 - 160, 100 - 10, -3, 3, DECK + 1700 - 110, DECK + 1700 - 10, solid=False)
    add_shape(bp, "FlagSkull", SPHERE, WHITE, v(100 - 85, 0, DECK + 1700 - 60), v(40, 12, 40), solid=False)

    # beaupré (incliné vers l'avant), cordage avant
    add_shape(bp, "Bowsprit", CYLINDER, BROWN, v(half_l + BOW_RADIUS + 200, 0, DECK + 150), v(30, 30, 650),
              unreal.Rotator(roll=0.0, pitch=-70.0, yaw=0.0), solid=False)

    # canons : 3 par flanc, qui dépassent de la coque
    for side, sign in (("Port", -1), ("Starboard", 1)):
        for i, x in enumerate((-600, 400, 800)):  # rien devant l'ouverture d'embarquement (bâbord, X = 0)
            add_shape(bp, f"Cannon{side}{i}", CYLINDER, BLACK, v(x, sign * (half_w + 40), DECK - 70), v(45, 45, 140),
                      unreal.Rotator(roll=90.0, pitch=0.0, yaw=0.0))

    # planche d'embarquement à bâbord : de l'ouverture du pont jusqu'à la hauteur du ponton
    drop = DECK - (15.0 + 30.0)  # pont (+210 au-dessus de la mer) -> dessus du ponton (Z 15, mer à -30)
    run = 380.0
    angle = math.degrees(math.atan2(drop, run))
    plank = add_shape(bp, "Gangplank", CUBE, BROWN, v(0, -half_w - run / 2 + 20, DECK - drop / 2 - 5),
                      v(220, math.hypot(run, drop) + 40, 10), unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    # bon sens de rotation : le bout extérieur (-Y) doit descendre
    for roll in (angle, -angle):
        end = unreal.Rotator(roll=roll, pitch=0.0, yaw=0.0).quaternion().rotate_vector(v(0, -100, 0))
        if end.z < 0:
            plank.set_editor_property("relative_rotation", unreal.Rotator(roll=roll, pitch=0.0, yaw=0.0))
            break
    log(f"planche d'embarquement : pente {angle:.1f}°")


try:
    if not assets.does_asset_exist(SHIP_BP):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.YSPirateShip)
        bp = tools.create_asset(NAME, SHIP_DIR, unreal.Blueprint, factory)
        build_ship(bp)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assets.save_asset(SHIP_BP, only_if_is_dirty=False)
        log(f"créé : {SHIP_BP} ({count['solid']} formes solides, {count['decor']} décoratives)")
    else:
        log(f"existe déjà : {SHIP_BP}")
    ship_class = assets.load_blueprint_class(SHIP_BP)

    # GameMap : amarré au bout du ponton, flanc bâbord (ouverture) contre le ponton
    levels.load_level(LEVEL)
    existing = [a for a in actors_sub.get_all_level_actors() if isinstance(a, unreal.YSPirateShip)]
    pier = next((a for a in actors_sub.get_all_level_actors() if a.get_actor_label() == PIER_LABEL), None)
    ocean = next((a for a in actors_sub.get_all_level_actors() if isinstance(a, unreal.YSOcean)), None)
    if existing:
        log(f"GameMap : bateau déjà placé ({existing[0].get_actor_label()})")
    elif pier is None:
        log("GameMap : pas de ponton « Pier », bateau non placé")
    else:
        sea = ocean.get_actor_location().z if ocean else -30.0
        pier_end = pier.get_actor_location() + pier.get_actor_forward_vector() * PIER_LENGTH
        # yaw 90 : proue vers +Y (est), bâbord (-Y local) vers +X (le nord, côté ponton)
        center = v(pier_end.x - WIDTH / 2 - DOCK_GAP - 380.0 + 20.0, pier_end.y, sea)
        ship = actors_sub.spawn_actor_from_class(ship_class, center, unreal.Rotator(roll=0.0, pitch=0.0, yaw=90.0))
        ship.set_actor_label("PirateShip")
        ship.set_folder_path("Island")
        levels.save_current_level()
        log(f"GameMap : bateau amarré en ({center.x:.0f}, {center.y:.0f}, {center.z:.0f}), bout du ponton ({pier_end.x:.0f}, {pier_end.y:.0f})")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_pirate_ship.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
