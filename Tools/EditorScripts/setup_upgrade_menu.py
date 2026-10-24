"""
Menu de montée de niveau en cartes + écran de sélection en 4 colonnes :
  - /Game/Core/UI/WBP_UpgradeMenu : Widget Blueprint enfant de YSUpgradeMenuWidget (arbre construit en C++,
    placements dans Class Defaults, catégorie Layout), choisi par BP_YSPlayerController (UpgradeMenuClass)
  - BP_TestGameMode (et BP_TestMapGameMode) : 3 propositions par montée de niveau (Upgrade Choice Count)
  - WBP_CharacterSelect : 5 couleurs / visages par ligne (la personnalisation tient dans sa colonne, plus étroite)
Ne recrée pas WBP_UpgradeMenu s'il existe déjà (les réglages de Paul sont conservés).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_upgrade_menu.py"
Rapport : Saved/Logs/setup_upgrade_menu.txt
"""
import os
import unreal

WIDGET_DIR = "/Game/Core/UI"
WIDGET_NAME = "WBP_UpgradeMenu"
WIDGET_BP = f"{WIDGET_DIR}/{WIDGET_NAME}"
SELECT_BP = f"{WIDGET_DIR}/WBP_CharacterSelect"
PC_BP = "/Game/Core/Player/BP_YSPlayerController"
GAME_MODE_BPS = ["/Game/Core/GameModes/BP_TestGameMode", "/Game/Core/GameModes/BP_TestMapGameMode"]
UPGRADE_CHOICES = 3
SWATCHES_PER_ROW = 5

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
report = []


def log(msg):
    unreal.log(f"[setup_upgrade_menu] {msg}")
    report.append(msg)


try:
    if assets.does_asset_exist(WIDGET_BP):
        log(f"existe déjà : {WIDGET_BP}")
    else:
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", unreal.YSUpgradeMenuWidget)
        widget_bp = tools.create_asset(WIDGET_NAME, WIDGET_DIR, unreal.WidgetBlueprint, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(widget_bp)
        assets.save_asset(WIDGET_BP, only_if_is_dirty=False)
        log(f"créé : {WIDGET_BP}")
    widget_class = assets.load_blueprint_class(WIDGET_BP)

    pc = unreal.get_default_object(assets.load_blueprint_class(PC_BP))
    pc.set_editor_property("upgrade_menu_class", widget_class)
    assets.save_asset(PC_BP, only_if_is_dirty=False)
    log(f"{PC_BP} : UpgradeMenuClass = {widget_class.get_name()}")

    for path in GAME_MODE_BPS:
        if not assets.does_asset_exist(path):
            log(f"absent : {path}")
            continue
        gm = unreal.get_default_object(assets.load_blueprint_class(path))
        before = gm.get_editor_property("upgrade_choice_count")
        gm.set_editor_property("upgrade_choice_count", UPGRADE_CHOICES)
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"{path} : Upgrade Choice Count {before} -> {UPGRADE_CHOICES}")

    select = unreal.get_default_object(assets.load_blueprint_class(SELECT_BP))
    for prop in ["left_column_weight", "center_column_weight", "right_column_weight", "swatch_size", "swatch_spacing", "preview_size"]:
        log(f"WBP_CharacterSelect : {prop} = {select.get_editor_property(prop)}")
    before = select.get_editor_property("swatches_per_row")
    select.set_editor_property("swatches_per_row", SWATCHES_PER_ROW)
    assets.save_asset(SELECT_BP, only_if_is_dirty=False)
    log(f"WBP_CharacterSelect : Swatches Per Row {before} -> {SWATCHES_PER_ROW}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_upgrade_menu.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
