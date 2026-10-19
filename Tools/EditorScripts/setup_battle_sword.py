"""
BattleSword : /Game/Placeable/Weapons/BP_BattleSword, enfant de BP_Epee (Coup d'sabre en Auto atk, hérité)
+ un composant « DashingSlash » (UYSDashingSlashAbility, Ability) : reprend l'animation, les timings, la hitbox de debug
et les tags du Coup d'sabre de l'épée ; 75 dégâts, recharge 5 s, akimbo 0 s.
DA_WeaponSet_AkimboSabre : deux BP_BattleSword (nom affiché « Akimbo BattleSword »).
Ne recrée pas l'arme ni le composant s'ils existent (les réglages de Paul sont conservés).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_battle_sword.py"
Rapport : Saved/Logs/setup_battle_sword.txt
"""
import os
import unreal

WEAPON_DIR = "/Game/Placeable/Weapons"
SWORD_BP = f"{WEAPON_DIR}/BP_Epee"
NAME = "BP_BattleSword"
BATTLE_BP = f"{WEAPON_DIR}/{NAME}"
WEAPON_SET = "/Game/Core/WeaponSets/DA_WeaponSet_AkimboSabre"
COPIED = ["hit_tags", "show_hitbox", "hitbox_color", "windup_duration", "swing_duration", "recover_duration",
          "windup_hand_location", "windup_hand_rotation", "swing_end_hand_location", "swing_end_hand_rotation",
          "circular_swing", "windup_ease_exponent", "swing_ease_exponent", "recover_ease_exponent"]

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_battle_sword] {msg}")
    report.append(msg)


def components(bp):
    """(handle, objet gabarit) de chaque composant du Blueprint (hérités compris)"""
    result = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if obj:
            result.append((handle, obj))
    return result


try:
    sword = assets.load_asset(SWORD_BP)
    sabre = next((obj for _, obj in components(sword)
                  if isinstance(obj, unreal.YSCoupDSabreAbility) and not isinstance(obj, unreal.YSDashingSlashAbility)), None)
    if sabre is None:
        raise RuntimeError("pas de Coup d'sabre dans BP_Epee")
    log(f"Coup d'sabre de l'épée : {sabre.get_editor_property('damage')} dégâts, préparation {sabre.get_editor_property('windup_duration')} s, "
        f"coupe {sabre.get_editor_property('swing_duration')} s, retour {sabre.get_editor_property('recover_duration')} s")

    if assets.does_asset_exist(BATTLE_BP):
        battle = assets.load_asset(BATTLE_BP)
        log(f"existe déjà : {BATTLE_BP}")
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", assets.load_blueprint_class(SWORD_BP))
        battle = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, WEAPON_DIR, unreal.Blueprint, factory)
        log(f"créé : {BATTLE_BP}")

    dash = next((obj for _, obj in components(battle) if isinstance(obj, unreal.YSDashingSlashAbility)), None)
    if dash is None:
        root = subsystem.k2_gather_subobject_data_for_blueprint(battle)[0]
        params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.YSDashingSlashAbility, blueprint_context=battle)
        handle, fail = subsystem.add_new_subobject(params)
        if not lib.is_handle_valid(handle):
            raise RuntimeError(f"ajout du Dashing Slash impossible : {fail}")
        subsystem.rename_subobject(handle, unreal.Text("DashingSlash"))
        dash = lib.get_object(lib.get_data(handle))
        for prop in COPIED:
            dash.set_editor_property(prop, sabre.get_editor_property(prop))
        dash.set_editor_property("damage", 75.0)
        dash.set_editor_property("cooldown", 5.0)
        dash.set_editor_property("akimbo_fire_delay", 0.0)
        log("Dashing Slash ajouté (réglages d'animation copiés du Coup d'sabre)")

    unreal.BlueprintEditorLibrary.compile_blueprint(battle)
    weapon = unreal.get_default_object(assets.load_blueprint_class(BATTLE_BP))
    weapon.set_editor_property("display_name", "BattleSword")
    assets.save_asset(BATTLE_BP, only_if_is_dirty=False)

    # set d'arme de l'upgrade de classe « 2 sabres » : deux BattleSword
    weapon_set = assets.load_asset(WEAPON_SET)
    battle_class = assets.load_blueprint_class(BATTLE_BP)
    weapon_set.set_editor_property("right_hand_weapon", battle_class)
    weapon_set.set_editor_property("left_hand_weapon", battle_class)
    weapon_set.set_editor_property("display_name", "Akimbo BattleSword")
    assets.save_asset(WEAPON_SET, only_if_is_dirty=False)

    # relecture
    for _, obj in components(assets.load_asset(BATTLE_BP)):
        if isinstance(obj, unreal.YSAbilityComponent):
            log(f"relu : {obj.get_name()} ({obj.get_class().get_name()}) dégâts {obj.get_editor_property('damage')}, "
                f"recharge {obj.get_editor_property('cooldown')} s, akimbo {obj.get_editor_property('akimbo_fire_delay')} s")
    check = assets.load_asset(WEAPON_SET)
    log(f"relu : {WEAPON_SET} « {check.get_editor_property('display_name')} » "
        f"{check.get_editor_property('right_hand_weapon').get_name()} / {check.get_editor_property('left_hand_weapon').get_name()}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_battle_sword.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
