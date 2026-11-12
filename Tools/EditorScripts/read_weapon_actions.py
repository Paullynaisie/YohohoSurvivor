"""Lecture seule : réglages des composants d action (UYSAbilityComponent) des armes listées.
UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="Tools/EditorScripts/read_weapon_actions.py" -unattended
"""
import unreal

WEAPONS = [
    "/Game/Placeable/Weapons/Player/Wp_Melee/BP_Epee",
    "/Game/Placeable/Weapons/Player/Wp_Guns/BP_Pistolet",
    "/Game/Placeable/Weapons/Player/Wp_Guns/BP_BattlePetoir",
    "/Game/Placeable/Weapons/Player/Wp_Melee/BP_BattleSword",
]
PROPS = ["display_name", "action_type", "damage", "cooldown", "auto_atk_cooldown", "recover_duration",
         "bullets_per_burst", "burst_duration", "roll_duration", "roll_speed_multiplier", "aim_duration",
         "critical_interval", "dash_distance", "dash_duration", "akimbo_fire_delay"]

out = []
def p(s):
    out.append(s)

sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
for path in WEAPONS:
    bp = unreal.EditorAssetLibrary.load_asset(path)
    p("== " + path)
    handles = sds.k2_gather_subobject_data_for_blueprint(bp)
    for h in handles:
        data = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(data)
        if obj is None or not isinstance(obj, unreal.YSAbilityComponent):
            continue
        p("  [%s] %s" % (obj.get_class().get_name(), obj.get_name()))
        for prop in PROPS:
            try:
                p("     %s = %s" % (prop, obj.get_editor_property(prop)))
            except Exception:
                pass

with open(unreal.Paths.project_saved_dir() + "Logs/read_weapon_actions.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out))
