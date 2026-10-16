"""
Réglages de BP_TestGameMode :
  - XP par niveau : LevelUpBaseExperience 5, LevelUpExperiencePerLevel 5 (5, 10, 15...)
  - Vagues de NME (UYSTimedWaveSpawnRule) : WaveInterval 5 s

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_level_and_waves.py"
Rapport : Saved/Logs/setup_level_and_waves.txt
"""
import os
import unreal

GAME_MODE_BP = "/Game/Core/GameModes/BP_TestGameMode"

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_level_and_waves] {msg}")
    report.append(msg)


try:
    gm = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP))
    log(f"avant : XP {gm.get_editor_property('level_up_base_experience')} + "
        f"{gm.get_editor_property('level_up_experience_per_level')} / niveau")
    gm.set_editor_property("level_up_base_experience", 5.0)
    gm.set_editor_property("level_up_experience_per_level", 5.0)

    for rule in gm.get_editor_property("spawn_rules"):
        if rule and isinstance(rule, unreal.YSTimedWaveSpawnRule):
            log(f"avant : {rule.get_name()} intervalle {rule.get_editor_property('wave_interval')} s")
            rule.set_editor_property("wave_interval", 5.0)

    assets.save_asset(GAME_MODE_BP, only_if_is_dirty=False)

    check = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP))
    log(f"relu : XP {check.get_editor_property('level_up_base_experience')} + "
        f"{check.get_editor_property('level_up_experience_per_level')} / niveau, intervalles "
        f"{[r.get_editor_property('wave_interval') for r in check.get_editor_property('spawn_rules') if r and isinstance(r, unreal.YSTimedWaveSpawnRule)]}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_level_and_waves.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
