"""
Prévisus et postes de combat (demandes de Paul du 2026-11-10), relançable :
  - /Game/Core/Ship/M_ShipPreview : matériau translucide non éclairé, deux faces, couleur = couleur des sommets (alpha = opacité) ;
    sert aux prévisus des postes de combat et aux zones abordables des bateaux
  - /Game/Core/Ship/DA_CombatPostPreview (UYSCombatPostPreviewData, réglages des prévisus) -> Preview Data de BP_ShipCannon (Gatling : hérité)
  - réparation du core : Core Repair Per Second 50 sur les bateaux des joueurs
  - postes (réceptacles et canons) rapprochés du bord : chaque emplacement Cannon* avancé dans sa direction jusqu'à EDGE_INSET du bord
    de la coque (enveloppe convexe des Hull Probe Points), pour que l'AMMO ne parte pas dans la coque ; jamais reculé

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="<chemin absolu>/Tools/EditorScripts/setup_previews_2026_11_10.py"
Rapport : Saved/Logs/setup_previews_2026_11_10.txt
"""
import math
import os
import unreal

CORE_SHIP = "/Game/Core/Ship"
MATERIAL = f"{CORE_SHIP}/M_ShipPreview"
DATA = f"{CORE_SHIP}/DA_CombatPostPreview"
SHIP_DIR = "/Game/Placeable/Ship"
CANNON_BP = f"{SHIP_DIR}/Stations/BP_ShipCannon"
PLAYER_SHIPS = ["BP_GameBoat_Sloop", "BP_GameBoat_Brigantin", "BP_GameBoat_Galion", "BP_PirateShip"]
POST_SHIPS = PLAYER_SHIPS + ["BP_NME_Galion", "BP_NME_Galion_Nemesis"]
CANNON_SLOTS = ["CannonFront", "CannonFront2", "CannonLeft", "CannonRight", "CannonLeft2", "CannonRight2"]
EDGE_INSET = 140.0
REPAIR_PER_SECOND = 50.0

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
report = []


def log(msg):
    unreal.log(f"[setup_previews] {msg}")
    report.append(str(msg))


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def make_material():
    if assets.does_asset_exist(MATERIAL):
        log(f"existe déjà : {MATERIAL}")
        return assets.load_asset(MATERIAL)
    material = tools.create_asset("M_ShipPreview", CORE_SHIP, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    vertex = mel.create_material_expression(material, unreal.MaterialExpressionVertexColor, -400, 0)
    mel.connect_material_property(vertex, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(vertex, "A", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(material)
    assets.save_asset(MATERIAL, only_if_is_dirty=False)
    log(f"créé : {MATERIAL}")
    return material


def make_data(material):
    if assets.does_asset_exist(DATA):
        data = assets.load_asset(DATA)
        log(f"existe déjà : {DATA}")
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.YSCombatPostPreviewData)
        data = tools.create_asset("DA_CombatPostPreview", CORE_SHIP, unreal.YSCombatPostPreviewData, factory)
        log(f"créé : {DATA}")
    if not data.get_editor_property("material"):
        data.set_editor_property("material", material)
    assets.save_loaded_asset(data, only_if_is_dirty=False)
    cannon = cdo(CANNON_BP)
    cannon.set_editor_property("PreviewData", data)
    assets.save_asset(CANNON_BP, only_if_is_dirty=False)
    log(f"BP_ShipCannon : Preview Data = {cdo(CANNON_BP).get_editor_property('PreviewData').get_name()}")


def convex_hull(points):
    points = sorted(set(points))
    if len(points) < 3:
        return points
    def cross(o, a, b):
        return (a[0] - o[0]) * (b[1] - o[1]) - (a[1] - o[1]) * (b[0] - o[0])
    lower, upper = [], []
    for p in points:
        while len(lower) >= 2 and cross(lower[-2], lower[-1], p) <= 0:
            lower.pop()
        lower.append(p)
    for p in reversed(points):
        while len(upper) >= 2 and cross(upper[-2], upper[-1], p) <= 0:
            upper.pop()
        upper.append(p)
    return lower[:-1] + upper[:-1]


def ray_exit(polygon, origin, direction):
    """Distance de origin (dedans) au bord du polygone convexe le long de direction"""
    best = None
    for i in range(len(polygon)):
        a, b = polygon[i], polygon[(i + 1) % len(polygon)]
        ex, ey = b[0] - a[0], b[1] - a[1]
        denom = direction[0] * ey - direction[1] * ex
        if abs(denom) < 1e-6:
            continue
        t = ((a[0] - origin[0]) * ey - (a[1] - origin[1]) * ex) / denom
        u = ((a[0] - origin[0]) * direction[1] - (a[1] - origin[1]) * direction[0]) / denom
        if t > 0 and -1e-4 <= u <= 1 + 1e-4 and (best is None or t < best):
            best = t
    return best


def move_posts(ship_name):
    path = f"{SHIP_DIR}/{ship_name}"
    if not assets.does_asset_exist(path):
        log(f"ABSENT : {path}")
        return
    ship = cdo(path)
    hull = convex_hull([(p.x, p.y) for p in ship.get_editor_property("HullProbePoints")])
    if len(hull) < 3:
        log(f"{ship_name} : pas de coque")
        return
    moved = []
    for slot in CANNON_SLOTS:
        try:
            comp = ship.get_editor_property(slot)
        except Exception:
            continue
        if not comp or not comp.get_editor_property("child_actor_class"):
            continue
        loc = comp.get_editor_property("relative_location")
        yaw = comp.get_editor_property("relative_rotation").yaw
        direction = (math.cos(math.radians(yaw)), math.sin(math.radians(yaw)))
        exit_distance = ray_exit(hull, (loc.x, loc.y), direction)
        if exit_distance is None:
            moved.append(f"{slot} : hors de la coque ?")
            continue
        advance = exit_distance - EDGE_INSET
        if advance <= 1.0:
            moved.append(f"{slot} : déjà au bord ({exit_distance:.0f} cm)")
            continue
        new = unreal.Vector(loc.x + direction[0] * advance, loc.y + direction[1] * advance, loc.z)
        comp.set_editor_property("relative_location", new)
        moved.append(f"{slot} ({loc.x:.0f}, {loc.y:.0f}) -> ({new.x:.0f}, {new.y:.0f}), +{advance:.0f} cm")
    if ship_name in PLAYER_SHIPS:
        ship.set_editor_property("CoreRepairPerSecond", REPAIR_PER_SECOND)
    assets.save_asset(path, only_if_is_dirty=False)
    log(f"{ship_name} : " + " ; ".join(moved))


try:
    material = make_material()
    make_data(material)
    for name in POST_SHIPS:
        move_posts(name)
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_previews_2026_11_10.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
