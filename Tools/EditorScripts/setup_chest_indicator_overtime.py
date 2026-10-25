"""
2026-10-25 :
  - bulle au bord de l'écran pour les coffres (bShowIndicator sur BP_InteractiveObject_Chest, hérité par le coffre au trésor)
  - carte au trésor : chemin de nouveau arrondi (bSmoothPath, sol vérifié aussi le long de la courbe)
  - rapport des réglages d'overtime de BP_TestGameMode (la nuit d'Overtime Day lance l'overtime)
Relançable (valeurs absolues).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_chest_indicator_overtime.py"
Rapport : Saved/Logs/setup_chest_indicator_overtime.txt
"""
import unreal

CHEST = "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_Chest"
TREASURE_CHEST = "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_TreasureChest"
TREASURE_MAP = "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_TreasureMap"
GAME_MODE = "/Game/Core/GameModes/BP_TestGameMode"

assets = unreal.EditorAssetLibrary
report = []


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def set_and_save(path, values):
    obj = cdo(path)
    for name, value in values.items():
        report.append("%s.%s : %s -> %s" % (path.split("/")[-1], name, obj.get_editor_property(name), value))
        obj.set_editor_property(name, value)
    report.append("save %s : %s" % (path, assets.save_asset(path, only_if_is_dirty=False)))


set_and_save(CHEST, {"bShowIndicator": True})
treasure = cdo(TREASURE_CHEST)
report.append("TreasureChest bShowIndicator (hérité) = %s" % treasure.get_editor_property("bShowIndicator"))
set_and_save(TREASURE_MAP, {"bSmoothPath": True})

gm = cdo(GAME_MODE)
report.append("GameMode : bOvertime %s, OvertimeDay %s, IslandSinkDelayAfterLastWave %s" % (
    gm.get_editor_property("bOvertime"), gm.get_editor_property("OvertimeDay"), gm.get_editor_property("IslandSinkDelayAfterLastWave")))
for wave in gm.get_editor_property("OvertimeWaves"):
    report.append("  vague : délai %s, %s NME, niveau +%s, %s" % (wave.get_editor_property("Delay"), wave.get_editor_property("EnemiesPerPlayer"),
        wave.get_editor_property("LevelOffset"), [c.get_name() for c in wave.get_editor_property("EnemyClasses") if c]))

unreal.log("\n".join(report))
with open(unreal.Paths.project_saved_dir() + "Logs/setup_chest_indicator_overtime.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
