"""
Réglages du 2026-10-25 (soir) :
  - XP par niveau : Level Up Experience Per Level 7 (au lieu de 5) sur BP_TestGameMode et BP_TestMapGameMode
  - minions du Captain : dégâts de l'attaque auto du minion gun (AutoShot de BP_MinionAutoGun) = ceux du minion sword
Relançable.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="<chemin absolu>/Tools/EditorScripts/setup_minion_xp_tweaks.py"
Rapport : Saved/Logs/setup_minion_xp_tweaks.txt
"""
import os
import unreal

GAME_MODES = ["/Game/Core/GameModes/BP_TestGameMode", "/Game/Core/GameModes/BP_TestMapGameMode"]
XP_PER_LEVEL = 7.0

assets = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
report = []


def log(msg):
    unreal.log(f"[setup_minion_xp_tweaks] {msg}")
    report.append(msg)


def find_path(name):
    for data in registry.get_assets_by_path("/Game/Placeable", recursive=True):
        if str(data.asset_name) == name:
            return str(data.package_name)
    raise RuntimeError(f"introuvable : {name}")


def component(path, cls):
    """Composant du Blueprint de cette classe (le sien ou hérité : modifiable via le CDO par son nom de variable)"""
    bp = assets.load_asset(path)
    cdo = unreal.get_default_object(assets.load_blueprint_class(path))
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    lib = unreal.SubobjectDataBlueprintFunctionLibrary
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(handle)
        obj = lib.get_object(data)
        # attaque auto seulement (le Dashing Slash est une sous-classe de l'Auto swing)
        if isinstance(obj, cls) and not isinstance(obj, unreal.YSDashingSlashAbility):
            name = str(lib.get_variable_name(data))
            try:
                value = cdo.get_editor_property(name)
            except Exception:
                value = None
            return value or obj, name
    raise RuntimeError(f"aucun composant {cls.__name__} dans {path}")


try:
    for path in GAME_MODES:
        cdo = unreal.get_default_object(assets.load_blueprint_class(path))
        old = cdo.get_editor_property("level_up_experience_per_level")
        cdo.set_editor_property("level_up_experience_per_level", XP_PER_LEVEL)
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"{path.split('/')[-1]} : XP par niveau {old} -> {XP_PER_LEVEL}")

    sword_path = find_path("BP_MinionSword")
    gun_path = find_path("BP_MinionAutoGun")
    sword, sword_name = component(sword_path, unreal.YSAutoSwingAbility)
    gun, gun_name = component(gun_path, unreal.YSAutoShotAbility)
    damage = sword.get_editor_property("damage")
    old = gun.get_editor_property("damage")
    gun.set_editor_property("damage", damage)
    assets.save_asset(gun_path, only_if_is_dirty=False)
    reread, _ = component(gun_path, unreal.YSAutoShotAbility)
    log(f"BP_MinionAutoGun.{gun_name} : dégâts {old} -> {reread.get_editor_property('damage')} (= {sword_name} de BP_MinionSword)")
    log("OK")
except Exception as error:
    import traceback
    log(f"ERREUR : {error}\n{traceback.format_exc()}")

with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_minion_xp_tweaks.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
