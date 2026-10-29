# Perf des foules : fusionne toutes les formes (primitives) de chaque arme de NME en un seul static mesh
# (/Game/Placeable/Weapons/Foe/Merged/SM_<arme>) et le branche sur la propriété Merged Mesh du Blueprint de l'arme.
# En jeu, AYSWeapon::OptimizeForCrowd remplace alors les formes par ce mesh (une primitive au lieu d'une dizaine).
# A RELANCER après avoir retouché les formes d'une de ces armes (sinon le NME garde l'ancien visuel en jeu).
# BP_NME_Bomb n'est pas fusionnée : la mèche de la bombe lancée colore son composant Mesh.
# Lancement (éditeur fermé) : UnrealEditor.exe <uproject> -ExecutePythonScript="Tools/EditorScripts/merge_foe_weapons.py"
import unreal

WEAPONS = [
    "/Game/Placeable/Weapons/Foe/BP_NME_Sword",
    "/Game/Placeable/Weapons/Foe/BP_NME_GreatSword",
    "/Game/Placeable/Weapons/Foe/BP_NME_Pistol",
]
MERGED_DIR = "/Game/Placeable/Weapons/Foe/Merged"

report = []
# restes d un premier essai (préfixe en double)
for leftover in ("SM_SM_NME_Sword", "SM_SM_NME_GreatSword", "SM_SM_NME_Pistol"):
    if unreal.EditorAssetLibrary.does_asset_exist(MERGED_DIR + "/" + leftover):
        unreal.EditorAssetLibrary.delete_asset(MERGED_DIR + "/" + leftover)
log = report.append
assets = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
meshes = unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)

for bp_path in WEAPONS:
    name = bp_path.rsplit("/", 1)[1]
    # le moteur préfixe le nom du mesh fusionné par SM_
    base_path = "%s/%s" % (MERGED_DIR, name.replace("BP_", ""))
    merged_path = "%s/SM_%s" % (MERGED_DIR, name.replace("BP_", ""))
    bp_class = unreal.load_object(None, bp_path + "." + name + "_C")
    if not bp_class:
        log("ERREUR %s introuvable" % bp_path)
        continue
    cdo = unreal.get_default_object(bp_class)

    # ancien mesh fusionné : débranché puis supprimé (refait à neuf)
    if assets.does_asset_exist(merged_path):
        cdo.set_editor_property("merged_mesh", None)
        assets.save_asset(bp_path, only_if_is_dirty=False)
        log("%s : ancien %s supprimé (%s)" % (name, merged_path, assets.delete_asset(merged_path)))

    # arme posée à l'origine, échelle 1 : le mesh fusionné est dans le repère de son composant Root
    actor = actors.spawn_actor_from_class(bp_class, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
    actor.set_actor_scale3d(unreal.Vector(1, 1, 1))
    shapes = [c for c in actor.get_components_by_class(unreal.StaticMeshComponent) if c.static_mesh]
    # la fusion n'accepte que des StaticMeshActor : une copie temporaire par forme (transform, mesh, matériaux)
    copies = []
    for shape in shapes:
        copy = actors.spawn_actor_from_class(unreal.StaticMeshActor, unreal.Vector(0, 0, 0), unreal.Rotator(0, 0, 0))
        component = copy.static_mesh_component
        component.set_mobility(unreal.ComponentMobility.MOVABLE)
        component.set_static_mesh(shape.static_mesh)
        for index in range(shape.get_num_materials()):
            component.set_material(index, shape.get_material(index))
        copy.set_actor_transform(shape.get_world_transform(), False, False)
        copies.append(copy)

    options = unreal.MergeStaticMeshActorsOptions()
    options.set_editor_property("base_package_name", base_path)
    options.set_editor_property("destroy_source_actors", False)
    options.set_editor_property("spawn_merged_actor", False)
    settings = options.get_editor_property("mesh_merging_settings")
    # pivot à l'origine du monde (= Root de l'arme posée à l'origine)
    pivot_enum = type(settings.get_editor_property("pivot_type"))
    members = [m for m in dir(pivot_enum) if m.isupper()]
    pivot = next((m for m in members if "ZERO" in m or "ORIGIN" in m), None)
    if pivot:
        settings.set_editor_property("pivot_type", getattr(pivot_enum, pivot))
    elif "CUSTOM" in members:
        settings.set_editor_property("pivot_type", pivot_enum.CUSTOM)
        settings.set_editor_property("custom_pivot_location", unreal.Vector(0, 0, 0))
    log("  pivot : %s (choix %s)" % (settings.get_editor_property("pivot_type"), members))
    for prop, value in (("merge_physics_data", False), ("generate_light_map_uv", False), ("merge_materials", False)):
        try:
            settings.set_editor_property(prop, value)
        except Exception as error:
            log("  réglage %s ignoré : %s" % (prop, error))
    options.set_editor_property("mesh_merging_settings", settings)

    result = meshes.merge_static_mesh_actors(copies, options)
    for copy in copies:
        copy.destroy_actor()
    actor.destroy_actor()

    merged = unreal.load_asset(merged_path)
    if not merged:
        log("ERREUR %s : fusion ratée (%s)" % (name, result))
        continue
    merged.set_editor_property("light_map_resolution", 4)
    assets.save_asset(merged_path, only_if_is_dirty=False)
    cdo.set_editor_property("merged_mesh", merged)
    assets.save_asset(bp_path, only_if_is_dirty=False)
    log("%s : %d forme(s) fusionnée(s) -> %s (%d section(s) de matériau)" % (name, len(shapes), merged_path, len(merged.static_materials)))

open(unreal.Paths.project_saved_dir() + "Logs/merge_foe_weapons.txt", "w", encoding="utf-8").write("\n".join(report))
unreal.SystemLibrary.quit_editor()
