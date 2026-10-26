"""
Fixes du 2026-10-26 :
  - Skull Gun : imprécision du tir (Aim Spread de l'action « NME shot » de BP_NME_Pistol) = 30° (+/- 30°)
Relançable.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_fixes_2026_10_26.py"
Rapport : Saved/Logs/setup_fixes_2026_10_26.txt
"""
import os
import unreal

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []

NME_PISTOL_BP = "/Game/Placeable/Weapons/Foe/BP_NME_Pistol"
AIM_SPREAD = 30.0


def log(msg):
    unreal.log(f"[setup_fixes_2026_10_26] {msg}")
    report.append(msg)


def components(bp):
    return [lib.get_object(lib.get_data(h)) for h in subsystem.k2_gather_subobject_data_for_blueprint(bp) if lib.get_object(lib.get_data(h))]


bp = assets.load_asset(NME_PISTOL_BP)
if not bp:
    log(f"ERREUR : {NME_PISTOL_BP} introuvable")
else:
    found = False
    for component in components(bp):
        if isinstance(component, unreal.YSFoeShotAction):
            found = True
            old = component.get_editor_property("AimSpread")
            component.set_editor_property("AimSpread", AIM_SPREAD)
            log(f"{component.get_name()} : Aim Spread {old} -> {AIM_SPREAD}")
    if found:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assets.save_asset(NME_PISTOL_BP, only_if_is_dirty=False)
        log("BP_NME_Pistol enregistré")
    else:
        log("ERREUR : aucune action NME shot dans BP_NME_Pistol")

path = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_fixes_2026_10_26.txt")
with open(path, "w", encoding="utf-8") as f:
    f.write("\n".join(report) + "\n")
unreal.SystemLibrary.quit_editor()
