"""
Retours de Paul du 2026-11-10 (Kraken et tailles des bateaux) :
  - Kraken : PV (tête 25 000, tentacules 4 000, tête finale 40 000), aspiration x0.6 (-20 %), coup 2000, tentacule à distance :
    3 boules de 500 par attaque, vol 4.5 s, imprécision 35 m ; tête finale : motifs cercles / lignes / spirale en cloche,
    un motif 3 s après le dernier impact du précédent
    (valeurs remises sur les Blueprints au cas où ils les surchargent)
  - BP_GameBoat_Galion x0.7 en largeur et longueur (hauteurs gardées, mâts et barre seulement replacés) : il hérite ses formes de
    BP_PirateShip, elles sont surchargées DANS LE GALION seulement (UYSEditorScriptLibrary::GetInheritedComponentOverride),
    BP_PirateShip n'est pas touché
  - BP_GameBoat_Brigantin x0.7 -> x0.55 de BP_PirateShip (entre le sloop x0.4 et le galion x0.7)
  - réglages liés à la taille (caméra de la barre, zone du core, chaloupe) interpolés comme avant ; postes de combat ramenés à 140 cm du bord
Relançable : la taille de chaque bateau est vérifiée d'après sa coque (pas de double réduction).

Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/setup_kraken_ships_2026_11_10.py"
Rapport : Saved/Logs/setup_kraken_ships_2026_11_10.txt
"""
import math
import os
import unreal

KRAKEN_DIR = "/Game/Placeable/Character/Foe/Kraken"
HEAD = f"{KRAKEN_DIR}/FoeCharacter_KrakenHead"
RANGE = f"{KRAKEN_DIR}/FoeCharacter_KrakenTentacle_Range"
MELEE = f"{KRAKEN_DIR}/FoeCharacter_KrakenTentacle_Melee"
FINAL = f"{KRAKEN_DIR}/FoeCharacter_KrakenHead_Final"

SHIP_DIR = "/Game/Placeable/Ship"
GALION_BP = f"{SHIP_DIR}/BP_GameBoat_Galion"
BRIGANTIN_BP = f"{SHIP_DIR}/BP_GameBoat_Brigantin"

FULL_HULL_X = 3400.0  # proue de BP_PirateShip (x1)
SLOOP_XY = 0.4
GALION_FROM, GALION_TO = 1.0, 0.7
BRIGANTIN_FROM, BRIGANTIN_TO = 0.7, 0.55
# réglages liés à la taille : valeurs du sloop (x0.4) et de l'ancien galion (x1), interpolées
CAMERA = (5184.0, 7776.0)
CORE_RANGE = (337.5, 675.0)
LONGBOAT_DISTANCE = (500.0, 800.0)

KEEP_SIZE = ("MainMast", "ForeMast", "MizzenMast", "CrowsNest", "Helm", "HelmWheel", "Lantern", "FlagSkull", "Bowsprit")
NATIVE_PLACED = ("HelmStation", "CoreStation", "CrowsNestStation", "LongboatStation", "CannonFront", "CannonFront2", "CannonLeft", "CannonRight",
                 "CannonLeft2", "CannonRight2", "BoardingAccess", "BoardingAccessStarboard", "BoardingAccess2", "BoardingAccess3", "BoardingAccess4",
                 "BoardingAccessStarboard2", "BoardingAccessStarboard3", "BoardingAccessStarboard4", "ShipHealthBar")
# 2e passe : rotation de BP_PirateShip (25 °/s) x1.5 pour les bateaux des joueurs ; caméra de la barre galion 6480 x1.35, brigantin 5832 x1.25
TURN_MULTIPLIER = 1.5
HELM_CAMERA = {f"{SHIP_DIR}/BP_GameBoat_Galion": 6480.0 * 1.35, f"{SHIP_DIR}/BP_GameBoat_Brigantin": 5832.0 * 1.25}
CANNON_SLOTS = ("CannonFront", "CannonFront2", "CannonLeft", "CannonRight", "CannonLeft2", "CannonRight2")
EDGE_INSET = 140.0

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
ys = unreal.YSEditorScriptLibrary
report = []


def log(msg):
    unreal.log(f"[setup_kraken_ships] {msg}")
    report.append(str(msg))


def v(x, y, z):
    return unreal.Vector(x, y, z)


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def compile_save(path):
    unreal.BlueprintEditorLibrary.compile_blueprint(assets.load_asset(path))
    assets.save_asset(path, only_if_is_dirty=False)


def lerp_size(xy, values):
    t = (xy - SLOOP_XY) / (1.0 - SLOOP_XY)
    return values[0] + (values[1] - values[0]) * t


# ---------------------------------------------------------------- Kraken

def setup_kraken():
    values = {
        HEAD: {"BaseHealth": 25000.0, "SuctionSpeedMultiplier": 0.6, "SmashDamage": 2000.0},
        RANGE: {"BaseHealth": 4000.0, "InkDamage": 500.0, "InkBallsPerAttack": 3, "InkBallInterval": 0.25, "InkScatterRadius": 3500.0,
                "InkFlightDuration": 3.5, "InkRadius": 900.0},
        MELEE: {"BaseHealth": 4000.0, "AttackDelay": 2.5},
        FINAL: {"BaseHealth": 40000.0, "SmashDamage": 2000.0, "PatternInterval": 1.0, "ShotRadius": 1080.0, "ShotSpacing": 1980.0, "ShotDamage": 500.0, "LineCount": 7,
                "RingCount": 5, "FirstRingRadius": unreal.Vector2D(2000.0, 7000.0), "RingBoatGap": 1800.0, "RingMinRadius": 2000.0,
                "RingMaxRadius": 0.0, "SpiralBoatGap": 1800.0,
                "Patterns": [unreal.YSKrakenInkPattern.RINGS, unreal.YSKrakenInkPattern.LINES, unreal.YSKrakenInkPattern.SPIRAL]},
    }
    for path, props in values.items():
        default = cdo(path)
        for key, value in props.items():
            default.set_editor_property(key, value)
        compile_save(path)
        default = cdo(path)
        log(f"{path.rsplit('/', 1)[1]} : " + ", ".join(f"{k} = {default.get_editor_property(k)}" for k in props))


# ---------------------------------------------------------------- bateaux

def scale_shape(comp, name, f):
    loc = comp.get_editor_property("relative_location")
    comp.set_editor_property("relative_location", v(loc.x * f, loc.y * f, loc.z))
    if not isinstance(comp, unreal.StaticMeshComponent) or name in KEEP_SIZE:
        return
    scale = comp.get_editor_property("relative_scale3d")
    if "_SailOpen" in name:
        comp.set_editor_property("relative_scale3d", v(scale.x, scale.y * f, scale.z))
    elif "_Yard" in name or "_Sail" in name:
        comp.set_editor_property("relative_scale3d", v(scale.x, scale.y, scale.z * f))
    else:
        comp.set_editor_property("relative_scale3d", v(scale.x * f, scale.y * f, scale.z))


def own_components(bp):
    out = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(handle)
        if lib.is_inherited_component(data) or lib.is_native_component(data):
            continue
        obj = lib.get_object(data)
        if obj and isinstance(obj, unreal.SceneComponent):
            out[obj.get_name().replace("_GEN_VARIABLE", "")] = obj
    return out


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


def posts_to_edge(ship, label):
    """Postes de combat ramenés à EDGE_INSET du bord de la coque dans leur direction (comme setup_previews_2026_11_10.py)"""
    hull = convex_hull([(p.x, p.y) for p in ship.get_editor_property("HullProbePoints")])
    moved = []
    for slot in CANNON_SLOTS:
        comp = ship.get_editor_property(slot)
        if not comp or not comp.get_editor_property("child_actor_class"):
            continue
        loc = comp.get_editor_property("relative_location")
        yaw = comp.get_editor_property("relative_rotation").yaw
        direction = (math.cos(math.radians(yaw)), math.sin(math.radians(yaw)))
        exit_distance = ray_exit(hull, (loc.x, loc.y), direction)
        if exit_distance is None:
            moved.append(f"{slot} hors de la coque ?")
            continue
        advance = exit_distance - EDGE_INSET
        new = v(loc.x + direction[0] * advance, loc.y + direction[1] * advance, loc.z)
        comp.set_editor_property("relative_location", new)
        moved.append(f"{slot} ({new.x:.0f}, {new.y:.0f})")
    log(f"  {label} postes : {', '.join(moved)}")


def rescale_ship(path, label, size_from, size_to, inherited):
    ship = cdo(path)
    hull = ship.get_editor_property("HullProbePoints")
    max_x = max(p.x for p in hull)
    if abs(max_x - FULL_HULL_X * size_to) < 1.0:
        log(f"{label} déjà à x{size_to} (coque jusqu'à {max_x:.0f})")
        return
    if abs(max_x - FULL_HULL_X * size_from) > 1.0:
        raise RuntimeError(f"{label} : coque jusqu'à {max_x:.0f}, ni x{size_from} ni x{size_to} : retouché à la main ?")
    f = size_to / size_from
    bp = assets.load_asset(path)
    shapes = 0
    if inherited:
        # formes héritées de BP_PirateShip : surchargées dans ce Blueprint seulement
        for name in ys.get_inherited_component_names(bp):
            comp = ys.get_inherited_component_override(bp, name)
            if isinstance(comp, unreal.SceneComponent):
                scale_shape(comp, str(name), f)
                shapes += 1
    else:
        for name, comp in own_components(bp).items():
            scale_shape(comp, name, f)
            shapes += 1
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)

    ship = cdo(path)
    for name in NATIVE_PLACED:
        comp = ship.get_editor_property(name)
        if comp:
            loc = comp.get_editor_property("relative_location")
            comp.set_editor_property("relative_location", v(loc.x * f, loc.y * f, loc.z))
    ship.set_editor_property("HullProbePoints", [unreal.Vector2D(p.x * f, p.y * f) for p in hull])
    arm = lerp_size(size_to, CAMERA)
    ship.get_editor_property("HelmStation").get_editor_property("child_actor_template").set_editor_property("CameraArmLength", arm)
    ship.set_editor_property("HelmCameraArmLength", arm)
    ship.get_editor_property("CoreStation").get_editor_property("child_actor_template").set_editor_property("AcceptRange", lerp_size(size_to, CORE_RANGE))
    ship.get_editor_property("LongboatStation").get_editor_property("child_actor_template").set_editor_property(
        "SpawnDistanceBehindHelm", lerp_size(size_to, LONGBOAT_DISTANCE))
    posts_to_edge(ship, label)
    compile_save(path)
    log(f"{label} x{size_from} -> x{size_to} : {shapes} formes, postes et coque x{f:.3f}")


def tune_helm():
    """2e passe de Paul (2026-11-10) : rotation des bateaux des joueurs x1.5, caméra de la barre du galion +35 %, du brigantin +25 %
    (valeurs absolues : relançable)"""
    base_turn = cdo(f"{SHIP_DIR}/BP_PirateShip").get_editor_property("MaxTurnRate")
    for path, label in ((f"{SHIP_DIR}/BP_GameBoat_Sloop", "sloop"), (BRIGANTIN_BP, "brigantin"), (GALION_BP, "galion")):
        ship = cdo(path)
        before = ship.get_editor_property("MaxTurnRate")
        ship.set_editor_property("MaxTurnRate", base_turn * TURN_MULTIPLIER)
        if path in HELM_CAMERA:
            ship.set_editor_property("HelmCameraArmLength", HELM_CAMERA[path])
            ship.get_editor_property("HelmStation").get_editor_property("child_actor_template").set_editor_property("CameraArmLength", HELM_CAMERA[path])
        compile_save(path)
        ship = cdo(path)
        log(f"{label} : rotation max {before:.1f} -> {ship.get_editor_property('MaxTurnRate'):.1f} °/s, caméra de la barre {ship.get_editor_property('HelmCameraArmLength'):.0f}")


def dump_ship(path, label):
    ship = cdo(path)
    hull = ship.get_editor_property("HullProbePoints")
    accesses = [n for n in NATIVE_PLACED if n.startswith("BoardingAccess") and ship.get_editor_property(n).get_editor_property("child_actor_class")]
    log(f"relu {label} : coque X {min(p.x for p in hull):.0f}..{max(p.x for p in hull):.0f}, Y +/-{max(abs(p.y) for p in hull):.0f}, "
        f"caméra {ship.get_editor_property('HelmCameraArmLength'):.0f}, "
        f"zone du core {ship.get_editor_property('CoreStation').get_editor_property('child_actor_template').get_editor_property('AcceptRange'):.0f}, "
        f"{len(accesses)} accès")


try:
    setup_kraken()
    pirate_before = max(p.x for p in cdo(f"{SHIP_DIR}/BP_PirateShip").get_editor_property("HullProbePoints"))
    rescale_ship(GALION_BP, "galion", GALION_FROM, GALION_TO, True)
    rescale_ship(BRIGANTIN_BP, "brigantin", BRIGANTIN_FROM, BRIGANTIN_TO, False)
    tune_helm()
    dump_ship(GALION_BP, "galion")
    dump_ship(BRIGANTIN_BP, "brigantin")
    dump_ship(f"{SHIP_DIR}/BP_GameBoat_Sloop", "sloop")
    # contrôle : BP_PirateShip (parent du galion) inchangé
    pirate = cdo(f"{SHIP_DIR}/BP_PirateShip")
    pirate_shapes = [str(n) for n in ys.get_inherited_component_names(assets.load_asset(GALION_BP))][:3]
    log(f"contrôle BP_PirateShip : proue {pirate_before:.0f} -> {max(p.x for p in pirate.get_editor_property('HullProbePoints')):.0f} (doit rester 3400), "
        f"formes vues par le galion : {pirate_shapes}...")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_kraken_ships_2026_11_10.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
