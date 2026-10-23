"""
Sniper : l'Ability BombThrower joue l'animation de lancer du NME bomb throw.
  - copie les réglages Throw Anim du composant BombThrow de BP_NME_Bomb sur le composant BombThrower de BP_Sniper
  - bombe dans la main = Mesh de BP_NME_Bomb (mesh, matériau, échelle, position)

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_sniper_throw_anim.py"
Rapport : Saved/Logs/setup_sniper_throw_anim.txt
"""
import os
import unreal

NME_BOMB_BP = "/Game/Placeable/Weapons/Foe/BP_NME_Bomb"
SNIPER_BP = "/Game/Placeable/Weapons/Player/BP_Sniper"
ANIM_PROPS = ["throw_windup_duration", "throw_duration", "throw_recover_duration", "windup_hand_location", "windup_hand_rotation",
              "release_hand_location", "release_hand_rotation", "throw_ease_exponent"]

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_sniper_throw_anim] {msg}")
    report.append(msg)


def templates(bp):
    result = []
    for h in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(h))
        if obj and obj not in result:
            result.append(obj)
    return result


def component(path, cls):
    return next((t for t in templates(assets.load_asset(path)) if isinstance(t, cls)), None)


try:
    src = component(NME_BOMB_BP, unreal.YSFoeBombAction)
    dst = component(SNIPER_BP, unreal.YSBombThrowerAbility)
    if not src or not dst:
        raise RuntimeError(f"composant introuvable : NME bomb throw {src}, BombThrower {dst}")

    for name in ANIM_PROPS:
        dst.set_editor_property(name, src.get_editor_property(name))
        log(f"{name} = {dst.get_editor_property(name)}")

    # bombe dans la main : le Mesh de l'arme du NME
    nme_bomb = unreal.get_default_object(assets.load_blueprint_class(NME_BOMB_BP))
    mesh = nme_bomb.get_editor_property("mesh")
    for t in templates(assets.load_asset(NME_BOMB_BP)):
        if isinstance(t, unreal.StaticMeshComponent):
            log(f"  forme de BP_NME_Bomb : {t.get_name()} {t.get_editor_property('static_mesh')} loc {t.get_editor_property('relative_location')} "
                f"échelle {t.get_editor_property('relative_scale3d')}")
    actor_scale = nme_bomb.get_editor_property("root_component").get_editor_property("relative_scale3d")
    scale = mesh.get_editor_property("relative_scale3d")
    dst.set_editor_property("held_bomb_mesh", mesh.get_editor_property("static_mesh"))
    materials = mesh.get_materials()
    dst.set_editor_property("held_bomb_material", materials[0] if materials else None)
    dst.set_editor_property("held_bomb_scale", unreal.Vector(scale.x * actor_scale.x, scale.y * actor_scale.y, scale.z * actor_scale.z))
    loc = mesh.get_editor_property("relative_location")
    dst.set_editor_property("held_bomb_offset", unreal.Vector(loc.x * actor_scale.x, loc.y * actor_scale.y, loc.z * actor_scale.z))
    log(f"position de la bombe : {dst.get_editor_property('held_bomb_offset')}")
    log(f"bombe en main : {dst.get_editor_property('held_bomb_mesh')}, {dst.get_editor_property('held_bomb_material')}, "
        f"échelle {dst.get_editor_property('held_bomb_scale')}")

    assets.save_asset(SNIPER_BP, only_if_is_dirty=False)
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_sniper_throw_anim.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
