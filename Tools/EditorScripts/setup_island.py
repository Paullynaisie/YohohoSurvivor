"""
Île pirate (GameMap et TestMap) :
  - le sol plat « Ground » est remplacé par un Landscape « Island » (6 x 6 composants de 63 quads, 50 cm par quad = 189 m de côté)
    sculpté par une formule : côte irrégulière, collines douces (pentes < 23°, toujours praticables à pied), plage en pente légère,
    jusqu'au bord de l'eau, puis à pic sous la mer (on passe à travers l'océan et on tombe sous la KillZ : deadzone)
    Surface doublée le 2026-10-24 (demande de Paul) : rayon, collines et dénivelé x racine de 2 (ISLAND_SCALE, mêmes pentes)
  - matériau /Game/Art/Materials/M_IslandGround : sable en bas, herbe en haut (selon l'altitude : suit les retouches au sculpt)
  - océan : acteur AYSOcean (plane bleu /Game/Art/Materials/M_Ocean, sans collision) au niveau de la mer : niveau de la mer
    pour le jeu (pas de NME ni de carte au trésor sous l'eau, balles et zones au sol au-dessus de la surface)
  - port au sud (-X : bas de l'écran) : ponton (/Game/Art/Props/BP_Prop_Pier) au bord de l'eau, Brooker et bateau gardent leur position
    par rapport au ponton ; tags « Harbor » (+ « HarborPivot » ponton, « HarborLand » Brooker, « HarborSea » bateau) : en jeu le
    GameMode (Random Harbor) déplace tout le port dans une direction tirée au hasard (GameMap ; décoché sur BP_TestMapGameMode)
  - les 4 PlayerStart au centre de l'île
  - palmiers : les anciens sont retirés, de nouveaux placés sur l'île (graine fixe : même placement à chaque lancement)
  - palmiers, ponton et Brooker accrochés au Landscape (ils coulent avec l'île en overtime)
  - NME de test reposés sur le sol
  - BP_TestGameMode : Shore Search Radius et zone des cartes au trésor agrandis avec l'île
Relançable : l'île, l'océan, le ponton et les palmiers qu'il a créés sont recréés.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="Tools/EditorScripts/setup_island.py"
Rapport : Saved/Logs/setup_island.txt
"""
import math
import os
import random
import unreal

LEVELS = ["/Game/Maps/GameMap", "/Game/Maps/TestMap"]
GM_DIR = "/Game/Core/GameModes"
GAME_GM = f"{GM_DIR}/BP_TestGameMode"
TEST_GM = f"{GM_DIR}/BP_TestMapGameMode"
HARBOR_TAGS = {"pier": ["Harbor", "HarborPivot"], "brooker": ["Harbor", "HarborLand"], "ship": ["Harbor", "HarborSea"]}
MAT_DIR = "/Game/Art/Materials"
GROUND_MAT = f"{MAT_DIR}/M_IslandGround"
OCEAN_MAT = f"{MAT_DIR}/M_Ocean"
PROPS_DIR = "/Game/Art/Props"
PIER_BP = f"{PROPS_DIR}/BP_Prop_Pier"
PROP_BP = f"{PROPS_DIR}/BP_Prop"
PALM_BP = f"{PROPS_DIR}/BP_Prop_PalmTree"
BROWN = "/Game/Art/Materials/PH_materials/m_PH_monochrome_Brown"
CUBE = "/Engine/BasicShapes/Cube"
CYLINDER = "/Engine/BasicShapes/Cylinder"

# taille : surface doublée le 2026-10-24 = rayon, collines et dénivelé x racine de 2 (mêmes pentes)
ISLAND_SCALE = math.sqrt(2.0)

# landscape
COMPONENTS = 6
QUADS = 63
QUAD_SIZE = 50.0
SIZE = COMPONENTS * QUADS + 1                      # 379 sommets
HALF = COMPONENTS * QUADS * QUAD_SIZE / 2.0        # 9450 cm
SEA_LEVEL = -30.0
SHORE_MARGIN = 20.0       # AYSOcean::ShoreMargin : un sol à moins de 20 cm au-dessus de la mer compte comme sous l'eau

# forme de l'île
ISLAND_RADIUS = 4600.0 * ISLAND_SCALE
LAND_HEIGHT = 30.0        # plateau côtier
SHORE_DEPTH = -22.0       # au bord de l'eau (d = 1) : juste au-dessus de la mer
CLIFF_WIDTH = 0.2         # puis pente douce jusqu'au fond (~13 m, 2026-10-25 : l'ancien 0.02 = à pic) ; smooth_island_shore.py
SEA_FLOOR = -650.0        # sous la KillZ (-300) : tomber dans l'océan = mort
HILLS = [(x * ISLAND_SCALE, y * ISLAND_SCALE, h * ISLAND_SCALE, s * ISLAND_SCALE) for x, y, h, s in [  # (x, y, hauteur, rayon) ; sud = -X
    (1600.0, 600.0, 380.0, 1500.0),
    (-200.0, -2400.0, 240.0, 1000.0),
    (-600.0, 2300.0, 200.0, 900.0),
    (2800.0, -1500.0, 140.0, 800.0),
]]
PALM_COUNT = 60           # 30 sur l'île d'avant : même densité
PLAYER_SPAWN_RADIUS = 1200.0  # pas de palmier autour des PlayerStart (centre de l'île)

# BP_TestGameMode : agrandis avec l'île
SHORE_SEARCH_RADIUS = 9000.0
TREASURE_EXTENT = 4500.0 * ISLAND_SCALE

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_island] {msg}")
    report.append(msg)


def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def coast_radius(theta):
    return ISLAND_RADIUS + ISLAND_SCALE * (350.0 * math.sin(3.0 * theta + 0.4) + 200.0 * math.sin(5.0 * theta + 1.3) + 120.0 * math.sin(9.0 * theta + 2.1))


def height(x, y):
    r = math.hypot(x, y)
    d = r / coast_radius(math.atan2(y, x))
    # profil de côte : plateau, plage jusqu'au bord de l'eau, puis à pic
    if d <= 0.82:
        base = LAND_HEIGHT
    elif d <= 1.0:
        base = LAND_HEIGHT + (SHORE_DEPTH - LAND_HEIGHT) * smoothstep((d - 0.82) / 0.18)
    else:
        base = SHORE_DEPTH + (SEA_FLOOR - SHORE_DEPTH) * smoothstep((d - 1.0) / CLIFF_WIDTH)
    # collines (seulement à l'intérieur des terres) + petites ondulations
    inland = smoothstep((0.82 - d) / 0.25)
    hills = sum(h * math.exp(-((x - cx) ** 2 + (y - cy) ** 2) / (2.0 * s * s)) for cx, cy, h, s in HILLS)
    ripples = 12.0 * ISLAND_SCALE * math.sin(x / (700.0 * ISLAND_SCALE)) * math.cos(y / (900.0 * ISLAND_SCALE))
    return base + (hills + ripples) * inland


def max_land_slope_deg(heights):
    worst = 0.0
    for j in range(SIZE - 1):
        for i in range(SIZE - 1):
            h = heights[j * SIZE + i]
            if h < SEA_LEVEL:
                continue
            for neighbor in (heights[j * SIZE + i + 1], heights[(j + 1) * SIZE + i]):
                if neighbor < SEA_LEVEL:
                    continue  # bord à pic sous l'eau : pas une pente de marche
                dh = neighbor - h
                worst = max(worst, math.degrees(math.atan(abs(dh) / QUAD_SIZE)))
    return worst


def get_or_create_material(path, build):
    if assets.does_asset_exist(path):
        return assets.load_asset(path)
    name = path.rsplit("/", 1)[1]
    material = tools.create_asset(name, MAT_DIR, unreal.Material, unreal.MaterialFactoryNew())
    build(material)
    mel.recompile_material(material)
    assets.save_asset(path, only_if_is_dirty=False)
    log(f"créé : {path}")
    return material


def vector_param(material, name, color, x, y):
    p = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", color)
    return p


def scalar_param(material, name, value, x, y):
    p = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, x, y)
    p.set_editor_property("parameter_name", name)
    p.set_editor_property("default_value", value)
    return p


def build_ground(material):
    # couleur = lerp(sable, herbe, saturate((Z - SandTop) / BlendHeight))
    world = mel.create_material_expression(material, unreal.MaterialExpressionWorldPosition, -1200, 0)
    mask = mel.create_material_expression(material, unreal.MaterialExpressionComponentMask, -1000, 0)
    mask.set_editor_property("r", False)
    mask.set_editor_property("g", False)
    mask.set_editor_property("b", True)
    mel.connect_material_expressions(world, "", mask, "")
    sand_top = scalar_param(material, "SandTop", 55.0, -1000, 150)
    blend = scalar_param(material, "BlendHeight", 60.0, -800, 250)
    sub = mel.create_material_expression(material, unreal.MaterialExpressionSubtract, -800, 0)
    mel.connect_material_expressions(mask, "", sub, "A")
    mel.connect_material_expressions(sand_top, "", sub, "B")
    div = mel.create_material_expression(material, unreal.MaterialExpressionDivide, -600, 0)
    mel.connect_material_expressions(sub, "", div, "A")
    mel.connect_material_expressions(blend, "", div, "B")
    sat = mel.create_material_expression(material, unreal.MaterialExpressionSaturate, -450, 0)
    mel.connect_material_expressions(div, "", sat, "")
    sand = vector_param(material, "SandColor", unreal.LinearColor(0.78, 0.66, 0.42, 1.0), -450, -250)
    grass = vector_param(material, "GrassColor", unreal.LinearColor(0.22, 0.46, 0.14, 1.0), -450, -100)
    lerp = mel.create_material_expression(material, unreal.MaterialExpressionLinearInterpolate, -250, 0)
    mel.connect_material_expressions(sand, "", lerp, "A")
    mel.connect_material_expressions(grass, "", lerp, "B")
    mel.connect_material_expressions(sat, "", lerp, "Alpha")
    mel.connect_material_property(lerp, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = scalar_param(material, "Roughness", 0.9, -250, 200)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)


def build_ocean(material):
    color = vector_param(material, "OceanColor", unreal.LinearColor(0.02, 0.2, 0.42, 1.0), -400, 0)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    rough = scalar_param(material, "Roughness", 0.15, -400, 200)
    mel.connect_material_property(rough, "", unreal.MaterialProperty.MP_ROUGHNESS)


def add_shape(bp, name, mesh, material, location, scale, rotation=unreal.Rotator(0, 0, 0)):
    root = subsystem.k2_gather_subobject_data_for_blueprint(bp)[0]
    params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.StaticMeshComponent, blueprint_context=bp)
    handle, fail = subsystem.add_new_subobject(params)
    if not lib.is_handle_valid(handle):
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    subsystem.rename_subobject(handle, unreal.Text(name))
    comp = lib.get_object(lib.get_data(handle))
    comp.set_editor_property("static_mesh", assets.load_asset(mesh))
    comp.set_material(0, assets.load_asset(material))
    comp.set_editor_property("relative_location", location)
    comp.set_editor_property("relative_rotation", rotation)
    comp.set_editor_property("relative_scale3d", scale)


def get_or_create_pier():
    """Ponton : planches de 3 m de large sur 15 m (vers +X du BP, à tourner vers la mer), poteaux tous les 3 m"""
    if assets.does_asset_exist(PIER_BP):
        return assets.load_blueprint_class(PIER_BP)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", assets.load_blueprint_class(PROP_BP))
    bp = tools.create_asset("BP_Prop_Pier", PROPS_DIR, unreal.Blueprint, factory)
    length, width, plank, gap = 1500.0, 300.0, 45.0, 5.0
    count = int(length // (plank + gap))
    for i in range(count):
        x = i * (plank + gap) + plank / 2.0
        add_shape(bp, f"Plank{i:02d}", CUBE, BROWN, unreal.Vector(x, 0, -5), unreal.Vector(plank / 100.0, width / 100.0, 0.1))
    for i in range(int(length // 300.0) + 1):
        x = min(i * 300.0 + 20.0, length - 20.0)
        for side in (-1, 1):
            add_shape(bp, f"Post{i:02d}{'L' if side < 0 else 'R'}", CYLINDER, BROWN,
                      unreal.Vector(x, side * (width / 2.0 - 15.0), -260), unreal.Vector(0.3, 0.3, 5.0))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(PIER_BP, only_if_is_dirty=False)
    log(f"créé : {PIER_BP} ({count} planches)")
    return assets.load_blueprint_class(PIER_BP)


def spawn(cls, location, yaw=0.0, label=None, folder="Island"):
    actor = actors_sub.spawn_actor_from_class(cls, location, unreal.Rotator(0, 0, yaw))
    if label:
        actor.set_actor_label(label)
    actor.set_folder_path(folder)
    return actor


def water_edge_x():
    """X du bord de l'eau au sud (-X) sur l'axe Y = 0 : premier sol hors de l'eau en venant du large (comme AYSGameMode::FindShorePoint)"""
    x = -HALF
    while x < 0.0 and height(x, 0.0) < SEA_LEVEL + SHORE_MARGIN:
        x += 10.0
    return x


def add_tags(actor, tags):
    current = [str(t) for t in actor.get_editor_property("tags")]
    actor.set_editor_property("tags", [unreal.Name(t) for t in current + [t for t in tags if t not in current]])


def build_map(level, ground_mat, ocean_mat, pier_class, palm_class):
    if not levels.load_level(level):
        raise RuntimeError(f"impossible d'ouvrir {level}")
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    log(f"--- {level}")

    # port actuel : Brooker et bateau gardent leur position par rapport au ponton
    old_pier = next((a for a in actors_sub.get_all_level_actors() if a.get_actor_label() == "Pier"), None)
    old_pier_location = old_pier.get_actor_location() if old_pier else None
    brookers = [a for a in actors_sub.get_all_level_actors() if isinstance(a, unreal.YSBrookerNPC)]
    ships = [a for a in actors_sub.get_all_level_actors() if isinstance(a, unreal.YSPirateShip)]
    offsets = {}
    for actor in brookers + ships:
        offsets[actor.get_name()] = actor.get_actor_location() - old_pier_location if old_pier_location else None

    # nettoyage : ancien sol, anciens palmiers, éléments d'un lancement précédent
    removed = 0
    for actor in actors_sub.get_all_level_actors():
        label = actor.get_actor_label()
        if label in ("Ground", "Island", "Ocean", "Pier") or isinstance(actor, unreal.Landscape) or actor.get_class() == palm_class:
            actors_sub.destroy_actor(actor)
            removed += 1
    log(f"retirés : {removed} acteur(s) (sol plat, palmiers, île précédente)")

    # landscape
    heights = []
    for j in range(SIZE):
        y = -HALF + j * QUAD_SIZE
        for i in range(SIZE):
            heights.append(height(-HALF + i * QUAD_SIZE, y))
    log(f"hauteurs : {min(heights):.0f} à {max(heights):.0f} cm, pente max sur la terre ferme {max_land_slope_deg(heights):.1f}°")
    landscape = unreal.YSEditorScriptLibrary.create_landscape_from_heights(
        world, unreal.Vector(-HALF, -HALF, 0.0), unreal.Vector(QUAD_SIZE, QUAD_SIZE, 100.0), COMPONENTS, QUADS, heights, ground_mat, "Island")
    if landscape is None:
        raise RuntimeError("création du landscape impossible")
    landscape.set_folder_path("Island")
    log(f"landscape : {landscape.get_actor_label()} ({SIZE} x {SIZE} sommets, {COMPONENTS * QUADS * QUAD_SIZE / 100:.0f} m de côté)")

    # océan
    ocean = spawn(unreal.YSOcean, unreal.Vector(0, 0, SEA_LEVEL), label="Ocean")
    ocean.get_component_by_class(unreal.StaticMeshComponent).set_material(0, ocean_mat)
    ocean.set_actor_scale3d(unreal.Vector(800.0, 800.0, 1.0))
    log(f"océan : AYSOcean, plane de 800 m à Z {SEA_LEVEL:.0f} (sans collision)")

    # port au sud : ponton au bord de l'eau (pivot du port en jeu), Brooker et bateau à la même place par rapport à lui
    edge_x = water_edge_x()
    pier = spawn(pier_class, unreal.Vector(edge_x, 0.0, 15.0), yaw=180.0, label="Pier")
    add_tags(pier, HARBOR_TAGS["pier"])
    pier_location = pier.get_actor_location()
    log(f"ponton : départ au bord de l'eau ({edge_x:.0f}, 0), 15 m vers la mer, tags {HARBOR_TAGS['pier']}")
    for actor in brookers:
        offset = offsets.get(actor.get_name()) or unreal.Vector(220.0, -550.0, 0.0)
        x, y = pier_location.x + offset.x, pier_location.y + offset.y
        actor.set_actor_location(unreal.Vector(x, y, height(x, y) + 60.0), False, False)
        add_tags(actor, HARBOR_TAGS["brooker"])
        log(f"Brooker : ({x:.0f}, {y:.0f}), {height(x, y):.0f} cm au-dessus de 0, tags {HARBOR_TAGS['brooker']}")
    for actor in ships:
        offset = offsets.get(actor.get_name())
        if offset is not None:
            actor.set_actor_location(pier_location + offset, False, False)
        add_tags(actor, HARBOR_TAGS["ship"])
        loc = actor.get_actor_location()
        log(f"bateau : ({loc.x:.0f}, {loc.y:.0f}, {loc.z:.0f}){'' if offset is not None else ' (inchangé : pas d ancien ponton)'}, tags {HARBOR_TAGS['ship']}")

    # joueurs au centre de l'île
    starts = [a for a in actors_sub.get_all_level_actors() if isinstance(a, unreal.PlayerStart)]
    for index, start in enumerate(starts):
        x = -125.0 + (index // 2) * 250.0
        y = -150.0 + (index % 2) * 300.0
        start.set_actor_location(unreal.Vector(x, y, height(x, y) + 100.0), False, False)
        start.set_actor_rotation(unreal.Rotator(0, 0, 0), False)
    log(f"{len(starts)} PlayerStart au centre de l'île")

    # palmiers : graine fixe, sur la terre ferme, espacés, loin des spawns (centre), du ponton et du Brooker de la map
    rng = random.Random(7)
    keep_out = [(0.0, 0.0, PLAYER_SPAWN_RADIUS), (pier_location.x, pier_location.y, 1200.0)]
    keep_out += [(b.get_actor_location().x, b.get_actor_location().y, 900.0) for b in brookers]
    palms = []
    tries = 0
    while len(palms) < PALM_COUNT and tries < 20000:
        tries += 1
        x, y = rng.uniform(-HALF, HALF), rng.uniform(-HALF, HALF)
        d = math.hypot(x, y) / coast_radius(math.atan2(y, x))
        if d > 0.9 or height(x, y) < 10.0:
            continue
        if any(math.hypot(x - kx, y - ky) < kr for kx, ky, kr in keep_out):
            continue
        if any(math.hypot(x - px, y - py) < 700.0 for px, py in palms):
            continue
        palms.append((x, y))
        spawn(palm_class, unreal.Vector(x, y, height(x, y) - 5.0), yaw=rng.uniform(0.0, 360.0), folder="Island/PalmTrees")
    log(f"palmiers : {len(palms)} placés")

    # NME de test : reposés sur le sol
    foes = 0
    for actor in actors_sub.get_all_level_actors():
        if isinstance(actor, unreal.YSFoeCharacter):
            loc = actor.get_actor_location()
            actor.set_actor_location(unreal.Vector(loc.x, loc.y, height(loc.x, loc.y) + 100.0), False, False)
            foes += 1
    log(f"NME de test reposés : {foes}")

    # palmiers, ponton, Brooker accrochés à l'île : ils coulent avec elle (overtime)
    attached = 0
    for actor in actors_sub.get_all_level_actors():
        if actor.get_class() == palm_class or actor is pier or actor in brookers:
            actor.attach_to_actor(landscape, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
            attached += 1 if actor.get_attach_parent_actor() == landscape else 0
    log(f"accrochés à l'île : {attached}")

    if not levels.save_current_level():
        raise RuntimeError(f"sauvegarde de {level} impossible")
    log(f"{level} sauvegardée")


try:
    ground_mat = get_or_create_material(GROUND_MAT, build_ground)
    ocean_mat = get_or_create_material(OCEAN_MAT, build_ocean)
    pier_class = get_or_create_pier()
    palm_class = assets.load_blueprint_class(PALM_BP)
    for level in LEVELS:
        build_map(level, ground_mat, ocean_mat, pier_class, palm_class)

    # GameModes : recherche de la plage et zone des cartes au trésor à la taille de l'île ; port au hasard dans GameMap seulement
    game_gm = unreal.get_default_object(assets.load_blueprint_class(GAME_GM))
    game_gm.set_editor_property("shore_search_radius", SHORE_SEARCH_RADIUS)
    game_gm.set_editor_property("treasure_map_area_extent", unreal.Vector2D(TREASURE_EXTENT, TREASURE_EXTENT))
    game_gm.set_editor_property("random_harbor", True)
    assets.save_asset(GAME_GM, only_if_is_dirty=False)
    test_gm = unreal.get_default_object(assets.load_blueprint_class(TEST_GM))
    test_gm.set_editor_property("random_harbor", False)
    assets.save_asset(TEST_GM, only_if_is_dirty=False)
    for path in (GAME_GM, TEST_GM):
        gm = unreal.get_default_object(assets.load_blueprint_class(path))
        log(f"relu {path.split('/')[-1]} : Shore Search Radius {gm.get_editor_property('shore_search_radius')}, "
            f"zone des cartes {gm.get_editor_property('treasure_map_area_extent')}, port au hasard {gm.get_editor_property('random_harbor')}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_island.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
