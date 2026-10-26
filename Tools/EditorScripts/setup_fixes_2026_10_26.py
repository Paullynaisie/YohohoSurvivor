"""
Fixes du 2026-10-26 :
  - Skull Gun : imprécision du tir (Aim Spread de l'action « NME shot » de BP_NME_Pistol) = 30° (+/- 30°)
  - Sniper : visée assistée de la « Roll, bomb and shot » (Assist Aim Angle) = 1°
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
SNIPER_BP_NAME = "BP_Sniper"
ROLL_BOMB_SHOT_ASSIST_ANGLE = 1.0


def log(msg):
    unreal.log(f"[setup_fixes_2026_10_26] {msg}")
    report.append(msg)


def find_blueprint(name, root="/Game/Placeable"):
    """chemin du Blueprint de ce nom dans les sous-dossiers (Paul range les assets)"""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for data in registry.get_assets_by_path(root, recursive=True):
        if str(data.asset_name) == name:
            return str(data.package_name)
    return None


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

SNIPER_BP = find_blueprint(SNIPER_BP_NAME)
bp = assets.load_asset(SNIPER_BP) if SNIPER_BP else None
if not bp:
    log(f"ERREUR : {SNIPER_BP} introuvable")
else:
    found = False
    for component in components(bp):
        if isinstance(component, unreal.YSRollBombShotAbility):
            found = True
            old = component.get_editor_property("AssistAimAngle")
            component.set_editor_property("AssistAimAngle", ROLL_BOMB_SHOT_ASSIST_ANGLE)
            log(f"{component.get_name()} : Assist Aim Angle {old} -> {ROLL_BOMB_SHOT_ASSIST_ANGLE}")
    if found:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assets.save_asset(SNIPER_BP, only_if_is_dirty=False)
        log("BP_Sniper enregistré")
    else:
        log("ERREUR : aucune Roll, bomb and shot dans BP_Sniper")

path = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_fixes_2026_10_26.txt")
with open(path, "w", encoding="utf-8") as f:
    f.write("\n".join(report) + "\n")
unreal.SystemLibrary.quit_editor()
