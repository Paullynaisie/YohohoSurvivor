"""
Battle Petoir : /Game/Placeable/Weapons/BP_BattlePetoir, enfant de BP_Pistolet (Coup d'Petoir en Ability)
+ un composant « AutoShot » (UYSAutoShotAbility, Auto atk) : même projectile et même visée assistée que le
Coup d'Petoir du pistolet, 20 dégâts de base.
Ne recrée pas l'arme ni le composant s'ils existent (les réglages de Paul sont conservés).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_battle_petoir.py"
Rapport : Saved/Logs/setup_battle_petoir.txt
"""
import os
import unreal

WEAPON_DIR = "/Game/Placeable/Weapons"
PISTOL_BP = f"{WEAPON_DIR}/BP_Pistolet"
NAME = "BP_BattlePetoir"
BATTLE_BP = f"{WEAPON_DIR}/{NAME}"

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_battle_petoir] {msg}")
    report.append(msg)


def components(bp):
    """(handle, objet gabarit) de chaque composant du Blueprint (hérités compris)"""
    result = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if obj:
            result.append((handle, obj))
    return result


try:
    pistol = assets.load_asset(PISTOL_BP)
    petoir = next((obj for _, obj in components(pistol)
                   if isinstance(obj, unreal.YSCoupDPetoirAbility) and not isinstance(obj, unreal.YSAutoShotAbility)), None)
    if petoir is None:
        raise RuntimeError("pas de Coup d'Petoir dans BP_Pistolet")
    log(f"Coup d'Petoir du pistolet : projectile {petoir.get_editor_property('projectile_class')}, "
        f"visée {petoir.get_editor_property('assist_aim_distance')} cm / {petoir.get_editor_property('assist_aim_angle')}°")

    if assets.does_asset_exist(BATTLE_BP):
        battle = assets.load_asset(BATTLE_BP)
        log(f"existe déjà : {BATTLE_BP}")
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", assets.load_blueprint_class(PISTOL_BP))
        battle = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, WEAPON_DIR, unreal.Blueprint, factory)
        log(f"créé : {BATTLE_BP}")

    auto_shot = next((obj for _, obj in components(battle) if isinstance(obj, unreal.YSAutoShotAbility)), None)
    if auto_shot is None:
        root = subsystem.k2_gather_subobject_data_for_blueprint(battle)[0]
        params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.YSAutoShotAbility, blueprint_context=battle)
        handle, fail = subsystem.add_new_subobject(params)
        if not fail.is_empty():
            raise RuntimeError(f"ajout de l'Auto shot impossible : {fail}")
        subsystem.rename_subobject(handle, unreal.Text("AutoShot"))
        auto_shot = lib.get_object(lib.get_data(handle))
        auto_shot.set_editor_property("projectile_class", petoir.get_editor_property("projectile_class"))
        auto_shot.set_editor_property("assist_aim_distance", petoir.get_editor_property("assist_aim_distance"))
        auto_shot.set_editor_property("assist_aim_angle", petoir.get_editor_property("assist_aim_angle"))
        auto_shot.set_editor_property("damage", 20.0)
        log("Auto shot ajouté")

    unreal.BlueprintEditorLibrary.compile_blueprint(battle)
    weapon = unreal.get_default_object(assets.load_blueprint_class(BATTLE_BP))
    weapon.set_editor_property("display_name", "Battle Petoir")
    assets.save_asset(BATTLE_BP, only_if_is_dirty=False)

    # relecture
    check = assets.load_asset(BATTLE_BP)
    for _, obj in components(check):
        if isinstance(obj, unreal.YSAbilityComponent):
            log(f"relu : {obj.get_name()} ({obj.get_class().get_name()}) dégâts {obj.get_editor_property('damage')}, "
                f"recharge {obj.get_editor_property('cooldown')} s, projectile {obj.get_editor_property('projectile_class')}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_battle_petoir.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
