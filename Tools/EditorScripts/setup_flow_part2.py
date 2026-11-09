"""
Flow en 3 parties, partie 2 (2026-11-09), relançable (ne recrée rien qui existe ; les réglages de BP_FlowMod sont réécrits) :
  - îles de la partie 2 (rayon 75 m, une par biome, tournées au hasard) = copies de DA_I_StartIsland_Map1 (flow, jour 90 s, nuit 90 s,
    évacuation 30 s, vagues, spawn constant, objectifs secondaires : carte au trésor, poste de combat au hasard, 2 Basic Chest) avec la
    génération de DA_I_P1_basicIsland (Tropical, Désert) / DA_I_P1_objectifIsland (Port Town) ; plus d'objectif principal (les objets des
    zones d'intérêt sont ajoutés par le FlowMode) ; ni pontons de plage, ni barre verrouillée, ni game over
      /Game/Core/Islands/DA_I_Tropical_Map1, DA_I_Desert_Map1, DA_I_PortTown_Map1
  - /Game/Core/NavalBattles/DT_NavalBattles_Map1 = copie de DT_NavalBattles
  - /Game/Placeable/InteractiveObjects/BP_InteractiveObject_Compass (boussole, primitives)
  - /Game/Placeable/InteractiveObjects/BP_InteractiveObject_SuperTreasureMap (grande carte couverte de croix, sans effet pour l'instant)
  - /Game/Placeable/Ship/BP_NME_Galion_Nemesis = copie violette de BP_NME_Galion : capitaine Skull Elite + capitaine en plus Skull Gun Elite,
    équipage de Skull Nemesis
  - BP_FlowMod : îles suivantes + batailles navales en zones d'intérêt (pentagone), objets répartis, séquence de nuits
    (élite / boss / 2 élites / boss), partie 2 de 20 min puis Galion Nemesis toutes les minutes

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_flow_part2.py"
Rapport : Saved/Logs/setup_flow_part2.txt
"""
import os
import unreal

ISLAND_DIR = "/Game/Core/Islands"
START_MAP1 = f"{ISLAND_DIR}/DA_I_StartIsland_Map1"
BASIC = f"{ISLAND_DIR}/DA_I_P1_basicIsland"
OBJECTIF = f"{ISLAND_DIR}/DA_I_P1_objectifIsland"
BATTLES = "/Game/Core/NavalBattles/DT_NavalBattles"
BATTLES_MAP1 = "/Game/Core/NavalBattles/DT_NavalBattles_Map1"
FLOW_GM = "/Game/Core/GameModes/BP_FlowMod"
OBJECTS_DIR = "/Game/Placeable/InteractiveObjects"
COMPASS = f"{OBJECTS_DIR}/BP_InteractiveObject_Compass"
SUPER_MAP = f"{OBJECTS_DIR}/BP_InteractiveObject_SuperTreasureMap"
TREASURE_MAP = f"{OBJECTS_DIR}/BP_InteractiveObject_TreasureMap"
GALION = "/Game/Placeable/Ship/BP_NME_Galion"
NEMESIS_GALION = "/Game/Placeable/Ship/BP_NME_Galion_Nemesis"
MAT = "/Game/Art/Materials/PH_materials"
CUBE = "/Engine/BasicShapes/Cube.Cube"
CYLINDER = "/Engine/BasicShapes/Cylinder.Cylinder"

RADIUS = 7500.0
DAY_DURATION = 90.0
NIGHT_DURATION = 90.0
EVACUATION_DURATION = 30.0
# (nom, biome, Data Asset dont on prend la génération)
ISLANDS = [
    ("DA_I_Tropical_Map1", unreal.YSIslandGeneration.TROPICAL, BASIC),
    ("DA_I_Desert_Map1", unreal.YSIslandGeneration.DESERT, BASIC),
    ("DA_I_PortTown_Map1", unreal.YSIslandGeneration.PORT_TOWN, OBJECTIF),
]
PURPLE_MATERIALS = {
    "m_PH_monochrome_Red": f"{MAT}/m_PH_monochrome_Purple",
    "m_PH_monochrome_LightRed": f"{MAT}/m_PH_monochrome_Pink",
}

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_flow_part2] {msg}")
    report.append(str(msg))


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def find_class(name, root="/Game/Placeable"):
    for path in assets.list_assets(root, recursive=True):
        package = path.split(".")[0]
        if package.rsplit("/", 1)[1] == name:
            return assets.load_blueprint_class(package)
    raise RuntimeError(f"introuvable : {name}")


def components(bp):
    out = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if obj:
            out[obj.get_name().replace("_GEN_VARIABLE", "")] = obj
    return out


def get_or_create(directory, name, parent):
    path = f"{directory}/{name}"
    created = not assets.does_asset_exist(path)
    if created:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent)
        tools.create_asset(name, directory, unreal.Blueprint, factory)
        log(f"créé : {path}")
    else:
        log(f"existe déjà : {path}")
    bp = assets.load_asset(path)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    return bp, path, created


def add_shape(bp, name, mesh, material, location, scale, rotation=None):
    """Forme (StaticMeshComponent) sous la racine « Root » du Blueprint, sans collision"""
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    root = next(h for h in handles if str(lib.get_variable_name(lib.get_data(h))) == "Root")
    params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.StaticMeshComponent, blueprint_context=bp)
    handle, fail = subsystem.add_new_subobject(params)
    if not lib.is_handle_valid(handle):
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    subsystem.rename_subobject(handle, unreal.Text(name))
    shape = lib.get_object(lib.get_data(handle))
    shape.set_static_mesh(assets.load_asset(mesh))
    shape.set_material(0, assets.load_asset(f"{MAT}/{material}"))
    shape.set_collision_profile_name("NoCollision")
    shape.set_editor_property("relative_location", location)
    shape.set_editor_property("relative_scale3d", scale)
    if rotation is not None:
        shape.set_editor_property("relative_rotation", rotation)


def v(x, y, z):
    return unreal.Vector(x, y, z)


def yaw(angle):
    return unreal.Rotator(roll=0, pitch=0, yaw=angle)


def foe_entry(classes, count):
    entry = unreal.YSIslandFoeEntry()
    if len(classes) == 1:
        entry.set_editor_property("foe_class", classes[0])
    else:
        entry.set_editor_property("random_foe_classes", classes)
    entry.set_editor_property("count", count)
    return entry


def night(entries):
    setup = unreal.YSFlowNightSetup()
    setup.set_editor_property("foes", entries)
    return setup


try:
    # --- 1) îles de la partie 2
    island_datas = []
    for name, biome, generation_source in ISLANDS:
        path = f"{ISLAND_DIR}/{name}"
        if not assets.does_asset_exist(path):
            data = assets.duplicate_asset(START_MAP1, path)
            rules = assets.load_asset(generation_source).get_editor_property("generation")
            rules.set_editor_property("radius", RADIUS)
            rules.set_editor_property("beach_piers", False)
            data.set_editor_property("generation", rules)
            data.set_editor_property("island_name", unreal.Text(name.replace("DA_I_", "").replace("_Map1", "")))
            data.set_editor_property("generation_type", biome)
            data.set_editor_property("random_generation_types", [])
            data.set_editor_property("random_rotation", True)
            # objets des zones d'intérêt ajoutés par le FlowMode ; secondaires de l'île de départ gardés
            data.set_editor_property("main_objectives", [])
            data.set_editor_property("day_duration", DAY_DURATION)
            data.set_editor_property("night_duration", NIGHT_DURATION)
            data.set_editor_property("evacuation_duration", EVACUATION_DURATION)
            data.set_editor_property("find_text", unreal.Text("Ramène les objets de l'île au bateau"))
            data.set_editor_property("defend_text", unreal.Text("Survivez à la nuit"))
            data.set_editor_property("victory_on_evacuation", False)
            data.set_editor_property("lock_helm_until_evacuation", False)
            data.set_editor_property("game_over_without_main_objectives", False)
            assets.save_loaded_asset(data, only_if_is_dirty=False)
            log(f"créé : {path} (génération de {generation_source})")
        else:
            log(f"existe déjà (pas retouché) : {path}")
        data = assets.load_asset(path)
        island_datas.append(data)
        rules = data.get_editor_property("generation")
        secondary = [(o.get_editor_property("item_class").get_name() if o.get_editor_property("item_class") else "-", o.get_editor_property("random_combat_post"))
                     for o in data.get_editor_property("secondary_objectives")]
        log(f"relu : {name} biome {data.get_editor_property('generation_type')}, rayon {rules.get_editor_property('radius')}, rotation {data.get_editor_property('random_rotation')}, "
            f"jour {data.get_editor_property('day_duration')} nuit {data.get_editor_property('night_duration')} évacuation {data.get_editor_property('evacuation_duration')}, "
            f"{len(data.get_editor_property('main_objectives'))} principal(aux), secondaires {secondary}, maisons {rules.get_editor_property('small_house_prop').get_editor_property('class')}")

    # --- 2) batailles navales
    if not assets.does_asset_exist(BATTLES_MAP1):
        assets.duplicate_asset(BATTLES, BATTLES_MAP1)
        log(f"créé : {BATTLES_MAP1}")
    assets.save_asset(BATTLES_MAP1, only_if_is_dirty=False)
    battles = assets.load_asset(BATTLES_MAP1)
    rows = [str(r) for r in unreal.DataTableFunctionLibrary.get_data_table_row_names(battles)]
    log(f"relu : {BATTLES_MAP1} lignes {rows}")

    prompt_class = cdo(TREASURE_MAP).get_editor_property("prompt_widget_class")

    # --- 3) boussole
    compass_bp, _, created = get_or_create(OBJECTS_DIR, "BP_InteractiveObject_Compass", unreal.YSInteractiveObject)
    if created:
        add_shape(compass_bp, "Case", CYLINDER, "m_PH_monochrome_Yellow", v(0, 0, 4), v(0.42, 0.42, 0.08))
        add_shape(compass_bp, "Face", CYLINDER, "m_PH_monochrome_white", v(0, 0, 8.2), v(0.34, 0.34, 0.01))
        add_shape(compass_bp, "NeedleNorth", CUBE, "m_PH_monochrome_Red", v(6, 0, 8.9), v(0.12, 0.025, 0.008))
        add_shape(compass_bp, "NeedleSouth", CUBE, "m_PH_monochrome_LightBlack", v(-6, 0, 8.9), v(0.12, 0.025, 0.008))
        add_shape(compass_bp, "Pin", CYLINDER, "m_PH_monochrome_Yellow", v(0, 0, 9.3), v(0.04, 0.04, 0.02))
        add_shape(compass_bp, "Ring", CYLINDER, "m_PH_monochrome_Yellow", v(23, 0, 4), v(0.08, 0.08, 0.06))
        unreal.BlueprintEditorLibrary.compile_blueprint(compass_bp)
        compass = cdo(COMPASS)
        compass.set_editor_property("display_name", unreal.Text("Boussole"))
        compass.set_editor_property("prompt_widget_class", prompt_class)
        compass.set_editor_property("carry_location", v(55, 0, -5))
        compass.set_editor_property("carry_rotation", unreal.Rotator(roll=0, pitch=25, yaw=0))
        compass.get_editor_property("LeftHandPoint").set_editor_property("relative_location", v(0, -22, 5))
        compass.get_editor_property("RightHandPoint").set_editor_property("relative_location", v(0, 22, 5))
        compass.get_editor_property("InteractPrompt").set_editor_property("relative_location", v(0, 0, 70))
        assets.save_asset(COMPASS, only_if_is_dirty=False)

    # --- 4) Super Treasure Map : grande carte, plein de croix
    super_bp, _, created = get_or_create(OBJECTS_DIR, "BP_InteractiveObject_SuperTreasureMap", unreal.YSInteractiveObject)
    if created:
        add_shape(super_bp, "Parchment", CUBE, "m_PH_monochrome_Sand", v(0, 0, 3), v(0.55, 0.72, 0.02))
        add_shape(super_bp, "RollLeft", CYLINDER, "m_PH_monochrome_Brown", v(0, -37, 4), v(0.08, 0.08, 0.6), unreal.Rotator(roll=0, pitch=90, yaw=0))
        add_shape(super_bp, "RollRight", CYLINDER, "m_PH_monochrome_Brown", v(0, 37, 4), v(0.08, 0.08, 0.6), unreal.Rotator(roll=0, pitch=90, yaw=0))
        crosses = [(-18, -22), (-4, -26), (14, -18), (-20, -6), (2, -8), (19, 0), (-12, 8), (8, 12), (-20, 22), (0, 24), (18, 20), (-6, -16)]
        for index, (x, y) in enumerate(crosses):
            size = 0.07 if index % 3 else 0.1
            add_shape(super_bp, f"Cross{index}A", CUBE, "m_PH_monochrome_Red", v(x, y, 4.2), v(size, 0.015, 0.004), yaw(45))
            add_shape(super_bp, f"Cross{index}B", CUBE, "m_PH_monochrome_Red", v(x, y, 4.2), v(size, 0.015, 0.004), yaw(-45))
        unreal.BlueprintEditorLibrary.compile_blueprint(super_bp)
        super_map = cdo(SUPER_MAP)
        super_map.set_editor_property("display_name", unreal.Text("Super carte au trésor"))
        super_map.set_editor_property("prompt_widget_class", prompt_class)
        super_map.set_editor_property("carry_location", v(60, 0, -5))
        super_map.set_editor_property("carry_rotation", unreal.Rotator(roll=0, pitch=25, yaw=0))
        super_map.get_editor_property("LeftHandPoint").set_editor_property("relative_location", v(0, -40, 4))
        super_map.get_editor_property("RightHandPoint").set_editor_property("relative_location", v(0, 40, 4))
        super_map.get_editor_property("InteractPrompt").set_editor_property("relative_location", v(0, 0, 80))
        assets.save_asset(SUPER_MAP, only_if_is_dirty=False)

    # --- 5) Galion Nemesis : copie violette du Galion
    skull_elite = find_class("FoeCharacter_Skull_Elite")
    gun_elite = find_class("FoeCharacter_Skull_Gun_Elite")
    skull_boss = find_class("FoeCharacter_Skull_Boss")
    gun_boss = find_class("FoeCharacter_Skull_Gun_Boss")
    nemesis = find_class("FoeCharacter_Skull_Nemesis")
    if not assets.does_asset_exist(NEMESIS_GALION):
        assets.duplicate_asset(GALION, NEMESIS_GALION)
        galion_bp = assets.load_asset(NEMESIS_GALION)
        unreal.BlueprintEditorLibrary.compile_blueprint(galion_bp)
        recolored = 0
        for name, comp in components(galion_bp).items():
            if isinstance(comp, unreal.StaticMeshComponent):
                material = comp.get_material(0)
                target = PURPLE_MATERIALS.get(material.get_name()) if material else None
                if target:
                    comp.set_material(0, assets.load_asset(target))
                    recolored += 1
        unreal.BlueprintEditorLibrary.compile_blueprint(galion_bp)
        galion = cdo(NEMESIS_GALION)
        galion.set_editor_property("ship_name", unreal.Text("Galion Nemesis"))
        galion.set_editor_property("captain_class", skull_elite)
        galion.set_editor_property("captain_classes", [])
        galion.set_editor_property("extra_captain_classes", [gun_elite])
        group = unreal.YSGalionCrewGroup()
        group.set_editor_property("classes", [nemesis])
        group.set_editor_property("count", 15)
        galion.set_editor_property("crew_groups", [group])
        galion.set_editor_property("boarding_crew_groups", [group])
        unreal.BlueprintEditorLibrary.compile_blueprint(galion_bp)
        assets.save_asset(NEMESIS_GALION, only_if_is_dirty=False)
        log(f"créé : {NEMESIS_GALION} ({recolored} formes en violet)")
    else:
        log(f"existe déjà : {NEMESIS_GALION}")
    galion = cdo(NEMESIS_GALION)
    log(f"relu : Galion Nemesis capitaine {galion.get_editor_property('captain_class').get_name()} + {[c.get_name() for c in galion.get_editor_property('extra_captain_classes')]}, "
        f"équipage {[(g.get_editor_property('classes')[0].get_name(), g.get_editor_property('count')) for g in galion.get_editor_property('crew_groups')]}")

    # --- 6) BP_FlowMod
    gm = cdo(FLOW_GM)
    gm.set_editor_property("next_islands_enabled", True)
    gm.set_editor_property("next_islands", island_datas)
    gm.set_editor_property("naval_battle_table", battles)
    gm.set_editor_property("naval_battle_rows", [unreal.Name("Base"), unreal.Name("Base")])
    gm.set_editor_property("zones_of_interest", True)
    gm.set_editor_property("boat_core_item_class", find_class("BP_InteractiveObject_BoatCore"))
    gm.set_editor_property("compass_item_class", assets.load_blueprint_class(COMPASS))
    gm.set_editor_property("compass_count", 3)
    gm.set_editor_property("crew_item_classes", [find_class("BP_InteractiveObject_Medic"), find_class("BP_InteractiveObject_Carpenter")])
    gm.set_editor_property("super_treasure_map_class", assets.load_blueprint_class(SUPER_MAP))
    gm.set_editor_property("night_sequence", [
        night([foe_entry([skull_elite, gun_elite], 1)]),
        night([foe_entry([skull_boss, gun_boss], 1)]),
        night([foe_entry([skull_elite, gun_elite], 2)]),
        night([foe_entry([skull_boss, gun_boss], 1)]),
    ])
    gm.set_editor_property("part2_duration", 1200.0)
    gm.set_editor_property("part2_overtime_interval", 60.0)
    gm.set_editor_property("part2_overtime_galion_class", assets.load_blueprint_class(NEMESIS_GALION))
    assets.save_asset(FLOW_GM, only_if_is_dirty=False)

    gm = cdo(FLOW_GM)
    log(f"relu : BP_FlowMod îles {[d.get_name() for d in gm.get_editor_property('next_islands')]}, batailles {gm.get_editor_property('naval_battle_table').get_name()} "
        f"{[str(r) for r in gm.get_editor_property('naval_battle_rows')]}, ZI {gm.get_editor_property('zones_of_interest')}, "
        f"core {gm.get_editor_property('boat_core_item_class').get_name()}, boussole {gm.get_editor_property('compass_item_class').get_name()} x{gm.get_editor_property('compass_count')}, "
        f"équipage {[c.get_name() for c in gm.get_editor_property('crew_item_classes')]}, super carte {gm.get_editor_property('super_treasure_map_class').get_name()}, "
        f"{len(gm.get_editor_property('night_sequence'))} nuits, partie 2 {gm.get_editor_property('part2_duration')} s, Galion {gm.get_editor_property('part2_overtime_galion_class').get_name()}, "
        f"île finale {gm.get_editor_property('final_island')}, gap {gm.get_editor_property('next_islands_gap')} x{gm.get_editor_property('next_islands_distance_multiplier')}")
    log("OK")
except Exception as error:
    import traceback
    log(f"ERREUR : {error}\n{traceback.format_exc()}")

with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_flow_part2.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
