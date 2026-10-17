"""
Jour / nuit :
  - crée /Game/Placeable/Character/FoeCharacter_Skull_Elite (BP enfant de FoeCharacter_Skull) s'il n'existe pas :
    élite, PV x10, dégâts x5, taille x3, nom « Foe Skull Elite »
  - FoeCharacter_Skull : nom « Foe Skull »
  - BP_TestGameMode : jour de 30 s ; règle « Vagues jour / nuit » : jour 2 NME / joueur / jour (temporaire, pour les tests),
    nuit toutes les 30 s, 5 NME / joueur / jour, élite = FoeCharacter_Skull_Elite

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_day_night.py"
Rapport : Saved/Logs/setup_day_night.txt
"""
import os
import unreal

CHARACTER_DIR = "/Game/Placeable/Character"
FOE_BP = f"{CHARACTER_DIR}/FoeCharacter_Skull"
ELITE_NAME = "FoeCharacter_Skull_Elite"
ELITE_BP = f"{CHARACTER_DIR}/{ELITE_NAME}"
GAME_MODE_BP = "/Game/Core/GameModes/BP_TestGameMode"

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_day_night] {msg}")
    report.append(msg)


try:
    foe_class = assets.load_blueprint_class(FOE_BP)
    foe = unreal.get_default_object(foe_class)
    foe.set_editor_property("foe_name", "Foe Skull")
    assets.save_asset(FOE_BP, only_if_is_dirty=False)

    if assets.does_asset_exist(ELITE_BP):
        log(f"existe déjà : {ELITE_BP}")
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", foe_class)
        elite_bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(ELITE_NAME, CHARACTER_DIR, unreal.Blueprint, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(elite_bp)
        assets.save_asset(ELITE_BP, only_if_is_dirty=False)
        log(f"créé : {ELITE_BP}")

    elite_class = assets.load_blueprint_class(ELITE_BP)
    elite = unreal.get_default_object(elite_class)
    elite.set_editor_property("foe_name", "Foe Skull Elite")
    elite.set_editor_property("bIsElite", True)
    elite.set_editor_property("health_multiplier", 10.0)
    elite.set_editor_property("damage_multiplier", 5.0)
    elite.set_editor_property("size_multiplier", 3.0)
    assets.save_asset(ELITE_BP, only_if_is_dirty=False)

    gm = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP))
    rules = gm.get_editor_property("spawn_rules")
    wave = next((r for r in rules if r and isinstance(r, unreal.YSTimedWaveSpawnRule)), None)
    if wave is None:
        raise RuntimeError("pas de règle de vagues dans SpawnRules")
    log(f"avant : jour {wave.get_editor_property('enemies_per_player')} NME/joueur toutes les {wave.get_editor_property('wave_interval')} s")
    wave.set_editor_property("enemies_per_player", 2)
    wave.set_editor_property("night_wave_interval", 30.0)
    wave.set_editor_property("night_enemies_per_player", 5)
    wave.set_editor_property("elite_classes", [elite_class])
    gm.set_editor_property("day_duration", 30.0)
    log(f"durée du jour {gm.get_editor_property('day_duration')} s")
    assets.save_asset(GAME_MODE_BP, only_if_is_dirty=False)

    # relecture après rechargement
    check_elite = unreal.get_default_object(assets.load_blueprint_class(ELITE_BP))
    log(f"élite relu : {check_elite.get_editor_property('foe_name')}, élite {check_elite.get_editor_property('bIsElite')}, "
        f"PV x{check_elite.get_editor_property('health_multiplier')}, dégâts x{check_elite.get_editor_property('damage_multiplier')}, "
        f"taille x{check_elite.get_editor_property('size_multiplier')}")
    check_wave = next(r for r in unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP)).get_editor_property("spawn_rules")
                      if r and isinstance(r, unreal.YSTimedWaveSpawnRule))
    log(f"vagues relues : jour {check_wave.get_editor_property('enemies_per_player')} NME/joueur/jour toutes les "
        f"{check_wave.get_editor_property('wave_interval')} s ; nuit {check_wave.get_editor_property('night_enemies_per_player')} "
        f"toutes les {check_wave.get_editor_property('night_wave_interval')} s ; élites "
        f"{[c.get_name() for c in check_wave.get_editor_property('elite_classes')]}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_day_night.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
