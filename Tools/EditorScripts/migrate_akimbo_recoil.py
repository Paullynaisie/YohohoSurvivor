"""
Les réglages Akimbo / Recoil passent des armes (AYSWeapon) à leurs actions : reporte les valeurs que Paul avait
réglées sur les armes (relevées avant la migration) sur les composants d'action des BP d'armes.
Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/migrate_akimbo_recoil.py"
Rapport : Saved/Logs/migrate_akimbo_recoil.txt
"""
import os
import unreal

WEAPONS = "/Game/Placeable/Weapons"
PISTOL = {"akimbo_fire_delay": 0.15, "recoil_pitch": 90.0, "recoil_kick_back": 50.0,
          "recoil_kick_duration": 0.05, "recoil_recover_duration": 0.5}
# (BP, nom du composant ajouté dans ce BP, valeurs)
TARGETS = [
    (f"{WEAPONS}/BP_Pistolet", "CoupDPetoir", PISTOL),
    (f"{WEAPONS}/BP_BattlePetoir", "AutoShot", PISTOL),
    (f"{WEAPONS}/BP_Epee", "CoupDSabre", {"akimbo_fire_delay": 0.25}),
]

subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[migrate_akimbo_recoil] {msg}")
    report.append(msg)


try:
    for path, component, values in TARGETS:
        bp = unreal.EditorAssetLibrary.load_asset(path)
        template = None
        for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
            obj = lib.get_object(lib.get_data(handle))
            # composant ajouté dans CE BP (pas celui d'un parent)
            if isinstance(obj, unreal.YSAbilityComponent) and obj.get_name().startswith(component) \
                    and obj.get_outer().get_name() == f"{bp.get_name()}_C":
                template = obj
        if template is None:
            log(f"ERREUR : {component} introuvable dans {path}")
            continue
        template.modify()
        for prop, value in values.items():
            template.set_editor_property(prop, value)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
        check = ", ".join(f"{p}={template.get_editor_property(p):.3f}" for p in values)
        log(f"{path}.{component} : {check}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "migrate_akimbo_recoil.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
