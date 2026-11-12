"""
Écran des scores de fin de partie (2026-11-12) :
  - /Game/Core/UI/WBP_Scoreboard : enfant de UYSScoreboardWidget (placements, tailles, couleurs et textes réglables : Class Defaults, Layout)
  - BP_YSPlayerController : Scoreboard Class = WBP_Scoreboard
Ne recrée rien qui existe (les réglages de Paul sont gardés).

Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/setup_scoreboard.py"
Rapport : Saved/Logs/setup_scoreboard.txt
"""
import os
import unreal

UI_DIR = "/Game/Core/UI"
WIDGET = f"{UI_DIR}/WBP_Scoreboard"
PC_BP = "/Game/Core/Player/BP_YSPlayerController"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
report = []


def log(msg):
    unreal.log(f"[setup_scoreboard] {msg}")
    report.append(msg)


try:
    if assets.does_asset_exist(WIDGET):
        log(f"existe déjà : {WIDGET}")
    else:
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", unreal.YSScoreboardWidget)
        tools.create_asset("WBP_Scoreboard", UI_DIR, unreal.WidgetBlueprint, factory)
        log(f"créé : {WIDGET}")
    widget_bp = assets.load_asset(WIDGET)
    unreal.BlueprintEditorLibrary.compile_blueprint(widget_bp)
    assets.save_asset(WIDGET, only_if_is_dirty=False)

    pc_cdo = unreal.get_default_object(assets.load_blueprint_class(PC_BP))
    if pc_cdo.get_editor_property("scoreboard_class"):
        log(f"BP_YSPlayerController a déjà un Scoreboard Class : {pc_cdo.get_editor_property('scoreboard_class').get_name()}")
    else:
        pc_cdo.set_editor_property("scoreboard_class", assets.load_blueprint_class(WIDGET))
        assets.save_asset(PC_BP, only_if_is_dirty=False)
    check = unreal.get_default_object(assets.load_blueprint_class(PC_BP)).get_editor_property("scoreboard_class")
    log(f"relu : BP_YSPlayerController -> {check.get_name() if check else 'AUCUN'}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_scoreboard.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
