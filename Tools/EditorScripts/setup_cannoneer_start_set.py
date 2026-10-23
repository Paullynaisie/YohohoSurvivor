"""
Retouches demandées par Paul (2026-10-23) :
  - upgrades : Atk speed / Shoot system n'exigent plus Atk critique, mais restent en Bronze tant que le joueur ne l'a pas
    (Higher Rarities Upgrade) ; Atk critique en Gold uniquement
  - sets d'arme des joueurs : PV max de base et pushback fixés par le set (valeurs reprises du personnage qui l'utilise)
  - BP_BattlePetoir Auto shot : 4 balles par rafale, 20 dégâts
  - BP_Shotgun Auto atk : 2 s entre deux tirs ; ShotgunAMMO : durée de vie 1 s
  - BP_NME_GreatSword (élite) : Auto Atk CD 3 s
  - nouvelle arme /Game/Placeable/Weapons/Player/BP_AutoPetoir : modèle du pistolet, sans Ability, Auto shot 2 balles x 20 dégâts
    (réglages de l'Auto shot du Battle Petoir pour le reste)
  - nouvelle arme /Game/Placeable/Weapons/Player/BP_Bomb : bombe en main (copie de BP_NME_Bomb, répliquée), Ability Roll and Bomb
    (réglages copiés de celle du Shotgun)
  - set /Game/Core/WeaponSets/DA_WeaponSet_AutoPetoirBomb (droite AutoPetoir, gauche Bomb) = set de départ du Cannoneer
Ne recrée pas les armes / le set s'ils existent.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_cannoneer_start_set.py"
Rapport : Saved/Logs/setup_cannoneer_start_set.txt
"""
import os
import unreal

UPGRADES = "/Game/Core/Upgrades"
ATK_CRITIQUE = f"{UPGRADES}/DA_Upgrade_AtkCritique"
ATK_SPEED = f"{UPGRADES}/DA_Upgrade_AtkSpeed"
SHOOT_SYSTEM = f"{UPGRADES}/DA_Upgrade_ShootSystem"
SETS = "/Game/Core/WeaponSets"
CHARS = "/Game/Placeable/Character/Player"
MOUSS = f"{CHARS}/PlayerCharacter_mouss"
CANNONEER = f"{CHARS}/PlayerCharacter_Cannoneer"
# set -> personnage dont on reprend PV max et pushback
PLAYER_SETS = {
    "DA_WeaponSet_SabrePistolet": MOUSS,
    "DA_WeaponSet_AkimboBattlePetoir": MOUSS,
    "DA_WeaponSet_AkimboSabre": MOUSS,
    "DA_WeaponSet_Shotgun": CANNONEER,
    "DA_WeaponSet_AutoPetoirBomb": CANNONEER,
}
WEAPONS = "/Game/Placeable/Weapons/Player"
PISTOL = f"{WEAPONS}/BP_Pistolet"
BATTLE_PETOIR = f"{WEAPONS}/BP_BattlePetoir"
SHOTGUN = f"{WEAPONS}/BP_Shotgun"
AUTO_PETOIR_NAME = "BP_AutoPetoir"
AUTO_PETOIR = f"{WEAPONS}/{AUTO_PETOIR_NAME}"
BOMB_NAME = "BP_Bomb"
BOMB = f"{WEAPONS}/{BOMB_NAME}"
NME_BOMB = "/Game/Placeable/Weapons/Foe/BP_NME_Bomb"
GREAT_SWORD = "/Game/Placeable/Weapons/Foe/BP_NME_GreatSword"
SHOTGUN_AMMO = "/Game/Placeable/AMMO/ShotgunAMMO"
START_SET_NAME = "DA_WeaponSet_AutoPetoirBomb"
START_SET = f"{SETS}/{START_SET_NAME}"

PUSHBACK = ["pushback_min_damage_ratio", "pushback_max_damage_ratio", "pushback_min_force", "pushback_max_force"]
AUTO_SHOT_PROPS = ["display_name", "cooldown", "damage", "projectile_class", "assist_aim_distance", "assist_aim_angle", "shoot_system",
                   "bullets_per_burst", "burst_duration", "recoil_pitch", "recoil_kick_back", "recoil_kick_duration", "recoil_recover_duration",
                   "recoil_kick_ease_exponent", "recoil_recover_ease_exponent", "fit_recoil_to_fire_rate", "play_reload_anim",
                   "reload_spin_duration", "reload_spin_turns", "reload_spin_axis", "reload_spin_ease_exponent", "akimbo_fire_delay",
                   "icon", "icon_tint", "weapon_type"]
ROLL_BOMB_PROPS = ["display_name", "cooldown", "roll_duration", "roll_speed_multiplier", "force_move_without_input", "roll_speed_decay_exponent",
                   "roll_through_foes", "extra_invulnerability", "aim_duration", "aim_move_speed_multiplier", "roll_anim_turns",
                   "roll_anim_duration", "roll_anim_ease_exponent", "roll_anim_tuck_height", "bomb_class", "bomb_damage", "icon", "icon_tint",
                   "akimbo_fire_delay", "weapon_type"]

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_cannoneer_start_set] {msg}")
    report.append(msg)


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def templates(bp):
    result = []
    for h in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(h))
        if obj and obj not in result:
            result.append(obj)
    return result


def component(path, cls):
    return next((t for t in templates(assets.load_asset(path)) if type(t) is cls), None)


def add_component(bp, cls, name):
    root = subsystem.k2_gather_subobject_data_for_blueprint(bp)[0]
    handle, fail = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=root, new_class=cls, blueprint_context=bp))
    if not fail.is_empty():
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    subsystem.rename_subobject(handle, unreal.Text(name))
    return lib.get_object(lib.get_data(handle))


def remove_components(bp, cls):
    removed = 0
    while True:
        handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
        old = next((h for h in handles if isinstance(lib.get_object(lib.get_data(h)), cls)), None)
        if old is None:
            return removed
        subsystem.delete_subobject(handles[0], old, bp)
        removed += 1


def copy_props(src, dst, names):
    for n in names:
        dst.set_editor_property(n, src.get_editor_property(n))


def save(path):
    assets.save_asset(path, only_if_is_dirty=False)


try:
    # 1) upgrades
    atk_critique = assets.load_asset(ATK_CRITIQUE)
    atk_critique.set_editor_property("can_be_bronze", False)
    atk_critique.set_editor_property("can_be_silver", False)
    atk_critique.set_editor_property("can_be_gold", True)
    save(ATK_CRITIQUE)
    log("Atk critique : Gold uniquement")
    for path in (ATK_SPEED, SHOOT_SYSTEM):
        upgrade = assets.load_asset(path)
        upgrade.set_editor_property("required_upgrade", None)
        upgrade.set_editor_property("higher_rarities_upgrade", atk_critique)
        save(path)
        log(f"{upgrade.get_name()} : plus de Required Upgrade, Bronze seulement sans Atk critique")

    # 2) tweaks d'armes et d'AMMO
    auto_shot = component(BATTLE_PETOIR, unreal.YSAutoShotAbility)
    auto_shot.set_editor_property("bullets_per_burst", 4)
    auto_shot.set_editor_property("damage", 20.0)
    save(BATTLE_PETOIR)
    log("BP_BattlePetoir Auto shot : 4 balles, 20 dégâts")

    shotgun_shot = component(SHOTGUN, unreal.YSAutoShotAbility)
    shotgun_shot.set_editor_property("cooldown", 2.0)
    save(SHOTGUN)
    log("BP_Shotgun Auto atk : 2 s entre deux tirs")

    cdo(SHOTGUN_AMMO).set_editor_property("life_time", 1.0)
    save(SHOTGUN_AMMO)
    log("ShotgunAMMO : durée de vie 1 s")

    swing = component(GREAT_SWORD, unreal.YSAutoSwingAbility)
    swing.set_editor_property("auto_atk_cooldown", 3.0)
    save(GREAT_SWORD)
    log("BP_NME_GreatSword (élite) : Auto Atk CD 3 s")

    # 3) AutoPetoir : le pistolet sans Ability, avec l'Auto shot du Battle Petoir (2 balles x 20)
    if not assets.does_asset_exist(AUTO_PETOIR):
        tools.duplicate_asset(AUTO_PETOIR_NAME, WEAPONS, assets.load_asset(PISTOL))
        bp = assets.load_asset(AUTO_PETOIR)
        log(f"BP_AutoPetoir : {remove_components(bp, unreal.YSAbilityComponent)} action(s) du pistolet retirée(s)")
        shot = add_component(bp, unreal.YSAutoShotAbility, "AutoShot")
        copy_props(auto_shot, shot, AUTO_SHOT_PROPS)
        shot.set_editor_property("bullets_per_burst", 2)
        shot.set_editor_property("damage", 20.0)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        weapon = cdo(AUTO_PETOIR)
        weapon.set_editor_property("display_name", "AutoPetoir")
        copy_props(cdo(BATTLE_PETOIR), weapon, ["override_hand_location", "hand_location", "override_hand_rotation", "hand_rotation"])
        save(AUTO_PETOIR)
        log(f"créé : {AUTO_PETOIR} (Auto shot 2 balles x 20 dégâts)")
    else:
        log(f"existe déjà : {AUTO_PETOIR}")

    # 4) Bomb : bombe en main des NME, répliquée, avec la Roll and Bomb du Shotgun
    if not assets.does_asset_exist(BOMB):
        roll_src = component(SHOTGUN, unreal.YSRollAndBombAbility)
        tools.duplicate_asset(BOMB_NAME, WEAPONS, assets.load_asset(NME_BOMB))
        bp = assets.load_asset(BOMB)
        log(f"BP_Bomb : {remove_components(bp, unreal.YSFoeRangedAction)} action(s) de NME retirée(s)")
        roll = add_component(bp, unreal.YSRollAndBombAbility, "RollAndBomb")
        copy_props(roll_src, roll, ROLL_BOMB_PROPS)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        weapon = cdo(BOMB)
        weapon.set_editor_property("replicates", True)
        weapon.set_editor_property("display_name", "Bomb")
        save(BOMB)
        log(f"créé : {BOMB} (Roll and Bomb copiée du Shotgun : bombe {roll.get_editor_property('bomb_damage')} dégâts, recharge {roll.get_editor_property('cooldown')} s)")
    else:
        log(f"existe déjà : {BOMB}")

    # 5) set de départ du Cannoneer
    if not assets.does_asset_exist(START_SET):
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.YSWeaponSet)
        weapon_set = tools.create_asset(START_SET_NAME, SETS, unreal.YSWeaponSet, factory)
        weapon_set.set_editor_property("display_name", "AutoPetoir & Bomb")
        weapon_set.set_editor_property("right_hand_weapon", assets.load_blueprint_class(AUTO_PETOIR))
        weapon_set.set_editor_property("left_hand_weapon", assets.load_blueprint_class(BOMB))
        save(START_SET)
        log(f"créé : {START_SET} (droite AutoPetoir, gauche Bomb)")
    else:
        log(f"existe déjà : {START_SET}")
    cannoneer = cdo(CANNONEER)
    cannoneer.set_editor_property("weapon_set", assets.load_asset(START_SET))
    save(CANNONEER)
    log("Cannoneer : set de départ AutoPetoir & Bomb")

    # 6) PV max et pushback sur les sets des joueurs (repris du personnage, sans écraser un set déjà réglé)
    for name, char_path in PLAYER_SETS.items():
        path = f"{SETS}/{name}"
        weapon_set = assets.load_asset(path)
        if weapon_set is None:
            log(f"{name} introuvable")
            continue
        character = cdo(char_path)
        if not weapon_set.get_editor_property("override_max_health"):
            weapon_set.set_editor_property("override_max_health", True)
            weapon_set.set_editor_property("max_health", character.get_editor_property("max_health"))
        if not weapon_set.get_editor_property("override_pushback"):
            weapon_set.set_editor_property("override_pushback", True)
            copy_props(character, weapon_set, PUSHBACK)
        save(path)
        log(f"{name} : PV max {weapon_set.get_editor_property('max_health')}, pushback "
            + " / ".join(f"{weapon_set.get_editor_property(p)}" for p in PUSHBACK))

    # relecture
    for path in (AUTO_PETOIR, BOMB, SHOTGUN, BATTLE_PETOIR):
        for t in templates(assets.load_asset(path)):
            if isinstance(t, unreal.YSAbilityComponent):
                extra = f", {t.get_editor_property('bullets_per_burst')} balles x {t.get_editor_property('damage')}" if isinstance(t, unreal.YSAutoShotAbility) else ""
                log(f"relu {path.split('/')[-1]}.{t.get_name()} : {t.get_class().get_name()} recharge {t.get_editor_property('cooldown')}{extra}")
    log(f"relu Cannoneer : set {cdo(CANNONEER).get_editor_property('weapon_set').get_name()} ; BP_Bomb répliquée {cdo(BOMB).get_editor_property('replicates')}")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_cannoneer_start_set.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
