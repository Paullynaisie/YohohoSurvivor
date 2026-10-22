"""
Île pirate (GameMap) :
  - le sol plat « Ground » est remplacé par un Landscape « Island » (4 x 4 composants de 63 quads, 50 cm par quad = 126 m de côté)
    sculpté par une formule : côte irrégulière, collines douces (pentes < 20°, toujours praticables à pied), plage en pente légère,
    jusqu'au bord de l'eau, puis à pic sous la mer (on passe à travers l'océan et on tombe sous la KillZ : deadzone)
  - matériau /Game/Art/Materials/M_IslandGround : sable en bas, herbe en haut (selon l'altitude : suit les retouches au sculpt)
  - océan : acteur AYSOcean (plane bleu /Game/Art/Materials/M_Ocean, sans collision) au niveau de la mer : niveau de la mer
    pour le jeu (pas de NME ni de carte au trésor sous l'eau, balles et zones au sol au-dessus de la surface)
  - Brooker en bord de mer au sud (-X : bas de l'écran), les 4 PlayerStart juste à côté, ponton (/Game/Art/Props/BP_Prop_Pier) qui part
    vers la mer à côté du Brooker
  - palmiers : les anciens sont retirés, de nouveaux placés sur l'île (graine fixe : même placement à chaque lancement)
  - NME de test reposés sur le sol
Relançable : l'île, l'océan, le ponton et les palmiers qu'il a créés sont recréés.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_island.py"
Rapport : Saved/Logs/setup_island.txt
"""
import math
import os
import random
import unreal

LEVEL = "/Game/Maps/GameMap"
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

# landscape
COMPONENTS = 4
QUADS = 63
QUAD_SIZE = 50.0
SIZE = COMPONENTS * QUADS + 1                      # 253 sommets
HALF = COMPONENTS * QUADS * QUAD_SIZE / 2.0        # 6300 cm
SEA_LEVEL = -30.0

# forme de l'île
ISLAND_RADIUS = 4600.0
LAND_HEIGHT = 30.0        # plateau côtier
SHORE_DEPTH = -22.0       # au bord de l'eau (d = 1) : juste au-dessus de la mer
CLIFF_WIDTH = 0.02        # puis à pic jusqu'au fond (~1 m) : on tombe dans l'eau au lieu d'y marcher
SEA_FLOOR = -650.0        # sous la KillZ (-300) : tomber dans l'océan = mort
HILLS = [                 # (x, y, hauteur, rayon) ; sud = -X
    (1600.0, 600.0, 380.0, 1500.0),
    (-200.0, -2400.0, 240.0, 1000.0),
    (-600.0, 2300.0, 200.0, 900.0),
    (2800.0, -1500.0, 140.0, 800.0),
]

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
    return ISLAND_RADIUS + 350.0 * math.sin(3.0 * theta + 0.4) + 200.0 * math.sin(5.0 * theta + 1.3) + 120.0 * math.sin(9.0 * theta + 2.1)


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
    ripples = 12.0 * math.sin(x / 700.0) * math.cos(y / 900.0)
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


def shore_x(target_height):
    """X (vers le sud, -X) où le sol descend à target_height sur l'axe Y = 0"""
    x = -2000.0
    while x > -HALF and height(x, 0.0) > target_height:
        x -= 10.0
    return x


try:
    levels.load_level(LEVEL)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()

    ground_mat = get_or_create_material(GROUND_MAT, build_ground)
    ocean_mat = get_or_create_material(OCEAN_MAT, build_ocean)
    pier_class = get_or_create_pier()
    palm_class = assets.load_blueprint_class(PALM_BP)

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

    # Brooker au bord de la mer, au sud ; joueurs juste au nord de lui ; ponton à côté vers la mer
    brooker_x = shore_x(LAND_HEIGHT - 5.0) + 150.0
    for actor in actors_sub.get_all_level_actors():
        if actor.get_class().get_name().startswith("BP_NPC_Brooker"):
            actor.set_actor_location(unreal.Vector(brooker_x, 0.0, height(brooker_x, 0.0) + 60.0), False, False)
            actor.set_actor_rotation(unreal.Rotator(0, 0, 0), False)
            log(f"Brooker : ({brooker_x:.0f}, 0)")
    starts = [a for a in actors_sub.get_all_level_actors() if isinstance(a, unreal.PlayerStart)]
    for index, start in enumerate(starts):
        x = brooker_x + 600.0 + (index // 2) * 250.0
        y = -150.0 + (index % 2) * 300.0
        start.set_actor_location(unreal.Vector(x, y, height(x, y) + 100.0), False, False)
        start.set_actor_rotation(unreal.Rotator(0, 0, 0), False)
    log(f"{len(starts)} PlayerStart près du Brooker")

    pier_y = 550.0
    pier_x = shore_x(5.0)
    pier = spawn(pier_class, unreal.Vector(pier_x + 100.0, pier_y, 15.0), yaw=180.0, label="Pier")
    log(f"ponton : départ ({pier_x + 100:.0f}, {pier_y:.0f}), 15 m vers la mer")

    # palmiers : graine fixe, sur la terre ferme, espacés, loin du Brooker / des spawns / du ponton
    rng = random.Random(7)
    keep_out = [(brooker_x, 0.0, 900.0), (brooker_x + 700.0, 0.0, 700.0), (pier_x - 700.0, pier_y, 900.0)]
    palms = []
    tries = 0
    while len(palms) < 30 and tries < 5000:
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

    if not levels.save_current_level():
        raise RuntimeError("sauvegarde de la map impossible")
    log("map sauvegardée")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_island.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
