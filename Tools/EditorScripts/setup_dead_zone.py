"""
Deadzone : règle le KillZ (World Settings) des maps jouables à DEAD_ZONE_DEPTH cm sous le sol le plus bas.
Un joueur ou un NME qui passe sous le KillZ meurt (FellOutOfWorld -> Die, serveur) ; les autres acteurs sont détruits.
Paul peut ensuite régler la valeur à la main : World Settings > World > Kill Z.

Lancer (éditeur fermé) :
UnrealEditor.exe <uproject> -ExecutePythonScript="Tools/EditorScripts/setup_dead_zone.py"
"""
import unreal

MAPS = ["/Game/Maps/GameMap", "/Game/Maps/TestMap"]
DEAD_ZONE_DEPTH = 300.0

report = []


def lowest_ground_top():
    # dessus du sol le plus bas parmi les StaticMeshActor larges (sol, plateformes)
    lowest = None
    for actor in unreal.EditorLevelLibrary.get_all_level_actors():
        if not isinstance(actor, unreal.StaticMeshActor):
            continue
        origin, extent = actor.get_actor_bounds(False)
        if extent.x < 500.0 or extent.y < 500.0:
            continue
        top = origin.z + extent.z
        if lowest is None or top < lowest:
            lowest = top
    return lowest if lowest is not None else 0.0


for map_path in MAPS:
    world = unreal.EditorLoadingAndSavingUtils.load_map(map_path)
    settings = world.get_world_settings()
    ground = lowest_ground_top()
    kill_z = ground - DEAD_ZONE_DEPTH
    settings.set_editor_property("enable_world_bounds_checks", True)
    settings.set_editor_property("kill_z", kill_z)
    saved = unreal.EditorLoadingAndSavingUtils.save_map(world, map_path)
    report.append("%s : sol %.0f -> KillZ %.0f (sauvegarde %s)" % (map_path, ground, kill_z, saved))

for line in report:
    unreal.log_warning("[YSDeadZone] " + line)

with open(unreal.Paths.project_saved_dir() + "Logs/setup_dead_zone_report.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(report))

unreal.SystemLibrary.quit_editor()
