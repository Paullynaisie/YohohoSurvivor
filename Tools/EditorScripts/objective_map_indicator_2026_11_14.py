"""
Carte au trésor d'objectif toujours repérable (demande de Paul du 2026-11-14) : sur BP_InteractiveObject_ObjectiveTreasureMap, bulle au bord de
l'écran toujours affichée (bShowIndicator, bRevealIndicatorNearby décoché : plus cachée tant qu'aucun joueur n'est passé à côté ni quand tout le
monde est loin), en vert comme les pointillés de la carte ; icône gardée (t_i_RoadmapIcon par défaut). Rapporte aussi PathScale (chemin raccourci).
Relançable.

Lancement (éditeur fermé, chemin ABSOLU du script, avec des / ) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/objective_map_indicator_2026_11_14.py"
Ou depuis l'éditeur ouvert : Output Log, Cmd -> Python, py "<projet>/Tools/EditorScripts/objective_map_indicator_2026_11_14.py"
Rapport : Saved/Logs/objective_map_indicator_2026_11_14.txt
"""
import os
import unreal

MAP = "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_ObjectiveTreasureMap"
ICON = "/Game/Art/Textures/Icones/t_i_RoadmapIcon"
GREEN = unreal.LinearColor(0.1, 1.0, 0.25, 1.0)
report = []


def log(msg):
    unreal.log(f"[objective_map_indicator] {msg}")
    report.append(msg)


cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(MAP))
before = {name: cdo.get_editor_property(name) for name in ("show_indicator", "reveal_indicator_nearby", "indicator_color", "indicator_icon")}
log(f"avant : {before}")
cdo.set_editor_property("show_indicator", True)
cdo.set_editor_property("reveal_indicator_nearby", False)
cdo.set_editor_property("indicator_color", GREEN)
if not cdo.get_editor_property("indicator_icon"):
    cdo.set_editor_property("indicator_icon", unreal.EditorAssetLibrary.load_asset(ICON))
log(f"après : bulle {cdo.get_editor_property('show_indicator')}, toujours visible {not cdo.get_editor_property('reveal_indicator_nearby')}, "
    f"couleur {cdo.get_editor_property('indicator_color')}, icône {cdo.get_editor_property('indicator_icon').get_name() if cdo.get_editor_property('indicator_icon') else None}, "
    f"PathScale {cdo.get_editor_property('path_scale')}")
log(f"sauvé {unreal.EditorAssetLibrary.save_asset(MAP, only_if_is_dirty=False)}")

path = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "objective_map_indicator_2026_11_14.txt")
with open(path, "w", encoding="utf-8") as file:
    file.write("\n".join(report))
if "executepythonscript" in unreal.SystemLibrary.get_command_line().lower():
    unreal.SystemLibrary.quit_editor()
