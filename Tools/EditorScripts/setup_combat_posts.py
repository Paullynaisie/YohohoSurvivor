"""
Coffres, objectifs secondaires et postes de combat (2026-11-09), relançable (ne recrée rien qui existe ; les valeurs listées sont réimposées) :
  - coffres : BP_InteractiveObject_Chest = « Basic Chest » (35 doublons), BP_InteractiveObject_TreasureChest = « Elite Chest » (50 doublons, 1 plume),
    BP_InteractiveObject_BossChest = « Boss Chest » (75 doublons, 2 plumes, bandes rouges émissives ajoutées)
  - butin : élites (Foe/Elites, capitaine de Galion) -> Elite Chest, boss (Foe/Boss) -> Boss Chest
  - carte au trésor : 1 Elite Chest + 2 Basic Chest + 1 poste de combat au hasard
  - postes de combat : /Game/Placeable/InteractiveObjects/CombatPosts/BP_CombatPostItem_Cannon / _Gatling (objets ramassables),
    /Game/Placeable/Ship/Stations/BP_ShipGatling (enfant de BP_ShipCannon : 10 balles en 1 s, 100 dégâts) + /Game/Placeable/AMMO/GatlingAMMO
    (enfant de CanonAMMO : 2x plus vite, 2x moins longtemps ; 2x moins large via Projectile Scale), BP_CombatPostReceptacle
  - GameBoat : réceptacles à la place des canons (sloop 4, brigantin 5, galion 6), plus de Slooplus (supprimé)
  - GameModes : Combat Post Item Classes = tous les postes de combat
  - DA_I_StartIsland_Map1 : objectifs secondaires du jour 1 = 1 carte au trésor, 2 Basic Chest, 1 poste de combat au hasard

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="<chemin absolu>/Tools/EditorScripts/setup_combat_posts.py"
Rapport : Saved/Logs/setup_combat_posts.txt
"""
import os
import unreal

OBJ_DIR = "/Game/Placeable/InteractiveObjects"
POST_DIR = f"{OBJ_DIR}/CombatPosts"
STATION_DIR = "/Game/Placeable/Ship/Stations"
SHIP_DIR = "/Game/Placeable/Ship"
AMMO_DIR = "/Game/Placeable/AMMO"
MAT = "/Game/Art/Materials/PH_materials"
CUBE = "/Engine/BasicShapes/Cube"
CYL = "/Engine/BasicShapes/Cylinder"
SPHERE = "/Engine/BasicShapes/Sphere"

CHEST = f"{OBJ_DIR}/BP_InteractiveObject_Chest"
ELITE_CHEST = f"{OBJ_DIR}/BP_InteractiveObject_TreasureChest"
BOSS_CHEST = f"{OBJ_DIR}/BP_InteractiveObject_BossChest"
TREASURE_MAP = f"{OBJ_DIR}/BP_InteractiveObject_TreasureMap"
BASE_OBJECT = f"{OBJ_DIR}/BP_InteractiveObject"
CANNON_ITEM = f"{POST_DIR}/BP_CombatPostItem_Cannon"
GATLING_ITEM = f"{POST_DIR}/BP_CombatPostItem_Gatling"
CANNON = f"{STATION_DIR}/BP_ShipCannon"
GATLING = f"{STATION_DIR}/BP_ShipGatling"
RECEPTACLE = f"{STATION_DIR}/BP_CombatPostReceptacle"
CANON_AMMO = f"{AMMO_DIR}/CanonAMMO"
GATLING_AMMO = f"{AMMO_DIR}/GatlingAMMO"
SLOOP = f"{SHIP_DIR}/BP_GameBoat_Sloop"
SLOOPLUS = f"{SHIP_DIR}/BP_GameBoat_Slooplus"
BRIGANTIN = f"{SHIP_DIR}/BP_GameBoat_Brigantin"
GALION = f"{SHIP_DIR}/BP_GameBoat_Galion"
GAMEBOAT_DA = "/Game/Core/GameBoat/DA_GameBoat"
MAP1_DA = "/Game/Core/Islands/DA_I_StartIsland_Map1"
GAME_MODES = ["/Game/Core/GameModes/BP_FlowMod", "/Game/Core/GameModes/BP_TestGameMode", "/Game/Core/GameModes/BP_TestMapGameMode"]
FOE_ROOT = "/Game/Placeable/Character/Foe"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    report.append(str(msg))


def v(x, y, z):
    return unreal.Vector(x, y, z)


def r(pitch=0.0, yaw=0.0, roll=0.0):
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def handles(bp):
    return subsystem.k2_gather_subobject_data_for_blueprint(bp)


def find_handle(bp, name):
    for h in handles(bp):
        if str(lib.get_variable_name(lib.get_data(h))) == name:
            return h
    return None


def has_component(bp, name):
    return find_handle(bp, name) is not None


def shape(bp, name, mesh, material, location, rotation, scale, parent=None):
    """Primitive sans collision (ne recrée pas un composant qui existe)"""
    if has_component(bp, name):
        return
    parent_handle = find_handle(bp, parent) if parent else handles(bp)[0]
    handle, fail = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent_handle, new_class=unreal.StaticMeshComponent, blueprint_context=bp))
    if not fail.is_empty():
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    subsystem.rename_subobject(handle, unreal.Text(name))
    comp = lib.get_object(lib.get_data(handle))
    comp.set_editor_property("static_mesh", assets.load_asset(mesh))
    comp.set_material(0, assets.load_asset(f"{MAT}/{material}"))
    comp.set_editor_property("relative_location", location)
    comp.set_editor_property("relative_rotation", rotation)
    comp.set_editor_property("relative_scale3d", scale)
    comp.set_collision_profile_name("NoCollision")


def create_bp(path, parent_class):
    if assets.does_asset_exist(path):
        return assets.load_asset(path), False
    folder, name = path.rsplit("/", 1)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    bp = tools.create_asset(name, folder, unreal.Blueprint, factory)
    log(f"créé : {path}")
    return bp, True


def save(path):
    bp = assets.load_asset(path)
    if isinstance(bp, unreal.Blueprint):
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(path, only_if_is_dirty=False)


def set_chest(path, name, doubloons, feathers):
    c = cdo(path)
    c.set_editor_property("display_name", name)
    c.set_editor_property("doubloon_value", doubloons)
    c.set_editor_property("phoenix_feather_value", feathers)
    save(path)
    c = cdo(path)
    log(f"relu : {path.rsplit('/', 1)[1]} « {c.get_editor_property('display_name')} » {c.get_editor_property('doubloon_value')} doublons, {c.get_editor_property('phoenix_feather_value')} plume(s)")


def place_receptacle(ship, name, location, yaw, receptacle_class):
    comp = ship.get_editor_property(name)
    if comp is None:
        raise RuntimeError(f"composant {name} absent")
    comp.set_child_actor_class(receptacle_class)
    if location is not None:
        comp.set_editor_property("relative_location", location)
        comp.set_editor_property("relative_rotation", r(yaw=yaw))
    loc = comp.get_editor_property("relative_location")
    log(f"  {name} : réceptacle en ({loc.x:.0f}, {loc.y:.0f}, {loc.z:.0f}), yaw {comp.get_editor_property('relative_rotation').yaw:.0f}")


try:
    prompt = cdo(BASE_OBJECT).get_editor_property("prompt_widget_class")

    # 1) coffres
    set_chest(CHEST, "Basic Chest", 35, 0)
    set_chest(ELITE_CHEST, "Elite Chest", 50, 1)
    boss_bp = assets.load_asset(BOSS_CHEST)
    # bandes rouges émissives (comme les bandes dorées de l'Elite Chest), sur le couvercle et la serrure
    shape(boss_bp, "RedBandFront", CUBE, "m_PH_emissive_turboRed", v(0, -22, 26), r(), v(0.67, 0.072, 0.6))
    shape(boss_bp, "RedBandBack", CUBE, "m_PH_emissive_turboRed", v(0, 22, 26), r(), v(0.67, 0.072, 0.6))
    shape(boss_bp, "RedLidBand", CYL, "m_PH_emissive_turboRed", v(0, 0, 26.3), r(roll=90.0), v(0.56, 0.56, 0.12))
    shape(boss_bp, "RedLock", CUBE, "m_PH_emissive_turboRed", v(28, 0, 20.3), r(), v(0.07, 0.14, 0.14))
    save(BOSS_CHEST)
    set_chest(BOSS_CHEST, "Boss Chest", 75, 2)

    elite_class = assets.load_blueprint_class(ELITE_CHEST)
    boss_class = assets.load_blueprint_class(BOSS_CHEST)
    chest_class = assets.load_blueprint_class(CHEST)
    for path in assets.list_assets(FOE_ROOT, recursive=True):
        package = path.split(".")[0]
        if assets.find_asset_data(package).asset_class_path.asset_name != "Blueprint":
            continue
        foe = cdo(package)
        if not isinstance(foe, unreal.YSFoeCharacter) or not foe.get_editor_property("is_elite"):
            continue
        loot = boss_class if "/Boss/" in package else elite_class
        foe.set_editor_property("death_loot_class", loot)
        save(package)
        log(f"relu : {package.rsplit('/', 1)[1]} butin {cdo(package).get_editor_property('death_loot_class').get_name()}")

    # 2) postes de combat : AMMO et Gatling
    if not assets.does_asset_exist(GATLING_AMMO):
        create_bp(GATLING_AMMO, assets.load_blueprint_class(CANON_AMMO))
        save(GATLING_AMMO)
        canon_ammo = cdo(CANON_AMMO)
        ammo = cdo(GATLING_AMMO)
        ammo.set_editor_property("speed", canon_ammo.get_editor_property("speed") * 2.0)
        ammo.set_editor_property("life_time", canon_ammo.get_editor_property("life_time") * 0.5)
        save(GATLING_AMMO)
    ammo = cdo(GATLING_AMMO)
    log(f"relu : GatlingAMMO vitesse {ammo.get_editor_property('speed')}, durée de vie {ammo.get_editor_property('life_time')}")

    gatling_bp, created = create_bp(GATLING, assets.load_blueprint_class(CANNON))
    if created:
        save(GATLING)
        gatling = cdo(GATLING)
        cannon = cdo(CANNON)
        gatling.set_editor_property("display_name", "Gatling")
        gatling.set_editor_property("projectile_class", assets.load_blueprint_class(GATLING_AMMO))
        gatling.set_editor_property("projectile_scale", cannon.get_editor_property("projectile_scale") * 0.5)
        gatling.set_editor_property("damage", 100.0)
        gatling.set_editor_property("ammo_per_shot", 1)
        gatling.set_editor_property("spread", 0.0)
        gatling.set_editor_property("shots_per_burst", 10)
        gatling.set_editor_property("burst_interval", 0.1)
        # fût central fin + 3 canons autour (primitives)
        gatling.get_editor_property("CannonBarrel").set_editor_property("relative_scale3d", v(0.18, 0.18, 1.7))
        for index, (y, z) in enumerate([(0, 12), (-11, -6), (11, -6)]):
            shape(gatling_bp, f"GatlingBarrel{index + 1}", CYL, "m_PH_monochrome_Black", v(45, y, z), r(pitch=90.0), v(0.12, 0.12, 1.6), "CannonPivot")
        shape(gatling_bp, "GatlingHub", CYL, "m_PH_monochrome_Grey", v(-10, 0, 0), r(pitch=90.0), v(0.5, 0.5, 0.4), "CannonPivot")
        save(GATLING)
    g = cdo(GATLING)
    log(f"relu : BP_ShipGatling {g.get_editor_property('shots_per_burst')} x {g.get_editor_property('burst_interval')} s, {g.get_editor_property('damage')} dégâts, "
        f"échelle {g.get_editor_property('projectile_scale')}, recharge {g.get_editor_property('cooldown')} s, angle {g.get_editor_property('max_aim_angle')}°")

    # 3) objets ramassables
    for path, display, station, gatling_look in [(CANNON_ITEM, "Poste de combat Canon", CANNON, False), (GATLING_ITEM, "Poste de combat Gatling", GATLING, True)]:
        bp, created = create_bp(path, unreal.YSCombatPostItem)
        if created:
            shape(bp, "Crate", CUBE, "m_PH_monochrome_Brown", v(0, 0, 20), r(), v(0.6, 0.6, 0.4))
            if gatling_look:
                for index, (y, z) in enumerate([(0, 52), (-9, 45), (9, 45)]):
                    shape(bp, f"Barrel{index + 1}", CYL, "m_PH_monochrome_Black", v(5, y, z), r(pitch=90.0), v(0.1, 0.1, 0.7))
                shape(bp, "Hub", CYL, "m_PH_monochrome_Grey", v(-25, 0, 48), r(pitch=90.0), v(0.35, 0.35, 0.2))
            else:
                shape(bp, "Barrel", CYL, "m_PH_monochrome_Black", v(5, 0, 50), r(pitch=90.0), v(0.25, 0.25, 0.8))
            save(path)
        item = cdo(path)
        item.set_editor_property("display_name", display)
        item.set_editor_property("prompt_widget_class", prompt)
        item.set_editor_property("post_class", assets.load_blueprint_class(station))
        item.set_editor_property("show_indicator", True)
        save(path)
        post = cdo(station)
        post.set_editor_property("combat_post_item_class", assets.load_blueprint_class(path))
        save(station)
        log(f"relu : {path.rsplit('/', 1)[1]} -> {cdo(path).get_editor_property('post_class').get_name()} -> objet {cdo(station).get_editor_property('combat_post_item_class').get_name()}")
    items = [assets.load_blueprint_class(CANNON_ITEM), assets.load_blueprint_class(GATLING_ITEM)]

    # 4) réceptacle
    create_bp(RECEPTACLE, unreal.YSCombatPostReceptacle)
    save(RECEPTACLE)
    receptacle = cdo(RECEPTACLE)
    receptacle.set_editor_property("prompt_widget_class", prompt)
    save(RECEPTACLE)
    receptacle_class = assets.load_blueprint_class(RECEPTACLE)

    # 5) GameBoat : réceptacles (sloop 4, brigantin 5, galion 6), plus de Slooplus
    log("BP_GameBoat_Sloop :")
    sloop = cdo(SLOOP)
    place_receptacle(sloop, "CannonLeft", None, 0, receptacle_class)
    place_receptacle(sloop, "CannonRight", None, 0, receptacle_class)
    place_receptacle(sloop, "CannonLeft2", v(360, -373.2, 210), -90.0, receptacle_class)
    place_receptacle(sloop, "CannonRight2", v(360, 373.2, 210), 90.0, receptacle_class)
    save(SLOOP)
    log("BP_GameBoat_Brigantin :")
    brigantin = cdo(BRIGANTIN)
    place_receptacle(brigantin, "CannonFront", v(1730, 0, 210), 0.0, receptacle_class)  # brigantin x0.55 depuis le 2026-11-10
    place_receptacle(brigantin, "CannonLeft", v(495, -410, 210), -90.0, receptacle_class)
    place_receptacle(brigantin, "CannonRight", v(495, 410, 210), 90.0, receptacle_class)
    place_receptacle(brigantin, "CannonLeft2", v(-385, -410, 210), -90.0, receptacle_class)
    place_receptacle(brigantin, "CannonRight2", v(-385, 410, 210), 90.0, receptacle_class)
    save(BRIGANTIN)
    log("BP_GameBoat_Galion :")
    galion = cdo(GALION)
    place_receptacle(galion, "CannonFront", v(2124, -245, 210), -15.0, receptacle_class)  # galion x0.7 depuis le 2026-11-10
    place_receptacle(galion, "CannonFront2", v(2124, 245, 210), 15.0, receptacle_class)
    for name in ["CannonLeft", "CannonRight", "CannonLeft2", "CannonRight2"]:
        place_receptacle(galion, name, None, 0, receptacle_class)
    save(GALION)

    data = assets.load_asset(GAMEBOAT_DA)
    stages = [s for s in data.get_editor_property("Stages") if s and s.get_name() != "BP_GameBoat_Slooplus_C"]
    data.set_editor_property("Stages", stages)
    assets.save_loaded_asset(data, only_if_is_dirty=False)
    log(f"relu : DA_GameBoat {[s.get_name() for s in assets.load_asset(GAMEBOAT_DA).get_editor_property('Stages')]}")
    if assets.does_asset_exist(SLOOPLUS):
        log(f"suppression de {SLOOPLUS} : {assets.delete_asset(SLOOPLUS)}")

    # 6) gamemodes, carte au trésor
    for path in GAME_MODES:
        if assets.does_asset_exist(path):
            gm = cdo(path)
            gm.set_editor_property("combat_post_item_classes", items)
            save(path)
            log(f"relu : {path.rsplit('/', 1)[1]} postes de combat {[c.get_name() for c in cdo(path).get_editor_property('combat_post_item_classes')]}")
    treasure = cdo(TREASURE_MAP)
    treasure.set_editor_property("reward_classes", [elite_class, chest_class, chest_class])
    treasure.set_editor_property("reward_random_combat_post", True)
    save(TREASURE_MAP)
    t = cdo(TREASURE_MAP)
    log(f"relu : carte au trésor {[c.get_name() for c in t.get_editor_property('reward_classes')]} + poste au hasard {t.get_editor_property('reward_random_combat_post')}")

    # 7) objectifs secondaires de la Map 1 (jour 1)
    def objective(name, item, delivery, distance, random_post=False):
        o = unreal.YSIslandObjectiveDef()
        o.set_editor_property("display_name", name)
        o.set_editor_property("item_class", item)
        o.set_editor_property("delivery_class", delivery)
        o.set_editor_property("distance_from_boat", unreal.Vector2D(distance[0], distance[1]))
        o.set_editor_property("spawn_day", 1)
        o.set_editor_property("random_combat_post", random_post)
        return o
    map1 = assets.load_asset(MAP1_DA)
    map1.set_editor_property("secondary_objectives", [
        objective("Carte au trésor", assets.load_blueprint_class(TREASURE_MAP), elite_class, (3000.0, 9000.0)),
        objective("Basic Chest", chest_class, None, (2000.0, 9000.0)),
        objective("Basic Chest", chest_class, None, (2000.0, 9000.0)),
        objective("Poste de combat", None, None, (2000.0, 8000.0), True),
    ])
    assets.save_loaded_asset(map1, only_if_is_dirty=False)
    for o in assets.load_asset(MAP1_DA).get_editor_property("secondary_objectives"):
        item = o.get_editor_property("item_class")
        log(f"relu : objectif secondaire « {o.get_editor_property('display_name')} » {item.get_name() if item else '-'} poste au hasard {o.get_editor_property('random_combat_post')}")
    log("OK")
except Exception as error:
    import traceback
    log(f"ERREUR : {error}\n{traceback.format_exc()}")

with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_combat_posts.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
