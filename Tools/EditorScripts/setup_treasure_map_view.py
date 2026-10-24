"""
Carte au trésor : distance d'affichage du chemin doublée (VisibleRadius 750 -> 1500, FadeDistance 200 -> 300) et chemin en
segments droits (bSmoothPath faux) sur BP_InteractiveObject_TreasureMap. Relançable (valeurs absolues).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_treasure_map_view.py"
Rapport : Saved/Logs/setup_treasure_map_view.txt
"""
import unreal

PATH = "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_TreasureMap"
VALUES = {"VisibleRadius": 1500.0, "FadeDistance": 300.0, "bSmoothPath": False}

report = []
cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(PATH))
for name, value in VALUES.items():
    report.append("%s : %s -> %s" % (name, cdo.get_editor_property(name), value))
    cdo.set_editor_property(name, value)
for name in ["ReachRadius", "MinSegmentLength", "MaxSegmentLength", "SegmentCount"]:
    report.append("%s = %s" % (name, cdo.get_editor_property(name)))
report.append("save : %s" % unreal.EditorAssetLibrary.save_asset(PATH, only_if_is_dirty=False))

unreal.log("\n".join(report))
with open(unreal.Paths.project_saved_dir() + "Logs/setup_treasure_map_view.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
