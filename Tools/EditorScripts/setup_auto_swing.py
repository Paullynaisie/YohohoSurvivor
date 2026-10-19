"""
Auto swing (ex Coup d'sabre, classe UYSAutoSwingAbility ; redirection YSCoupDSabreAbility -> YSAutoSwingAbility dans DefaultEngine.ini) :
  - BP_Epee : nom affiché « Auto swing », Melee System = Swing ; BP_BattleSword (enfant) recompilé
  - les deux BP sont réenregistrés avec le nouveau nom de classe ; les réglages existants (dégâts, recharge, poses) sont conservés

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_auto_swing.py"
Rapport : Saved/Logs/setup_auto_swing.txt
"""
import os
import unreal

SWORD_BP = "/Game/Placeable/Weapons/BP_Epee"
BATTLE_BP = "/Game/Placeable/Weapons/BP_BattleSword"

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_auto_swing] {msg}")
    report.append(msg)


def swing_components(bp):
    """composants de mêlée du Blueprint (hérités compris)"""
    found = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if isinstance(obj, unreal.YSAutoSwingAbility):
            found.append(obj)
    return found


def describe(obj):
    return (f"{obj.get_name()} ({obj.get_class().get_name()}) « {obj.get_editor_property('display_name')} » "
            f"dégâts {obj.get_editor_property('damage')}, Auto Atk CD {obj.get_editor_property('auto_atk_cooldown')} s, "
            f"recharge {obj.get_editor_property('cooldown')} s, retour {obj.get_editor_property('recover_duration')} s, "
            f"akimbo {obj.get_editor_property('akimbo_fire_delay')} s, melee {obj.get_editor_property('melee_system')}")


try:
    for path in [SWORD_BP, BATTLE_BP]:
        bp = assets.load_asset(path)
        comps = swing_components(bp)
        if not comps:
            raise RuntimeError(f"aucun composant Auto swing / Dashing Slash dans {path} (redirection ?)")
        for obj in comps:
            log(f"avant : {path} -> {describe(obj)}")

    # nom et catégorie de l'Auto swing de l'épée (hérités par la BattleSword)
    sword = assets.load_asset(SWORD_BP)
    for obj in swing_components(sword):
        if not isinstance(obj, unreal.YSDashingSlashAbility):
            obj.set_editor_property("display_name", "Auto swing")
            obj.set_editor_property("melee_system", unreal.YSMeleeSystem.SWING)

    for path in [SWORD_BP, BATTLE_BP]:
        bp = assets.load_asset(path)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assets.save_asset(path, only_if_is_dirty=False)

    for path in [SWORD_BP, BATTLE_BP]:
        for obj in swing_components(assets.load_asset(path)):
            log(f"après : {path} -> {describe(obj)}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_auto_swing.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
