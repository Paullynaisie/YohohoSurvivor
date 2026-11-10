"""
Kraken en primitives (demande de Paul, 2026-11-10), SANS CODE : deux props à retoucher dans l'éditeur
  - /Game/Art/Props/Kraken/BP_Prop_KrakenHead : tête à la taille d'un Galion (manteau ~26 x 24 x 40 m, visage, yeux à pupille fendue,
    bec, taches) ; origine = niveau de la mer, regard vers +X
  - /Game/Art/Props/Kraken/BP_Prop_KrakenTentacle : tentacule à la taille d'un sloop (~23 m le long de la courbe) qui sort de l'eau et
    s'enroule vers +X, chaîne de sphères qui s'affinent + ventouses côté intérieur ; origine = niveau de la mer
  - FlowMap : la tête entourée de 8 tentacules au centre de la future zone « Boss fight final », à 750 m du centre (au nord, +X :
    provisoire, la zone sera placée par le jeu), tête tournée vers le centre de la map ; dossier BossFightFinal, tag KrakenPreview
Toutes les formes sans collision (rien ne gêne les bateaux en attendant le combat).
Relançable : les Blueprints qui existent ne sont pas reconstruits (retouches gardées) ; les acteurs tagués KrakenPreview sont reposés.

Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/setup_kraken_preview.py"
Rapport : Saved/Logs/setup_kraken_preview.txt
"""
import math
import os
import unreal

PROP_DIR = "/Game/Art/Props"
KRAKEN_DIR = f"{PROP_DIR}/Kraken"
HEAD = f"{KRAKEN_DIR}/BP_Prop_KrakenHead"
TENTACLE = f"{KRAKEN_DIR}/BP_Prop_KrakenTentacle"
FLOW_MAP = "/Game/Maps/FlowMap"
MATS = "/Game/Art/Materials/PH_materials"
SPHERE = "/Engine/BasicShapes/Sphere"
CONE = "/Engine/BasicShapes/Cone"
ZONE_DISTANCE = 75000.0          # 750 m du centre de la map
TENTACLE_RING = 2400.0           # distance tête -> pied des tentacules
TENTACLE_SCALES = (1.0, 0.85)    # en alternance
TAG = "KrakenPreview"
HEAD_SCALE = 1.3                 # tête ~34 x 31 x 52 m (Galion : 58 m de long)

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
report = []


def log(msg):
    unreal.log(f"[setup_kraken_preview] {msg}")
    report.append(str(msg))


def mat(name):
    return assets.load_asset(f"{MATS}/m_PH_{name}")


def v(x, y, z):
    return unreal.Vector(x, y, z)


def r(roll=0.0, pitch=0.0, yaw=0.0):
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


REBUILD = False  # True = reconstruit les formes des Blueprints (efface les retouches)
PLACE = False   # True = repose le kraken de prévisualisation dans la FlowMap (plus utile : BP_KrakenArena le pose en jeu)


def make_bp(path):
    if assets.does_asset_exist(path):
        bp = assets.load_asset(path)
        if not REBUILD:
            return bp, False
        handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
        own = [h for h in handles if lib.get_object(lib.get_data(h)) and isinstance(lib.get_object(lib.get_data(h)), unreal.StaticMeshComponent)
               and not lib.is_inherited_component(lib.get_data(h))]
        if own:
            subsystem.delete_subobjects(handles[0], own, bp)
        return bp, True
    directory, name = path.rsplit("/", 1)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", assets.load_blueprint_class(f"{PROP_DIR}/BP_Prop"))
    return tools.create_asset(name, directory, unreal.Blueprint, factory), True


def add_shape_raw(*args):
    return add_shape(*args)


def add_shape(bp, name, mesh, material, location, rotation, size_cm):
    """size_cm : taille en cm (forme de base de 100 cm)"""
    root = subsystem.k2_gather_subobject_data_for_blueprint(bp)[0]
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if obj and obj.get_name().replace("_GEN_VARIABLE", "") == "Root":
            root = handle
    params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.StaticMeshComponent, blueprint_context=bp)
    handle, fail = subsystem.add_new_subobject(params)
    if not lib.is_handle_valid(handle):
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    subsystem.rename_subobject(handle, unreal.Text(name))
    shape = lib.get_object(lib.get_data(handle))
    shape.set_editor_property("static_mesh", assets.load_asset(mesh))
    shape.set_material(0, material)
    shape.set_editor_property("relative_location", location)
    shape.set_editor_property("relative_rotation", rotation)
    shape.set_editor_property("relative_scale3d", v(size_cm.x / 100.0, size_cm.y / 100.0, size_cm.z / 100.0))
    shape.set_collision_profile_name("NoCollision")
    shape.set_editor_property("generate_overlap_events", False)
    return shape


def build_head(bp):
    def add_shape(bp_, name, mesh, material, location, rotation, size_cm):  # tête x HEAD_SCALE
        return add_shape_raw(bp_, name, mesh, material, location * HEAD_SCALE, rotation, size_cm * HEAD_SCALE)

    body, spot, eye, pupil, beak = mat("monochrome_Red"), mat("monochrome_LightRed"), mat("monochrome_Yellow"), mat("monochrome_Black"), mat("monochrome_LightBlack")
    n = 0
    # manteau penché vers l'arrière, visage large au ras de l'eau
    add_shape(bp, "Mantle", SPHERE, body, v(-500, 0, 1900), r(pitch=14), v(2600, 2400, 4000)); n += 1
    add_shape(bp, "MantleTip", SPHERE, body, v(-1250, 0, 3500), r(pitch=30), v(1500, 1400, 1700)); n += 1
    add_shape(bp, "Face", SPHERE, body, v(300, 0, 350), r(), v(3000, 3000, 1600)); n += 1
    add_shape(bp, "Cheeks", SPHERE, body, v(700, 0, 150), r(), v(2200, 3400, 1100)); n += 1
    for side, y in (("L", -1), ("R", 1)):
        add_shape(bp, f"Eye{side}", SPHERE, eye, v(1300, 720 * y, 780), r(), v(900, 900, 900)); n += 1
        add_shape(bp, f"Pupil{side}", SPHERE, pupil, v(1730, 740 * y, 780), r(), v(140, 240, 640)); n += 1
        add_shape(bp, f"Brow{side}", SPHERE, body, v(1250, 740 * y, 1180), r(roll=-14 * y, pitch=-10), v(1000, 950, 300)); n += 1
    # bec : deux cônes noirs pointés vers l'avant (+X), entrouverts
    add_shape(bp, "BeakUpper", CONE, beak, v(1550, 0, 120), r(pitch=-80), v(320, 320, 420)); n += 1
    add_shape(bp, "BeakLower", CONE, beak, v(1520, 0, -60), r(pitch=-100), v(260, 260, 340)); n += 1
    # taches sur le manteau
    for i, (x, y, z, s) in enumerate(((-150, -900, 2300, 520), (-300, 950, 1900, 450), (-700, -500, 3000, 380),
                                      (-650, 650, 3150, 340), (100, -1100, 1300, 420), (50, 1150, 1500, 380), (-1150, 0, 3900, 300))):
        add_shape(bp, f"Spot{i + 1}", SPHERE, spot, v(x, y, z), r(), v(s, s, s * 0.45)); n += 1
    return n


def bezier(p, t):
    u = 1.0 - t
    return [u ** 3 * p[0][k] + 3 * u * u * t * p[1][k] + 3 * u * t * t * p[2][k] + t ** 3 * p[3][k] for k in range(2)]


def bezier_d1(p, t):
    u = 1.0 - t
    return [3 * u * u * (p[1][k] - p[0][k]) + 6 * u * t * (p[2][k] - p[1][k]) + 3 * t * t * (p[3][k] - p[2][k]) for k in range(2)]


def bezier_d2(p, t):
    u = 1.0 - t
    return [6 * u * (p[2][k] - 2 * p[1][k] + p[0][k]) + 6 * t * (p[3][k] - 2 * p[2][k] + p[1][k]) for k in range(2)]


def build_tentacle(bp):
    body, sucker = mat("monochrome_Red"), mat("monochrome_Pink")
    # courbe dans le plan XZ (x, z) : sort de l'eau, monte, s'enroule vers l'avant (~23 m)
    curve = ((0.0, -300.0), (-200.0, 1500.0), (1300.0, 2700.0), (1700.0, 1100.0))
    count, n = 44, 0
    for i in range(count):
        t = i / (count - 1)
        x, z = bezier(curve, t)
        radius = 380.0 * (1.0 - t) + 70.0 * t
        add_shape(bp, f"Segment{i + 1:02d}", SPHERE, body, v(x, 0, z), r(), v(radius * 2, radius * 2, radius * 2)); n += 1
        if i % 3 != 1 or i == count - 1:
            continue
        # ventouses côté intérieur de la courbe (vers le centre de courbure), deux rangées
        dx, dz = bezier_d1(curve, t)
        ax, az = bezier_d2(curve, t)
        nx, nz = -dz, dx
        if nx * ax + nz * az < 0.0:
            nx, nz = -nx, -nz
        length = math.hypot(nx, nz) or 1.0
        nx, nz = nx / length, nz / length
        pitch = math.degrees(math.atan2(-nx, nz))
        size = radius * 0.7
        for side, y in (("A", -0.33), ("B", 0.33)):
            add_shape(bp, f"Sucker{i + 1:02d}{side}", SPHERE, sucker, v(x + nx * radius * 0.82, y * radius, z + nz * radius * 0.82), r(pitch=pitch),
                      v(size, size, size * 0.3)); n += 1
    return n


def place(sea_z):
    world = unreal.EditorLevelLibrary.get_editor_world()
    for actor in actors.get_all_level_actors():
        if actor.actor_has_tag(TAG):
            actors.destroy_actor(actor)
    center = v(ZONE_DISTANCE, 0.0, sea_z)
    head = actors.spawn_actor_from_class(assets.load_blueprint_class(HEAD), center, r(yaw=180.0))
    head.tags = [unreal.Name(TAG)]
    head.set_actor_label("Kraken_Head")
    head.set_folder_path("BossFightFinal")
    tentacle_class = assets.load_blueprint_class(TENTACLE)
    for i in range(8):
        angle = 22.5 + 45.0 * i  # aucun tentacule devant le regard (180°)
        rad = math.radians(angle)
        loc = v(center.x + math.cos(rad) * TENTACLE_RING, center.y + math.sin(rad) * TENTACLE_RING, sea_z)
        tentacle = actors.spawn_actor_from_class(tentacle_class, loc, r(yaw=angle))
        scale = TENTACLE_SCALES[i % len(TENTACLE_SCALES)]
        tentacle.set_actor_scale3d(v(scale, scale, scale))
        tentacle.tags = [unreal.Name(TAG)]
        tentacle.set_actor_label(f"Kraken_Tentacle{i + 1}")
        tentacle.set_folder_path("BossFightFinal")
    log(f"FlowMap : tête en {center} (yaw 180) + 8 tentacules à {TENTACLE_RING} cm, mer à {sea_z}")


try:
    for path, builder in ((HEAD, build_head), (TENTACLE, build_tentacle)):
        bp, created = make_bp(path)
        if created:
            count = builder(bp)
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            assets.save_asset(path, only_if_is_dirty=False)
            log(f"{path} : créé, {count} formes")
        else:
            log(f"{path} : existe déjà (gardé)")

    if not PLACE:
        raise SystemExit
    if not levels.load_level(FLOW_MAP):
        raise RuntimeError("FlowMap introuvable")
    oceans = [a for a in actors.get_all_level_actors() if isinstance(a, unreal.YSOcean)]
    sea_z = oceans[0].get_actor_location().z if oceans else 0.0
    place(sea_z)
    if not levels.save_current_level():
        raise RuntimeError("sauvegarde de la FlowMap impossible")
    log("OK")
except SystemExit:
    log("OK (sans pose)")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_kraken_preview.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
