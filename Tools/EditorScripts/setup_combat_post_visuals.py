"""
Postes de combat (demande de Paul, 2026-11-10) :
  - visuels des postes du bateau (BP_ShipCannon / Gatling / Mortar / GrapeShot) = ceux des objets transportables
    (BP_CombatPostItem_*), à l'échelle Item Scale de l'objet : la caisse devient l'affût (CannonBase, collision gardée),
    le reste (fût, frettes, canons...) est copié sous BarrelRoot (pivot du canon : tourne avec la visée et recule au tir) ;
    le fût natif CannonBarrel est caché ; pivot, bouche (Muzzle) et points des mains recalés sur les formes copiées
  - Gatling : 20 balles x 65 dégâts (Burst Interval inchangé : 0.1 s -> 2 s de rafale)
  - Mortier : rayon d'explosion de l'obus (BP_MortarShell) / 2
Relançable : les formes copiées (préfixe Post_) et les anciennes formes de la Gatling (Gatling*) sont supprimées puis recréées
depuis l'objet transportable ; à relancer après avoir retouché un BP_CombatPostItem_*.
Les dégâts des postes des Galions de NME / 2 sont en C++ (AYSFoeGalion::PostDamageMultiplier 0.5).

Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/setup_combat_post_visuals.py"
Rapport : Saved/Logs/setup_combat_post_visuals.txt
"""
import os
import unreal

STATIONS = "/Game/Placeable/Ship/Stations"
ITEMS = "/Game/Placeable/InteractiveObjects/CombatPosts"
PAIRS = (
    (f"{ITEMS}/BP_CombatPostItem_Cannon", f"{STATIONS}/BP_ShipCannon"),
    (f"{ITEMS}/BP_CombatPostItem_Gatling", f"{STATIONS}/BP_ShipGatling"),
    (f"{ITEMS}/BP_CombatPostItem_Mortar", f"{STATIONS}/BP_ShipMortar"),
    (f"{ITEMS}/BP_CombatPostItem_GrapeShot", f"{STATIONS}/BP_ShipGrapeShot"),
)
SHELL = "/Game/Placeable/AMMO/BP_MortarShell"
GATLING = f"{STATIONS}/BP_ShipGatling"
SKIPPED = {"Root", "Crate", "InteractPrompt", "LeftHandPoint", "RightHandPoint"}
OLD_PREFIXES = ("Post", "Gatling")  # formes copiées : Post<Objet>_<forme> ; Gatling* = anciennes formes de la Gatling
# formes laissées sans préfixe par un premier passage du script (collision de noms au renommage, 2026-11-10)
LEFTOVERS = {"BP_ShipGrapeShot": {"Scene", "Scene1"} | {"StaticMesh"} | {f"StaticMesh{i}" for i in range(1, 10)}}
HAND_BEHIND = 20.0     # mains derrière l'arrière des formes
HAND_SIDE = 22.0
HAND_MIN_X = -70.0  # jamais derrière le joueur (Occupant Point à -110)
MUZZLE_AHEAD = 5.0

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report = []


def log(msg):
    unreal.log(f"[setup_combat_post_visuals] {msg}")
    report.append(str(msg))


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def compile_save(path):
    unreal.BlueprintEditorLibrary.compile_blueprint(assets.load_asset(path))
    assets.save_asset(path, only_if_is_dirty=False)


def clean(name):
    return name.replace("_GEN_VARIABLE", "")


def gather(bp):
    """nom -> (handle, objet, nom du parent) ; un seul handle par nom"""
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    by_handle = []
    for handle in handles:
        obj = lib.get_object(lib.get_data(handle))
        by_handle.append((handle, obj))
    out = {}
    for handle, obj in by_handle:
        if not obj or not isinstance(obj, unreal.ActorComponent):
            continue
        name = clean(obj.get_name())
        if name in out:
            continue
        parent_handle = lib.get_parent_handle(lib.get_data(handle))
        parent_name = None
        if lib.is_handle_valid(parent_handle):
            parent_obj = lib.get_object(lib.get_data(parent_handle))
            if parent_obj and isinstance(parent_obj, unreal.ActorComponent):
                parent_name = clean(parent_obj.get_name())
        out[name] = (handle, obj, parent_name)
    return out


def rel_transform(comp):
    return unreal.Transform(comp.get_editor_property("relative_location"), comp.get_editor_property("relative_rotation"),
                            comp.get_editor_property("relative_scale3d"))


def world_transform(name, comps, cache):
    if name in cache:
        return cache[name]
    _, obj, parent = comps[name]
    rel = rel_transform(obj)
    if parent and parent in comps and parent != "Root":
        result = rel.multiply(world_transform(parent, comps, cache))
    else:
        result = rel
    cache[name] = result
    return result


def mesh_x_range(transform):
    """x min / max (repère de l'objet) d'une forme de base (cube / cylindre / sphère de 100 cm)"""
    xs = []
    for cx in (-50.0, 50.0):
        for cy in (-50.0, 50.0):
            for cz in (-50.0, 50.0):
                xs.append(unreal.MathLibrary.transform_location(transform, unreal.Vector(cx, cy, cz)).x)
    return min(xs), max(xs)


def visual_names(comps):
    """formes à copier : tout sauf la racine, la caisse, l'invite et les mains (avec leurs enfants)"""
    out = []
    for name, (_, obj, parent) in comps.items():
        if name in SKIPPED or not isinstance(obj, unreal.SceneComponent) or isinstance(obj, unreal.WidgetComponent):
            continue
        if obj.get_class().get_name().startswith("BP_"):
            continue
        chain, p = [], parent
        while p and p in comps:
            chain.append(p)
            p = comps[p][2]
        if any(c in SKIPPED - {"Root"} for c in chain):
            continue
        out.append(name)
    return out


def depth(name, comps):
    d, p = 0, comps[name][2]
    while p and p in comps:
        d, p = d + 1, comps[p][2]
    return d


def remove_old(station_bp):
    comps = gather(station_bp)
    leftovers = LEFTOVERS.get(station_bp.get_name(), ())
    old = [n for n, (h, o, p) in comps.items() if (n.startswith(OLD_PREFIXES) or n in leftovers) and not lib.is_inherited_component(lib.get_data(h))]
    old.sort(key=lambda n: -depth(n, comps))
    root = subsystem.k2_gather_subobject_data_for_blueprint(station_bp)[0]
    for name in old:
        handle = gather(station_bp).get(name)
        if handle:
            subsystem.delete_subobject(root, handle[0], station_bp)
    return old


def gather_instance(item_path):
    """formes de l'objet lues sur une instance posée (retouches des composants hérités des BP enfants comprises) :
    nom -> (None, composant, nom du parent)"""
    actor = actors.spawn_actor_from_class(assets.load_blueprint_class(item_path), unreal.Vector(0, 0, 0), unreal.Rotator())
    out = {}
    for comp in actor.get_components_by_class(unreal.ActorComponent):
        parent = comp.get_attach_parent() if isinstance(comp, unreal.SceneComponent) else None
        out[comp.get_name()] = (None, comp, parent.get_name() if parent else None)
    return actor, out


def copy_visuals(item_path, station_path):
    station_bp = assets.load_asset(station_path)
    item_actor, item = gather_instance(item_path)
    scale = cdo(item_path).get_editor_property("ItemScale") or 1.0
    removed = remove_old(station_bp)

    cache = {}
    names = visual_names(item)
    meshes = [n for n in names if isinstance(item[n][1], unreal.StaticMeshComponent)]
    if not meshes:
        raise RuntimeError(f"{item_path} : aucune forme à copier")
    pivot_z = round(sum(world_transform(n, item, cache).translation.z for n in meshes) / len(meshes) * scale, 1)
    x_min = min(mesh_x_range(world_transform(n, item, cache))[0] for n in meshes) * scale
    x_max = max(mesh_x_range(world_transform(n, item, cache))[1] for n in meshes) * scale
    # bouche : à l'avant, à la hauteur du centre de la forme la plus en avant (fût levé du mortier compris)
    front = max(meshes, key=lambda n: mesh_x_range(world_transform(n, item, cache))[1])
    muzzle_z = round(world_transform(front, item, cache).translation.z * scale - pivot_z, 1)

    station = gather(station_bp)
    barrel_root = station["BarrelRoot"][0]
    # préfixe propre au poste (PostCannon_, PostGatling_...) ; formes héritées d'un poste parent = cachées (Hidden Component Names)
    prefix = "Post" + item_path.rsplit("_", 1)[1] + "_"
    inherited = sorted(n for n, (h, o, p) in station.items() if n.startswith(OLD_PREFIXES) and lib.is_inherited_component(lib.get_data(h)))
    created = {}
    for name in sorted(names, key=lambda n: depth(n, item)):
        _, src, parent = item[name]
        top_level = not parent or parent == "Root" or parent not in created
        parent_handle = barrel_root if top_level else created[parent]
        params = unreal.AddNewSubobjectParams(parent_handle=parent_handle, new_class=src.get_class(), blueprint_context=station_bp)
        handle, fail = subsystem.add_new_subobject(params)
        if not lib.is_handle_valid(handle):
            raise RuntimeError(f"{station_path} : ajout de {name} impossible : {fail}")
        subsystem.rename_subobject(handle, unreal.Text(prefix + name))
        dst = lib.get_object(lib.get_data(handle))
        loc = src.get_editor_property("relative_location")
        sc = src.get_editor_property("relative_scale3d")
        if top_level:
            loc = unreal.Vector(loc.x * scale, loc.y * scale, loc.z * scale - pivot_z)
            sc = unreal.Vector(sc.x * scale, sc.y * scale, sc.z * scale)
        dst.set_editor_property("relative_location", loc)
        dst.set_editor_property("relative_rotation", src.get_editor_property("relative_rotation"))
        dst.set_editor_property("relative_scale3d", sc)
        if isinstance(src, unreal.StaticMeshComponent):
            dst.set_editor_property("static_mesh", src.get_editor_property("static_mesh"))
            for index in range(src.get_num_materials()):
                dst.set_material(index, src.get_material(index))
            dst.set_collision_profile_name("NoCollision")
            dst.set_editor_property("generate_overlap_events", False)
            dst.set_editor_property("cast_shadow", src.get_editor_property("cast_shadow"))
            dst.set_editor_property("mobility", unreal.ComponentMobility.MOVABLE)
        dst.set_editor_property("visible", src.get_editor_property("visible"))
        dst.set_editor_property("hidden_in_game", src.get_editor_property("hidden_in_game"))
        created[name] = handle
    item_actor.destroy_actor()

    # affût = caisse de l'objet (collision BlockAll gardée), pivot à la hauteur des formes, fût natif caché
    default = cdo(station_path)
    default.set_editor_property("HiddenComponentNames", [unreal.Name(n) for n in inherited])
    crate = item.get("Crate")
    base = default.get_editor_property("CannonBase")
    if crate:
        c = crate[1]
        cl, cs = c.get_editor_property("relative_location"), c.get_editor_property("relative_scale3d")
        base.set_editor_property("relative_location", unreal.Vector(cl.x * scale, cl.y * scale, cl.z * scale))
        base.set_editor_property("relative_scale3d", unreal.Vector(cs.x * scale, cs.y * scale, cs.z * scale))
        base.set_material(0, c.get_material(0))
    pivot = default.get_editor_property("CannonPivot")
    pivot.set_editor_property("relative_location", unreal.Vector(0.0, 0.0, pivot_z))
    default.get_editor_property("CannonBarrel").set_editor_property("visible", False)
    default.get_editor_property("Muzzle").set_editor_property("relative_location", unreal.Vector(round(x_max + MUZZLE_AHEAD, 1), 0.0, muzzle_z))
    hand_x = round(max(x_min - HAND_BEHIND, HAND_MIN_X), 1)
    default.get_editor_property("RightHandPoint").set_editor_property("relative_location", unreal.Vector(hand_x, HAND_SIDE, 5.0))
    default.get_editor_property("LeftHandPoint").set_editor_property("relative_location", unreal.Vector(hand_x, -HAND_SIDE, 5.0))
    compile_save(station_path)
    log(f"{station_path.rsplit('/', 1)[1]} <- {item_path.rsplit('/', 1)[1]} (x{scale}) : {len(created)} formes copiées, {len(removed)} anciennes retirées "
        f"{removed}, pivot z {pivot_z}, bouche ({round(x_max + MUZZLE_AHEAD, 1)}, 0, {muzzle_z}), mains x {hand_x}, hérités cachés {inherited}")


try:
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).new_level("/Temp/CombatPostVisuals")
    for item_path, station_path in PAIRS:
        copy_visuals(item_path, station_path)

    gatling = cdo(GATLING)
    gatling.set_editor_property("ShotsPerBurst", 20)
    gatling.set_editor_property("Damage", 65.0)
    compile_save(GATLING)
    gatling = cdo(GATLING)
    log(f"Gatling : {gatling.get_editor_property('ShotsPerBurst')} balles x {gatling.get_editor_property('Damage')} toutes les "
        f"{gatling.get_editor_property('BurstInterval'):.2f} s")

    shell = cdo(SHELL)
    radius = shell.get_editor_property("ExplosionRadius")
    if radius > 1000.0:  # 1500 -> 750 une seule fois (relançable)
        shell.set_editor_property("ExplosionRadius", radius / 2.0)
        compile_save(SHELL)
    log(f"BP_MortarShell : rayon {cdo(SHELL).get_editor_property('ExplosionRadius')}")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_combat_post_visuals.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
