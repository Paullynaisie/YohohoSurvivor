# Retakes de Paul (2026-11-13) sur les upgrades du Swordsman. Relançable (valeurs absolues) :
# - Lesser Spinning Blade 1.5x plus rapide (= Spinning Sword x4.5) ;
# - Blade Throw : pas en arrière 2x plus long sans changer la distance (x4 / vitesse x0.5), aller / retour 0.5 s, Swordsrang 2 s ;
# - Mark for Delete : roulades et visées 2x plus courtes (Rolling shot de BP_Pistolet / 2) ;
# - Pistolet de The Gun was better : tire tout seul, sans le clic gauche.
import os
import unreal

assets = unreal.EditorAssetLibrary
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary

PLAYER = "/Game/Placeable/Weapons/Player"
report = []


def log(msg):
    report.append(str(msg))
    unreal.log(f"[setup_retakes_swordsman] {msg}")


def component(path, name):
    bp = assets.load_asset(path)
    for h in sds.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(h)
        if str(lib.get_variable_name(data)) == name:
            return bp, lib.get_object(data)
    return bp, None


def save(bp, path):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(path, only_if_is_dirty=False)


try:
    spinning = unreal.get_default_object(assets.load_blueprint_class(f"{PLAYER}/BP_SpinningSword"))
    lesser_bp = assets.load_asset(f"{PLAYER}/BP_LesserSpinningBlade")
    lesser = unreal.get_default_object(lesser_bp.generated_class())
    lesser.set_editor_property("rotation_speed", spinning.get_editor_property("rotation_speed") * 4.5)
    save(lesser_bp, f"{PLAYER}/BP_LesserSpinningBlade")
    log(f"Lesser : rotation {lesser.get_editor_property('rotation_speed')}")

    _, rolling = component(f"{PLAYER}/Wp_Guns/BP_Pistolet", "RollingShot")
    for path in (f"{PLAYER}/Wp_Melee/BP_BattleSword", f"{PLAYER}/Wp_Melee/BP_GunSword"):
        bp, ability = component(path, "DashingSlash")
        ability.set_editor_property("blade_throw_step_back_multiplier", 4.0)
        ability.set_editor_property("blade_throw_step_back_speed_scale", 0.5)
        ability.set_editor_property("blade_throw_duration", 0.5)
        ability.set_editor_property("swordsrang_duration", 2.0)
        ability.set_editor_property("roll_duration", rolling.get_editor_property("roll_duration") * 0.5)
        ability.set_editor_property("aim_duration", rolling.get_editor_property("aim_duration") * 0.5)
        save(bp, path)
        log(f"{path} : pas en arrière x{ability.get_editor_property('blade_throw_step_back_multiplier')} vitesse x{ability.get_editor_property('blade_throw_step_back_speed_scale')}, "
            f"aller {ability.get_editor_property('blade_throw_duration')} s, Swordsrang {ability.get_editor_property('swordsrang_duration')} s, "
            f"roulade {ability.get_editor_property('roll_duration')} s, visée {ability.get_editor_property('aim_duration')} s")

    bp, auto_shot = component(f"{PLAYER}/Wp_Guns/BP_GunPistol", "AutoShot")
    auto_shot.set_editor_property("auto_attack_without_click", True)
    save(bp, f"{PLAYER}/Wp_Guns/BP_GunPistol")
    log(f"BP_GunPistol.AutoShot : sans clic {auto_shot.get_editor_property('auto_attack_without_click')}")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_retakes_swordsman.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
