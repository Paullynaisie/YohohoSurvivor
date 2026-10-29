"""
Réglages du 2026-10-29 :
  - carte au trésor : distance d'affichage des pointillés -30 % (VisibleRadius 1500 -> 1050) sur BP_InteractiveObject_TreasureMap
  - BP_TestGameMode : tous les timings de spawn à 30 s passent à 45 s (et leurs multiples suivent la même cadence) :
    circle spawns de la nuit (30 / 60 s -> 45 / 90 s), répétition du dernier toutes les 45 s, 2e vague d'overtime (30 s -> 45 s). La descente de l'île (pas un spawn) n'est pas touchée.
Relançable (valeurs absolues).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_tweaks_2026_10_29.py"
Rapport : Saved/Logs/setup_tweaks_2026_10_29.txt
"""
import unreal

TREASURE_MAP = "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_TreasureMap"
VISIBLE_RADIUS = 1050.0
GAME_GM = "/Game/Core/GameModes/BP_TestGameMode"
# ancienne cadence de 30 s -> 45 s : délais absolus, dans l'ordre (circle spawns de nuit : 30 / 60 s avant ; overtime : 0 / 30 s avant)
NIGHT_EVENT_DELAYS = [45.0, 90.0]
OVERTIME_DELAYS = [0.0, 45.0]
INTERVAL = 45.0
NIGHT_WAVE_INTERVAL = 60.0

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_tweaks_2026_10_29] {msg}")
    report.append(msg)


def new_delay(delays, index, old):
    return delays[index] if index < len(delays) else old


# carte au trésor
cdo = unreal.get_default_object(assets.load_blueprint_class(TREASURE_MAP))
log(f"VisibleRadius : {cdo.get_editor_property('VisibleRadius')} -> {VISIBLE_RADIUS} (FadeDistance {cdo.get_editor_property('FadeDistance')})")
cdo.set_editor_property("VisibleRadius", VISIBLE_RADIUS)
log(f"save carte : {assets.save_asset(TREASURE_MAP, only_if_is_dirty=False)}")

# timings de spawn
gm = unreal.get_default_object(assets.load_blueprint_class(GAME_GM))
wave = next((r for r in gm.get_editor_property("spawn_rules") if r and isinstance(r, unreal.YSTimedWaveSpawnRule)), None)
if wave is None:
    log("ERREUR : pas de règle de vagues dans SpawnRules de BP_TestGameMode")
else:
    events = []
    for index, event in enumerate(wave.get_editor_property("night_circle_events")):
        old = event.get_editor_property("delay")
        event.set_editor_property("delay", new_delay(NIGHT_EVENT_DELAYS, index, old))
        log(f"circle spawn de nuit : {old} -> {event.get_editor_property('delay')} s")
        events.append(event)
    wave.set_editor_property("night_circle_events", events)
    log(f"répétition : {wave.get_editor_property('night_circle_repeat_interval')} -> {INTERVAL} s")
    wave.set_editor_property("night_circle_repeat_interval", INTERVAL)
    # vagues de nuit par timer (désactivées) : 60 s dans le BP, pas 30 : inchangées (remises à 60 après un premier passage erroné)
    wave.set_editor_property("night_wave_interval", NIGHT_WAVE_INTERVAL)

waves = []
for index, overtime in enumerate(gm.get_editor_property("overtime_waves")):
    old = overtime.get_editor_property("delay")
    overtime.set_editor_property("delay", new_delay(OVERTIME_DELAYS, index, old))
    log(f"vague d'overtime : {old} -> {overtime.get_editor_property('delay')} s")
    waves.append(overtime)
gm.set_editor_property("overtime_waves", waves)
log(f"save GameMode : {assets.save_asset(GAME_GM, only_if_is_dirty=False)}")

# relecture
check_gm = unreal.get_default_object(assets.load_blueprint_class(GAME_GM))
check = next((r for r in check_gm.get_editor_property("spawn_rules") if r and isinstance(r, unreal.YSTimedWaveSpawnRule)), None)
if check:
    log(f"relu : nuit {[e.get_editor_property('delay') for e in check.get_editor_property('night_circle_events')]}, "
        f"répétition {check.get_editor_property('night_circle_repeat_interval')} s, vagues de nuit {check.get_editor_property('night_wave_interval')} s")
log(f"relu : overtime {[w.get_editor_property('delay') for w in check_gm.get_editor_property('overtime_waves')]}, "
    f"pointillés {unreal.get_default_object(assets.load_blueprint_class(TREASURE_MAP)).get_editor_property('VisibleRadius')}")

with open(unreal.Paths.project_saved_dir() + "Logs/setup_tweaks_2026_10_29.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
