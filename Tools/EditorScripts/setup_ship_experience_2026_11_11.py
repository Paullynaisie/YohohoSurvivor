"""
Retours de Paul du 2026-11-11 (expérience bateau) :
  - bateaux des joueurs (sloop, brigantin, galion) : rotation max -20 % (BP_PirateShip x1.5 x0.8), décélération +50 % (BP_PirateShip x1.5)
  - 2e escalier du gaillard (bâbord, miroir de celui de tribord : StepPort1..6), ouverture du bastingage du gaillard des deux côtés,
    porte de la cabine et chalouperie au centre entre les deux escaliers
  - ShootPost : plateforme ronde (ShootPostPlatform, solide) autour du mât avant à Platform Height au-dessus du pont, 4 places
    (composants natifs ShootPost1..4 -> /Game/Placeable/Ship/Stations/BP_ShootPost, enfant d'AYSShootPost)
  - Kraken : tentacule à distance, l'ancienne attaque (8 boules dispersées à 50 m) revient dans la liste des attaques
Relançable : les formes ajoutées sont mises à jour si elles existent, les valeurs sont recalculées depuis BP_PirateShip.

Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/setup_ship_experience_2026_11_11.py"
Rapport : Saved/Logs/setup_ship_experience_2026_11_11.txt
"""
import os
import unreal

SHIP_DIR = "/Game/Placeable/Ship"
STATION_DIR = f"{SHIP_DIR}/Stations"
SHOOT_POST_BP = f"{STATION_DIR}/BP_ShootPost"
BOATS = ((f"{SHIP_DIR}/BP_GameBoat_Sloop", "sloop", False), (f"{SHIP_DIR}/BP_GameBoat_Brigantin", "brigantin", False),
         (f"{SHIP_DIR}/BP_GameBoat_Galion", "galion", True))
RANGE = "/Game/Placeable/Character/Foe/Kraken/FoeCharacter_KrakenTentacle_Range"

TURN_MULTIPLIER = 1.5 * 0.8
DECEL_MULTIPLIER = 1.5
STEPS = 6
DECK = 210.0
PLATFORM_HEIGHT = 220.0       # dessus de la plateforme au-dessus du pont (assez haut pour passer dessous : capsule ~192)
PLATFORM_RADIUS = 170.0
PLATFORM_THICKNESS = 10.0
SEAT_RADIUS = 100.0
CYLINDER = "/Engine/BasicShapes/Cylinder.Cylinder"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
ys = unreal.YSEditorScriptLibrary
report = []


def log(msg):
    unreal.log(f"[setup_ship_experience] {msg}")
    report.append(str(msg))


def v(x, y, z):
    return unreal.Vector(x, y, z)


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def compile_save(path):
    unreal.BlueprintEditorLibrary.compile_blueprint(assets.load_asset(path))
    if not assets.save_asset(path, only_if_is_dirty=False):
        log(f"  ÉCHEC de la sauvegarde : {path}")


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


def shapes_of(bp, inherited):
    """Formes du bateau : les siennes + (galion) les surcharges de celles héritées de BP_PirateShip"""
    out = {}
    if inherited:
        for name in ys.get_inherited_component_names(bp):
            comp = ys.get_inherited_component_override(bp, name)
            if isinstance(comp, unreal.SceneComponent):
                out[str(name)] = comp
    out.update(own_components(bp))
    return out


def add_shape(bp, name, mesh, material, solid):
    root = subsystem.k2_gather_subobject_data_for_blueprint(bp)[0]
    params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.StaticMeshComponent, blueprint_context=bp)
    handle, fail = subsystem.add_new_subobject(params)
    if not lib.is_handle_valid(handle):
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    subsystem.rename_subobject(handle, unreal.Text(name))
    comp = lib.get_object(lib.get_data(handle))
    comp.set_editor_property("static_mesh", assets.load_asset(mesh) if isinstance(mesh, str) else mesh)
    comp.set_material(0, material)
    comp.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    comp.set_collision_profile_name("BlockAll" if solid else "NoCollision")
    return comp


def place(comp, location, size, yaw=0.0):
    comp.set_editor_property("relative_location", location)
    comp.set_editor_property("relative_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    comp.set_editor_property("relative_scale3d", v(size.x / 100.0, size.y / 100.0, size.z / 100.0))


def get_size(comp):
    s = comp.get_editor_property("relative_scale3d")
    return v(s.x * 100.0, s.y * 100.0, s.z * 100.0)


def ensure_shoot_post_bp():
    if assets.does_asset_exist(SHOOT_POST_BP):
        return
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", unreal.YSShootPost)
    tools.create_asset("BP_ShootPost", STATION_DIR, unreal.Blueprint, factory)
    compile_save(SHOOT_POST_BP)
    log(f"créé : {SHOOT_POST_BP}")


def setup_boat(path, label, inherited, base):
    bp = assets.load_asset(path)
    shapes = shapes_of(bp, inherited)
    ship = cdo(path)

    # manœuvre
    ship.set_editor_property("MaxTurnRate", base["MaxTurnRate"] * TURN_MULTIPLIER)
    ship.set_editor_property("Deceleration", base["Deceleration"] * DECEL_MULTIPLIER)

    # 2e escalier : miroir de Step1..6 (tribord) côté bâbord
    inner = None
    for k in range(1, STEPS + 1):
        step = shapes[f"Step{k}"]
        loc = step.get_editor_property("relative_location")
        size = get_size(step)
        inner = abs(loc.y) - size.y / 2.0
        name = f"StepPort{k}"
        comp = shapes.get(name) or add_shape(bp, name, step.get_editor_property("static_mesh"), step.get_material(0), True)
        place(comp, v(loc.x, -loc.y, loc.z), size)
    # bastingage avant du gaillard : ouvert au-dessus des deux escaliers (centré, même écart qu'avant côté tribord)
    rail = shapes["CastleRailFront"]
    rloc = rail.get_editor_property("relative_location")
    rsize = get_size(rail)
    rail_end = rloc.y + rsize.y / 2.0
    if rail_end < inner - 1.0 or abs(rloc.y) > 1.0:
        half = min(rail_end, inner - 16.0)
        place(rail, v(rloc.x, 0.0, rloc.z), v(rsize.x, 2.0 * half, rsize.z))
    # porte de la cabine au milieu, entre les escaliers (même largeur, sans dépasser)
    door = shapes["CastleDoor"]
    dloc = door.get_editor_property("relative_location")
    dsize = get_size(door)
    place(door, v(dloc.x, 0.0, dloc.z), v(dsize.x, min(dsize.y, 2.0 * inner), dsize.z))

    # plateforme du ShootPost autour du mât avant
    mast = shapes["ForeMast"]
    mloc = mast.get_editor_property("relative_location")
    deck_material = shapes["Deck"].get_material(0)
    platform = shapes.get("ShootPostPlatform") or add_shape(bp, "ShootPostPlatform", CYLINDER, deck_material, True)
    place(platform, v(mloc.x, 0.0, DECK + PLATFORM_HEIGHT - PLATFORM_THICKNESS / 2.0), v(PLATFORM_RADIUS * 2, PLATFORM_RADIUS * 2, PLATFORM_THICKNESS))
    rim = shapes.get("ShootPostRim") or add_shape(bp, "ShootPostRim", CYLINDER, shapes["Castle"].get_material(0), False)
    place(rim, v(mloc.x, 0.0, DECK + PLATFORM_HEIGHT - PLATFORM_THICKNESS - 6.0), v(PLATFORM_RADIUS * 2 + 16, PLATFORM_RADIUS * 2 + 16, 12.0))
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)

    # places natives (pied sur le pont, joueur sur la plateforme), chalouperie au centre
    ship = cdo(path)
    seat_class = assets.load_blueprint_class(SHOOT_POST_BP)
    for index, (dx, dy, yaw) in enumerate(((SEAT_RADIUS, 0.0, 0.0), (0.0, SEAT_RADIUS, 90.0), (-SEAT_RADIUS, 0.0, 180.0), (0.0, -SEAT_RADIUS, -90.0)), 1):
        comp = ship.get_editor_property(f"ShootPost{index}")
        comp.set_editor_property("child_actor_class", seat_class)
        comp.set_editor_property("relative_location", v(mloc.x + dx, dy, DECK))
        comp.set_editor_property("relative_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=yaw))
    launcher = ship.get_editor_property("LongboatStation")
    lloc = launcher.get_editor_property("relative_location")
    launcher.set_editor_property("relative_location", v(lloc.x, 0.0, lloc.z))
    compile_save(path)

    ship = cdo(path)
    log(f"{label} : rotation {ship.get_editor_property('MaxTurnRate'):.1f} °/s, décélération {ship.get_editor_property('Deceleration'):.0f}, "
        f"escalier bâbord (bord intérieur à {inner:.0f}), porte et chalouperie au centre, plateforme du ShootPost au mât avant X {mloc.x:.0f} "
        f"(+{PLATFORM_HEIGHT:.0f}), 4 places")


try:
    ensure_shoot_post_bp()

    pirate = cdo(f"{SHIP_DIR}/BP_PirateShip")
    base = {"MaxTurnRate": pirate.get_editor_property("MaxTurnRate"), "Deceleration": pirate.get_editor_property("Deceleration")}
    log(f"BP_PirateShip : rotation {base['MaxTurnRate']}, décélération {base['Deceleration']}")
    for path, label, inherited in BOATS:
        setup_boat(path, label, inherited, base)

    tentacle = cdo(RANGE)
    tentacle.set_editor_property("Attacks", [unreal.YSKrakenTentacleAttack.CROSS, unreal.YSKrakenTentacleAttack.CIRCLE, unreal.YSKrakenTentacleAttack.SCATTER_BALLS])
    tentacle.set_editor_property("InkBallsPerAttack", 8)
    tentacle.set_editor_property("InkScatterRadius", 5000.0)
    compile_save(RANGE)
    tentacle = cdo(RANGE)
    log(f"Tentacule à distance : {tentacle.get_editor_property('Attacks')}, {tentacle.get_editor_property('InkBallsPerAttack')} boules, "
        f"imprécision {tentacle.get_editor_property('InkScatterRadius'):.0f}")
except Exception as error:
    log(f"ERREUR : {error}")
    raise
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_ship_experience_2026_11_11.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
