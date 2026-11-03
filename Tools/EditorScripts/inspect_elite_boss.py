"""
Lecture seule : réglages de l'élite, du Skull Gun, des NME du pavé numérique de TestMap et de la nuit de DA_I_StartIsland.
  UnrealEditor-Cmd.exe YohohoSurvivor.uproject -run=pythonscript -script="Tools/EditorScripts/inspect_elite_boss.py" -unattended
Rapport : Saved/Logs/inspect_elite_boss.txt
"""
import os
import unreal

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    report.append(str(msg))


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


FOE_DIR = "/Game/Placeable/Character/Foe"
PROPS = ["foe_name", "is_elite", "health_multiplier", "damage_multiplier", "size_multiplier", "boss_armor", "boss_shield",
         "weapon_set", "left_handed_chance", "numpad_spawn_key", "spawn_weight", "death_loot_class", "elite_health_bar_under",
         "max_rage", "elite_wade_depth"]

try:
    for path in [f"{FOE_DIR}/FoeCharacter_Skull", f"{FOE_DIR}/FoeCharacter_Skull_Elite", f"{FOE_DIR}/FoeCharacter_Skull_Gun"]:
        obj = cdo(path)
        log(f"== {path}")
        for p in PROPS:
            try:
                log(f"  {p} = {obj.get_editor_property(p)}")
            except Exception as e:
                log(f"  {p} : {e}")
    diff = unreal.YSEditorScriptLibrary.describe_changed_properties(cdo(f"{FOE_DIR}/FoeCharacter_Skull_Elite"), cdo(f"{FOE_DIR}/FoeCharacter_Skull")) \
        if hasattr(unreal.YSEditorScriptLibrary, "describe_changed_properties") else "(pas de describe_changed_properties)"
    log(f"== diff Elite / Skull :\n{diff}")

    for gm_path in ["/Game/Core/GameModes/BP_TestMapGameMode", "/Game/Core/GameModes/BP_TestGameMode", "/Game/Core/GameModes/BP_FlowMod"]:
        if not assets.does_asset_exist(gm_path):
            continue
        gm = cdo(gm_path)
        log(f"== {gm_path} : numpad {gm.get_editor_property('numpad_foe_spawn')}, jolly {gm.get_editor_property('numpad_jolly_ship_key')}, galion {gm.get_editor_property('numpad_galion_key')}")
        for c in gm.get_editor_property("numpad_foe_classes"):
            if c:
                log(f"  {c.get_name()} touche {unreal.get_default_object(c).get_editor_property('numpad_spawn_key')}")

    for reg in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path(FOE_DIR, recursive=True):
        name = str(reg.asset_name)
        try:
            c = assets.load_blueprint_class(f"{reg.package_name}")
            o = unreal.get_default_object(c)
            log(f"foe {name} : touche {o.get_editor_property('numpad_spawn_key')}")
        except Exception:
            pass

    island = assets.load_asset("/Game/Core/Islands/DA_I_StartIsland")
    if island:
        for cat in ["day_events", "night_events", "sink_events"]:
            for ev in island.get_editor_property(cat):
                foes = [(f.get_editor_property("foe_class").get_name() if f.get_editor_property("foe_class") else None, f.get_editor_property("count")) for f in ev.get_editor_property("foes")]
                log(f"island {cat} t={ev.get_editor_property('time')} shape={ev.get_editor_property('shape')} {foes}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "inspect_elite_boss.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
