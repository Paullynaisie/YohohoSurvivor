"""Lecture seule : résumé des personnages jouables (set, PV, passif, upgrades de classe).
UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="Tools/EditorScripts/read_characters_summary.py" -unattended
"""
import unreal

out = []
def p(s):
    out.append(s)

def safe(o, n):
    try:
        return o.get_editor_property(n)
    except Exception:
        return None

def nm(o):
    return o.get_name() if o else "None"

gm_cls = unreal.EditorAssetLibrary.load_blueprint_class("/Game/Core/GameModes/BP_TestGameMode")
gm = unreal.get_default_object(gm_cls)
chars = safe(gm, "selectable_characters") or []
for c in chars:
    cdo = unreal.get_default_object(c)
    ws = safe(cdo, "weapon_set")
    pas = safe(cdo, "passive")
    p("== %s (%s) unique=%s scale=%s" % (safe(cdo, "character_name"), nm(c), safe(cdo, "b_unique_character"), safe(cdo, "character_scale")))
    if ws:
        p("   set %s '%s' PV %s R=%s L=%s" % (nm(ws), safe(ws, "display_name"), safe(ws, "max_health"), nm(safe(ws, "right_hand_weapon")), nm(safe(ws, "left_hand_weapon"))))
    if pas:
        p("   passif '%s' : %s" % (safe(pas, "display_name"), safe(pas, "description")))
    for up in safe(cdo, "upgrade_pool") or []:
        if up and isinstance(up, unreal.YSClassUpgrade):
            t = safe(up, "target_weapon_set")
            p("   classe %s : '%s' -> %s '%s' PV %s (%s)" % (nm(up), safe(up, "description"), nm(t), safe(t, "display_name") if t else None, safe(t, "max_health") if t else None, safe(up, "source_weapon_set")))

with open(unreal.Paths.project_saved_dir() + "Logs/read_characters_summary.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out))
