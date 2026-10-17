"""
Écran de sélection réglable dans l'éditeur :
  - /Game/Core/UI/WBP_CharacterSelect : Widget Blueprint enfant de YSCharacterSelectWidget (arbre construit en C++,
    placements dans Class Defaults, catégorie Layout)
  - /Game/Core/Player/BP_YSPlayerController : BP enfant de YSPlayerController, CharacterSelectMenuClass = WBP_CharacterSelect
    (réglages de l'aperçu : catégorie UI|Preview)
  - BP_TestGameMode : PlayerControllerClass = BP_YSPlayerController
Ne recrée pas ce qui existe déjà (les réglages de Paul sont conservés).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_character_select_widget.py"
Rapport : Saved/Logs/setup_character_select_widget.txt
"""
import os
import unreal

WIDGET_DIR = "/Game/Core/UI"
WIDGET_NAME = "WBP_CharacterSelect"
WIDGET_BP = f"{WIDGET_DIR}/{WIDGET_NAME}"
PC_DIR = "/Game/Core/Player"
PC_NAME = "BP_YSPlayerController"
PC_BP = f"{PC_DIR}/{PC_NAME}"
GAME_MODE_BP = "/Game/Core/GameModes/BP_TestGameMode"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
report = []


def log(msg):
    unreal.log(f"[setup_character_select_widget] {msg}")
    report.append(msg)


try:
    if assets.does_asset_exist(WIDGET_BP):
        log(f"existe déjà : {WIDGET_BP}")
    else:
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", unreal.YSCharacterSelectWidget)
        widget_bp = tools.create_asset(WIDGET_NAME, WIDGET_DIR, unreal.WidgetBlueprint, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(widget_bp)
        assets.save_asset(WIDGET_BP, only_if_is_dirty=False)
        log(f"créé : {WIDGET_BP}")
    widget_class = assets.load_blueprint_class(WIDGET_BP)

    if assets.does_asset_exist(PC_BP):
        log(f"existe déjà : {PC_BP}")
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.YSPlayerController)
        pc_bp = tools.create_asset(PC_NAME, PC_DIR, unreal.Blueprint, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(pc_bp)
        assets.save_asset(PC_BP, only_if_is_dirty=False)
        log(f"créé : {PC_BP}")
    pc_class = assets.load_blueprint_class(PC_BP)

    pc = unreal.get_default_object(pc_class)
    pc.set_editor_property("character_select_menu_class", widget_class)
    assets.save_asset(PC_BP, only_if_is_dirty=False)

    gm = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP))
    gm.set_editor_property("player_controller_class", pc_class)
    assets.save_asset(GAME_MODE_BP, only_if_is_dirty=False)

    check_pc = unreal.get_default_object(assets.load_blueprint_class(PC_BP))
    check_gm = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP))
    log(f"relu : menu {check_pc.get_editor_property('character_select_menu_class').get_name()}, "
        f"PlayerController du GameMode {check_gm.get_editor_property('player_controller_class').get_name()}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_character_select_widget.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
