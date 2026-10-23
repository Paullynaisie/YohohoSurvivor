"""
Animations de l'Auto swing, Dégât critique et point critique des NME :
  - Second swing (une attaque auto sur deux) : poses, durées et courbes copiées du swing principal de chaque épée
    (BP_Epee, BP_NME_Sword, BP_NME_GreatSword ; BP_BattleSword hérite de BP_Epee), seulement s'il a encore les valeurs C++
  - DA_Upgrade_CritDamage (UYSCriticalDamageUpgrade, « Dégât critique ») : valeurs copiées de DA_Upgrade_AtkBonus,
    ajouté à la fin des pools de PlayerCharacter_mouss et PlayerCharacter_JohnnyPlaytest (index 14)
  - /Game/Core/UI/WBP_FoeHealthBar (enfant de UYSHealthBarWidget, réglages Layout) : widget de la barre de vie de FoeCharacter_Skull
    (hérité par ses enfants)
Les réglages déjà faits par Paul ne sont pas réécrits.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_swing_variants.py"
Rapport : Saved/Logs/setup_swing_variants.txt
"""
import os
import unreal

SWORDS = ["/Game/Placeable/Weapons/Player/BP_Epee", "/Game/Placeable/Weapons/Foe/BP_NME_Sword", "/Game/Placeable/Weapons/Foe/BP_NME_GreatSword"]
CHILD_SWORDS = ["/Game/Placeable/Weapons/Player/BP_BattleSword"]
UPGRADES = "/Game/Core/Upgrades"
ATK_BONUS = f"{UPGRADES}/DA_Upgrade_AtkBonus"
CRIT_DAMAGE = f"{UPGRADES}/DA_Upgrade_CritDamage"
CHARACTERS = ["/Game/Placeable/Character/Player/PlayerCharacter_mouss", "/Game/Placeable/Character/Player/PlayerCharacter_JohnnyPlaytest"]
FOE_BASE = "/Game/Placeable/Character/Foe/FoeCharacter_Skull"
FOE_CHILDREN = ["/Game/Placeable/Character/Foe/FoeCharacter_Skull_Bomb", "/Game/Placeable/Character/Foe/FoeCharacter_Skull_Gun",
                "/Game/Placeable/Character/Foe/FoeCharacter_Skull_Elite"]
UI_DIR = "/Game/Core/UI"
STAT_COPIED = ["offer_weight", "can_be_bronze", "can_be_silver", "can_be_gold", "max_stacks", "bronze_value", "silver_value", "gold_value"]
POSES = ["windup_hand_location", "windup_hand_rotation", "swing_end_hand_location", "swing_end_hand_rotation", "circular_swing"]
TIMINGS = ["windup_duration", "swing_duration", "recover_duration", "windup_ease_exponent", "swing_ease_exponent", "recover_ease_exponent"]

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_swing_variants] {msg}")
    report.append(msg)


def swing_components(bp):
    found = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if isinstance(obj, unreal.YSAutoSwingAbility) and not isinstance(obj, unreal.YSDashingSlashAbility):
            found.append(obj)
    return found


def same(a, b):
    if isinstance(a, float) or isinstance(b, float):
        return abs(a - b) < 1e-4
    if isinstance(a, unreal.Vector):
        return a.is_near_equal(b, 1e-3) if hasattr(a, "is_near_equal") else (abs(a.x - b.x) + abs(a.y - b.y) + abs(a.z - b.z)) < 1e-3
    if isinstance(a, unreal.Rotator):
        return abs(a.pitch - b.pitch) + abs(a.yaw - b.yaw) + abs(a.roll - b.roll) < 1e-3
    return a == b


def describe_anim(anim):
    return ", ".join(f"{p} {anim.get_editor_property(p)}" for p in POSES + TIMINGS)


try:
    # second swing = copie du swing principal
    default_anim = unreal.YSSwingAnim()
    for path in SWORDS:
        bp = assets.load_asset(path)
        comps = swing_components(bp)
        if not comps:
            raise RuntimeError(f"aucun Auto swing dans {path}")
        for comp in comps:
            anim = comp.get_editor_property("second_swing")
            untouched = all(same(anim.get_editor_property(p), default_anim.get_editor_property(p)) for p in POSES + TIMINGS)
            if untouched:
                for p in POSES + TIMINGS:
                    anim.set_editor_property(p, comp.get_editor_property(p))
                comp.set_editor_property("second_swing", anim)
                log(f"{path.rsplit('/', 1)[1]}.{comp.get_name()} : second swing copié du swing principal")
            else:
                log(f"{path.rsplit('/', 1)[1]}.{comp.get_name()} : second swing déjà réglé, inchangé")
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assets.save_asset(path, only_if_is_dirty=False)
    for path in CHILD_SWORDS:
        bp = assets.load_asset(path)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assets.save_asset(path, only_if_is_dirty=False)

    # Dégât critique
    atk = assets.load_asset(ATK_BONUS)
    if assets.does_asset_exist(CRIT_DAMAGE):
        crit = assets.load_asset(CRIT_DAMAGE)
        log(f"existe déjà : {CRIT_DAMAGE}")
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.YSCriticalDamageUpgrade)
        crit = tools.create_asset("DA_Upgrade_CritDamage", UPGRADES, unreal.YSCriticalDamageUpgrade, factory)
        for prop in STAT_COPIED:
            crit.set_editor_property(prop, atk.get_editor_property(prop))
        log(f"créé : {CRIT_DAMAGE} (valeurs de DA_Upgrade_AtkBonus)")
    assets.save_asset(CRIT_DAMAGE, only_if_is_dirty=False)

    for path in CHARACTERS:
        cdo = unreal.get_default_object(assets.load_blueprint_class(path))
        pool = list(cdo.get_editor_property("upgrade_pool"))
        if crit not in pool:
            pool.append(crit)
            cdo.set_editor_property("upgrade_pool", pool)
            assets.save_asset(path, only_if_is_dirty=False)
            log(f"{path.rsplit('/', 1)[1]} : Dégât critique ajouté au pool (index {len(pool) - 1})")

    # barre de vie des NME : Widget Blueprint réglable
    wbp_path = f"{UI_DIR}/WBP_FoeHealthBar"
    if not assets.does_asset_exist(wbp_path):
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", unreal.YSHealthBarWidget)
        tools.create_asset("WBP_FoeHealthBar", UI_DIR, unreal.WidgetBlueprint, factory)
        log(f"créé : {wbp_path}")
    wbp = assets.load_asset(wbp_path)
    unreal.BlueprintEditorLibrary.compile_blueprint(wbp)
    assets.save_asset(wbp_path, only_if_is_dirty=False)
    wbp_class = assets.load_blueprint_class(wbp_path)

    foe_cdo = unreal.get_default_object(assets.load_blueprint_class(FOE_BASE))
    foe_cdo.get_editor_property("health_bar").set_editor_property("widget_class", wbp_class)
    assets.save_asset(FOE_BASE, only_if_is_dirty=False)

    # relecture
    for path in SWORDS + CHILD_SWORDS:
        for comp in swing_components(assets.load_asset(path)):
            log(f"relu : {path.rsplit('/', 1)[1]}.{comp.get_name()} : second swing ({describe_anim(comp.get_editor_property('second_swing'))}), "
                f"alterné {comp.get_editor_property('alternate_second_swing')}, estoc {comp.get_editor_property('critical_thrust')}")
    crit = assets.load_asset(CRIT_DAMAGE)
    log(f"relu : {crit.get_name()} « {crit.get_editor_property('display_name')} » valeurs {crit.get_editor_property('bronze_value')} / "
        f"{crit.get_editor_property('silver_value')} / {crit.get_editor_property('gold_value')}, max {crit.get_editor_property('max_stacks')}, poids {crit.get_editor_property('offer_weight')}")
    log(f"relu : AtkBonus valeurs {atk.get_editor_property('bronze_value')} / {atk.get_editor_property('silver_value')} / {atk.get_editor_property('gold_value')}")
    for path in CHARACTERS:
        pool = unreal.get_default_object(assets.load_blueprint_class(path)).get_editor_property("upgrade_pool")
        log(f"relu : pool {path.rsplit('/', 1)[1]} : {[(i, u.get_name() if u else None) for i, u in enumerate(pool)]}")
    for path in [FOE_BASE] + FOE_CHILDREN:
        cdo = unreal.get_default_object(assets.load_blueprint_class(path))
        cls = cdo.get_editor_property("health_bar").get_editor_property("widget_class")
        log(f"relu : {path.rsplit('/', 1)[1]} barre de vie {cls.get_name() if cls else None}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_swing_variants.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
