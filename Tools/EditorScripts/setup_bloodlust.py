"""
Bloodlust (upgrade d'arme des épées, jumeau du Ricochet) :
  - /Game/Core/Upgrades/DA_Upgrade_Bloodlust (UYSBloodlustUpgrade : Bloodlust / Bloodlust Atk speed / Golden bloodlust)
  - jumeaux : DA_Upgrade_Ricochet <-> DA_Upgrade_Bloodlust (Twin Upgrade)
  - ajouté à l'Upgrade Pool de chaque personnage qui a le Ricochet (à la fin du pool : les index existants ne bougent pas)
  - /Game/Core/UI/WBP_BloodlustStacks (affichage des BL stacks, réglable) choisi par BP_YSPlayerController
Ne recrée pas ce qui existe déjà (les réglages de Paul sont conservés).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_bloodlust.py"
Rapport : Saved/Logs/setup_bloodlust.txt
"""
import os
import unreal

UPGRADES_DIR = "/Game/Core/Upgrades"
BLOODLUST = f"{UPGRADES_DIR}/DA_Upgrade_Bloodlust"
RICOCHET = f"{UPGRADES_DIR}/DA_Upgrade_Ricochet"
CHAR_DIR = "/Game/Placeable/Character"
UI_DIR = "/Game/Core/UI"
WIDGET = f"{UI_DIR}/WBP_BloodlustStacks"
PC_BP = "/Game/Core/Player/BP_YSPlayerController"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
registry = unreal.AssetRegistryHelpers.get_asset_registry()
report = []


def log(msg):
    unreal.log(f"[setup_bloodlust] {msg}")
    report.append(msg)


try:
    # asset d'upgrade
    if assets.does_asset_exist(BLOODLUST):
        bloodlust = assets.load_asset(BLOODLUST)
        log(f"existe déjà : {BLOODLUST}")
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.YSBloodlustUpgrade)
        bloodlust = tools.create_asset("DA_Upgrade_Bloodlust", UPGRADES_DIR, unreal.YSBloodlustUpgrade, factory)
        log(f"créé : {BLOODLUST}")

    # jumeaux
    ricochet = assets.load_asset(RICOCHET)
    bloodlust.set_editor_property("twin_upgrade", ricochet)
    ricochet.set_editor_property("twin_upgrade", bloodlust)
    assets.save_asset(BLOODLUST, only_if_is_dirty=False)
    assets.save_asset(RICOCHET, only_if_is_dirty=False)

    # pools : tous les personnages qui proposent le Ricochet
    for data in registry.get_assets_by_path(CHAR_DIR, recursive=True):
        if str(data.asset_class_path.asset_name) != "Blueprint":
            continue
        path = str(data.package_name)
        cls = assets.load_blueprint_class(path)
        cdo = unreal.get_default_object(cls) if cls else None
        if cdo is None or not isinstance(cdo, unreal.YSPlayerCharacter):
            continue
        pool = list(cdo.get_editor_property("upgrade_pool"))
        if ricochet in pool and bloodlust not in pool:
            pool.append(bloodlust)
            cdo.set_editor_property("upgrade_pool", pool)
            assets.save_asset(path, only_if_is_dirty=False)
            log(f"{path} : Bloodlust ajouté au pool (index {len(pool) - 1})")
        elif bloodlust in pool:
            log(f"{path} : Bloodlust déjà dans le pool (index {pool.index(bloodlust)})")

    # affichage des BL stacks
    if assets.does_asset_exist(WIDGET):
        log(f"existe déjà : {WIDGET}")
    else:
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", unreal.YSBloodlustStacksWidget)
        tools.create_asset("WBP_BloodlustStacks", UI_DIR, unreal.WidgetBlueprint, factory)
        log(f"créé : {WIDGET}")
    widget_bp = assets.load_asset(WIDGET)
    unreal.BlueprintEditorLibrary.compile_blueprint(widget_bp)
    assets.save_asset(WIDGET, only_if_is_dirty=False)

    pc_cdo = unreal.get_default_object(assets.load_blueprint_class(PC_BP))
    pc_cdo.set_editor_property("bloodlust_widget_class", assets.load_blueprint_class(WIDGET))
    assets.save_asset(PC_BP, only_if_is_dirty=False)

    # relecture
    check = assets.load_asset(BLOODLUST)
    log(f"relu : {check.get_name()} jumeau {check.get_editor_property('twin_upgrade').get_name()}, "
        f"{check.get_editor_property('max_stacks')} stacks, {check.get_editor_property('stack_duration')} s, "
        f"{check.get_editor_property('atk_speed_per_stack')} par stack, bonus {check.get_editor_property('bonus_silver_value')} / {check.get_editor_property('bonus_gold_value')}")
    log(f"relu : {assets.load_asset(RICOCHET).get_name()} jumeau {assets.load_asset(RICOCHET).get_editor_property('twin_upgrade').get_name()}")
    log(f"relu : BP_YSPlayerController -> {unreal.get_default_object(assets.load_blueprint_class(PC_BP)).get_editor_property('bloodlust_widget_class').get_name()}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_bloodlust.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
