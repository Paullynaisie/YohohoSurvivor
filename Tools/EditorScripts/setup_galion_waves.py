"""
Vagues de la mer (2026-10-30) : BP_TestGameMode : 3 Jolly Ships par vague, Galions réguliers ; BP_TestMapGameMode : pas de Galions réguliers
(pavé numérique seulement). Relançable.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_galion_waves.py"
Rapport : Saved/Logs/setup_galion_waves.txt
"""
import os
import unreal

TEST_GAME_MODE = "/Game/Core/GameModes/BP_TestGameMode"
TEST_MAP_GAME_MODE = "/Game/Core/GameModes/BP_TestMapGameMode"

assets = unreal.EditorAssetLibrary
report = []
try:
    game = unreal.get_default_object(assets.load_blueprint_class(TEST_GAME_MODE))
    game.set_editor_property("JollyShipsPerWave", 3)
    game.set_editor_property("JollyShipWaveInterval", 10.0)
    game.set_editor_property("bGalionWaves", True)
    assets.save_asset(TEST_GAME_MODE, only_if_is_dirty=False)
    test = unreal.get_default_object(assets.load_blueprint_class(TEST_MAP_GAME_MODE))
    test.set_editor_property("bGalionWaves", False)
    assets.save_asset(TEST_MAP_GAME_MODE, only_if_is_dirty=False)
    report.append(f"BP_TestGameMode : {game.get_editor_property('JollyShipsPerWave')} Jolly Ships / {game.get_editor_property('JollyShipWaveInterval')} s, Galion Class {game.get_editor_property('GalionClass')}")
    report.append(f"BP_TestMapGameMode : Galion Waves {test.get_editor_property('bGalionWaves')}, Jolly Ship Waves {test.get_editor_property('bJollyShipWaves')}")
    report.append("OK")
except Exception as e:
    report.append(f"ERREUR : {e!r}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_galion_waves.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
