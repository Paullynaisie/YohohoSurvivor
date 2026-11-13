# Upgrades d'arme du Gunslinger (2026-11-13, demande de Paul) :
# - CircleBullet : /Game/Placeable/AMMO/CircleBulletAMMO (enfant de PetoirAMMO, plus grosse) et LesserCircleBulletAMMO (enfant de
#   PetoirAMMO_AutoShot, plus petite, portée LESSER_RANGE) ; réglages du cercle sur le personnage (3C|CircleBullet, défauts C++) ;
# - Ricochet : 3 ricochets au déblocage, ++ = +2, 2 Golden (Prise de vitesse, Bullet Hell) ;
# - « The Sword was Better » : BP_SwordWasBetterPetoir (copie de BP_BattlePetoir : Auto shot 100, Rolling shot 150 en CircleBullet
#   level 3, laisse la place au Mark for Delete), BP_SwordWasBetterSword (copie de BP_Epee : Auto swing 50 + Mark for Delete en
#   sommeil, réglages copiés de celui de BP_GunSword), set DA_WeaponSet_SwordWasBetter, upgrade DA_Up_Weapon_SwordWasBetter
#   (2 Golden : Stylish Sequence, Mark for Delete), ajoutée en fin de pool de Mouss, JohnnyPlaytest et HiredMouss.
# Relançable : ne recrée rien qui existe ; les réglages listés ici sont réappliqués.
import os
import unreal

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
ys = unreal.YSEditorScriptLibrary
T = unreal.Text

AMMO = "/Game/Placeable/AMMO"
PETOIR_AMMO = f"{AMMO}/PetoirAMMO"
AUTO_AMMO = f"{AMMO}/PetoirAMMO_AutoShot"
CIRCLE_AMMO = f"{AMMO}/CircleBulletAMMO"
LESSER_AMMO = f"{AMMO}/LesserCircleBulletAMMO"
CIRCLE_SCALE = 1.4   # CircleBullet : 40 % plus grosse que PetoirAMMO (choix de Claude)
LESSER_SCALE = 0.7   # LesserCircleBullet : 30 % plus petite que la balle de l'Auto shot (choix de Claude)
LESSER_RANGE = 800.0  # portée d'une LesserCircleBullet en cm (choix de Claude) : durée de vie = portée / vitesse

MELEE = "/Game/Placeable/Weapons/Player/Wp_Melee"
GUNS = "/Game/Placeable/Weapons/Player/Wp_Guns"
EPEE = f"{MELEE}/BP_Epee"
GUN_SWORD = f"{MELEE}/BP_GunSword"
BATTLE_PETOIR = f"{GUNS}/BP_BattlePetoir"
SWB_PETOIR = f"{GUNS}/BP_SwordWasBetterPetoir"
SWB_SWORD = f"{MELEE}/BP_SwordWasBetterSword"
SETS = "/Game/Core/WeaponSets"
AKIMBO_SET = f"{SETS}/DA_WeaponSet_AkimboBattlePetoir"
SWB_SET = f"{SETS}/DA_WeaponSet_SwordWasBetter"
UP_WEAPONS = "/Game/Core/Upgrades/Up_Character/Up_Weapons"
RICOCHET = f"{UP_WEAPONS}/DA_Up_Weapon_Ricochet"
SWB_UPGRADE = f"{UP_WEAPONS}/DA_Up_Weapon_SwordWasBetter"
CHARACTERS = ["/Game/Placeable/Character/Player/PlayerCharacter_mouss", "/Game/Placeable/Character/Player/PlayerCharacter_JohnnyPlaytest",
              "/Game/Placeable/Character/Player/PlayerCharacter_HiredMouss"]

report = []


def log(msg):
    report.append(str(msg))
    unreal.log(f"[setup_circle_bullet] {msg}")


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


def child_bp(parent, folder, name):
    path = f"{folder}/{name}"
    if assets.does_asset_exist(path):
        log(f"{path} : existe déjà")
        return assets.load_asset(path), False
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", assets.load_blueprint_class(parent))
    bp = tools.create_asset(name, folder, unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    log(f"{path} : créé (enfant de {parent})")
    return bp, True


def save_bp(bp, path):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(path, only_if_is_dirty=False)


try:
    # --- AMMO : CircleBullet (plus grosse) et LesserCircleBullet (plus petite, portée courte) ---
    base_scale = unreal.get_default_object(assets.load_blueprint_class(PETOIR_AMMO)).get_editor_property("mesh").get_editor_property("relative_scale3d")
    circle, created = child_bp(PETOIR_AMMO, AMMO, "CircleBulletAMMO")
    if created:
        unreal.get_default_object(circle.generated_class()).get_editor_property("mesh").set_editor_property("relative_scale3d", base_scale * CIRCLE_SCALE)
        save_bp(circle, CIRCLE_AMMO)
    auto_scale = unreal.get_default_object(assets.load_blueprint_class(AUTO_AMMO)).get_editor_property("mesh").get_editor_property("relative_scale3d")
    lesser, created = child_bp(AUTO_AMMO, AMMO, "LesserCircleBulletAMMO")
    if created:
        lesser_cdo = unreal.get_default_object(lesser.generated_class())
        lesser_cdo.get_editor_property("mesh").set_editor_property("relative_scale3d", auto_scale * LESSER_SCALE)
        speed = lesser_cdo.get_editor_property("speed")
        lesser_cdo.set_editor_property("life_time", LESSER_RANGE / max(speed, 1.0))
        save_bp(lesser, LESSER_AMMO)

    # --- Petoir : copie du BattlePetoir (Auto shot 100, Rolling shot 150 en CircleBullet level 3) ---
    petoir, _ = duplicate(BATTLE_PETOIR, GUNS, "BP_SwordWasBetterPetoir")
    rolling = ys.get_inherited_component_override(petoir, "RollingShot")
    rolling.set_editor_property("damage", 150.0)
    rolling.set_editor_property("circle_bullet_level", 3)
    rolling.set_editor_property("projectile_class", assets.load_blueprint_class(CIRCLE_AMMO))
    rolling.set_editor_property("replaced_by_mark_for_delete", True)
    _, auto_shot = component(petoir, "AutoShot")
    auto_shot.set_editor_property("damage", 100.0)
    # demande de Paul : une balle par salve, pas de rafale
    auto_shot.set_editor_property("bullets_per_burst", 1)
    auto_shot.set_editor_property("burst_duration", 0.0)
    unreal.get_default_object(petoir.generated_class()).set_editor_property("display_name", T("Petoir"))
    save_bp(petoir, SWB_PETOIR)

    # --- Sword : copie de BP_Epee (Auto swing 50) + Mark for Delete en sommeil (réglages de celui de BP_GunSword) ---
    sword, _ = duplicate(EPEE, MELEE, "BP_SwordWasBetterSword")
    _, swing = component(sword, "CoupDSabre")
    if swing is None:
        swing = ys.get_inherited_component_override(sword, "CoupDSabre")
    swing.set_editor_property("damage", 50.0)
    swing.set_editor_property("base_damage_bonus_stat", "SwordWasBetterDamage")
    # demande de Paul : la Sword attaque toute seule, indépendamment du clic (comme le Pistolet de The Gun was better)
    swing.set_editor_property("auto_attack_without_click", True)
    _, mark = component(sword, "DashingSlash")
    if mark is None:
        mark = add_component(sword, unreal.YSMarkForDeleteAbility, "DashingSlash")
        _, gun_sword_mark = component(assets.load_asset(GUN_SWORD), "DashingSlash")
        count = ys.copy_editable_properties(gun_sword_mark, mark)
        log(f"BP_SwordWasBetterSword : Mark for Delete ajouté, {count} réglages copiés de celui de BP_GunSword ({gun_sword_mark.get_class().get_name()})")
    mark.set_editor_property("only_as_mark_for_delete", True)
    mark.set_editor_property("damage", 180.0)
    mark.set_editor_property("mark_for_delete_cooldown", 20.0)
    mark.set_editor_property("shot_count", 3)
    mark.set_editor_property("shot_damage", 100.0)
    unreal.get_default_object(sword.generated_class()).set_editor_property("display_name", T("Sword"))
    save_bp(sword, SWB_SWORD)

    # --- Set d'arme : copie du set Akimbo Battle Petoir, Petoir à droite (main principale), Sword à gauche ---
    weapon_set, _ = duplicate(AKIMBO_SET, SETS, "DA_WeaponSet_SwordWasBetter")
    weapon_set.set_editor_property("display_name", T("The Sword was Better"))
    weapon_set.set_editor_property("right_hand_weapon", assets.load_blueprint_class(SWB_PETOIR))
    weapon_set.set_editor_property("left_hand_weapon", assets.load_blueprint_class(SWB_SWORD))
    assets.save_asset(SWB_SET, only_if_is_dirty=False)

    # --- Ricochet : 3 de base, ++ +2, 2 Golden ---
    ricochet = assets.load_asset(RICOCHET)
    ricochet.set_editor_property("unlock_ricochets", 3)
    ricochet.set_editor_property("bonus_silver_value", 2.0)
    ricochet.set_editor_property("bonus_gold_value", 2.0)
    ricochet.set_editor_property("bullet_hell_circle_level", 1)
    ricochet.set_editor_property("unlock_description", T("Après avoir touché, la balle ricoche\nvers le NME le plus proche (3 fois)"))
    ricochet.set_editor_property("bonus_description", T("La balle ricoche 2 fois de plus"))
    ricochet.set_editor_property("super_name", T("Prise de vitesse"))
    ricochet.set_editor_property("golden_variants", [
        golden("Prise de vitesse", "+25 % de dégâts à chaque ricochet"),
        golden("Bullet Hell", "Le Rolling shot akimbo ne tire plus qu'une balle,\nqui déclenche une CircleBullet level 1\nà chaque impact"),
    ])
    assets.save_asset(RICOCHET, only_if_is_dirty=False)

    # --- Upgrade « The Sword was Better » ---
    if assets.does_asset_exist(SWB_UPGRADE):
        upgrade = assets.load_asset(SWB_UPGRADE)
        log(f"{SWB_UPGRADE} : existe déjà")
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.YSSwordWasBetterUpgrade)
        upgrade = tools.create_asset("DA_Up_Weapon_SwordWasBetter", UP_WEAPONS, unreal.YSSwordWasBetterUpgrade, factory)
        for prop in ("unlock_shards", "bonus_shards", "unlock_weight", "bonus_weight", "super_weight", "icon"):
            upgrade.set_editor_property(prop, ricochet.get_editor_property(prop))
        log(f"{SWB_UPGRADE} : créé (shards, poids et icône du Ricochet)")
    upgrade.set_editor_property("unlock_weapon_set", weapon_set)
    upgrade.set_editor_property("required_weapon_classes", [assets.load_blueprint_class(BATTLE_PETOIR), assets.load_blueprint_class(SWB_PETOIR)])
    upgrade.set_editor_property("stylish_circle_level", 3)
    upgrade.set_editor_property("golden_variants", [
        golden("Stylish Sequence", "Un NME touché par l'Auto atk du Petoir,\nl'Auto atk de la Sword et l'Ability\ndéclenche une CircleBullet level 3"),
        golden("Mark for Delete", "L'Ability devient Mark for Delete (recharge 20 s) :\n3 roulades + tir (100), puis un dash and slash (180)\nvers chaque NME touché"),
    ])
    assets.save_asset(SWB_UPGRADE, only_if_is_dirty=False)

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
    for path in (CIRCLE_AMMO, LESSER_AMMO):
        cdo = unreal.get_default_object(assets.load_blueprint_class(path))
        log(f"relu {path} : échelle {cdo.get_editor_property('mesh').get_editor_property('relative_scale3d')}, vitesse {cdo.get_editor_property('speed')}, "
            f"durée de vie {cdo.get_editor_property('life_time'):.3f}, couleur du joueur {cdo.get_editor_property('use_player_color')}")
    petoir = assets.load_asset(SWB_PETOIR)
    rolling = ys.get_inherited_component_override(petoir, "RollingShot")
    log(f"relu Petoir.RollingShot : dégâts {rolling.get_editor_property('damage')}, CircleBullet {rolling.get_editor_property('circle_bullet_level')}, "
        f"projectile {rolling.get_editor_property('projectile_class')}, remplacé par Mark for Delete {rolling.get_editor_property('replaced_by_mark_for_delete')}")
    _, comp = component(petoir, "AutoShot")
    log(f"relu Petoir.AutoShot : dégâts {comp.get_editor_property('damage')}, {comp.get_editor_property('bullets_per_burst')} balle(s) en {comp.get_editor_property('burst_duration')} s, "
        f"recharge {comp.get_editor_property('cooldown')} s, {comp.get_editor_property('shoot_system')}")
    _, comp = component(assets.load_asset(BATTLE_PETOIR), "AutoShot")
    log(f"relu BP_BattlePetoir.AutoShot (doit rester inchangé) : dégâts {comp.get_editor_property('damage')}")
    sword = assets.load_asset(SWB_SWORD)
    _, comp = component(sword, "CoupDSabre")
    log(f"relu Sword.CoupDSabre : dégâts {comp.get_editor_property('damage')}, bonus {comp.get_editor_property('base_damage_bonus_stat')}, "
        f"sans clic {comp.get_editor_property('auto_attack_without_click')}")
    _, comp = component(sword, "DashingSlash")
    log(f"relu Sword.DashingSlash : {comp.get_class().get_name()}, dégâts {comp.get_editor_property('damage')}, sommeil {comp.get_editor_property('only_as_mark_for_delete')}, "
        f"tir {comp.get_editor_property('shot_projectile_class')}, roulade {comp.get_editor_property('roll_duration')} s")
    _, comp = component(assets.load_asset(EPEE), "CoupDSabre")
    log(f"relu BP_Epee.CoupDSabre (doit rester inchangé) : dégâts {comp.get_editor_property('damage')}")
    ricochet = assets.load_asset(RICOCHET)
    log(f"relu Ricochet : {ricochet.get_editor_property('unlock_ricochets')} + {ricochet.get_editor_property('bonus_gold_value')} par ++, "
        f"Golden {[str(g.get_editor_property('name')) for g in ricochet.get_editor_property('golden_variants')]}")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_circle_bullet.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
