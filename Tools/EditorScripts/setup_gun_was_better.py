# Upgrades d'arme du Swordsman (2026-11-13, demande de Paul) :
# - Bloodlust : 2 Golden (Critical Blood = l'ancien Golden bloodlust, Blood Swords), et la Sword de « The Gun was better » concernée ;
# - « The Gun was better » : BP_GunSword (copie de BP_BattleSword, Auto swing 80, Ability Mark for Delete = Dashing Slash 180
#   tant que la Golden n'est pas prise), BP_GunPistol (copie de BP_Pistolet, Auto shot du BattlePetoir : 40 dégâts, visée assistée
#   +/- 45°, Pierce piloté par la Melee Range), set DA_WeaponSet_GunWasBetter, upgrade DA_Up_Weapon_GunWasBetter (2 Golden :
#   Stylish Sequence, Mark for Delete), ajoutée en fin de pool de Mouss, JohnnyPlaytest et HiredMouss.
# Relançable : ne recrée rien qui existe ; les réglages listés ici sont réappliqués.
import os
import unreal

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
ys = unreal.YSEditorScriptLibrary
T = unreal.Text

MELEE = "/Game/Placeable/Weapons/Player/Wp_Melee"
GUNS = "/Game/Placeable/Weapons/Player/Wp_Guns"
BATTLE_SWORD = f"{MELEE}/BP_BattleSword"
GUN_SWORD = f"{MELEE}/BP_GunSword"
PISTOLET = f"{GUNS}/BP_Pistolet"
BATTLE_PETOIR = f"{GUNS}/BP_BattlePetoir"
GUN_PISTOL = f"{GUNS}/BP_GunPistol"
SETS = "/Game/Core/WeaponSets"
SABRE_PISTOLET_SET = f"{SETS}/DA_WeaponSet_SabrePistolet"
GUN_SET = f"{SETS}/DA_WeaponSet_GunWasBetter"
UP_WEAPONS = "/Game/Core/Upgrades/Up_Character/Up_Weapons"
BLOODLUST = f"{UP_WEAPONS}/DA_Up_Weapon_Bloodlust"
GUN_UPGRADE = f"{UP_WEAPONS}/DA_Up_Weapon_GunWasBetter"
CHARACTERS = ["/Game/Placeable/Character/Player/PlayerCharacter_mouss", "/Game/Placeable/Character/Player/PlayerCharacter_JohnnyPlaytest",
              "/Game/Placeable/Character/Player/PlayerCharacter_HiredMouss"]

report = []


def log(msg):
    report.append(str(msg))
    unreal.log(f"[setup_gun_was_better] {msg}")


def handles(bp):
    return sds.k2_gather_subobject_data_for_blueprint(bp)


def component(bp, name):
    for h in handles(bp):
        data = lib.get_data(h)
        if str(lib.get_variable_name(data)) == name:
            return h, lib.get_object(data)
    return None, None


def add_component(bp, cls, name):
    handle, fail = sds.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=handles(bp)[0], new_class=cls, blueprint_context=bp))
    if not fail.is_empty():
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    sds.rename_subobject(handle, T(name))
    return lib.get_object(lib.get_data(handle))


def golden(name, desc):
    variant = unreal.YSGoldenMoveAction()
    variant.set_editor_property("name", T(name))
    variant.set_editor_property("description", T(desc))
    return variant


def duplicate(source, folder, name):
    path = f"{folder}/{name}"
    if assets.does_asset_exist(path):
        log(f"{path} : existe déjà")
        return assets.load_asset(path), False
    asset = tools.duplicate_asset(name, folder, assets.load_asset(source))
    log(f"{path} : copie de {source}")
    return asset, True


def save_bp(bp, path):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(path, only_if_is_dirty=False)


try:
    # --- Sword : copie de la BattleSword, Ability Mark for Delete (= Dashing Slash sans la Golden) ---
    sword, created = duplicate(BATTLE_SWORD, MELEE, "BP_GunSword")
    battle = assets.load_asset(BATTLE_SWORD)
    _, battle_dash = component(battle, "DashingSlash")
    pistolet = assets.load_asset(PISTOLET)
    _, rolling = component(pistolet, "RollingShot")
    handle, ability = component(sword, "DashingSlash")
    if ability is not None and not isinstance(ability, unreal.YSMarkForDeleteAbility):
        sds.delete_subobjects(handles(sword)[0], [handle], sword)
        ability = None
        log("BP_GunSword : Dashing Slash de la copie retiré")
    if ability is None:
        ability = add_component(sword, unreal.YSMarkForDeleteAbility, "DashingSlash")
        count = ys.copy_editable_properties(battle_dash, ability)
        log(f"BP_GunSword : Mark for Delete ajouté, {count} réglages copiés du Dashing Slash de la BattleSword")
        # roulades et tirs : ceux du Rolling shot de BP_Pistolet
        for src, dst in (("roll_duration", "roll_duration"), ("roll_speed_multiplier", "roll_speed_multiplier"), ("aim_duration", "aim_duration"),
                         ("aim_move_speed_multiplier", "aim_move_speed_multiplier"), ("roll_anim_turns", "roll_anim_turns"),
                         ("roll_anim_ease_exponent", "roll_anim_ease_exponent"), ("roll_anim_tuck_height", "roll_anim_tuck_height"),
                         ("projectile_class", "shot_projectile_class"), ("assist_aim_distance", "shot_assist_aim_distance"),
                         ("assist_aim_angle", "shot_assist_aim_angle")):
            ability.set_editor_property(dst, rolling.get_editor_property(src))
    ability.set_editor_property("damage", 180.0)
    ability.set_editor_property("mark_for_delete_cooldown", 20.0)
    ability.set_editor_property("shot_count", 3)
    ability.set_editor_property("shot_damage", 100.0)
    # Auto swing hérité de BP_Epee : gabarit surchargé de la copie (le handle de sous-objet renvoie celui du parent)
    ys.get_inherited_component_override(sword, "CoupDSabre").set_editor_property("damage", 80.0)
    unreal.get_default_object(sword.generated_class()).set_editor_property("display_name", T("Sword"))
    save_bp(sword, GUN_SWORD)

    # --- Pistolet : copie de BP_Pistolet, Auto shot du BattlePetoir (40, +/- 45°, Pierce piloté par la Melee Range) ---
    pistol, created = duplicate(PISTOLET, GUNS, "BP_GunPistol")
    handle, old_rolling = component(pistol, "RollingShot")
    if old_rolling is not None:
        sds.delete_subobjects(handles(pistol)[0], [handle], pistol)
        log("BP_GunPistol : Rolling shot retiré (pas d'Ability)")
    _, auto_shot = component(pistol, "AutoShot")
    if auto_shot is None:
        auto_shot = add_component(pistol, unreal.YSAutoShotAbility, "AutoShot")
        _, petoir_auto = component(assets.load_asset(BATTLE_PETOIR), "AutoShot")
        count = ys.copy_editable_properties(petoir_auto, auto_shot)
        log(f"BP_GunPistol : Auto shot ajouté, {count} réglages copiés de celui du BattlePetoir")
    auto_shot.set_editor_property("damage", 40.0)
    auto_shot.set_editor_property("assist_aim_angle", 45.0)
    # demande de Paul : une balle par salve, pas de rafale, recharge 1.35 s
    auto_shot.set_editor_property("bullets_per_burst", 1)
    auto_shot.set_editor_property("burst_duration", 0.0)
    auto_shot.set_editor_property("cooldown", 1.35)
    auto_shot.set_editor_property("shoot_system", unreal.YSShootSystem.PIERCE)
    auto_shot.set_editor_property("pierce_max_hits", 1)
    auto_shot.set_editor_property("pierce_from_melee_range", True)
    auto_shot.set_editor_property("base_damage_bonus_stat", "GunWasBetterDamage")
    unreal.get_default_object(pistol.generated_class()).set_editor_property("display_name", T("Pistolet"))
    save_bp(pistol, GUN_PISTOL)

    # --- Set d'arme : copie du set Sabre et Pistolet (même disposition) ---
    weapon_set, _ = duplicate(SABRE_PISTOLET_SET, SETS, "DA_WeaponSet_GunWasBetter")
    weapon_set.set_editor_property("display_name", T("The Gun was better"))
    weapon_set.set_editor_property("right_hand_weapon", assets.load_blueprint_class(GUN_SWORD))
    weapon_set.set_editor_property("left_hand_weapon", assets.load_blueprint_class(GUN_PISTOL))
    assets.save_asset(GUN_SET, only_if_is_dirty=False)

    # --- Bloodlust : 2 Golden, la Sword concernée aussi ---
    bloodlust = assets.load_asset(BLOODLUST)
    required = list(bloodlust.get_editor_property("required_weapon_classes"))
    gun_sword_class = assets.load_blueprint_class(GUN_SWORD)
    if gun_sword_class not in required:
        required.append(gun_sword_class)
    bloodlust.set_editor_property("required_weapon_classes", required)
    bloodlust.set_editor_property("golden_variants", [
        golden("Critical Blood", "Avec toutes ses BL stacks,\nles Auto swing sont critiques"),
        golden("Blood Swords", "À la fin du pas en arrière de l'Ability,\ntoutes les BL stacks deviennent\nautant de levels de Self Spinning Sword"),
    ])
    assets.save_asset(BLOODLUST, only_if_is_dirty=False)

    # --- Upgrade « The Gun was better » ---
    if assets.does_asset_exist(GUN_UPGRADE):
        upgrade = assets.load_asset(GUN_UPGRADE)
        log(f"{GUN_UPGRADE} : existe déjà")
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.YSGunWasBetterUpgrade)
        upgrade = tools.create_asset("DA_Up_Weapon_GunWasBetter", UP_WEAPONS, unreal.YSGunWasBetterUpgrade, factory)
        for prop in ("unlock_shards", "bonus_shards", "unlock_weight", "bonus_weight", "super_weight"):
            upgrade.set_editor_property(prop, bloodlust.get_editor_property(prop))
        log(f"{GUN_UPGRADE} : créé (shards et poids du Bloodlust)")
    upgrade.set_editor_property("unlock_weapon_set", weapon_set)
    upgrade.set_editor_property("required_weapon_classes", [assets.load_blueprint_class(BATTLE_SWORD), gun_sword_class])
    upgrade.set_editor_property("golden_variants", [
        golden("Stylish Sequence", "Un NME touché par l'Auto atk de la Sword,\nl'Auto atk du Pistolet et l'Ability\nfait apparaître une Spinning Sword level 3"),
        golden("Mark for Delete", "L'Ability devient Mark for Delete (recharge 20 s) :\n3 roulades + tir (100), puis un dash and slash (180)\nvers chaque NME touché"),
    ])
    assets.save_asset(GUN_UPGRADE, only_if_is_dirty=False)

    # --- Pools ---
    for path in CHARACTERS:
        if not assets.does_asset_exist(path):
            log(f"{path} : introuvable")
            continue
        bp = assets.load_asset(path)
        cdo = unreal.get_default_object(bp.generated_class())
        pool = list(cdo.get_editor_property("upgrade_pool"))
        if upgrade in pool:
            log(f"{path} : déjà dans le pool (index {pool.index(upgrade)})")
            continue
        pool.append(upgrade)
        cdo.set_editor_property("upgrade_pool", pool)
        save_bp(bp, path)
        log(f"{path} : ajoutée au pool, index {len(pool) - 1}")

    # --- Relecture ---
    sword = assets.load_asset(GUN_SWORD)
    sword_cdo = unreal.get_default_object(sword.generated_class())
    log(f"relu BP_GunSword.CoupDSabre : dégâts {ys.get_inherited_component_override(sword, 'CoupDSabre').get_editor_property('damage')}")
    _, epee_swing = component(assets.load_asset(f"{MELEE}/BP_Epee"), "CoupDSabre")
    log(f"relu BP_Epee.CoupDSabre (doit rester inchangé) : dégâts {epee_swing.get_editor_property('damage')}")
    for name in ("DashingSlash",):
        _, comp = component(sword, name)
        log(f"relu BP_GunSword.{name} : {comp.get_class().get_name()} dégâts {comp.get_editor_property('damage')}")
    _, comp = component(sword, "DashingSlash")
    log(f"relu Mark for Delete : tir {comp.get_editor_property('shot_projectile_class')}, roulade {comp.get_editor_property('roll_duration')} s, recharge {comp.get_editor_property('mark_for_delete_cooldown')}")
    _, comp = component(assets.load_asset(GUN_PISTOL), "AutoShot")
    log(f"relu BP_GunPistol.AutoShot : dégâts {comp.get_editor_property('damage')}, angle {comp.get_editor_property('assist_aim_angle')}, "
        f"{comp.get_editor_property('bullets_per_burst')} balle(s), rafale {comp.get_editor_property('burst_duration')} s, recharge {comp.get_editor_property('cooldown')} s, "
        f"{comp.get_editor_property('shoot_system')}, pierce {comp.get_editor_property('pierce_max_hits')}, melee range {comp.get_editor_property('pierce_from_melee_range')}, "
        f"stat {comp.get_editor_property('base_damage_bonus_stat')}, projectile {comp.get_editor_property('projectile_class')}")
    log(f"relu Bloodlust : {[str(g.get_editor_property('name')) for g in assets.load_asset(BLOODLUST).get_editor_property('golden_variants')]}")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_gun_was_better.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
