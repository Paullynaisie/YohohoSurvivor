"""
Bord de l'île moins abrupt (2026-10-25, demande de Paul) : sur GameMap et TestMap, SEULS les sommets du Landscape « Island »
au-delà du bord de l'eau (profil de setup_island.py, d > 1) sont recalculés avec la nouvelle largeur de pente CLIFF_WIDTH
(0.02 = à pic avant ; 0.2 = ~13 m de pente douce de -22 jusqu'au fond -650). Le reste de l'île (plage, terre, retouches
éventuelles au sculpt) et les acteurs accrochés (palmiers, ponton, Brooker) ne bougent pas.
Relançable (même résultat).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="<chemin absolu>/Tools/EditorScripts/smooth_island_shore.py"
Rapport : Saved/Logs/smooth_island_shore.txt
"""
import math
import os
import unreal

LEVELS = ["/Game/Maps/GameMap", "/Game/Maps/TestMap"]
ISLAND_SCALE = math.sqrt(2.0)
ISLAND_RADIUS = 4600.0 * ISLAND_SCALE
SHORE_DEPTH = -22.0
SEA_FLOOR = -650.0
CLIFF_WIDTH = 0.2

actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
report = []


def log(msg):
    unreal.log(f"[smooth_island_shore] {msg}")
    report.append(msg)


def smoothstep(t):
    t = max(0.0, min(1.0, t))
    return t * t * (3.0 - 2.0 * t)


def coast_radius(theta):
    return ISLAND_RADIUS + ISLAND_SCALE * (350.0 * math.sin(3.0 * theta + 0.4) + 200.0 * math.sin(5.0 * theta + 1.3) + 120.0 * math.sin(9.0 * theta + 2.1))


def sea_height(x, y):
    """Hauteur sous l'eau du profil (None = sommet de l'île, inchangé)"""
    d = math.hypot(x, y) / coast_radius(math.atan2(y, x))
    if d <= 1.0:
        return None
    return SHORE_DEPTH + (SEA_FLOOR - SHORE_DEPTH) * smoothstep((d - 1.0) / CLIFF_WIDTH)


try:
    for level in LEVELS:
        if not levels.load_level(level):
            raise RuntimeError(f"impossible d'ouvrir {level}")
        landscape = next((a for a in actors_sub.get_all_level_actors() if isinstance(a, unreal.Landscape)), None)
        if landscape is None:
            raise RuntimeError(f"pas de Landscape dans {level}")
        heights, size_x, size_y = unreal.YSEditorScriptLibrary.get_landscape_heights(landscape)
        heights = list(heights)
        origin = landscape.get_actor_location()
        scale = landscape.get_actor_scale3d()
        changed = 0
        biggest = 0.0
        for j in range(size_y):
            for i in range(size_x):
                index = j * size_x + i
                target = sea_height(origin.x + i * scale.x, origin.y + j * scale.y)
                if target is not None and abs(heights[index] - target) > 0.5:
                    biggest = max(biggest, abs(heights[index] - target))
                    heights[index] = target
                    changed += 1
        if changed and not unreal.YSEditorScriptLibrary.set_landscape_heights(landscape, heights):
            raise RuntimeError(f"hauteurs refusées pour {level}")
        check, _, _ = unreal.YSEditorScriptLibrary.get_landscape_heights(landscape)
        error = max(abs(a - b) for a, b in zip(check, heights))
        log(f"{level} : {landscape.get_actor_label()} {size_x} x {size_y}, {changed} sommet(s) sous l'eau recalculé(s) "
            f"(écart max {biggest:.0f} cm), relu à {error:.1f} cm près")
        if not levels.save_current_level():
            raise RuntimeError(f"sauvegarde de {level} impossible")
        log(f"{level} sauvegardée")
    log("OK")
except Exception as error:
    import traceback
    log(f"ERREUR : {error}\n{traceback.format_exc()}")

with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "smooth_island_shore.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
