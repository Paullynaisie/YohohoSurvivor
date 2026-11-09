"""
Partie 2 du flow, retours de Paul du 2026-11-09 (relançable : ne recrée pas une voile déployée qui existe, réécrit les réglages des îles) :
  - îles de la partie 2 (DA_I_Tropical_Map1, DA_I_Desert_Map1, DA_I_PortTown_Map1) : bateau bloqué à quai quand il accoste devant la plage
    (Lock Helm When Docked) jusqu'à la fin de l'évacuation, objectif « Ramène {0} au bateau » (boussole / Medic / Charpentier de l'île)
  - DA_I_PortTown_Map1 : plus de pontons, ville sur tous les massifs (tout sauf la plage : Town Half Angle 180, Town Inner Ratio 0)
  - Objective Name des objets (« la boussole », « le Medic », « le Charpentier », « le CoreBoat »)
  - voiles déployées : pour chaque voile carguée <Mât>_Sail<n> des bateaux, une voile <Mât>_SailOpen<n> (plan vertical sous la vergue,
    même matériau, sans collision, cachée dans le BP) ; le C++ (AYSPirateShip::UpdateSails) affiche les déployées quand le bateau peut
    naviguer, les carguées quand il est bloqué

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_phase2_docking.py"
Rapport : Saved/Logs/setup_phase2_docking.txt
"""
import os
import re
import unreal

ISLAND_DIR = "/Game/Core/Islands"
ISLANDS = ["DA_I_Tropical_Map1", "DA_I_Desert_Map1", "DA_I_PortTown_Map1"]
PORT_TOWN = "DA_I_PortTown_Map1"
FIND_FORMAT = "Ramène {0} au bateau"
OBJECTIVE_NAMES = {
    "BP_InteractiveObject_Compass": "la boussole",
    "BP_InteractiveObject_Medic": "le Medic",
    "BP_InteractiveObject_Carpenter": "le Charpentier",
    "BP_InteractiveObject_BoatCore": "le CoreBoat",
}
SHIP_DIR = "/Game/Placeable/Ship"
SHIPS = ["BP_PirateShip", "BP_GameBoat_Sloop", "BP_GameBoat_Brigantin", "BP_NME_Galion", "BP_NME_Galion_Nemesis"]
CUBE = "/Engine/BasicShapes/Cube.Cube"
SAIL_THICKNESS = 6.0
SAIL_TOP_GAP = 20.0        # sous la vergue
SAIL_BETWEEN_YARDS = 40.0  # au-dessus de la vergue du dessous
SAIL_DECK_CLEARANCE = 350.0  # la voile du bas s'arrête à cette hauteur au-dessus du pied du mât (pont dégagé vu de dessus)
SAIL_FORWARD = 12.0

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_phase2_docking] {msg}")
    report.append(str(msg))


def find_package(name, root="/Game/Placeable"):
    for path in assets.list_assets(root, recursive=True):
        package = path.split(".")[0]
        if package.rsplit("/", 1)[1] == name:
            return package
    raise RuntimeError(f"introuvable : {name}")


def components(bp):
    out = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if obj:
            out[obj.get_name().replace("_GEN_VARIABLE", "")] = obj
    return out


def setup_islands():
    for name in ISLANDS:
        path = f"{ISLAND_DIR}/{name}"
        if not assets.does_asset_exist(path):
            log(f"ABSENT : {path}")
            continue
        data = assets.load_asset(path)
        data.set_editor_property("lock_helm_when_docked", True)
        data.set_editor_property("lock_helm_until_evacuation", False)
        data.set_editor_property("find_objective_format", unreal.Text(FIND_FORMAT))
        if name == PORT_TOWN:
            rules = data.get_editor_property("generation")
            rules.set_editor_property("port_town_piers", False)
            rules.set_editor_property("town_half_angle", 180.0)
            rules.set_editor_property("town_inner_ratio", 0.0)
            data.set_editor_property("generation", rules)
        assets.save_loaded_asset(data, only_if_is_dirty=False)
        rules = data.get_editor_property("generation")
        log(f"{name} : bloqué à quai {data.get_editor_property('lock_helm_when_docked')}, objectif « {data.get_editor_property('find_objective_format')} », "
            f"pontons Port Town {rules.get_editor_property('port_town_piers')}, ville {rules.get_editor_property('town_half_angle')}° / {rules.get_editor_property('town_inner_ratio')}")


def setup_objective_names():
    for name, text in OBJECTIVE_NAMES.items():
        package = find_package(name)
        default = unreal.get_default_object(assets.load_blueprint_class(package))
        default.set_editor_property("ObjectiveName", unreal.Text(text))
        assets.save_asset(package, only_if_is_dirty=False)
        log(f"{name} : Objective Name « {text} »")


def add_cube(bp, name, material, location, size, attach_parent):
    params = unreal.AddNewSubobjectParams(parent_handle=attach_parent, new_class=unreal.StaticMeshComponent, blueprint_context=bp)
    handle, fail = subsystem.add_new_subobject(params)
    if not lib.is_handle_valid(handle):
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    subsystem.rename_subobject(handle, unreal.Text(name))
    comp = lib.get_object(lib.get_data(handle))
    comp.set_editor_property("static_mesh", unreal.load_asset(CUBE))
    if material:
        comp.set_material(0, material)
    comp.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
    comp.set_editor_property("relative_location", location)
    comp.set_editor_property("relative_rotation", unreal.Rotator(roll=0.0, pitch=0.0, yaw=0.0))
    comp.set_editor_property("relative_scale3d", unreal.Vector(size.x / 100.0, size.y / 100.0, size.z / 100.0))
    comp.set_collision_profile_name("NoCollision")
    comp.set_editor_property("generate_overlap_events", False)
    comp.set_editor_property("cast_shadow", True)
    try:
        comp.set_editor_property("visible", False)
    except Exception as e:
        log(f"  visible : {e!r}")
    return comp


def setup_sails(ship_name):
    path = f"{SHIP_DIR}/{ship_name}"
    if not assets.does_asset_exist(path):
        log(f"ABSENT : {path}")
        return
    bp = assets.load_asset(path)
    comps = components(bp)
    root = subsystem.k2_gather_subobject_data_for_blueprint(bp)[0]
    furled = sorted(n for n in comps if re.fullmatch(r"\w+_Sail\d+", n) and "_SailOpen" not in n)
    if not furled:
        log(f"{ship_name} : aucune voile carguée (<Mât>_Sail<n>) dans ce BP (hérité d'un parent ?)")
        return
    added = 0
    for name in furled:
        mast_name, index = re.fullmatch(r"(\w+)_Sail(\d+)", name).groups()
        open_name = f"{mast_name}_SailOpen{index}"
        if open_name in comps:
            continue
        sail = comps[name]
        yard = comps.get(f"{mast_name}_Yard{index}")
        mast = comps.get(mast_name)
        sail_loc = sail.get_editor_property("relative_location")
        width = sail.get_editor_property("relative_scale3d").z * 100.0
        top = (yard.get_editor_property("relative_location").z if yard else sail_loc.z + 22.0) - SAIL_TOP_GAP
        # bas : au-dessus de la vergue du dessous, sinon dégagé du pied du mât
        lower = [c.get_editor_property("relative_location").z for n, c in comps.items()
                 if re.fullmatch(rf"{mast_name}_Yard\d+", n) and c.get_editor_property("relative_location").z < top]
        if lower:
            bottom = max(lower) + SAIL_BETWEEN_YARDS
        elif mast:
            mast_loc = mast.get_editor_property("relative_location")
            bottom = mast_loc.z - mast.get_editor_property("relative_scale3d").z * 50.0 + SAIL_DECK_CLEARANCE
        else:
            bottom = top - 400.0
        height = max(top - bottom, 100.0)
        x = (yard.get_editor_property("relative_location").x if yard else sail_loc.x) + SAIL_FORWARD
        add_cube(bp, open_name, sail.get_material(0), unreal.Vector(x, sail_loc.y, (top + bottom) / 2.0),
                 unreal.Vector(SAIL_THICKNESS, width, height), root)
        log(f"  {ship_name}.{open_name} : largeur {width:.0f}, de {bottom:.0f} à {top:.0f}")
        added += 1
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(path, only_if_is_dirty=False)
    log(f"{ship_name} : {added} voile(s) déployée(s) ajoutée(s) ({len(furled)} carguée(s))")


try:
    setup_islands()
    setup_objective_names()
    for ship in SHIPS:
        setup_sails(ship)
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_phase2_docking.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
