"""
Events de nuit (règle « Vagues jour / nuit » de BP_TestGameMode) :
  - pas de vagues par timer la nuit (bNightTimedWaves faux)
  - élite au centre de l'île
  - circle spawn du début de nuit : 50 FoeCharacter_Skull sur le contour de la plage
  - circle spawn quand l'élite passe sous 50 % de ses PV : 50 FoeCharacter_Skull_Gun

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_circle_spawn.py"
Rapport : Saved/Logs/setup_circle_spawn.txt
"""
import os
import unreal

GAME_MODE_BP = "/Game/Core/GameModes/BP_TestGameMode"
SKULL_BP = "/Game/Placeable/Character/Foe/FoeCharacter_Skull"
SKULL_GUN_BP = "/Game/Placeable/Character/Foe/FoeCharacter_Skull_Gun"
MAX_ENEMIES = 50

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_circle_spawn] {msg}")
    report.append(msg)


def set_circle(wave, prop, foe_class):
    circle = wave.get_editor_property(prop)
    circle.set_editor_property("enabled", True)
    circle.set_editor_property("enemy_classes", [foe_class])
    circle.set_editor_property("max_enemies", MAX_ENEMIES)
    wave.set_editor_property(prop, circle)


try:
    gm = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP))
    rules = gm.get_editor_property("spawn_rules")
    wave = next((r for r in rules if r and isinstance(r, unreal.YSTimedWaveSpawnRule)), None)
    if wave is None:
        raise RuntimeError("pas de règle de vagues dans SpawnRules")

    wave.set_editor_property("night_timed_waves", False)
    wave.set_editor_property("elite_at_island_center", True)
    set_circle(wave, "night_start_circle_spawn", assets.load_blueprint_class(SKULL_BP))
    set_circle(wave, "elite_health_circle_spawn", assets.load_blueprint_class(SKULL_GUN_BP))
    wave.set_editor_property("elite_health_circle_threshold", 0.5)
    assets.save_asset(GAME_MODE_BP, only_if_is_dirty=False)

    # relecture
    check = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP)).get_editor_property("spawn_rules")
    for r in check:
        if r and isinstance(r, unreal.YSTimedWaveSpawnRule):
            for prop in ("night_start_circle_spawn", "elite_health_circle_spawn"):
                c = r.get_editor_property(prop)
                log(f"relu {prop} : {[x.get_name() for x in c.get_editor_property('enemy_classes')]} x {c.get_editor_property('max_enemies')}")
            log(f"relu vagues de nuit : {r.get_editor_property('night_timed_waves')}, élite au centre : {r.get_editor_property('elite_at_island_center')}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_circle_spawn.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
