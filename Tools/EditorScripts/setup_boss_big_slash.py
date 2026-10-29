"""
Boss armor / bouclier de boss de l'élite : son attaque en plus « Boss Big Slash ».
  - /Game/Placeable/Weapons/Foe/BP_NME_EliteGreatSword : enfant de BP_NME_GreatSword (visuel, Auto swing hérités)
    + composant « BossBigSlash » (UYSBossBigSlashAbility) : réglages copiés du Dashing Slash de BP_BattleSword (Swordsman),
    puis préparation (Step Back Duration) 2 s, recharge 10 s, HitTags = ceux du Auto swing de la NME great sword (Team.Player).
  - /Game/Core/WeaponSets/DA_WeaponSet_NME_EliteGreatSword : copie du set de la NME great sword avec cette arme
    -> FoeCharacter_Skull_Elite (le Skull Nemesis garde la NME great sword sans l'attaque en plus).
  - /Game/Core/UI/WBP_EliteHealth (enfant de YSEliteHealthWidget, réglages Layout) -> BP_YSPlayerController.
Ne recrée rien qui existe (les réglages de Paul sont conservés).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_boss_big_slash.py"
Rapport : Saved/Logs/setup_boss_big_slash.txt
"""
import os
import unreal

FOE_WEAPON_DIR = "/Game/Placeable/Weapons/Foe"
GREAT_SWORD_BP = f"{FOE_WEAPON_DIR}/BP_NME_GreatSword"
ELITE_SWORD_NAME = "BP_NME_EliteGreatSword"
ELITE_SWORD_BP = f"{FOE_WEAPON_DIR}/{ELITE_SWORD_NAME}"
BATTLE_SWORD_BP = "/Game/Placeable/Weapons/Player/Wp_Melee/BP_BattleSword"
GREAT_SWORD_SET = "/Game/Core/WeaponSets/DA_WeaponSet_NME_GreatSword"
ELITE_SET = "/Game/Core/WeaponSets/DA_WeaponSet_NME_EliteGreatSword"
ELITE_BP = "/Game/Placeable/Character/Foe/FoeCharacter_Skull_Elite"
UI_DIR = "/Game/Core/UI"
ELITE_WIDGET = f"{UI_DIR}/WBP_EliteHealth"
PC_BP = "/Game/Core/Player/BP_YSPlayerController"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
ys = unreal.YSEditorScriptLibrary
report = []


def log(msg):
    unreal.log(f"[setup_boss_big_slash] {msg}")
    report.append(msg)


def components(bp):
    result = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if obj:
            result.append((handle, obj))
    return result


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


try:
    # réglages du Swordsman et tags de l'arme de NME
    dash_source = next((obj for _, obj in components(assets.load_asset(BATTLE_SWORD_BP)) if isinstance(obj, unreal.YSDashingSlashAbility)), None)
    if dash_source is None:
        raise RuntimeError("pas de Dashing Slash dans BP_BattleSword")
    foe_swing = next((obj for _, obj in components(assets.load_asset(GREAT_SWORD_BP))
                      if isinstance(obj, unreal.YSAutoSwingAbility) and not isinstance(obj, unreal.YSDashingSlashAbility)), None)
    if foe_swing is None:
        raise RuntimeError("pas de Auto swing dans BP_NME_GreatSword")

    # arme de l'élite : enfant de la NME great sword
    if assets.does_asset_exist(ELITE_SWORD_BP):
        elite_sword = assets.load_asset(ELITE_SWORD_BP)
        log(f"existe déjà : {ELITE_SWORD_BP}")
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", assets.load_blueprint_class(GREAT_SWORD_BP))
        elite_sword = tools.create_asset(ELITE_SWORD_NAME, FOE_WEAPON_DIR, unreal.Blueprint, factory)
        log(f"créé : {ELITE_SWORD_BP}")

    slash = next((obj for _, obj in components(elite_sword) if isinstance(obj, unreal.YSBossBigSlashAbility)), None)
    if slash is None:
        root = subsystem.k2_gather_subobject_data_for_blueprint(elite_sword)[0]
        params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.YSBossBigSlashAbility, blueprint_context=elite_sword)
        handle, fail = subsystem.add_new_subobject(params)
        if not lib.is_handle_valid(handle):
            raise RuntimeError(f"ajout du Boss Big Slash impossible : {fail}")
        subsystem.rename_subobject(handle, unreal.Text("BossBigSlash"))
        slash = lib.get_object(lib.get_data(handle))
        ys.copy_editable_properties(dash_source, slash)
        slash.set_editor_property("display_name", "Boss Big Slash")
        slash.set_editor_property("hit_tags", foe_swing.get_editor_property("hit_tags"))
        slash.set_editor_property("step_back_duration", 2.0)
        slash.set_editor_property("cooldown", 10.0)
        slash.set_editor_property("akimbo_fire_delay", 0.0)
        log("Boss Big Slash ajouté (réglages du Dashing Slash de BP_BattleSword, préparation 2 s, recharge 10 s, touche les joueurs)")
    unreal.BlueprintEditorLibrary.compile_blueprint(elite_sword)
    cdo(ELITE_SWORD_BP).set_editor_property("display_name", "NME elite great sword")
    assets.save_asset(ELITE_SWORD_BP, only_if_is_dirty=False)

    # set d'arme de l'élite
    if assets.does_asset_exist(ELITE_SET):
        log(f"existe déjà : {ELITE_SET}")
    else:
        assets.duplicate_asset(GREAT_SWORD_SET, ELITE_SET)
        log(f"créé : {ELITE_SET} (copie de {GREAT_SWORD_SET})")
    elite_set = assets.load_asset(ELITE_SET)
    elite_set.set_editor_property("right_hand_weapon", assets.load_blueprint_class(ELITE_SWORD_BP))
    elite_set.set_editor_property("display_name", "NME elite great sword")
    assets.save_asset(ELITE_SET, only_if_is_dirty=False)

    elite = cdo(ELITE_BP)
    elite.set_editor_property("weapon_set", elite_set)
    assets.save_asset(ELITE_BP, only_if_is_dirty=False)
    log(f"{ELITE_BP} : set {ELITE_SET}")

    # barre de l'élite dans le HUD (réglages Layout)
    if not assets.does_asset_exist(ELITE_WIDGET):
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", unreal.YSEliteHealthWidget)
        tools.create_asset("WBP_EliteHealth", UI_DIR, unreal.WidgetBlueprint, factory)
        assets.save_asset(ELITE_WIDGET, only_if_is_dirty=False)
        log(f"créé : {ELITE_WIDGET}")
    cdo(PC_BP).set_editor_property("elite_health_widget_class", assets.load_blueprint_class(ELITE_WIDGET))
    assets.save_asset(PC_BP, only_if_is_dirty=False)

    # relecture
    for _, obj in components(assets.load_asset(ELITE_SWORD_BP)):
        if isinstance(obj, unreal.YSBossBigSlashAbility):
            log(f"relu : {obj.get_name()} préparation {obj.get_editor_property('step_back_duration')} s, dash {obj.get_editor_property('dash_duration')} s, "
                f"distance {obj.get_editor_property('dash_distance')}, portée {obj.get_editor_property('trigger_range')}, recharge {obj.get_editor_property('cooldown')} s, "
                f"tags {obj.get_editor_property('hit_tags')}")
    log(f"relu : élite set {cdo(ELITE_BP).get_editor_property('weapon_set').get_name()}, "
        f"widget {cdo(PC_BP).get_editor_property('elite_health_widget_class').get_name()}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_boss_big_slash.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
