"""
Branche le GameState du jeu (chiffres de dégâts) sur le GameMode de test :
  - BP_TestGameMode.GameStateClass = AYSGameState

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_combat_ui.py"
Rapport : Saved/Logs/setup_combat_ui.txt
"""
import os
import unreal

GAME_MODE_BP = "/Game/Core/GameModes/BP_TestGameMode"

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_combat_ui] {msg}")
    report.append(msg)


try:
    bp = assets.load_asset(GAME_MODE_BP)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP))
    cdo.set_editor_property("game_state_class", unreal.YSGameState.static_class())
    assets.save_asset(GAME_MODE_BP, only_if_is_dirty=False)
    log(f"{GAME_MODE_BP}.GameStateClass = {cdo.get_editor_property('game_state_class').get_name()}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_combat_ui.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
