"""
Minions du Captain comme les joueurs : /Game/Placeable/Character/Summon/CaptainMinion reçoit la Head Feature de Mouss
(calotte « Head Feature » + nœud « Sphere » + pans du bandana « Bandana » / « Bandana1 », copiés de PlayerCharacter_mouss).
En jeu : corps blanc, Head Feature de la couleur du Captain (Customization Color Component Name = Head Feature, défaut C++),
même visage que le Captain, armes de leur couleur de base (Weapon Coloration faux, défaut C++ du minion), sortie du sol en 0.5 s.
Ne recrée pas la Head Feature si elle existe déjà (retouches de Paul conservées).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_minion_head_feature.py"
Rapport : Saved/Logs/setup_minion_head_feature.txt
"""
import os
import unreal

MOUSS = "/Game/Placeable/Character/Player/PlayerCharacter_mouss"
MINION = "/Game/Placeable/Character/Summon/CaptainMinion"
HEAD = "Head Feature"

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
ys = unreal.YSEditorScriptLibrary
report = []


def log(msg):
    unreal.log(f"[setup_minion_head_feature] {msg}")
    report.append(msg)


def handles(bp):
    return subsystem.k2_gather_subobject_data_for_blueprint(bp)


def find_handle(bp, name):
    for h in handles(bp):
        if str(lib.get_variable_name(lib.get_data(h))) == name:
            return h
    return None


def children_of(bp, parent_name):
    """Composants (nom, gabarit) dont le parent est parent_name, sans doublon"""
    result = []
    for h in handles(bp):
        data = lib.get_data(h)
        parent = lib.get_data(lib.get_parent_handle(data))
        name = str(lib.get_variable_name(data))
        if str(lib.get_variable_name(parent)) == parent_name and name not in [n for n, _ in result]:
            result.append((name, lib.get_object(data)))
    return result


def copy_component(bp, source, name, parent_handle):
    handle, fail = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent_handle, new_class=source.get_class(), blueprint_context=bp))
    if not fail.is_empty():
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    subsystem.rename_subobject(handle, unreal.Text(name))
    comp = lib.get_object(lib.get_data(handle))
    for prop in ("relative_location", "relative_rotation", "relative_scale3d"):
        comp.set_editor_property(prop, source.get_editor_property(prop))
    if isinstance(source, unreal.StaticMeshComponent):
        comp.set_editor_property("static_mesh", source.get_editor_property("static_mesh"))
        for slot, material in enumerate(source.get_editor_property("override_materials")):
            comp.set_material(slot, material)
        comp.set_collision_profile_name(source.get_collision_profile_name())
        comp.set_editor_property("receives_decals", source.get_editor_property("receives_decals"))
        comp.set_editor_property("cast_shadow", source.get_editor_property("cast_shadow"))
    copied = ys.copy_editable_properties(source, comp)
    log(f"  {name} ({source.get_class().get_name()}) copié, {copied} réglage(s) du jeu")
    return handle


try:
    mouss_bp = assets.load_asset(MOUSS)
    minion_bp = assets.load_asset(MINION)
    if find_handle(minion_bp, HEAD) is not None:
        log(f"{MINION} : {HEAD} existe déjà, rien à faire")
    else:
        head_source = next(obj for name, obj in children_of(mouss_bp, "BodyPivot") if name == HEAD)
        body_handle = find_handle(minion_bp, "BodyPivot")
        head_handle = copy_component(minion_bp, head_source, HEAD, body_handle)
        for name, obj in children_of(mouss_bp, HEAD):
            copy_component(minion_bp, obj, name, head_handle)
        unreal.BlueprintEditorLibrary.compile_blueprint(minion_bp)
        assets.save_asset(MINION, only_if_is_dirty=False)
        log(f"{MINION} : {HEAD} de Mouss ajoutée")

    # relecture
    minion_bp = assets.load_asset(MINION)
    log("relu : " + ", ".join(f"{n} ({o.get_class().get_name()})" for n, o in children_of(minion_bp, "BodyPivot")))
    log("relu sous Head Feature : " + ", ".join(n for n, _ in children_of(minion_bp, HEAD)))
    minion = unreal.get_default_object(assets.load_blueprint_class(MINION))
    for prop in ("customization_color_component_name", "weapon_coloration", "spawn_rise_duration"):
        log(f"relu {prop} = {minion.get_editor_property(prop)}")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_minion_head_feature.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
