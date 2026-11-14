"""
Réglages du 2026-11-14 (demande de Paul), suite :
  - SilverCoin upgrade (objectif de doublons du bateau) : 100 de base + 25 par niveau (au lieu de + 50) : DoubloonGoalPerShipLevel 25
    sur tous les GameModes Blueprint (enfants de AYSGameMode ; le défaut C++ est aussi passé à 25) ;
  - îles de la partie 2 (NextIslands de BP_FlowMod) : jour de 120 s (comme l'île de départ).
Relançable.

Lancement (éditeur fermé, chemin ABSOLU du script, avec des / ) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/tune_flow_2026_11_14b.py"
Rapport : Saved/Logs/tune_flow_2026_11_14b.txt
"""
import os
import unreal

DAY_DURATION = 120.0
PER_LEVEL = 25
report = []


def log(msg):
    unreal.log(f"[tune_flow_b] {msg}")
    report.append(msg)


for path in ("/Game/Core/GameModes/BP_FlowMod", "/Game/Core/GameModes/BP_TestGameMode"):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if not cls:
        log(f"{path} : introuvable")
        continue
    cdo = unreal.get_default_object(cls)
    before = cdo.get_editor_property("doubloon_goal_per_ship_level")
    cdo.set_editor_property("doubloon_goal_per_ship_level", PER_LEVEL)
    log(f"{path} : DoubloonGoal {cdo.get_editor_property('doubloon_goal')} + {before} -> {PER_LEVEL} par niveau, sauvé {unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)}")

flow = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class("/Game/Core/GameModes/BP_FlowMod"))
for da in flow.get_editor_property("next_islands") or []:
    if not da:
        continue
    path = da.get_path_name().split(".")[0]
    before = da.get_editor_property("day_duration")
    da.set_editor_property("day_duration", DAY_DURATION)
    log(f"{path} : jour {before:g} -> {DAY_DURATION:g} s, fin du jour à la livraison {da.get_editor_property('end_day_on_objective_delivered')}, "
        f"sauvé {unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)}")

path = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "tune_flow_2026_11_14b.txt")
with open(path, "w", encoding="utf-8") as file:
    file.write("\n".join(report))
if "executepythonscript" in unreal.SystemLibrary.get_command_line().lower():
    unreal.SystemLibrary.quit_editor()
