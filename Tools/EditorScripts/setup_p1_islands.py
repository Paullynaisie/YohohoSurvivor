"""
Îles P1 du FlowMod (2026-11-04) :
  - /Game/Art/Props/BP_Prop_SmallHouse (~5 PJ de côté) et BP_Prop_BigHouse (~10 PJ) : maisons de Port Town (primitives, collision boîte)
  - /Game/Placeable/Character/NPC/BP_NPC_IslandMedic : copie de BP_NPC_Medic reparentée sur AYSIslandMedicNPC (répliquée)
  - /Game/Placeable/InteractiveObjects/BP_InteractiveObject_Medic : le Medic à ramener au bateau (comme le Charpentier)
  - /Game/Placeable/InteractiveObjects/BP_InteractiveObject_BossChest : coffre du boss = 2x le coffre de l'élite -> Skull Boss / Skull Gun Boss
  - /Game/Core/Islands/DA_I_P1_basicIsland : copie de DA_I_StartIsland, biome Tropical ou Désert au hasard, rotation au hasard,
    1 core du bateau (principal) + 1 carte au trésor (secondaire), élite de nuit Skull ou Gun au hasard
  - /Game/Core/Islands/DA_I_P1_objectifIsland : 100 m de rayon, Port Town, 3 jours ; nuit 1 = élite Skull + élite Gun, nuit 2 = Boss Skull ou Gun ;
    jour 1 : 2 cartes au trésor + core du bateau, jour 2 : Medic dans la ville
  - BP_FlowMod : Next Islands = [basic, basic, objectif] ; BP_NME_Galion : capitaine = élite Skull ou élite Gun ; FlowMap : mer de 4 km
Ne recrée rien qui existe (les deux îles comprises : les retouches de Paul sont gardées).

Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/setup_p1_islands.py"
Rapport : Saved/Logs/setup_p1_islands.txt
"""
import os
import unreal

ISLAND_DIR = "/Game/Core/Islands"
START_DA = f"{ISLAND_DIR}/DA_I_StartIsland"
BASIC_DA = f"{ISLAND_DIR}/DA_I_P1_basicIsland"
OBJECTIVE_DA = f"{ISLAND_DIR}/DA_I_P1_objectifIsland"
FLOW_GM = "/Game/Core/GameModes/BP_FlowMod"
FLOW_MAP = "/Game/Maps/FlowMap"
PROP_DIR = "/Game/Art/Props"
OBJECT_DIR = "/Game/Placeable/InteractiveObjects"
NPC_DIR = "/Game/Placeable/Character/NPC"
FOE_DIR = "/Game/Placeable/Character/Foe"
PH = "/Game/Art/Materials/PH_materials/m_PH_"
CUBE = "/Engine/BasicShapes/Cube"
CYLINDER = "/Engine/BasicShapes/Cylinder"
OCEAN_SCALE = 4000.0

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
copy_props = unreal.YSEditorScriptLibrary.copy_editable_properties
report = []


def log(msg):
    unreal.log(f"[setup_p1_islands] {msg}")
    report.append(msg)


def find_asset(name, root="/Game"):
    for path in assets.list_assets(root, recursive=True):
        package = path.split(".")[0]
        if package.rsplit("/", 1)[1] == name:
            return package
    raise RuntimeError(f"asset introuvable : {name}")


def bp_class(name, root="/Game/Placeable"):
    return assets.load_blueprint_class(find_asset(name, root))


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def material(name, fallback="Grey"):
    """m_PH_monochrome_<name> (ex. white, Red, Brown, Cyan)"""
    path = f"{PH}monochrome_{name}"
    return assets.load_asset(path) if assets.does_asset_exist(path) else assets.load_asset(f"{PH}monochrome_{fallback}")


HOUSE_COLORS = {"Walls": "white", "Roof": "Red", "Chimney": "Brown", "Door": "Brown", "Window": "Cyan"}


def paint_house(path):
    """Formes de la maison sans matériau (première version du script) : couleurs par défaut ; un matériau choisi par Paul est gardé"""
    bp = assets.load_asset(path)
    painted = 0
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        comp = lib.get_object(lib.get_data(handle))
        current = comp.get_material(0) if isinstance(comp, unreal.StaticMeshComponent) else None
        if not isinstance(comp, unreal.StaticMeshComponent) or (current is not None and not current.get_path_name().startswith("/Engine/")):
            continue
        name = comp.get_name().replace("_GEN_VARIABLE", "")
        color = next((c for prefix, c in HOUSE_COLORS.items() if name.startswith(prefix)), None)
        if color:
            comp.set_material(0, material(color))
            painted += 1
    if painted:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"{path} : {painted} forme(s) peinte(s)")


def add_component(bp, name, component_class, parent=None):
    root = parent or subsystem.k2_gather_subobject_data_for_blueprint(bp)[0]
    params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=component_class, blueprint_context=bp)
    handle, fail = subsystem.add_new_subobject(params)
    if not lib.is_handle_valid(handle):
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    subsystem.rename_subobject(handle, unreal.Text(name))
    return handle, lib.get_object(lib.get_data(handle))


def add_shape(bp, name, mesh, mat, location, scale, rotation=(0.0, 0.0, 0.0), parent=None):
    """rotation = (pitch, yaw, roll) ; visuel seulement"""
    _, comp = add_component(bp, name, unreal.StaticMeshComponent, parent)
    comp.set_editor_property("static_mesh", mesh if not isinstance(mesh, str) else assets.load_asset(mesh))
    comp.set_material(0, mat)
    comp.set_editor_property("relative_location", unreal.Vector(*location))
    comp.set_editor_property("relative_rotation", unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]))
    comp.set_editor_property("relative_scale3d", unreal.Vector(*scale))
    comp.set_collision_profile_name("NoCollision")
    comp.set_editor_property("generate_overlap_events", False)
    return comp


def create_bp(path, parent_class):
    if assets.does_asset_exist(path):
        log(f"existe déjà : {path}")
        return None
    directory, name = path.rsplit("/", 1)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    return tools.create_asset(name, directory, unreal.Blueprint, factory)


def finish_bp(bp, path):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(path, only_if_is_dirty=False)
    log(f"créé : {path}")


def make_house(name, side, height):
    """Maison en primitives : murs, toit en gradins, porte, fenêtres ; collision boîte (comme la capsule des autres props)"""
    path = f"{PROP_DIR}/{name}"
    bp = create_bp(path, assets.load_blueprint_class(f"{PROP_DIR}/BP_Prop"))
    if not bp:
        return path
    half = side / 2.0
    walls, roof, wood, glass = material(HOUSE_COLORS["Walls"]), material(HOUSE_COLORS["Roof"]), material(HOUSE_COLORS["Door"]), material(HOUSE_COLORS["Window"])
    add_shape(bp, "Walls", CUBE, walls, (0, 0, height / 2.0), (side / 100.0, side / 100.0, height / 100.0))
    # toit en gradins qui déborde un peu
    for step in range(4):
        width = side + 40.0 - step * side * 0.22
        add_shape(bp, f"Roof{step + 1}", CUBE, roof, (0, 0, height + 15.0 + step * 32.0), (width / 100.0, width / 100.0, 0.32))
    add_shape(bp, "Chimney", CUBE, wood, (half * 0.45, half * 0.4, height + 120.0), (0.4, 0.4, 1.2))
    # porte et fenêtres sur la façade (+X), fenêtres sur les côtés
    add_shape(bp, "Door", CUBE, wood, (half + 2.0, 0, 100.0), (0.08, 1.0 if side < 600 else 1.6, 2.0))
    for index, y in enumerate((-half * 0.6, half * 0.6)):
        add_shape(bp, f"WindowFront{index + 1}", CUBE, glass, (half + 2.0, y, height * 0.6), (0.06, 0.8, 0.7))
        add_shape(bp, f"WindowSide{index + 1}", CUBE, glass, (0, (half + 2.0) * (1 if index else -1), height * 0.6), (1.0, 0.06, 0.7))
    _, box = add_component(bp, "Collision", unreal.BoxComponent)
    box.set_editor_property("box_extent", unreal.Vector(half, half, height / 2.0))
    box.set_editor_property("relative_location", unreal.Vector(0, 0, height / 2.0))
    box.set_collision_profile_name("BlockAllDynamic")
    box.set_editor_property("can_character_step_up_on", unreal.CanBeCharacterBase.ECB_NO)
    box.set_editor_property("generate_overlap_events", False)
    finish_bp(bp, path)
    return path


def make_medic():
    npc_path = f"{NPC_DIR}/BP_NPC_IslandMedic"
    if not assets.does_asset_exist(npc_path):
        assets.duplicate_asset(find_asset("BP_NPC_Medic", NPC_DIR), npc_path)
        bp = assets.load_asset(npc_path)
        unreal.BlueprintEditorLibrary.reparent_blueprint(bp, unreal.YSIslandMedicNPC)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assets.save_asset(npc_path, only_if_is_dirty=False)
        log(f"créé : {npc_path} (copie de BP_NPC_Medic reparentée sur AYSIslandMedicNPC)")
    else:
        log(f"existe déjà : {npc_path}")

    item_path = f"{OBJECT_DIR}/BP_InteractiveObject_Medic"
    bp = create_bp(item_path, unreal.YSCarpenter)
    if bp:
        # visuel du Medic (copie des formes), porté couché comme le Charpentier
        source = assets.load_asset(npc_path)
        used = set()
        for handle in subsystem.k2_gather_subobject_data_for_blueprint(source):
            comp = lib.get_object(lib.get_data(handle))
            if not isinstance(comp, unreal.StaticMeshComponent) or not comp.get_editor_property("static_mesh"):
                continue
            name = comp.get_name().replace("_GEN_VARIABLE", "")
            if name in used:
                continue
            used.add(name)
            loc = comp.get_editor_property("relative_location")
            rot = comp.get_editor_property("relative_rotation")
            scale = comp.get_editor_property("relative_scale3d")
            add_shape(bp, name, comp.get_editor_property("static_mesh"), comp.get_material(0), (loc.x, loc.y, loc.z), (scale.x, scale.y, scale.z), (rot.pitch, rot.yaw, rot.roll))
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        item = cdo(item_path)
        copy_props(cdo(f"{OBJECT_DIR}/BP_InteractiveObject_Carpenter"), item)
        item.set_editor_property("display_name", unreal.Text("Medic"))
        item.set_editor_property("delivered_npc_class", assets.load_blueprint_class(npc_path))
        finish_bp(bp, item_path)
    return item_path


def make_boss_chest():
    path = f"{OBJECT_DIR}/BP_InteractiveObject_BossChest"
    chest_path = find_asset("BP_InteractiveObject_Chest", OBJECT_DIR)
    bp = create_bp(path, assets.load_blueprint_class(chest_path))
    if bp:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        chest, boss_chest = cdo(chest_path), cdo(path)
        boss_chest.set_editor_property("display_name", unreal.Text("Coffre du boss"))
        boss_chest.set_editor_property("doubloon_value", chest.get_editor_property("doubloon_value") * 2)
        boss_chest.set_editor_property("phoenix_feather_value", chest.get_editor_property("phoenix_feather_value") * 2)
        finish_bp(bp, path)
    for name in ("FoeCharacter_Skull_Boss", "FoeCharacter_Skull_Gun_Boss"):
        foe_path = find_asset(name, FOE_DIR)
        cdo(foe_path).set_editor_property("death_loot_class", assets.load_blueprint_class(path))
        assets.save_asset(foe_path, only_if_is_dirty=False)
    return path


def prop_def(path, radius, collision, weight, scale_min, scale_max, sink):
    prop = unreal.YSIslandPropDef()
    prop.set_editor_property("Class", assets.load_blueprint_class(path))
    prop.set_editor_property("radius", radius)
    prop.set_editor_property("collision_radius", collision)
    prop.set_editor_property("weight", weight)
    prop.set_editor_property("scale_min", scale_min)
    prop.set_editor_property("scale_max", scale_max)
    prop.set_editor_property("sink", sink)
    return prop


def objective(name, item, delivery=None, day=1, town=False, distance=(3000.0, 9000.0)):
    state = unreal.YSIslandObjectiveDef()
    state.set_editor_property("display_name", unreal.Text(name))
    state.set_editor_property("item_class", assets.load_blueprint_class(item))
    if delivery:
        state.set_editor_property("delivery_class", assets.load_blueprint_class(delivery))
    state.set_editor_property("distance_from_boat", unreal.Vector2D(*distance))
    state.set_editor_property("SpawnDay", day)
    state.set_editor_property("bInTown", town)
    return state


def center_event(entries, day=0):
    """entries : [(classes, nombre)] ; plusieurs classes = au hasard"""
    event = unreal.YSIslandSpawnEvent()
    event.set_editor_property("time", 0.0)
    event.set_editor_property("repeat_count", 1)
    event.set_editor_property("shape", unreal.YSIslandSpawnShape.CENTER)
    event.set_editor_property("scale_with_players", False)
    event.set_editor_property("IslandDay", day)
    foes = []
    for classes, count in entries:
        entry = unreal.YSIslandFoeEntry()
        if len(classes) == 1:
            entry.set_editor_property("foe_class", bp_class(classes[0], FOE_DIR))
        else:
            entry.set_editor_property("RandomFoeClasses", [bp_class(name, FOE_DIR) for name in classes])
        entry.set_editor_property("count", count)
        foes.append(entry)
    event.set_editor_property("foes", foes)
    return event


def night_events(start, centers):
    """Nuits de l'île de départ, l'élite (événement au centre) remplacé par centers"""
    events = [e for e in start.get_editor_property("night_events") if e.get_editor_property("shape") != unreal.YSIslandSpawnShape.CENTER]
    return centers + events


def is_unfinished_copy(path, finished):
    """Copie de l'île de départ restée à mi-chemin (script interrompu) : refaite"""
    if assets.does_asset_exist(path) and not finished(assets.load_asset(path)):
        assets.delete_asset(path)
        log(f"copie inachevée supprimée : {path}")
    return assets.does_asset_exist(path)


def make_basic(map_item, chest_item, core_item):
    if is_unfinished_copy(BASIC_DA, lambda d: len(d.get_editor_property("RandomGenerationTypes")) > 0):
        log(f"existe déjà : {BASIC_DA}")
        return
    assets.duplicate_asset(START_DA, BASIC_DA)
    data = assets.load_asset(BASIC_DA)
    start = assets.load_asset(START_DA)
    data.set_editor_property("island_name", unreal.Text("Île basique"))
    data.set_editor_property("RandomGenerationTypes", [unreal.YSIslandGeneration.TROPICAL, unreal.YSIslandGeneration.DESERT])
    data.set_editor_property("bRandomRotation", True)
    data.set_editor_property("main_objectives", [objective("Ramener le core du bateau", core_item)])
    data.set_editor_property("secondary_objectives", [objective("Carte au trésor : ramener le coffre au bateau", map_item, chest_item)])
    data.set_editor_property("night_events", night_events(start, [center_event([(("FoeCharacter_Skull_Elite", "FoeCharacter_Skull_Gun_Elite"), 1)])]))
    assets.save_asset(BASIC_DA, only_if_is_dirty=False)
    log(f"créé : {BASIC_DA}")


def make_objective(map_item, chest_item, core_item, medic_item, small_house, big_house):
    if is_unfinished_copy(OBJECTIVE_DA, lambda d: d.get_editor_property("generation_type") == unreal.YSIslandGeneration.PORT_TOWN):
        log(f"existe déjà : {OBJECTIVE_DA}")
        return
    assets.duplicate_asset(START_DA, OBJECTIVE_DA)
    data = assets.load_asset(OBJECTIVE_DA)
    start = assets.load_asset(START_DA)
    data.set_editor_property("island_name", unreal.Text("Île objectif"))
    data.set_editor_property("generation_type", unreal.YSIslandGeneration.PORT_TOWN)
    data.set_editor_property("bRandomRotation", True)

    rules = data.get_editor_property("generation")
    # 2x le rayon de l'île de départ, côte 2x plus découpée ; plus d'ouvertures vers l'eau (pontons de la ville)
    rules.set_editor_property("radius", 10000.0)
    noise = rules.get_editor_property("coast_noise")
    rules.set_editor_property("coast_noise", unreal.Vector(noise.x * 2.0, noise.y * 2.0, noise.z * 2.0))
    rules.set_editor_property("water_access_count", unreal.IntPoint(5, 6))
    rules.set_editor_property("massif_count_min", 6)
    rules.set_editor_property("massif_layouts", 12)
    rules.set_editor_property("SmallHouseProp", prop_def(small_house, 300.0, 210.0, 3.0, 0.95, 1.05, 5.0))
    rules.set_editor_property("BigHouseProp", prop_def(big_house, 600.0, 420.0, 1.0, 0.95, 1.05, 5.0))
    rules.set_editor_property("PierMaterial", material("Brown"))
    data.set_editor_property("generation", rules)

    data.set_editor_property("main_objectives", [objective("Ramener le core du bateau", core_item),
                                                 objective("Ramener le Medic de la ville au bateau", medic_item, day=2, town=True, distance=(0.0, 1000000.0))])
    treasure = objective("Carte au trésor : ramener le coffre au bateau", map_item, chest_item, distance=(3000.0, 15000.0))
    data.set_editor_property("secondary_objectives", [treasure, treasure])
    data.set_editor_property("max_days", start.get_editor_property("max_days") + 1)
    data.set_editor_property("night_events", night_events(start, [
        center_event([(("FoeCharacter_Skull_Elite",), 1), (("FoeCharacter_Skull_Gun_Elite",), 1)], day=1),
        center_event([(("FoeCharacter_Skull_Boss", "FoeCharacter_Skull_Gun_Boss"), 1)], day=2),
    ]))
    assets.save_asset(OBJECTIVE_DA, only_if_is_dirty=False)
    log(f"créé : {OBJECTIVE_DA}")


def setup_flow_and_galion():
    gm = cdo(FLOW_GM)
    basic, goal = assets.load_asset(BASIC_DA), assets.load_asset(OBJECTIVE_DA)
    gm.set_editor_property("NextIslands", [basic, basic, goal])
    assets.save_asset(FLOW_GM, only_if_is_dirty=False)
    galion_path = find_asset("BP_NME_Galion")
    cdo(galion_path).set_editor_property("CaptainClasses", [bp_class("FoeCharacter_Skull_Elite", FOE_DIR), bp_class("FoeCharacter_Skull_Gun_Elite", FOE_DIR)])
    assets.save_asset(galion_path, only_if_is_dirty=False)


def grow_ocean():
    if not levels.load_level(FLOW_MAP):
        raise RuntimeError(f"impossible d'ouvrir {FLOW_MAP}")
    for actor in actors_sub.get_all_level_actors():
        if isinstance(actor, unreal.YSOcean):
            scale = actor.get_actor_scale3d()
            if scale.x < OCEAN_SCALE:
                actor.set_actor_scale3d(unreal.Vector(OCEAN_SCALE, OCEAN_SCALE, scale.z))
                log(f"FlowMap : mer {scale.x:.0f} -> {OCEAN_SCALE:.0f} (côté {OCEAN_SCALE / 1000.0:.0f} km)")
    if not levels.save_current_level():
        raise RuntimeError("sauvegarde de FlowMap impossible")


def fix_pier_material():
    """Pontons sans matériau (première version du script) : bois"""
    data = assets.load_asset(OBJECTIVE_DA)
    rules = data.get_editor_property("generation")
    if rules.get_editor_property("PierMaterial") is None:
        rules.set_editor_property("PierMaterial", material("Brown"))
        data.set_editor_property("generation", rules)
        assets.save_asset(OBJECTIVE_DA, only_if_is_dirty=False)
        log(f"{OBJECTIVE_DA} : pontons en {rules.get_editor_property('PierMaterial').get_name()}")


try:
    small = make_house("BP_Prop_SmallHouse", 420.0, 300.0)
    big = make_house("BP_Prop_BigHouse", 840.0, 450.0)
    paint_house(small)
    paint_house(big)
    medic = make_medic()
    boss_chest = make_boss_chest()
    map_item = find_asset("BP_InteractiveObject_TreasureMap", OBJECT_DIR)
    chest_item = find_asset("BP_InteractiveObject_TreasureChest", OBJECT_DIR)
    core_item = f"{OBJECT_DIR}/BP_InteractiveObject_BoatCore"
    make_basic(map_item, chest_item, core_item)
    make_objective(map_item, chest_item, core_item, medic, small, big)
    fix_pier_material()
    setup_flow_and_galion()
    grow_ocean()

    # relecture
    for path in (BASIC_DA, OBJECTIVE_DA):
        data = assets.load_asset(path)
        rules = data.get_editor_property("generation")
        log(f"relu {path.rsplit('/', 1)[1]} : biome {data.get_editor_property('generation_type')} / {list(data.get_editor_property('RandomGenerationTypes'))}, "
            f"rotation {data.get_editor_property('bRandomRotation')}, rayon {rules.get_editor_property('radius')}, {data.get_editor_property('max_days')} jours, "
            f"objectifs {[(o.get_editor_property('display_name'), o.get_editor_property('SpawnDay'), o.get_editor_property('bInTown')) for o in data.get_editor_property('main_objectives')]} + "
            f"{len(data.get_editor_property('secondary_objectives'))} secondaire(s)")
        for ev in data.get_editor_property("night_events"):
            foes = [(f.get_editor_property("foe_class").get_name() if f.get_editor_property("foe_class") else [c.get_name() for c in f.get_editor_property("RandomFoeClasses")], f.get_editor_property("count"))
                    for f in ev.get_editor_property("foes")]
            log(f"   nuit : jour {ev.get_editor_property('IslandDay')} {ev.get_editor_property('shape')} {foes}")
    log(f"relu BP_FlowMod : îles suivantes {[d.get_name() for d in cdo(FLOW_GM).get_editor_property('NextIslands')]}")
    log(f"relu Galion : capitaines {[c.get_name() for c in cdo(find_asset('BP_NME_Galion')).get_editor_property('CaptainClasses')]}")
    log(f"relu coffre du boss : {cdo(boss_chest).get_editor_property('doubloon_value')} doublons, {cdo(boss_chest).get_editor_property('phoenix_feather_value')} plume(s) ; "
        f"Skull Boss -> {cdo(find_asset('FoeCharacter_Skull_Boss', FOE_DIR)).get_editor_property('death_loot_class').get_name()}")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_p1_islands.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
