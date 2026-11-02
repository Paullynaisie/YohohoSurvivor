"""
Flow de partie : île de départ générée au lancement de FlowMap.
  - /Game/Core/Islands/DA_I_StartIsland : TOUT ce qui définit l'île (génération Tropical rayon 50 m, objectifs, temps, cycles de NME, coulaison).
    Copier cet asset pour créer une autre île. Jamais écrasé s'il existe.
  - /Game/Placeable/Ship/BP_StartBoat : chaloupe échouée (copie visuelle de BP_Longboat), zone de dépôt des objectifs
  - /Game/Placeable/InteractiveObjects/BP_InteractiveObject_Carpenter (porté comme un cadavre), BP_InteractiveObject_BoatCore
  - /Game/Placeable/Character/NPC/BP_NPC_Carpenter : le Charpentier une fois au bateau (Brooker)
  - BP_FlowMod : reparenté sur AYSFlowMode, Start Island = DA_I_StartIsland
  - FlowMap : le Landscape et les props figés sont retirés (l'île est générée en jeu) ; mer, deadzone, lumières gardées
Relançable : ne recrée rien qui existe.

Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/setup_start_island.py"
Rapport : Saved/Logs/setup_start_island.txt
"""
import os
import unreal

ISLAND_DIR = "/Game/Core/Islands"
ISLAND_DA = f"{ISLAND_DIR}/DA_I_StartIsland"
FLOW_GM = "/Game/Core/GameModes/BP_FlowMod"
FLOW_MAP = "/Game/Maps/FlowMap"
SHIP_DIR = "/Game/Placeable/Ship"
OBJECT_DIR = "/Game/Placeable/InteractiveObjects"
NPC_DIR = "/Game/Placeable/Character/NPC"
PH = "/Game/Art/Materials/PH_materials/m_PH_"
CUBE = "/Engine/BasicShapes/Cube"
SPHERE = "/Engine/BasicShapes/Sphere"
CYLINDER = "/Engine/BasicShapes/Cylinder"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
copy_props = unreal.YSEditorScriptLibrary.copy_editable_properties
report = []


def log(msg):
    unreal.log(f"[setup_start_island] {msg}")
    report.append(msg)


def find_asset(name, root="/Game"):
    """Chemin d'un asset par son nom (Paul range souvent les dossiers)"""
    for path in assets.list_assets(root, recursive=True):
        package = path.split(".")[0]
        if package.rsplit("/", 1)[1] == name:
            return package
    raise RuntimeError(f"asset introuvable : {name}")


def bp_class(name, root="/Game/Placeable"):
    return assets.load_blueprint_class(find_asset(name, root))


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def add_component(bp, name, component_class, parent=None):
    root = parent or subsystem.k2_gather_subobject_data_for_blueprint(bp)[0]
    params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=component_class, blueprint_context=bp)
    handle, fail = subsystem.add_new_subobject(params)
    if not lib.is_handle_valid(handle):
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    subsystem.rename_subobject(handle, unreal.Text(name))
    return handle, lib.get_object(lib.get_data(handle))


def add_shape(bp, name, mesh, material, location, scale, rotation=(0.0, 0.0, 0.0), parent=None):
    """rotation = (pitch, yaw, roll) ; visuel seulement"""
    _, comp = add_component(bp, name, unreal.StaticMeshComponent, parent)
    comp.set_editor_property("static_mesh", mesh if not isinstance(mesh, str) else assets.load_asset(mesh))
    comp.set_material(0, material if not isinstance(material, str) else assets.load_asset(PH + material))
    comp.set_editor_property("relative_location", unreal.Vector(*location))
    comp.set_editor_property("relative_rotation", unreal.Rotator(pitch=rotation[0], yaw=rotation[1], roll=rotation[2]))
    comp.set_editor_property("relative_scale3d", unreal.Vector(*scale))
    comp.set_collision_profile_name("NoCollision")
    comp.set_editor_property("generate_overlap_events", False)
    return comp


def create_bp(path, parent_class):
    """Nouveau Blueprint, ou None s'il existe déjà (retouches de Paul gardées)"""
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


def copy_mesh_shapes(source_path, bp, parent=None, skip=()):
    """Copie les formes (StaticMeshComponent) d'un Blueprint : retour = limites approximatives (x, y, z max)"""
    source = assets.load_asset(source_path)
    extent = [0.0, 0.0, 0.0]
    used = set()
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(source):
        comp = lib.get_object(lib.get_data(handle))
        if not isinstance(comp, unreal.StaticMeshComponent) or not comp.get_editor_property("static_mesh"):
            continue
        name = comp.get_name().replace("_GEN_VARIABLE", "")
        if name in used or name in skip:
            continue
        used.add(name)
        loc = comp.get_editor_property("relative_location")
        rot = comp.get_editor_property("relative_rotation")
        scale = comp.get_editor_property("relative_scale3d")
        add_shape(bp, name, comp.get_editor_property("static_mesh"), comp.get_material(0), (loc.x, loc.y, loc.z), (scale.x, scale.y, scale.z), (rot.pitch, rot.yaw, rot.roll), parent)
        extent = [max(extent[0], abs(loc.x) + abs(scale.x) * 50.0), max(extent[1], abs(loc.y) + abs(scale.y) * 50.0), max(extent[2], abs(loc.z) + abs(scale.z) * 50.0)]
    return extent


def make_start_boat():
    path = f"{SHIP_DIR}/BP_StartBoat"
    bp = create_bp(path, unreal.YSStartBoat)
    if not bp:
        return
    # chaloupe penchée sur le sable
    pivot_handle, pivot = add_component(bp, "BoatPivot", unreal.SceneComponent)
    pivot.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, 30.0))
    pivot.set_editor_property("relative_rotation", unreal.Rotator(pitch=7.0, yaw=0.0, roll=-11.0))
    extent = copy_mesh_shapes(find_asset("BP_Longboat", SHIP_DIR), bp, pivot_handle)
    _, box = add_component(bp, "Collision", unreal.BoxComponent)
    box.set_editor_property("box_extent", unreal.Vector(extent[0], extent[1], 150.0))
    box.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, 130.0))
    box.set_collision_profile_name("BlockAllDynamic")
    box.set_editor_property("can_character_step_up_on", unreal.CanBeCharacterBase.ECB_NO)
    box.set_editor_property("generate_overlap_events", False)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    # zone : mêmes réglages que le Brooker, autre couleur
    boat = cdo(path)
    copy_props(cdo(find_asset("BP_NPC_Brooker", NPC_DIR)), boat)
    boat.set_editor_property("brooker_name", unreal.Text("Chaloupe"))
    boat.set_editor_property("zone_color", unreal.LinearColor(0.2, 0.8, 1.0, 1.0))
    boat.set_editor_property("indicator_color", unreal.LinearColor(0.2, 0.8, 1.0, 1.0))
    finish_bp(bp, path)


def make_carpenter_shapes(bp):
    """Le Charpentier : le Brooker en bleu de travail, avec un marteau"""
    copy_mesh_shapes(find_asset("BP_NPC_Brooker", NPC_DIR), bp)
    add_shape(bp, "HammerHandle", CYLINDER, "Brown", (45, -55, 85), (0.09, 0.09, 0.6), (0, 0, 20))
    add_shape(bp, "HammerHead", CUBE, "Grey", (45, -75, 115), (0.3, 0.16, 0.16), (0, 0, 20))


def make_carpenter():
    npc_path = f"{NPC_DIR}/BP_NPC_Carpenter"
    bp = create_bp(npc_path, unreal.YSIslandNPC)
    if bp:
        make_carpenter_shapes(bp)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        npc = cdo(npc_path)
        copy_props(cdo(find_asset("BP_NPC_Brooker", NPC_DIR)), npc)
        npc.set_editor_property("brooker_name", unreal.Text("Charpentier"))
        npc.set_editor_property("accept_range", 1500.0)
        npc.set_editor_property("show_zone", False)
        finish_bp(bp, npc_path)

    item_path = f"{OBJECT_DIR}/BP_InteractiveObject_Carpenter"
    bp = create_bp(item_path, unreal.YSCarpenter)
    if bp:
        make_carpenter_shapes(bp)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        item = cdo(item_path)
        copy_props(cdo(find_asset("BP_InteractiveObject_Chest", OBJECT_DIR)), item)
        item.set_editor_property("display_name", unreal.Text("Charpentier"))
        item.set_editor_property("doubloon_value", 0)
        item.set_editor_property("phoenix_feather_value", 0)
        item.set_editor_property("show_indicator", False)
        # porté couché en travers des bras, comme un cadavre
        item.set_editor_property("carry_location", unreal.Vector(70.0, 90.0, 20.0))
        item.set_editor_property("carry_rotation", unreal.Rotator(pitch=0.0, yaw=-90.0, roll=90.0))
        item.set_editor_property("delivered_npc_class", assets.load_blueprint_class(npc_path))
        finish_bp(bp, item_path)
    return item_path


def make_boat_core():
    path = f"{OBJECT_DIR}/BP_InteractiveObject_BoatCore"
    bp = create_bp(path, unreal.YSInteractiveObject)
    if bp:
        add_shape(bp, "Base", CYLINDER, "Brown", (0, 0, 15), (0.8, 0.8, 0.3))
        add_shape(bp, "Core", SPHERE, "emissive_turboBlue", (0, 0, 60), (0.65, 0.65, 0.65))
        add_shape(bp, "Ring1", CYLINDER, "Yellow", (0, 0, 60), (0.85, 0.85, 0.06))
        add_shape(bp, "Ring2", CYLINDER, "Yellow", (0, 0, 60), (0.85, 0.85, 0.06), (90, 0, 0))
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        item = cdo(path)
        copy_props(cdo(find_asset("BP_InteractiveObject_Chest", OBJECT_DIR)), item)
        item.set_editor_property("display_name", unreal.Text("Core du bateau"))
        item.set_editor_property("doubloon_value", 0)
        item.set_editor_property("phoenix_feather_value", 0)
        item.set_editor_property("show_indicator", False)
        finish_bp(bp, path)
    return path


def foe_entry(name, count):
    entry = unreal.YSIslandFoeEntry()
    entry.set_editor_property("foe_class", bp_class(name, "/Game/Placeable/Character"))
    entry.set_editor_property("count", count)
    return entry


def spawn_event(time, foes, shape=unreal.YSIslandSpawnShape.OFF_SCREEN, repeat=1, interval=0.0, scale=True, overtime=False, offset=3, step=0):
    event = unreal.YSIslandSpawnEvent()
    event.set_editor_property("time", time)
    event.set_editor_property("repeat_count", repeat)
    event.set_editor_property("repeat_interval", interval)
    event.set_editor_property("shape", shape)
    event.set_editor_property("foes", [foe_entry(name, count) for name, count in foes])
    event.set_editor_property("scale_with_players", scale)
    event.set_editor_property("overtime_level", overtime)
    event.set_editor_property("level_offset", offset)
    event.set_editor_property("level_step_per_repeat", step)
    return event


def prop_def(name, radius, collision, weight, scale_min, scale_max, sink):
    prop = unreal.YSIslandPropDef()
    prop.set_editor_property("Class", assets.load_blueprint_class(find_asset(name, "/Game/Art/Props")))
    prop.set_editor_property("radius", radius)
    prop.set_editor_property("collision_radius", collision)
    prop.set_editor_property("weight", weight)
    prop.set_editor_property("scale_min", scale_min)
    prop.set_editor_property("scale_max", scale_max)
    prop.set_editor_property("sink", sink)
    return prop


def trio_def(weight, scale, circumradius):
    trio = unreal.YSIslandTrioDef()
    trio.set_editor_property("weight", weight)
    trio.set_editor_property("scale", scale)
    trio.set_editor_property("circumradius", circumradius)
    return trio


def objective(name, item_path, delivery_path=None):
    state = unreal.YSIslandObjectiveDef()
    state.set_editor_property("display_name", unreal.Text(name))
    state.set_editor_property("item_class", assets.load_blueprint_class(item_path))
    if delivery_path:
        state.set_editor_property("delivery_class", assets.load_blueprint_class(delivery_path))
    state.set_editor_property("distance_from_boat", unreal.Vector2D(3000.0, 9000.0))
    return state


def make_island_data(carpenter_path, core_path):
    if assets.does_asset_exist(ISLAND_DA):
        log(f"existe déjà : {ISLAND_DA}")
        return
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.YSIslandData)
    data = tools.create_asset("DA_I_StartIsland", ISLAND_DIR, unreal.YSIslandData, factory)

    data.set_editor_property("island_name", unreal.Text("Île de départ"))
    data.set_editor_property("generation_type", unreal.YSIslandGeneration.TROPICAL)

    rules = data.get_editor_property("generation")
    rules.set_editor_property("radius", 5000.0)
    rules.set_editor_property("ground_material", assets.load_asset("/Game/Art/Materials/M_IslandGround_Cliff"))
    rules.set_editor_property("tree_prop", prop_def("BP_Prop_PalmTree", 82.0, 75.0, 3.0, 0.9, 1.1, 5.0))
    rules.set_editor_property("bush_prop", prop_def("BP_Prop_Bush", 115.0, 115.0, 4.0, 0.9, 1.15, 10.0))
    rules.set_editor_property("rock_prop", prop_def("BP_Prop_Rock", 125.0, 125.0, 3.0, 0.9, 1.15, 10.0))
    rules.set_editor_property("tree_trio", trio_def(1.0, 0.85, 95.0))
    rules.set_editor_property("bush_trio", trio_def(1.0, 0.65, 110.0))
    data.set_editor_property("generation", rules)

    data.set_editor_property("main_objectives", [objective("Ramener le Charpentier au bateau", carpenter_path), objective("Ramener le core du bateau", core_path)])
    data.set_editor_property("secondary_objectives", [objective("Carte au trésor : ramener le coffre au bateau",
                                                                 find_asset("BP_InteractiveObject_TreasureMap", OBJECT_DIR), find_asset("BP_InteractiveObject_TreasureChest", OBJECT_DIR))])

    data.set_editor_property("max_days", 2)
    data.set_editor_property("day_duration", 90.0)

    circle = unreal.YSIslandSpawnShape.CIRCLE
    data.set_editor_property("day_events", [
        spawn_event(10.0, [("FoeCharacter_Skull", 20)]),
        spawn_event(30.0, [("FoeCharacter_Skull", 7), ("FoeCharacter_Skull_Gun", 2), ("FoeCharacter_Skull_Bomb", 1)], repeat=3, interval=20.0),
    ])
    data.set_editor_property("night_events", [
        spawn_event(0.0, [("FoeCharacter_Skull_Elite", 1)], unreal.YSIslandSpawnShape.CENTER, scale=False),
        spawn_event(0.0, [("FoeCharacter_Skull", 20)], circle),
        spawn_event(30.0, [("FoeCharacter_Skull", 20)], circle, repeat=2, interval=30.0),
        spawn_event(90.0, [("FoeCharacter_Skull_Nemesis", 20)], circle, repeat=0, interval=30.0, overtime=True, offset=3, step=2),
    ])
    data.set_editor_property("sink_events", [
        spawn_event(0.0, [("FoeCharacter_Skull_Gun", 20)], circle),
        spawn_event(30.0, [("FoeCharacter_Skull_Nemesis", 20)], circle),
    ])
    data.set_editor_property("sink_delay", 60.0)

    data.set_editor_property("start_boat_class", assets.load_blueprint_class(f"{SHIP_DIR}/BP_StartBoat"))
    data.set_editor_property("boat_zone_radius", 1500.0)
    assets.save_asset(ISLAND_DA, only_if_is_dirty=False)
    log(f"créé : {ISLAND_DA}")


def setup_game_mode():
    bp = assets.load_asset(FLOW_GM)
    unreal.BlueprintEditorLibrary.reparent_blueprint(bp, unreal.YSFlowMode)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    gm = cdo(FLOW_GM)
    gm.set_editor_property("start_island", assets.load_asset(ISLAND_DA))
    assets.save_asset(FLOW_GM, only_if_is_dirty=False)
    log(f"BP_FlowMod : parent {unreal.YSFlowMode.static_class().get_name()}, île {gm.get_editor_property('start_island').get_name()}")


def clean_map():
    if not levels.load_level(FLOW_MAP):
        raise RuntimeError(f"impossible d'ouvrir {FLOW_MAP}")
    removed = 0
    for actor in actors_sub.get_all_level_actors():
        if isinstance(actor, unreal.Landscape) or (str(actor.get_folder_path()).startswith("Island") and not isinstance(actor, unreal.YSOcean)):
            actors_sub.destroy_actor(actor)
            removed += 1
    if not levels.save_current_level():
        raise RuntimeError("sauvegarde de FlowMap impossible")
    counts = {}
    for actor in actors_sub.get_all_level_actors():
        name = actor.get_class().get_name()
        counts[name] = counts.get(name, 0) + 1
    log(f"FlowMap : {removed} acteurs figés retirés, reste {sorted(counts.items())}")


try:
    make_start_boat()
    carpenter = make_carpenter()
    core = make_boat_core()
    make_island_data(carpenter, core)
    setup_game_mode()
    clean_map()
    data = assets.load_asset(ISLAND_DA)
    log(f"relu DA_I_StartIsland : rayon {data.get_editor_property('generation').get_editor_property('radius')}, objectifs {len(data.get_editor_property('main_objectives'))} + {len(data.get_editor_property('secondary_objectives'))}, "
        f"vagues {len(data.get_editor_property('day_events'))} / {len(data.get_editor_property('night_events'))} / {len(data.get_editor_property('sink_events'))}, {data.get_editor_property('max_days')} jours")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_start_island.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
