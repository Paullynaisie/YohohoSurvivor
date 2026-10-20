"""
Balle de l'Auto shot (Auto atk des Guns) différente de celle du Rolling shot :
  - /Game/Placeable/AMMO/PetoirAMMO_AutoShot : enfant de PetoirAMMO, mesh 35 % plus fin (échelle x 0.65),
    matériau émissif m_PH_emissive_turboRed (collision et réglages AMMO hérités de PetoirAMMO)
  - chaque composant Auto shot (UYSAutoShotAbility) des armes de /Game/Placeable/Weapons tire cette balle ;
    le Rolling shot (et le Coup d'Petoir) gardent PetoirAMMO
Ne recrée pas la balle si elle existe (les réglages de Paul sont conservés).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_auto_shot_ammo.py"
Rapport : Saved/Logs/setup_auto_shot_ammo.txt
"""
import os
import unreal

AMMO_DIR = "/Game/Placeable/AMMO"
BASE_AMMO = f"{AMMO_DIR}/PetoirAMMO"
NAME = "PetoirAMMO_AutoShot"
AUTO_AMMO = f"{AMMO_DIR}/{NAME}"
WEAPON_DIR = "/Game/Placeable/Weapons"
MATERIAL = "/Game/Art/Materials/PH_materials/m_PH_emissive_turboRed"
THINNER = 0.65  # 35 % plus fine

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_auto_shot_ammo] {msg}")
    report.append(msg)


def components(bp):
    result = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if obj:
            result.append(obj)
    return result


try:
    base_mesh = unreal.get_default_object(assets.load_blueprint_class(BASE_AMMO)).get_editor_property("mesh")
    base_scale = base_mesh.get_editor_property("relative_scale3d")
    log(f"PetoirAMMO : mesh {base_mesh.get_editor_property('static_mesh').get_name()}, échelle {base_scale}, matériau {base_mesh.get_material(0).get_name() if base_mesh.get_material(0) else None}")

    if not assets.does_asset_exist(AUTO_AMMO):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", assets.load_blueprint_class(BASE_AMMO))
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, AMMO_DIR, unreal.Blueprint, factory)
        bp = assets.load_asset(AUTO_AMMO)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        mesh = unreal.get_default_object(assets.load_blueprint_class(AUTO_AMMO)).get_editor_property("mesh")
        mesh.set_editor_property("relative_scale3d", base_scale * THINNER)
        mesh.set_material(0, assets.load_asset(MATERIAL))
        assets.save_asset(AUTO_AMMO, only_if_is_dirty=False)
        log(f"créé : {AUTO_AMMO}")
    else:
        log(f"existe déjà : {AUTO_AMMO}")

    auto_class = assets.load_blueprint_class(AUTO_AMMO)
    for path in assets.list_assets(WEAPON_DIR, recursive=True, include_folder=False):
        bp = assets.load_asset(path)
        if not isinstance(bp, unreal.Blueprint):
            continue
        changed = False
        for obj in components(bp):
            if isinstance(obj, unreal.YSAutoShotAbility) and obj.get_editor_property("projectile_class") != auto_class:
                obj.set_editor_property("projectile_class", auto_class)
                changed = True
                log(f"{bp.get_name()}.{obj.get_name()} : projectile -> {NAME}")
        if changed:
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            assets.save_asset(path.split(".")[0], only_if_is_dirty=False)

    # relecture
    mesh = unreal.get_default_object(assets.load_blueprint_class(AUTO_AMMO)).get_editor_property("mesh")
    log(f"relu : {NAME} échelle {mesh.get_editor_property('relative_scale3d')}, matériau {mesh.get_material(0).get_name() if mesh.get_material(0) else None}")
    for path in assets.list_assets(WEAPON_DIR, recursive=True, include_folder=False):
        bp = assets.load_asset(path)
        if isinstance(bp, unreal.Blueprint):
            for obj in components(bp):
                if isinstance(obj, unreal.YSCoupDPetoirAbility):
                    projectile = obj.get_editor_property("projectile_class")
                    log(f"relu : {bp.get_name()}.{obj.get_name()} ({obj.get_class().get_name()}) -> {projectile.get_name() if projectile else None}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_auto_shot_ammo.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
