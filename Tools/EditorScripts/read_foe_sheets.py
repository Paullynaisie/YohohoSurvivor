"""Lecture seule : fiches des NME (base, élites, boss, capitaines de Galion, Kraken), bateaux de NME,
batailles navales et overtime, pour la doc Google Slides.
UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="Tools/EditorScripts/read_foe_sheets.py" -unattended
"""
import re
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

def fmt(v):
    if isinstance(v, float):
        return ("%.3f" % v).rstrip("0").rstrip(".")
    if isinstance(v, unreal.Object):
        return v.get_name()
    if isinstance(v, unreal.Array):
        return "[" + ", ".join(fmt(x) for x in v) + "]"
    if isinstance(v, unreal.StructBase):
        try:
            return "{" + v.export_text() + "}"
        except Exception:
            return str(v)
    return str(v)

SKIP = re.compile(r"(material|color|opacity|glow|ease|anim|sway|hand_|_hand|mesh|decal|icon|layout|widget|sort_priority|preview|visual|vfx|sound|fx_|_fx|tint|recoil|reload|spin|log|debug|slash_|bar_|_bar|hit_reaction|tilt|lean|hop|wobble|_class$)")
_base = {}
def own_props(obj, base_cls, keep_all=False):
    if base_cls not in _base:
        _base[base_cls] = set(dir(base_cls))
    res = []
    for n in dir(obj):
        if n.startswith("_") or n in _base[base_cls]:
            continue
        if not keep_all and SKIP.search(n):
            continue
        v = safe(obj, n)
        if v is None or callable(v):
            continue
        if isinstance(v, (bool, int, float, str, unreal.Name, unreal.Text, unreal.Object, unreal.Array, unreal.EnumBase, unreal.StructBase)):
            if isinstance(v, unreal.Array) and len(v) > 8:
                continue
            res.append("%s=%s" % (n, fmt(v)))
    return res

sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
def components(bp_path, base=unreal.ActorComponent, only=None):
    bp = unreal.EditorAssetLibrary.load_asset(bp_path)
    if not bp:
        p("   (introuvable %s)" % bp_path)
        return
    for h in sds.k2_gather_subobject_data_for_blueprint(bp):
        d = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h)
        o = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(d)
        if o is None or not isinstance(o, unreal.ActorComponent):
            continue
        if only and not isinstance(o, only):
            continue
        p("      [%s] %s" % (o.get_class().get_name(), o.get_name().replace("_GEN_VARIABLE", "")))
        p("         " + " | ".join(own_props(o, base)))

def weapon(wc):
    if not wc:
        return
    path = wc.get_path_name().rsplit(".", 1)[0]
    p("    arme %s" % nm(wc))
    components(path, only=unreal.YSAbilityComponent)
    try:
        components(path, only=unreal.YSFoeRangedAction)
    except Exception:
        pass

def foe(path):
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if not cls:
        p("== introuvable " + path)
        return
    cdo = unreal.get_default_object(cls)
    p("=" * 10 + " " + path.rsplit("/", 1)[1] + " (parent " + nm(cls.get_super_class() if hasattr(cls, "get_super_class") else None) + ")")
    p("   " + " | ".join(own_props(cdo, unreal.Character)))
    ws = safe(cdo, "weapon_set")
    if ws:
        p("   SET %s R=%s L=%s" % (nm(ws), nm(safe(ws, "right_hand_weapon")), nm(safe(ws, "left_hand_weapon"))))
        seen = set()
        for side in ["right_hand_weapon", "left_hand_weapon"]:
            w = safe(ws, side)
            if w and nm(w) not in seen:
                seen.add(nm(w)); weapon(w)

ROOT = "/Game/Placeable/Character/Foe/"
for f in ["Base/FoeCharacter_Skull", "Base/FoeCharacter_Skull_Gun", "Base/FoeCharacter_Skull_Bomb", "Base/FoeCharacter_Skull_Nemesis",
          "Elites/FoeCharacter_Skull_Elite", "Elites/FoeCharacter_Skull_Gun_Elite",
          "Boss/FoeCharacter_Skull_Boss", "Boss/FoeCharacter_Skull_Gun_Boss",
          "Ships/FoeCharacter_Galion_Captain", "Ships/FoeCharacter_JollyHull",
          "Kraken/FoeCharacter_KrakenHead", "Kraken/FoeCharacter_KrakenTentacle_Range", "Kraken/FoeCharacter_KrakenTentacle_Melee", "Kraken/FoeCharacter_KrakenHead_Final"]:
    try:
        foe(ROOT + f)
    except Exception as e:
        p("!! %s : %s" % (f, e))

p("=" * 10 + " ARENE")
try:
    cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(ROOT + "Kraken/BP_KrakenArena"))
    p("   " + " | ".join(own_props(cdo, unreal.Actor)))
except Exception as e:
    p("!! arène : %s" % e)

# Bateaux de NME
reg = unreal.AssetRegistryHelpers.get_asset_registry()
for a in reg.get_assets_by_path("/Game/Placeable/Ship", recursive=True):
    n = str(a.asset_name)
    if not n.startswith("BP_NME") and "Jolly" not in n:
        continue
    try:
        cls = unreal.EditorAssetLibrary.load_blueprint_class(str(a.package_name))
        cdo = unreal.get_default_object(cls)
        p("=" * 10 + " BATEAU " + n)
        p("   " + " | ".join(own_props(cdo, unreal.Actor))[:4000])
    except Exception as e:
        p("!! %s : %s" % (n, e))

# Batailles navales
for a in reg.get_assets_by_path("/Game/Core/NavalBattles", recursive=True):
    dt = unreal.EditorAssetLibrary.load_asset(str(a.package_name))
    if isinstance(dt, unreal.DataTable):
        p("=" * 10 + " DT " + str(a.asset_name))
        try:
            rows = unreal.DataTableFunctionLibrary.get_data_table_column_names(dt)
            p("   colonnes " + ", ".join(str(r) for r in rows))
            for c in rows:
                vals = unreal.DataTableFunctionLibrary.get_data_table_column_as_string(dt, c)
                p("   %s = %s" % (c, list(vals)))
        except Exception as e:
            p("!! %s" % e)

# FlowMod : overtime, nuits, parties
for a in reg.get_assets_by_path("/Game", recursive=True):
    if str(a.asset_name) == "BP_FlowMod":
        cdo = unreal.get_default_object(unreal.EditorAssetLibrary.load_blueprint_class(str(a.package_name)))
        p("=" * 10 + " BP_FlowMod")
        for prop in own_props(cdo, unreal.GameModeBase):
            if re.search(r"(overtime|night|part2|part_2|naval|galion|nemesis|kraken|elite|boss|day)", prop.split("=")[0]):
                p("   " + prop[:1500])
        break

with open(unreal.Paths.project_saved_dir() + "Logs/read_foe_sheets.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out))
