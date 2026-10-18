"""
Rolling shot : remplace le Coup d'Petoir dans BP_Pistolet (et donc BP_BattlePetoir, son enfant).
Ajoute un composant « RollingShot » (UYSRollingShotAbility) qui reprend tous les réglages du Coup d'Petoir
(projectile, dégâts, recharge, visée assistée, shoot system, recul, rechargement, akimbo), puis retire le Coup d'Petoir.
Ne refait rien si le Rolling shot est déjà là.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_rolling_shot.py"
Rapport : Saved/Logs/setup_rolling_shot.txt
"""
import os
import unreal

PISTOL_BP = "/Game/Placeable/Weapons/BP_Pistolet"
CHILD_BP = "/Game/Placeable/Weapons/BP_BattlePetoir"
COPIED = ["cooldown", "akimbo_fire_delay", "icon", "icon_tint", "projectile_class", "damage", "assist_aim_distance",
          "assist_aim_angle", "shoot_system", "bullets_per_burst", "burst_duration", "recoil_pitch", "recoil_kick_back",
          "recoil_kick_duration", "recoil_recover_duration", "play_reload_anim", "reload_spin_duration",
          "reload_spin_turns", "reload_spin_axis", "reload_spin_ease_exponent"]

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_rolling_shot] {msg}")
    report.append(msg)


def actions(bp):
    """(handle, composant) des actions ajoutées dans ce Blueprint"""
    found = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if isinstance(obj, unreal.YSAbilityComponent) and obj.get_outer().get_name() == f"{bp.get_name()}_C":
            found.append((handle, obj))
    return found


try:
    pistol = assets.load_asset(PISTOL_BP)
    current = actions(pistol)
    if any(isinstance(obj, unreal.YSRollingShotAbility) for _, obj in current):
        log("Rolling shot déjà présent dans BP_Pistolet")
    else:
        old_handle, old = next((h, o) for h, o in current if type(o) is unreal.YSCoupDPetoirAbility)
        root = subsystem.k2_gather_subobject_data_for_blueprint(pistol)[0]
        params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.YSRollingShotAbility, blueprint_context=pistol)
        handle, fail = subsystem.add_new_subobject(params)
        if not lib.is_handle_valid(handle):
            raise RuntimeError(f"ajout du Rolling shot impossible : {fail}")
        subsystem.rename_subobject(handle, unreal.Text("RollingShot"))
        rolling = lib.get_object(lib.get_data(handle))
        for prop in COPIED:
            rolling.set_editor_property(prop, old.get_editor_property(prop))
        log("réglages copiés : " + ", ".join(f"{p}={old.get_editor_property(p)}" for p in ["damage", "cooldown", "akimbo_fire_delay", "assist_aim_angle", "bullets_per_burst"]))
        removed = subsystem.delete_subobject(root, old_handle, pistol)
        log(f"Coup d'Petoir retiré ({removed} composant(s))")
        unreal.BlueprintEditorLibrary.compile_blueprint(pistol)
        assets.save_asset(PISTOL_BP, only_if_is_dirty=False)

    # l'enfant recompile avec le nouveau composant hérité
    child = assets.load_asset(CHILD_BP)
    unreal.BlueprintEditorLibrary.compile_blueprint(child)
    assets.save_asset(CHILD_BP, only_if_is_dirty=False)

    # relecture
    for path in [PISTOL_BP, CHILD_BP]:
        bp = assets.load_asset(path)
        for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
            obj = lib.get_object(lib.get_data(handle))
            if isinstance(obj, unreal.YSAbilityComponent):
                log(f"relu : {path} -> {obj.get_name()} ({obj.get_class().get_name()}) dégâts {obj.get_editor_property('damage')}, "
                    f"recharge {obj.get_editor_property('cooldown')} s")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_rolling_shot.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
