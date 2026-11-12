"""Lecture seule : fiches complètes des personnages jouables (pour la doc Google Slides).
Pour chaque perso : stats 3C, passif, MoveAction, sets (départ + classes), actions de chaque arme
(toutes les propriétés propres à la classe d action), upgrades d arme du pool.
UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="Tools/EditorScripts/read_character_sheets.py" -unattended
"""
import unreal

SKIP = {"JohnnyPlaytest"}
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

def fmt(v):
    if isinstance(v, float):
        return ("%.3f" % v).rstrip("0").rstrip(".")
    if isinstance(v, unreal.Object):
        return v.get_name()
    if isinstance(v, (unreal.Array,)):
        return "[" + ", ".join(fmt(x) for x in v) + "]"
    return str(v)

_base_cache = {}
def own_props(obj, base_cls):
    """Propriétés lisibles de obj absentes de base_cls (valeurs simples seulement)."""
    key = base_cls
    if key not in _base_cache:
        _base_cache[key] = set(dir(base_cls))
    res = []
    for n in dir(obj):
        if n.startswith("_") or n in _base_cache[key]:
            continue
        v = safe(obj, n)
        if v is None:
            continue
        if isinstance(v, (bool, int, float, str, unreal.Name, unreal.Text, unreal.Object, unreal.Array, unreal.EnumBase)):
            if isinstance(v, unreal.Array) and len(v) > 6:
                continue
            res.append("%s=%s" % (n, fmt(v)))
    return res

sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
def weapon_actions(wc):
    if not wc:
        return
    path = wc.get_path_name().rsplit(".", 1)[0]
    bp = unreal.EditorAssetLibrary.load_asset(path)
    wcdo = unreal.get_default_object(wc)
    p("    arme %s « %s » type=%s" % (nm(wc), safe(wcdo, "display_name"), fmt(safe(wcdo, "weapon_type"))))
    for h in sds.k2_gather_subobject_data_for_blueprint(bp):
        d = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h)
        o = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(d)
        if o is None or not isinstance(o, unreal.YSAbilityComponent):
            continue
        p("      [%s] %s" % (o.get_class().get_name(), o.get_name().replace("_GEN_VARIABLE", "")))
        p("         " + " | ".join(own_props(o, unreal.ActorComponent)))
    # props propres à l arme (ex. réglages de bombe)
    p("      arme: " + " | ".join(own_props(wcdo, unreal.Actor))[:1500])

def dump_set(ws, label):
    if not ws:
        return
    p("  SET %s %s « %s » PV=%s scale=%s" % (label, nm(ws), safe(ws, "display_name"), fmt(safe(ws, "max_health")), fmt(safe(ws, "character_scale_multiplier"))))
    seen = set()
    for side in ["right_hand_weapon", "left_hand_weapon"]:
        wc = safe(ws, side)
        p("   %s = %s" % (side, nm(wc)))
        if wc and nm(wc) not in seen:
            seen.add(nm(wc))
            weapon_actions(wc)

gm = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class("/Game/Core/GameModes/BP_TestGameMode"))
for c in safe(gm, "selectable_characters") or []:
    cdo = unreal.get_default_object(c)
    name = str(safe(cdo, "character_name"))
    if name in SKIP:
        continue
    p("=" * 20 + " " + name)
    for n in ["max_health", "base_health_regen", "move_speed", "character_scale", "critical_damage_multiplier", "max_stamina",
              "stamina_regen_amount", "stamina_regen_delay", "carry_stamina_cost", "swim_stamina_cost", "b_unique_character", "bUniqueCharacter"]:
        v = safe(cdo, n)
        if v is not None:
            p("  %s=%s" % (n, fmt(v)))
    pas = safe(cdo, "passive")
    if pas:
        p("  PASSIF « %s » : %s | %s" % (safe(pas, "display_name"), safe(pas, "description"), " | ".join(own_props(pas, unreal.DataAsset))))
    mv = safe(cdo, "MoveActionAbility")
    if mv:
        p("  MOVEACTION [%s] « %s » stamina=%s" % (mv.get_class().get_name(), safe(mv, "display_name"), fmt(safe(mv, "stamina_cost"))))
    ws = safe(cdo, "weapon_set")
    dump_set(ws, "DEPART")
    for up in safe(cdo, "upgrade_pool") or []:
        if not up:
            continue
        if isinstance(up, unreal.YSClassUpgrade):
            src = safe(up, "source_weapon_set")
            if src and ws and nm(src) != nm(ws):
                continue
            p("  CLASSE %s : %s" % (nm(up), str(safe(up, "description")).replace("\n", " / ")))
            dump_set(safe(up, "target_weapon_set"), "CLASSE")
    for up in safe(cdo, "upgrade_pool") or []:
        if up and isinstance(up, unreal.YSWeaponUpgrade):
            p("  UPGRADE_ARME %s [%s] « %s » : %s" % (nm(up), up.get_class().get_name(), safe(up, "display_name"), str(safe(up, "description")).replace("\n", " / ")))
            p("     " + " | ".join(own_props(up, unreal.DataAsset))[:1500])
        elif up and not isinstance(up, unreal.YSClassUpgrade):
            p("  UPGRADE %s [%s]" % (nm(up), up.get_class().get_name()))

with open(unreal.Paths.project_saved_dir() + "Logs/read_character_sheets.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out))
