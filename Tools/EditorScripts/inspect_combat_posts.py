"""
Lecture seule (2026-11-10) : composants des objets postes de combat transportables et des postes du bateau.
  UnrealEditor-Cmd.exe YohohoSurvivor.uproject -run=pythonscript -script="<projet>/Tools/EditorScripts/inspect_combat_posts.py" -unattended
Rapport : Saved/Logs/inspect_combat_posts.txt
"""
import os
import unreal

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    report.append(str(msg))


def fmt(v):
    return f"({v.x:.1f}, {v.y:.1f}, {v.z:.1f})"


def rot(r):
    return f"(R {r.roll:.1f}, P {r.pitch:.1f}, Y {r.yaw:.1f})"


def dump(path):
    bp = assets.load_asset(path)
    log(f"== {path}")
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(handle)
        obj = lib.get_object(data)
        if not obj:
            continue
        name = obj.get_name().replace("_GEN_VARIABLE", "")
        inherited = lib.is_inherited_component(data)
        line = f"  {name} [{obj.get_class().get_name()}]{' (hérité)' if inherited else ''}"
        if isinstance(obj, unreal.SceneComponent):
            parent = obj.get_attach_parent()
            line += f" parent={parent.get_name().replace('_GEN_VARIABLE', '') if parent else '-'}"
            line += f" loc={fmt(obj.get_editor_property('relative_location'))} rot={rot(obj.get_editor_property('relative_rotation'))} scale={fmt(obj.get_editor_property('relative_scale3d'))}"
        if isinstance(obj, unreal.StaticMeshComponent):
            mesh = obj.get_editor_property("static_mesh")
            mat = obj.get_material(0)
            line += f" mesh={mesh.get_name() if mesh else '-'} mat={mat.get_name() if mat else '-'} coll={obj.get_collision_profile_name()} vis={obj.get_editor_property('visible')}"
        log(line)


try:
    for p in ("/Game/Placeable/InteractiveObjects/CombatPosts/BP_CombatPostItem_Cannon",
              "/Game/Placeable/InteractiveObjects/CombatPosts/BP_CombatPostItem_Gatling",
              "/Game/Placeable/InteractiveObjects/CombatPosts/BP_CombatPostItem_Mortar",
              "/Game/Placeable/InteractiveObjects/CombatPosts/BP_CombatPostItem_GrapeShot",
              "/Game/Placeable/Ship/Stations/BP_ShipCannon",
              "/Game/Placeable/Ship/Stations/BP_ShipGatling",
              "/Game/Placeable/Ship/Stations/BP_ShipMortar",
              "/Game/Placeable/Ship/Stations/BP_ShipGrapeShot",
              "/Game/Placeable/Ship/Stations/BP_CombatPostReceptacle"):
        dump(p)
    shell = unreal.get_default_object(assets.load_blueprint_class("/Game/Placeable/AMMO/BP_MortarShell"))
    log(f"MortarShell ExplosionRadius={shell.get_editor_property('ExplosionRadius')}")
    g = unreal.get_default_object(assets.load_blueprint_class("/Game/Placeable/Ship/Stations/BP_ShipGatling"))
    for k in ("ShotsPerBurst", "BurstInterval", "Damage", "Cooldown"):
        log(f"Gatling {k}={g.get_editor_property(k)}")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "inspect_combat_posts.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
