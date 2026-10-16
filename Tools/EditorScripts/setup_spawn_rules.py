"""
Spawn des NME :
  - BP_TestGameMode : règle « Vagues par joueur et par minute » (UYSTimedWaveSpawnRule, créée par défaut par AYSGameMode)
    avec FoeCharacter_Skull comme NME

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_spawn_rules.py"
Rapport : Saved/Logs/setup_spawn_rules.txt
"""
import os
import unreal

GAME_MODE_BP = "/Game/Core/GameModes/BP_TestGameMode"
FOE_BP = "/Game/Placeable/Character/FoeCharacter_Skull"

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_spawn_rules] {msg}")
    report.append(msg)


try:
    gm_bp = assets.load_asset(GAME_MODE_BP)
    unreal.BlueprintEditorLibrary.compile_blueprint(gm_bp)
    gm = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP))

    rules = gm.get_editor_property("spawn_rules")
    log(f"règles : {[r.get_class().get_name() if r else None for r in rules]}")
    wave = next((r for r in rules if r and isinstance(r, unreal.YSTimedWaveSpawnRule)), None)
    if wave is None:
        raise RuntimeError("pas de UYSTimedWaveSpawnRule dans SpawnRules")

    foe = assets.load_blueprint_class(FOE_BP)
    wave.set_editor_property("enemy_classes", [foe])
    log(f"EnemyClasses = [{foe.get_name()}], intervalle {wave.get_editor_property('wave_interval')} s, "
        f"zone sûre {wave.get_editor_property('safe_radius')} cm")

    assets.save_asset(GAME_MODE_BP, only_if_is_dirty=False)

    # relecture après rechargement pour vérifier la persistance
    check = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP)).get_editor_property("spawn_rules")
    log(f"relu : {[[c.get_name() for c in r.get_editor_property('enemy_classes')] for r in check if r]}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_spawn_rules.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
