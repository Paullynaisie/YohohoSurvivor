"""Lecture seule : valeurs de la fiche du Mouss (PV par set, regen, stamina, vitesse).
UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="Tools/EditorScripts/read_mouss_sheet.py" -unattended
"""
import unreal

out = []

def p(s):
    out.append(s)
    unreal.log("[MoussSheet] " + s)

def safe(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception as e:
        return "?(%s)" % e

reg = unreal.AssetRegistryHelpers.get_asset_registry()

def find(name):
    for a in reg.get_assets_by_path("/Game", recursive=True):
        if str(a.asset_name) == name:
            return a
    return None

bp = find("PlayerCharacter_mouss")
cdo = None
if bp:
    cls = unreal.EditorAssetLibrary.load_blueprint_class(str(bp.package_name))
    cdo = unreal.get_default_object(cls)
    for prop in ["character_name", "max_health", "base_health_regen", "health_regen_interval", "health_regen_delay",
                 "max_stamina", "stamina_regen_amount", "stamina_regen_delay", "carry_stamina_cost", "swim_stamina_cost",
                 "move_speed", "character_scale", "critical_damage_multiplier", "weapon_set", "passive"]:
        p("Mouss.%s = %s" % (prop, safe(cdo, prop)))
    try:
        mv = cdo.get_editor_property("MoveActionAbility")
        for prop in ["display_name", "stamina_cost", "cooldown"]:
            p("MoveAction.%s = %s" % (prop, safe(mv, prop)))
    except Exception as e:
        p("MoveAction ? %s" % e)

for setname in ["DA_WeaponSet_SabrePistolet", "DA_WeaponSet_AkimboBattlePetoir", "DA_WeaponSet_AkimboSabre"]:
    a = find(setname)
    if not a:
        p("%s introuvable" % setname)
        continue
    s = unreal.EditorAssetLibrary.load_asset(str(a.package_name))
    for prop in ["display_name", "b_override_max_health", "max_health", "right_hand_weapon", "left_hand_weapon", "character_scale_multiplier"]:
        p("%s.%s = %s" % (setname, prop, safe(s, prop)))
    for side in ["right_hand_weapon", "left_hand_weapon"]:
        wc = safe(s, side)
        if not wc or isinstance(wc, str):
            continue
        try:
            wcdo = unreal.get_default_object(wc)
        except Exception:
            continue
        p("  %s : %s (%s)" % (side, wc.get_name(), safe(wcdo, "display_name")))
        comps = []
        try:
            bpgc = wc
            sub = unreal.SubobjectDataSubsystem
        except Exception:
            pass
        for comp in wcdo.get_components_by_class(unreal.ActorComponent) if hasattr(wcdo, "get_components_by_class") else []:
            p("    comp %s" % comp.get_name())

for upname in ["DA_Up_Weapon_Ricochet", "DA_Up_Weapon_Bloodlust", "DA_ClassUpgrade_Mouss2Sabre", "DA_ClassUpgrade_Mouss2Pistol"]:
    a = find(upname)
    if not a:
        p("%s introuvable" % upname)
        continue
    u = unreal.EditorAssetLibrary.load_asset(str(a.package_name))
    for prop in ["display_name", "description", "target_weapon_set", "max_bonus_stacks", "bonus_gold_value", "super_damage_bonus",
                 "max_stacks", "stack_duration", "atk_speed_per_stack"]:
        v = safe(u, prop)
        if not str(v).startswith("?"):
            p("%s.%s = %s" % (upname, prop, v))

with open(unreal.Paths.project_saved_dir() + "Logs/read_mouss_sheet.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(out))
