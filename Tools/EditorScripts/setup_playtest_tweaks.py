"""
Réglages d'avant playtest (demandes de Paul) :
  - Cooldown 10 s pour TOUTES les Abilities (actions de clic) de toutes les armes de /Game/Placeable/Weapons (pas les attaques auto :
    Auto swing, Auto shot)
  - Quartermaster : plus unique (plusieurs joueurs peuvent le prendre)
  - MorningStar : hitbox de l'Auto swing active en permanence (Hitbox Always Active)
  - /Game/Core/Upgrades/DA_Upgrade_Yolo (« Yoloooooo », MorningStar seulement, poids copiés de DA_Upgrade_Mine), à la fin du pool du
    Quartermaster
Relançable : ne recrée pas l'upgrade si elle existe.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_playtest_tweaks.py"
Rapport : Saved/Logs/setup_playtest_tweaks.txt
"""
import os
import unreal

WEAPONS_ROOT = "/Game/Placeable/Weapons"
STAR = "/Game/Placeable/Weapons/Player/BP_MorningStar"
QM = "/Game/Placeable/Character/Player/PlayerCharacter_Quartermaster"
UPGRADES_DIR = "/Game/Core/Upgrades"
MINE_UPGRADE = f"{UPGRADES_DIR}/DA_Upgrade_Mine"
YOLO_NAME = "DA_Upgrade_Yolo"
YOLO = f"{UPGRADES_DIR}/{YOLO_NAME}"
ABILITY_COOLDOWN = 10.0
# attaques auto (pas des Abilities)
AUTO_CLASSES = (unreal.YSAutoSwingAbility, unreal.YSAutoShotAbility)

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_playtest_tweaks] {msg}")
    report.append(msg)


def ability_components(bp):
    """(nom de variable, template) des composants d'action du Blueprint"""
    result = []
    for h in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(h)
        obj = lib.get_object(data)
        if isinstance(obj, unreal.YSAbilityComponent) and all(o is not obj for _, o in result):
            result.append((str(lib.get_variable_name(data)), obj))
    return result


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


try:
    # 1) Cooldown des Abilities
    for path in sorted(str(p).split(".")[0] for p in assets.list_assets(WEAPONS_ROOT, recursive=True)):
        bp = assets.load_asset(path)
        if not isinstance(bp, unreal.Blueprint):
            continue
        changed = []
        for name, action in ability_components(bp):
            if type(action) in AUTO_CLASSES:
                continue
            # template du composant (Blueprint qui le déclare) + archétype vu par le CDO (composant hérité d'un Blueprint parent)
            targets = [action]
            try:
                archetype = cdo(path).get_editor_property(name)
                if isinstance(archetype, unreal.YSAbilityComponent) and archetype is not action:
                    targets.append(archetype)
            except Exception:
                pass
            for target in targets:
                before = target.get_editor_property("cooldown")
                if abs(before - ABILITY_COOLDOWN) > 0.001:
                    target.set_editor_property("cooldown", ABILITY_COOLDOWN)
                    changed.append(f"{name} ({type(action).__name__}) {before} -> {ABILITY_COOLDOWN}")
        if changed:
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            assets.save_asset(path, only_if_is_dirty=False)
            log(f"{path.split('/')[-1]} : " + ", ".join(changed))

    # 2) Quartermaster plus unique
    qm = cdo(QM)
    if qm.get_editor_property("unique_character"):
        qm.set_editor_property("unique_character", False)
        assets.save_asset(QM, only_if_is_dirty=False)
    log(f"Quartermaster unique : {cdo(QM).get_editor_property('unique_character')}")

    # 3) MorningStar : hitbox permanente
    star = assets.load_asset(STAR)
    for name, action in ability_components(star):
        if type(action) == unreal.YSAutoSwingAbility:
            action.set_editor_property("hitbox_always_active", True)
            log(f"BP_MorningStar.{name} : Hitbox Always Active")
    unreal.BlueprintEditorLibrary.compile_blueprint(star)
    assets.save_asset(STAR, only_if_is_dirty=False)

    # 4) Yoloooooo
    if not assets.does_asset_exist(YOLO):
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.YSYoloUpgrade)
        yolo = tools.create_asset(YOLO_NAME, UPGRADES_DIR, unreal.YSYoloUpgrade, factory)
        mine = assets.load_asset(MINE_UPGRADE)
        for prop in ("unlock_weight", "bonus_weight", "super_weight", "max_bonus_stacks"):
            yolo.set_editor_property(prop, mine.get_editor_property(prop))
        yolo.set_editor_property("required_weapon_classes", [assets.load_blueprint_class(STAR)])
        assets.save_asset(YOLO, only_if_is_dirty=False)
        log(f"créé : {YOLO} (MorningStar, poids de DA_Upgrade_Mine)")
    yolo = assets.load_asset(YOLO)
    qm = cdo(QM)
    pool = list(qm.get_editor_property("upgrade_pool"))
    if yolo not in pool:
        pool.append(yolo)
        qm.set_editor_property("upgrade_pool", pool)
        assets.save_asset(QM, only_if_is_dirty=False)
    log(f"Yoloooooo : index {pool.index(yolo)} du pool du Quartermaster, armes "
        + ", ".join(c.get_name() for c in yolo.get_editor_property("required_weapon_classes") if c))

    # relecture des recharges
    for path in sorted(str(p).split(".")[0] for p in assets.list_assets(WEAPONS_ROOT, recursive=True)):
        bp = assets.load_asset(path)
        if isinstance(bp, unreal.Blueprint):
            for name, action in ability_components(bp):
                if type(action) not in AUTO_CLASSES:
                    log(f"relu {path.split('/')[-1]}.{name} : {type(action).__name__} recharge {action.get_editor_property('cooldown')}")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_playtest_tweaks.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
