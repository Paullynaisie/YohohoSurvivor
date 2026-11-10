"""
Équipage du bateau et retouches des bateaux (demande de Paul, 2026-11-10) :
  - BP_GameBoat_Brigantin ramené de x0.8 à x0.7 de BP_PirateShip (moyenne du sloop x0.4 et du galion x1) : positions, formes, coque, postes
    (fait une seule fois : vérifié d'après la coque)
  - accès à bord = échelles de corde (AYSShipBoarding, Show Ladder) : sloop 2 + 2, brigantin 3 + 3, galion 4 + 4 ; planches d'embarquement
    (Gangplank, Gangplank Starboard) cachées (Hidden Component Names, rien de supprimé)
  - caméra de la barre +20 % : sloop 5184, brigantin 6480, galion 7776 (BP_GameBoat_Galion et BP_PirateShip), BP_Longboat 5184
  - zones de dépôt : plage (Boat Zone Radius des îles) x0.5, core des bateaux des joueurs x0.75
  - armes visuelles des membres d'équipage : /Game/Placeable/Weapons/BoatCrew (maillet, grande seringue, compas, poêle, sniper ; Replicates décoché)
  - /Game/Placeable/InteractiveObjects/BoatCrew/BP_BoatCrew_Core (AYSBoatCrew) + 5 enfants : Charpentier, Medic, Navigateur, Cuisinier,
    Flibustier ; BP_FlowMod : Crew Item Classes = les 5, Crew Count 2
Relançable : les Blueprints qui existent ne sont pas recréés (retouches de Paul gardées), leurs réglages de bonus et d'arme sont remis.

Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/setup_boat_crew.py"
Rapport : Saved/Logs/setup_boat_crew.txt
"""
import os
import unreal

SHIP_DIR = "/Game/Placeable/Ship"
SLOOP_BP = f"{SHIP_DIR}/BP_GameBoat_Sloop"
BRIGANTIN_BP = f"{SHIP_DIR}/BP_GameBoat_Brigantin"
GALION_BP = f"{SHIP_DIR}/BP_GameBoat_Galion"
PIRATE_BP = f"{SHIP_DIR}/BP_PirateShip"
LONGBOAT_BP = f"{SHIP_DIR}/BP_Longboat"
WEAPON_DIR = "/Game/Placeable/Weapons/BoatCrew"
ARME_BP = "/Game/Placeable/Weapons/BP_Arme"
SNIPER_BP = "/Game/Placeable/Weapons/Player/Wp_Guns/BP_Sniper"
CREW_DIR = "/Game/Placeable/InteractiveObjects/BoatCrew"
CORE_BP = f"{CREW_DIR}/BP_BoatCrew_Core"
CARPENTER_ITEM = "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_Carpenter"
FLOW_GM = "/Game/Core/GameModes/BP_FlowMod"
FACE_TABLE = "/Game/Core/Customization/DT_CustomizationFaces"
FACE_FALLBACK = "/Game/Art/Textures/CharacterFaces/PlayerFace/t_i_Face1"
FACE_DECAL = "/Game/Art/Materials/Decals/M_FaceDecal_1"
ICONS = "/Game/Art/Textures/Icones"
MAT = "/Game/Art/Materials/PH_materials"
CUBE = "/Engine/BasicShapes/Cube"
CYLINDER = "/Engine/BasicShapes/Cylinder"
SPHERE = "/Engine/BasicShapes/Sphere"

BRIGANTIN_FROM = 0.7  # 0.8 -> 0.7 le 2026-11-10, puis 0.55 par setup_kraken_ships_2026_11_10.py
BRIGANTIN_TO = 0.55
KEEP_SIZE = ("MainMast", "ForeMast", "MizzenMast", "CrowsNest", "Helm", "HelmWheel", "Lantern", "FlagSkull", "Bowsprit")
NATIVE_PLACED = ("HelmStation", "CoreStation", "CrowsNestStation", "LongboatStation", "CannonFront", "CannonFront2", "CannonLeft", "CannonRight",
                 "CannonLeft2", "CannonRight2", "BoardingAccess", "BoardingAccessStarboard", "BoardingAccess2", "BoardingAccess3", "BoardingAccess4",
                 "BoardingAccessStarboard2", "BoardingAccessStarboard3", "BoardingAccessStarboard4", "ShipHealthBar")
PORT_ACCESSES = ("BoardingAccess", "BoardingAccess2", "BoardingAccess3", "BoardingAccess4")
STARBOARD_ACCESSES = ("BoardingAccessStarboard", "BoardingAccessStarboard2", "BoardingAccessStarboard3", "BoardingAccessStarboard4")
# X des échelles (entre les postes de combat des flancs et loin de la chalouperie)
LADDERS = {SLOOP_BP: (-250.0, 650.0), BRIGANTIN_BP: (-943.0, 55.0, 982.0), GALION_BP: (-1330.0, -840.0, 140.0, 1120.0)}  # brigantin x0.55, galion x0.7
HELM = {SLOOP_BP: 4320.0 * 1.2, BRIGANTIN_BP: 7290.0, GALION_BP: 8748.0, PIRATE_BP: 6480.0 * 1.2, LONGBOAT_BP: 4320.0 * 1.2}
# zone du core x0.75 (brigantin : moyenne sloop / galion, déjà ramené à sa nouvelle taille)
CORE_RANGE = {SLOOP_BP: 450.0 * 0.75, BRIGANTIN_BP: 421.875, GALION_BP: 506.25, PIRATE_BP: 900.0 * 0.75}
BEACH_ZONE = 1500.0 * 0.5

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
copy_props = unreal.YSEditorScriptLibrary.copy_editable_properties
report = []


def log(msg):
    unreal.log(f"[setup_boat_crew] {msg}")
    report.append(str(msg))


def v(x, y, z):
    return unreal.Vector(x, y, z)


def r(roll=0.0, pitch=0.0, yaw=0.0):
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def mat(name):
    return assets.load_asset(f"{MAT}/{name}")


def compile_save(path):
    unreal.BlueprintEditorLibrary.compile_blueprint(assets.load_asset(path))
    assets.save_asset(path, only_if_is_dirty=False)


def scs_components(bp):
    out = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if obj:
            out[obj.get_name().replace("_GEN_VARIABLE", "")] = obj
    return out


def make_bp(path, parent_class):
    if assets.does_asset_exist(path):
        return assets.load_asset(path), False
    directory, name = path.rsplit("/", 1)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    bp = tools.create_asset(name, directory, unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    return bp, True


def add_shape(bp, name, mesh, material, location, rotation, scale):
    root = subsystem.k2_gather_subobject_data_for_blueprint(bp)[0]
    params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.StaticMeshComponent, blueprint_context=bp)
    handle, fail = subsystem.add_new_subobject(params)
    if not lib.is_handle_valid(handle):
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    subsystem.rename_subobject(handle, unreal.Text(name))
    shape = lib.get_object(lib.get_data(handle))
    shape.set_editor_property("static_mesh", mesh if isinstance(mesh, unreal.StaticMesh) else assets.load_asset(mesh))
    if material:
        shape.set_material(0, material)
    shape.set_editor_property("relative_location", location)
    shape.set_editor_property("relative_rotation", rotation)
    shape.set_editor_property("relative_scale3d", scale)
    shape.set_collision_profile_name("NoCollision")
    shape.set_editor_property("generate_overlap_events", False)
    return shape


# ---------------------------------------------------------------- bateaux

def rescale_brigantin():
    ship = cdo(BRIGANTIN_BP)
    hull = ship.get_editor_property("HullProbePoints")
    max_x = max(p.x for p in hull)
    expected_from = 3400.0 * BRIGANTIN_FROM
    if abs(max_x - 3400.0 * BRIGANTIN_TO) < 1.0:
        log(f"brigantin déjà à x{BRIGANTIN_TO} (coque jusqu'à {max_x:.0f})")
        return
    if abs(max_x - expected_from) > 1.0:
        raise RuntimeError(f"brigantin : coque jusqu'à {max_x:.0f}, ni x{BRIGANTIN_FROM} ni x{BRIGANTIN_TO} : retouché à la main ?")
    f = BRIGANTIN_TO / BRIGANTIN_FROM
    bp = assets.load_asset(BRIGANTIN_BP)
    shapes = 0
    for name, comp in scs_components(bp).items():
        if not isinstance(comp, unreal.StaticMeshComponent):
            continue
        loc = comp.get_editor_property("relative_location")
        scale = comp.get_editor_property("relative_scale3d")
        comp.set_editor_property("relative_location", v(loc.x * f, loc.y * f, loc.z))
        if name in KEEP_SIZE:
            pass
        elif "_SailOpen" in name:
            comp.set_editor_property("relative_scale3d", v(scale.x, scale.y * f, scale.z))
        elif "_Yard" in name or "_Sail" in name:
            comp.set_editor_property("relative_scale3d", v(scale.x, scale.y, scale.z * f))
        else:
            comp.set_editor_property("relative_scale3d", v(scale.x * f, scale.y * f, scale.z))
        shapes += 1
    ship = cdo(BRIGANTIN_BP)
    for name in NATIVE_PLACED:
        comp = ship.get_editor_property(name)
        if comp:
            loc = comp.get_editor_property("relative_location")
            comp.set_editor_property("relative_location", v(loc.x * f, loc.y * f, loc.z))
    ship.set_editor_property("HullProbePoints", [unreal.Vector2D(p.x * f, p.y * f) for p in hull])
    ship.get_editor_property("LongboatStation").get_editor_property("child_actor_template").set_editor_property("SpawnDistanceBehindHelm", 650.0)
    compile_save(BRIGANTIN_BP)
    log(f"brigantin x{BRIGANTIN_FROM} -> x{BRIGANTIN_TO} : {shapes} formes, postes et coque x{f:.3f} (coque jusqu'à {max(p.x for p in cdo(BRIGANTIN_BP).get_editor_property('HullProbePoints')):.0f})")


def setup_ladders(path):
    ship = cdo(path)
    hull = ship.get_editor_property("HullProbePoints")
    half_width = max(abs(p.y) for p in hull)
    source = ship.get_editor_property("BoardingAccess")
    source_template = source.get_editor_property("child_actor_template")
    access_class = source.get_editor_property("child_actor_class") or unreal.YSShipBoarding
    xs = LADDERS[path]
    for side, names, sign, yaw in (("bâbord", PORT_ACCESSES, -1.0, -90.0), ("tribord", STARBOARD_ACCESSES, 1.0, 90.0)):
        for index, name in enumerate(names):
            comp = ship.get_editor_property(name)
            if index >= len(xs):
                comp.set_child_actor_class(None)
                continue
            if comp.get_editor_property("child_actor_class") != access_class:
                comp.set_child_actor_class(access_class)
            comp.set_editor_property("relative_location", v(xs[index], sign * half_width, 0.0))
            comp.set_editor_property("relative_rotation", r(yaw=yaw))
            template = comp.get_editor_property("child_actor_template")
            if source_template and template and comp != source:
                copy_props(source_template, template)
            if template:
                template.set_editor_property("bShowLadder", True)
    ship.set_editor_property("HiddenComponentNames", [unreal.Name("Gangplank"), unreal.Name("GangplankStarboard")])
    ship.set_editor_property("bBoardingLadders", True)
    compile_save(path)
    ship = cdo(path)
    placed = [f"{n} ({ship.get_editor_property(n).get_editor_property('relative_location').x:.0f}, {ship.get_editor_property(n).get_editor_property('relative_location').y:.0f})"
              for n in PORT_ACCESSES + STARBOARD_ACCESSES if ship.get_editor_property(n).get_editor_property("child_actor_class")]
    log(f"{path.rsplit('/', 1)[1]} : {len(placed)} échelles de corde : {', '.join(placed)} ; planches cachées")


def setup_ship_values():
    for path, arm in HELM.items():
        template = cdo(path).get_editor_property("HelmStation").get_editor_property("child_actor_template")
        before = template.get_editor_property("CameraArmLength")
        template.set_editor_property("CameraArmLength", arm)
        # aussi sur le bateau (Helm Camera Arm Length) : le gabarit de la barre d un Blueprint enfant (BP_GameBoat_Galion) ne garde pas ses retouches
        cdo(path).set_editor_property("HelmCameraArmLength", arm)
        compile_save(path)
        log(f"{path.rsplit('/', 1)[1]} : caméra de la barre {before:.0f} -> {cdo(path).get_editor_property('HelmCameraArmLength'):.0f}")
    for path, radius in CORE_RANGE.items():
        template = cdo(path).get_editor_property("CoreStation").get_editor_property("child_actor_template")
        before = template.get_editor_property("AcceptRange")
        template.set_editor_property("AcceptRange", radius)
        compile_save(path)
        log(f"{path.rsplit('/', 1)[1]} : zone du core {before:.0f} -> {radius:.0f}")
    for path in assets.list_assets("/Game/Core/Islands", recursive=True):
        data = assets.load_asset(path.split(".")[0])
        if isinstance(data, unreal.YSIslandData):
            before = data.get_editor_property("BoatZoneRadius")
            data.set_editor_property("BoatZoneRadius", BEACH_ZONE)
            assets.save_loaded_asset(data, only_if_is_dirty=False)
            log(f"{data.get_name()} : zone de dépôt de la plage {before:.0f} -> {BEACH_ZONE:.0f}")


# ---------------------------------------------------------------- armes visuelles

def weapon(name, display, shapes):
    path = f"{WEAPON_DIR}/{name}"
    bp, created = make_bp(path, assets.load_blueprint_class(ARME_BP))
    if created:
        for shape in shapes:
            add_shape(bp, *shape)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    default = cdo(path)
    default.set_editor_property("replicates", False)
    default.set_editor_property("DisplayName", unreal.Text(display))
    compile_save(path)
    log(f"{name} : {'créé' if created else 'existe déjà (formes gardées)'}, replicates {cdo(path).get_editor_property('replicates')}")
    return assets.load_blueprint_class(path)


def make_weapons():
    brown, sand, grey, black, white, cyan, yellow, red = (mat(m) for m in ("m_PH_monochrome_Brown", "m_PH_monochrome_Sand", "m_PH_monochrome_Grey",
                                                                            "m_PH_monochrome_LightBlack", "m_PH_monochrome_white", "m_PH_monochrome_Cyan",
                                                                            "m_PH_monochrome_Yellow", "m_PH_monochrome_Red"))
    # repère de la main : +X devant le personnage, +Z vers le haut
    out = {}
    out["Mallet"] = weapon("BP_BoatCrew_Mallet", "Maillet", [
        ("Handle", CYLINDER, sand, v(0, 0, 25), r(), v(0.07, 0.07, 0.7)),
        ("HeadMallet", CYLINDER, brown, v(0, 0, 62), r(pitch=90), v(0.28, 0.28, 0.5)),
        ("HeadRingFront", CYLINDER, black, v(20, 0, 62), r(pitch=90), v(0.3, 0.3, 0.04)),
        ("HeadRingBack", CYLINDER, black, v(-20, 0, 62), r(pitch=90), v(0.3, 0.3, 0.04)),
    ])
    out["Syringe"] = weapon("BP_BoatCrew_Syringe", "Grande seringue", [
        ("Barrel", CYLINDER, white, v(30, 0, 10), r(pitch=-90), v(0.2, 0.2, 0.6)),
        ("Liquid", CYLINDER, cyan, v(36, 0, 10), r(pitch=-90), v(0.16, 0.16, 0.44)),
        ("Flange", CUBE, white, v(1, 0, 10), r(), v(0.04, 0.4, 0.08)),
        ("Plunger", CYLINDER, grey, v(-12, 0, 10), r(pitch=-90), v(0.04, 0.04, 0.3)),
        ("PlungerCap", CYLINDER, grey, v(-27, 0, 10), r(pitch=-90), v(0.16, 0.16, 0.03)),
        ("Nozzle", CYLINDER, white, v(63, 0, 10), r(pitch=-90), v(0.07, 0.07, 0.08)),
        ("Needle", CYLINDER, grey, v(80, 0, 10), r(pitch=-90), v(0.015, 0.015, 0.3)),
    ])
    out["Compass"] = weapon("BP_BoatCrew_Compass", "Compas", [
        ("Hinge", SPHERE, yellow, v(0, 0, 45), r(), v(0.09, 0.09, 0.09)),
        ("Grip", CYLINDER, yellow, v(0, 0, 54), r(), v(0.03, 0.03, 0.14)),
        ("LegLeft", CYLINDER, yellow, v(0, -7, 23), r(roll=18), v(0.03, 0.03, 0.45)),
        ("LegRight", CYLINDER, yellow, v(0, 7, 23), r(roll=-18), v(0.03, 0.03, 0.45)),
        ("TipLeft", CYLINDER, grey, v(0, -13.5, 2), r(roll=18), v(0.015, 0.015, 0.08)),
        ("TipRight", CYLINDER, grey, v(0, 13.5, 2), r(roll=-18), v(0.015, 0.015, 0.08)),
    ])
    out["FryingPan"] = weapon("BP_BoatCrew_FryingPan", "Poêle", [
        ("PanHandle", CUBE, brown, v(16, 0, 0), r(), v(0.32, 0.05, 0.04)),
        ("Pan", CYLINDER, black, v(55, 0, 0), r(), v(0.48, 0.48, 0.05)),
        ("PanRim", CYLINDER, grey, v(55, 0, 2), r(), v(0.5, 0.5, 0.03)),
        ("PanBottom", CYLINDER, black, v(55, 0, 3), r(), v(0.44, 0.44, 0.03)),
    ])
    # sniper : formes copiées de BP_Sniper (sans ses actions)
    path = f"{WEAPON_DIR}/BP_BoatCrew_Sniper"
    bp, created = make_bp(path, assets.load_blueprint_class(ARME_BP))
    if created:
        source = assets.load_asset(SNIPER_BP)
        copied = 0
        for name, comp in scs_components(source).items():
            if isinstance(comp, unreal.StaticMeshComponent) and comp.get_editor_property("static_mesh") and name != "Mesh":
                add_shape(bp, name, comp.get_editor_property("static_mesh"), comp.get_material(0), comp.get_editor_property("relative_location"),
                          comp.get_editor_property("relative_rotation"), comp.get_editor_property("relative_scale3d"))
                copied += 1
        sniper_mesh = cdo(SNIPER_BP).get_editor_property("Mesh")
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        mesh = cdo(path).get_editor_property("Mesh")
        mesh.set_editor_property("static_mesh", sniper_mesh.get_editor_property("static_mesh"))
        mesh.set_material(0, sniper_mesh.get_material(0))
        mesh.set_editor_property("relative_location", sniper_mesh.get_editor_property("relative_location"))
        mesh.set_editor_property("relative_rotation", sniper_mesh.get_editor_property("relative_rotation"))
        mesh.set_editor_property("relative_scale3d", sniper_mesh.get_editor_property("relative_scale3d"))
        log(f"BP_BoatCrew_Sniper : {copied} formes copiées de BP_Sniper (+ Mesh)")
    default = cdo(path)
    default.set_editor_property("replicates", False)
    default.set_editor_property("DisplayName", unreal.Text("Sniper"))
    compile_save(path)
    out["Sniper"] = assets.load_blueprint_class(path)
    return out


# ---------------------------------------------------------------- équipage

def first_face():
    try:
        table = assets.load_asset(FACE_TABLE)
        faces = unreal.DataTableFunctionLibrary.get_data_table_column_as_string(table, "Face")
        if faces:
            text = str(faces[0])
            path = text.split("'")[1] if "'" in text else text
            face = assets.load_asset(path.split(".")[0])
            if face:
                return face
    except Exception as e:
        log(f"visage 1 de {FACE_TABLE} : {e!r}")
    return assets.load_asset(FACE_FALLBACK)


def make_core():
    bp, created = make_bp(CORE_BP, unreal.YSBoatCrew)
    default = cdo(CORE_BP)
    if created:
        carpenter = cdo(CARPENTER_ITEM)
        for prop in ("PromptWidgetClass", "CarryLocation", "CarryRotation", "CarrierSpeedMultiplier", "FoeAttractionMultiplier", "bRevealIndicatorNearby",
                     "IndicatorColor", "bRisesToSurface", "IndicatorRevealDistance", "IndicatorHideDistance"):
            default.set_editor_property(prop, carpenter.get_editor_property(prop))
        default.set_editor_property("bShowIndicator", True)
        default.set_editor_property("IndicatorHeight", 230.0)
        default.get_editor_property("InteractPrompt").set_editor_property("relative_location", v(0, 0, 200))
        for side in ("LeftHandPoint", "RightHandPoint"):
            default.get_editor_property(side).set_editor_property("relative_location", carpenter.get_editor_property(side).get_editor_property("relative_location"))
        decal = default.get_editor_property("FaceDecal")
        decal.set_editor_property("decal_material", assets.load_asset(FACE_DECAL))
        decal.set_editor_property("FaceTexture", first_face())
        default.set_editor_property("DisplayName", unreal.Text("Membre d'équipage"))
        compile_save(CORE_BP)
    log(f"BP_BoatCrew_Core : {'créé' if created else 'existe déjà (gardé)'}, visage {cdo(CORE_BP).get_editor_property('FaceDecal').get_editor_property('FaceTexture')}")


def make_crew(weapons):
    bonus = unreal.YSBoatCrewBonus
    crew = [
        # (BP, nom, nom d'objectif, bonus, arme, couleur du corps, icône provisoire)
        ("BP_BoatCrew_Charpentier", "Charpentier", "le Charpentier", bonus.AUTO_REPAIR, "Mallet", "m_PH_monochrome_Yellow", "t_i_CDRicon"),
        ("BP_BoatCrew_Medic", "Medic", "le Medic", bonus.FREE_REVIVE, "Syringe", "m_PH_monochrome_Green", "t_i_RegenIcon"),
        ("BP_BoatCrew_Navigateur", "Navigateur", "le Navigateur", bonus.SHIP_SPEED, "Compass", "m_PH_monochrome_Blue", "t_i_FastnfuriousIcon"),
        ("BP_BoatCrew_Cuisinier", "Cuisinier", "le Cuisinier", bonus.SHIP_HEAL, "FryingPan", "m_PH_monochrome_white", "t_i_BulkUpIcon"),
        ("BP_BoatCrew_Flibustier", "Flibustier", "le Flibustier", bonus.CANNON_COOLDOWN, "Sniper", "m_PH_monochrome_LightBlack", "t_i_CriticIcon"),
    ]
    classes = []
    for name, display, objective, crew_bonus, weapon_key, body, icon in crew:
        path = f"{CREW_DIR}/{name}"
        bp, created = make_bp(path, assets.load_blueprint_class(CORE_BP))
        default = cdo(path)
        if created:
            default.set_editor_property("DisplayName", unreal.Text(display))
            default.set_editor_property("ObjectiveName", unreal.Text(objective))
            default.set_editor_property("IndicatorIcon", assets.load_asset(f"{ICONS}/{icon}"))
            default.get_editor_property("BodyMesh").set_material(0, mat(body))
        default.set_editor_property("Bonus", crew_bonus)
        default.set_editor_property("WeaponClass", weapons[weapon_key])
        compile_save(path)
        default = cdo(path)
        log(f"{name} : {'créé' if created else 'existe déjà'}, « {default.get_editor_property('ObjectiveName')} », bonus {default.get_editor_property('Bonus')}, "
            f"arme {default.get_editor_property('WeaponClass').get_name()}")
        classes.append(assets.load_blueprint_class(path))
    flow = cdo(FLOW_GM)
    flow.set_editor_property("CrewItemClasses", classes)
    flow.set_editor_property("CrewCount", 2)
    assets.save_asset(FLOW_GM, only_if_is_dirty=False)
    log(f"BP_FlowMod : Crew Item Classes {[c.get_name() for c in cdo(FLOW_GM).get_editor_property('CrewItemClasses')]}, Crew Count {cdo(FLOW_GM).get_editor_property('CrewCount')}")


try:
    rescale_brigantin()
    for ship_path in LADDERS:
        setup_ladders(ship_path)
    setup_ship_values()
    crew_weapons = make_weapons()
    make_core()
    make_crew(crew_weapons)
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_boat_crew.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
