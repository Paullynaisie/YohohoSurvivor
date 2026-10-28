"""
Split du Cannoneer en deux personnages (demande de Paul, 2026-10-28) :

HARQUEBUSIER (copie de l'ancien Cannoneer : Head Feature et passif Marksman gardés), entre Cannoneer et Quartermaster :
  - set de base DA_WeaponSet_Harquebusier : BP_HarquebusierSniper (copie de BP_Sniper) : Auto shot en PewPew (40 dégâts,
    recharge 2 s, rafale 1 s) + Ability « Pull » (Bumping Slash copié de la GreatAxe : coup de crosse, bump, recharge de
    l'attaque auto rendue) + Hitbox
  - classe Shotgun (BP_Shotgun) : Auto shot en éventail régulier, shoot system Pierce (traverse 1 fois, critique sur les NME
    bumpés) ; Ability « Dragon breath » (7 DragonBreathAMMO : vitesse 50, 1 s, traversent tout et bumpent, 35 dégâts, recul
    0.25 s puis recharge de l'attaque auto rendue) ; BombThrower retiré
  - classe Sniper (BP_Sniper) : Auto shot en Pierce (critique sur les NME bumpés) ; Ability Pull ; Roll bomb shot retirée
  - upgrades d'arme : Gros sel (Shotgun), Aiming (Sniper de base et Sniper), jumelles
CANNONEER (nouveau) :
  - passif « Maître canonnier » (sans effet pour l'instant), cheveux en pétard (cordes épaisses dressées) à la place des tresses
  - set de base inchangé (AutoPetoir + Bomb)
  - classe Artilleur : BP_HandCanon (canon à deux mains) : Auto shot Explosif (ExplosiveAMMO, 20 dégâts d'impact + explosion
    50 de la taille d'une mine, recharge 2 s, rafale 0.2 s) + Ability « Rocket jump » (explosion 150 aux pieds puis le saut du
    Meteor Jump, sans coupe) ; upgrade d'arme MineShooter (mines collées, BP_StickyMine)
  - classe Saboteur : akimbo de BP_BattleBomb (copie de BP_Bomb + attaque auto BombThrow en cloche, Explosif, 50 dégâts,
    recharge 2 s ; délai akimbo 0.25 ; Roll and Bomb avec une seconde bombe à mi-roulade) ; upgrade d'arme Une Mine !
Ne recrée rien qui existe déjà (réglages de Paul gardés) ; pools, sélection et jumelles remis à chaque lancement.

Lancement (éditeur fermé) :
  UnrealEditor-Cmd.exe YohohoSurvivor.uproject -run=pythonscript -script="<chemin absolu>/Tools/EditorScripts/setup_harquebusier.py" -unattended -nullrhi
Rapport : Saved/Logs/setup_harquebusier.txt
"""
import json
import math
import os
import unreal

CHAR_DIR = "/Game/Placeable/Character/Player"
GUNS = "/Game/Placeable/Weapons/Player/Wp_Guns"
MISC = "/Game/Placeable/Weapons/Player/Wp_Misc"
AMMO_DIR = "/Game/Placeable/AMMO"
SETS_DIR = "/Game/Core/WeaponSets"
UPGRADES_DIR = "/Game/Core/Upgrades"
CLASS_DIR = f"{UPGRADES_DIR}/Up_Class"
WEAPON_UP_DIR = f"{UPGRADES_DIR}/Up_Character/Up_Weapons"
PASSIVE_DIR = "/Game/Core/Passives"
RANDOM_SHARD = f"{UPGRADES_DIR}/Up_Shards/DA_Up_Shard_Random"
GAME_MODES = ["/Game/Core/GameModes/BP_TestGameMode", "/Game/Core/GameModes/BP_TestMapGameMode"]
MAT = "/Game/Art/Materials/PH_materials"
SPHERE = "/Engine/BasicShapes/Sphere"
CYLINDER = "/Engine/BasicShapes/Cylinder"
CUBE = "/Engine/BasicShapes/Cube"
HEAD = "Head Feature"
# Hitbox du Pull (coup de crosse) : autour de la main, les poses de coupe viennent de la GreatAxe (axe long Z) alors que le sniper est en X
HITBOX_LOCATION = unreal.Vector(20.0, 0.0, 0.0)
HITBOX_EXTENT = unreal.Vector(90.0, 90.0, 90.0)

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
registry = unreal.AssetRegistryHelpers.get_asset_registry()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
ys = unreal.YSEditorScriptLibrary
report = []


def log(msg):
    unreal.log(f"[setup_harquebusier] {msg}")
    report.append(str(msg))


def vec(x, y, z):
    return unreal.Vector(x, y, z)


def rot(pitch=0.0, yaw=0.0, roll=0.0):
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


def find_path(name, root="/Game"):
    for data in registry.get_assets_by_path(root, recursive=True):
        if str(data.asset_name) == name:
            return str(data.package_name)
    return None


def need_path(name, root="/Game"):
    path = find_path(name, root)
    if not path:
        raise RuntimeError(f"asset introuvable : {name}")
    return path


def bp_class(name):
    return assets.load_blueprint_class(need_path(name, "/Game/Placeable"))


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def handles(bp):
    return subsystem.k2_gather_subobject_data_for_blueprint(bp)


def find_handle(bp, name):
    for h in handles(bp):
        if str(lib.get_variable_name(lib.get_data(h))) == name:
            return h
    return None


def templates(bp):
    result = []
    for h in handles(bp):
        obj = lib.get_object(lib.get_data(h))
        if obj and obj not in result:
            result.append(obj)
    return result


def find_component(bp, cls, exact=False):
    for t in templates(bp):
        if (type(t) is cls) if exact else isinstance(t, cls):
            return t
    return None


def add_component(bp, cls, name, parent=None):
    parent = parent or handles(bp)[0]
    handle, fail = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent, new_class=cls, blueprint_context=bp))
    if not fail.is_empty():
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    subsystem.rename_subobject(handle, unreal.Text(name))
    return handle, lib.get_object(lib.get_data(handle))


def remove_where(bp, predicate):
    removed = 0
    while True:
        hs = handles(bp)
        old = next((h for h in hs if predicate(str(lib.get_variable_name(lib.get_data(h))), lib.get_object(lib.get_data(h)))), None)
        if old is None:
            return removed
        subsystem.delete_subobject(hs[0], old, bp)
        removed += 1


def shape(bp, name, mesh, material, location, rotation, scale, parent=None):
    _, comp = add_component(bp, unreal.StaticMeshComponent, name, parent)
    comp.set_editor_property("static_mesh", assets.load_asset(mesh))
    comp.set_material(0, material if isinstance(material, unreal.MaterialInterface) else assets.load_asset(f"{MAT}/{material}"))
    comp.set_editor_property("relative_location", location)
    comp.set_editor_property("relative_rotation", rotation)
    comp.set_editor_property("relative_scale3d", scale)
    comp.set_collision_profile_name("NoCollision")
    return comp


def create_bp(name, folder, parent_class):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    return tools.create_asset(name, folder, unreal.Blueprint, factory)


def create_data_asset(name, folder, cls):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    return tools.create_asset(name, folder, cls, factory)


def compile_save(bp, path):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(path, only_if_is_dirty=False)


def duplicate(src_path, folder, name):
    path = f"{folder}/{name}"
    if assets.does_asset_exist(path):
        return path, False
    assets.duplicate_asset(src_path, path)
    return path, True


def add_hitbox(bp, location, extent):
    if find_component(bp, unreal.BoxComponent):
        return
    _, hitbox = add_component(bp, unreal.BoxComponent, "Hitbox")
    hitbox.set_editor_property("relative_location", location)
    hitbox.set_editor_property("box_extent", extent)
    hitbox.set_collision_profile_name("NoCollision")


def add_pull(bp, source):
    """Ability « Pull » : Bumping Slash (coup de crosse) copié de Source"""
    pull = find_component(bp, unreal.YSBumpingSlashAbility)
    if pull is None:
        _, pull = add_component(bp, unreal.YSBumpingSlashAbility, "Pull")
        ys.copy_editable_properties(source, pull)
        pull.set_editor_property("display_name", unreal.Text("Pull"))
        pull.set_editor_property("weapon_type", unreal.YSWeaponType.MELEE)
        log(f"{bp.get_name()} : Pull ajouté (réglages de {source.get_name()})")
    pull.set_editor_property("reset_auto_attack_on_use", True)
    pull.set_editor_property("auto_attack_reset_delay", 0.0)
    return pull


try:
    # ------------------------------------------------------------------ chemins existants
    cannoneer_path = need_path("PlayerCharacter_Cannoneer", CHAR_DIR)
    quartermaster_path = need_path("PlayerCharacter_Quartermaster", CHAR_DIR)
    sniper_path = need_path("BP_Sniper", "/Game/Placeable/Weapons")
    shotgun_path = need_path("BP_Shotgun", "/Game/Placeable/Weapons")
    bomb_path = need_path("BP_Bomb", "/Game/Placeable/Weapons")
    arme_path = need_path("BP_Arme", "/Game/Placeable/Weapons")
    great_axe_path = need_path("BP_GreatAxe", "/Game/Placeable/Weapons")
    great_anchor_path = need_path("BP_GreatAnchor", "/Game/Placeable/Weapons")
    petoir_ammo_path = need_path("PetoirAMMO", AMMO_DIR)
    cannoneer_bomb_path = need_path("BP_CannoneerBomb", AMMO_DIR)
    cannoneer_mine_path = need_path("BP_CannoneerMine", AMMO_DIR)
    start_set_path = need_path("DA_WeaponSet_AutoPetoirBomb", SETS_DIR)
    shotgun_set_path = need_path("DA_WeaponSet_Shotgun", SETS_DIR)
    sniper_set_path = need_path("DA_WeaponSet_Sniper", SETS_DIR)
    random_shard = assets.load_asset(RANDOM_SHARD)

    axe_bump = find_component(assets.load_asset(great_axe_path), unreal.YSBumpingSlashAbility)
    anchor_jump = find_component(assets.load_asset(great_anchor_path), unreal.YSMeteorJumpAbility)

    # ------------------------------------------------------------------ 1) Harquebusier = copie de l'ancien Cannoneer (avant ses changements)
    harq_path, created = duplicate(cannoneer_path, CHAR_DIR, "PlayerCharacter_Harquebusier")
    log(f"{'créé' if created else 'existe déjà'} : {harq_path}")

    # ------------------------------------------------------------------ 2) AMMO et explosions
    dragon_ammo_path, created = f"{AMMO_DIR}/DragonBreathAMMO", False
    if not assets.does_asset_exist(dragon_ammo_path):
        create_bp("DragonBreathAMMO", AMMO_DIR, assets.load_blueprint_class(petoir_ammo_path))
        created = True
        ammo = cdo(dragon_ammo_path)
        ammo.set_editor_property("speed", 50.0)
        ammo.set_editor_property("life_time", 1.0)
        ammo.set_editor_property("pierce_all_targets", True)
        ammo.set_editor_property("bumps_targets", True)
        for prop in ("bump_distance", "bump_apex_height", "bump_duration", "bump_spin_turns", "bump_elites"):
            ammo.set_editor_property(prop, axe_bump.get_editor_property(prop))
        ammo.set_editor_property("color", unreal.LinearColor(1.0, 0.25, 0.0, 1.0))
        compile_save(assets.load_asset(dragon_ammo_path), dragon_ammo_path)
    log(f"{'créé' if created else 'existe déjà'} : {dragon_ammo_path}")

    explosive_ammo_path, created = f"{AMMO_DIR}/ExplosiveAMMO", False
    if not assets.does_asset_exist(explosive_ammo_path):
        create_bp("ExplosiveAMMO", AMMO_DIR, assets.load_blueprint_class(petoir_ammo_path))
        created = True
        cdo(explosive_ammo_path).set_editor_property("color", unreal.LinearColor(1.0, 0.45, 0.0, 1.0))
        compile_save(assets.load_asset(explosive_ammo_path), explosive_ammo_path)
    log(f"{'créé' if created else 'existe déjà'} : {explosive_ammo_path}")

    # explosion d'impact de la taille d'une mine (rayon de la bombe / diviseur de la mine), sans disque au sol
    blast_path, created = f"{AMMO_DIR}/BP_ExplosiveShotBlast", False
    if not assets.does_asset_exist(blast_path):
        create_bp("BP_ExplosiveShotBlast", AMMO_DIR, assets.load_blueprint_class(cannoneer_bomb_path))
        created = True
        mine = cdo(cannoneer_mine_path)
        blast = cdo(blast_path)
        blast.set_editor_property("explosion_radius", mine.get_editor_property("explosion_radius") / mine.get_editor_property("mine_radius_divisor"))
        blast.set_editor_property("show_zone", False)
        compile_save(assets.load_asset(blast_path), blast_path)
    log(f"{'créé' if created else 'existe déjà'} : {blast_path} (rayon {cdo(blast_path).get_editor_property('explosion_radius')})")

    # mine collée (MineShooter) : la mine du Cannoneer, plus petite
    sticky_path, created = f"{AMMO_DIR}/BP_StickyMine", False
    if not assets.does_asset_exist(sticky_path):
        create_bp("BP_StickyMine", AMMO_DIR, assets.load_blueprint_class(cannoneer_mine_path))
        created = True
        sticky = cdo(sticky_path)
        mesh = sticky.get_editor_property("mesh")
        scale = mesh.get_editor_property("relative_scale3d")
        mesh.set_editor_property("relative_scale3d", vec(scale.x * 0.5, scale.y * 0.5, scale.z * 0.5))
        sticky.set_editor_property("show_zone", False)
        compile_save(assets.load_asset(sticky_path), sticky_path)
    log(f"{'créé' if created else 'existe déjà'} : {sticky_path}")

    # ------------------------------------------------------------------ 3) Battle Bomb (avant de retirer le BombThrower du Shotgun)
    battle_bomb_path, created = duplicate(bomb_path, MISC, "BP_BattleBomb")
    battle_bomb = assets.load_asset(battle_bomb_path)
    if created:
        throw_src = find_component(assets.load_asset(shotgun_path), unreal.YSBombThrowerAbility)
        _, throw = add_component(battle_bomb, unreal.YSAutoBombThrowAbility, "BombThrow")
        if throw_src:
            ys.copy_editable_properties(throw_src, throw)
        throw.set_editor_property("display_name", unreal.Text("BombThrow"))
        throw.set_editor_property("bomb_count", 1)
        throw.set_editor_property("mine_class", None)
        throw.set_editor_property("throw_with_off_hand", False)
        throw.set_editor_property("show_held_bomb", False)
        throw.set_editor_property("icon", None)
    throw = find_component(battle_bomb, unreal.YSAutoBombThrowAbility)
    throw.set_editor_property("cooldown", 2.0)
    throw.set_editor_property("bomb_damage", 50.0)
    throw.set_editor_property("akimbo_fire_delay", 0.25)
    roll = find_component(battle_bomb, unreal.YSRollAndBombAbility, exact=True)
    if roll:
        roll.set_editor_property("always_drop_second_bomb", True)
        roll.set_editor_property("second_mine_roll_fraction", 0.5)
        roll.set_editor_property("akimbo_fire_delay", 0.25)
    compile_save(battle_bomb, battle_bomb_path)
    cdo(battle_bomb_path).set_editor_property("display_name", "Battle Bomb")
    assets.save_asset(battle_bomb_path, only_if_is_dirty=False)
    log(f"{'créé' if created else 'existe déjà'} : {battle_bomb_path} (BombThrow auto {throw.get_editor_property('bomb_damage')} dégâts / {throw.get_editor_property('cooldown')} s, Roll and Bomb 2e bombe)")

    # ------------------------------------------------------------------ 4) Sniper de base du Harquebusier
    harq_sniper_path, created = duplicate(sniper_path, GUNS, "BP_HarquebusierSniper")
    harq_sniper = assets.load_asset(harq_sniper_path)
    if created:
        removed = remove_where(harq_sniper, lambda n, o: isinstance(o, (unreal.YSRollBombShotAbility, unreal.YSBombThrowerAbility)))
        log(f"BP_HarquebusierSniper : {removed} Ability retirée(s)")
    add_hitbox(harq_sniper, HITBOX_LOCATION, HITBOX_EXTENT)
    add_pull(harq_sniper, axe_bump)
    auto = find_component(harq_sniper, unreal.YSAutoShotAbility)
    if created:
        auto.set_editor_property("shoot_system", unreal.YSShootSystem.PEW_PEW)
        auto.set_editor_property("spread_fire", False)
        auto.set_editor_property("cooldown", 2.0)
        auto.set_editor_property("burst_duration", 1.0)
        auto.set_editor_property("bullets_per_burst", 3)
        auto.set_editor_property("damage", 40.0)
    compile_save(harq_sniper, harq_sniper_path)
    if created:
        cdo(harq_sniper_path).set_editor_property("display_name", "Harquebuse")
        assets.save_asset(harq_sniper_path, only_if_is_dirty=False)
    pull_template = find_component(harq_sniper, unreal.YSBumpingSlashAbility)

    # ------------------------------------------------------------------ 5) Sniper (classe 2) : Pierce + Pull
    sniper = assets.load_asset(sniper_path)
    removed = remove_where(sniper, lambda n, o: isinstance(o, unreal.YSRollBombShotAbility))
    if removed:
        log(f"BP_Sniper : Roll bomb shot retirée ({removed})")
    add_hitbox(sniper, HITBOX_LOCATION, HITBOX_EXTENT)
    add_pull(sniper, pull_template)
    auto = find_component(sniper, unreal.YSAutoShotAbility)
    auto.set_editor_property("shoot_system", unreal.YSShootSystem.PIERCE)
    auto.set_editor_property("spread_fire", False)
    compile_save(sniper, sniper_path)

    # ------------------------------------------------------------------ 6) Shotgun (classe 1) : éventail régulier en Pierce + Dragon breath
    shotgun = assets.load_asset(shotgun_path)
    auto = find_component(shotgun, unreal.YSAutoShotAbility)
    dragon = find_component(shotgun, unreal.YSDragonBreathAbility)
    if dragon is None:
        _, dragon = add_component(shotgun, unreal.YSDragonBreathAbility, "DragonBreath")
        ys.copy_editable_properties(auto, dragon)
        dragon.set_editor_property("display_name", unreal.Text("Dragon breath"))
        dragon.set_editor_property("projectile_class", assets.load_blueprint_class(dragon_ammo_path))
        dragon.set_editor_property("shoot_system", unreal.YSShootSystem.PEW_PEW)
        dragon.set_editor_property("spread_fire", True)
        dragon.set_editor_property("shotgun_ammo_per_shot", 7)
        dragon.set_editor_property("damage", 35.0)
        dragon.set_editor_property("cooldown", 10.0)
        dragon.set_editor_property("recoil_kick_duration", 0.05)
        dragon.set_editor_property("recoil_recover_duration", 0.2)
        dragon.set_editor_property("reset_auto_attack_on_use", True)
        dragon.set_editor_property("auto_attack_reset_delay", 0.25)
        dragon.set_editor_property("play_reload_anim", False)
        dragon.set_editor_property("icon", None)
        log("BP_Shotgun : Dragon breath ajouté (réglages de tir de l'Auto shot)")
    removed = remove_where(shotgun, lambda n, o: isinstance(o, unreal.YSBombThrowerAbility))
    if removed:
        log(f"BP_Shotgun : BombThrower retiré ({removed})")
    auto.set_editor_property("shoot_system", unreal.YSShootSystem.PIERCE)
    auto.set_editor_property("spread_fire", True)
    auto.set_editor_property("pierce_max_hits", 2)
    compile_save(shotgun, shotgun_path)

    # ------------------------------------------------------------------ 7) HandCanon (Artilleur)
    canon_path = f"{GUNS}/BP_HandCanon"
    if not assets.does_asset_exist(canon_path):
        bp = create_bp("BP_HandCanon", GUNS, assets.load_blueprint_class(arme_path))
        # fût = Mesh (natif, réglé plus bas) ; culasse, bouche, poignée ; bandes sous « Coloration » (couleur de main / de classe)
        shape(bp, "Breech", SPHERE, "m_PH_monochrome_LightBlack", vec(0.0, 0.0, 10.0), rot(), vec(0.34, 0.34, 0.34))
        shape(bp, "Cascabel", SPHERE, "m_PH_monochrome_LightBlack", vec(-17.0, 0.0, 10.0), rot(), vec(0.12, 0.12, 0.12))
        shape(bp, "MuzzleRing", CYLINDER, "m_PH_monochrome_LightBlack", vec(88.0, 0.0, 10.0), rot(pitch=-90.0), vec(0.38, 0.38, 0.08))
        shape(bp, "Grip", CUBE, "m_PH_monochrome_Brown", vec(-2.0, 0.0, -10.0), rot(pitch=-15.0), vec(0.12, 0.08, 0.26))
        shape(bp, "Carriage", CUBE, "m_PH_monochrome_Brown", vec(35.0, 0.0, -6.0), rot(), vec(0.55, 0.14, 0.06))
        coloration, _ = add_component(bp, unreal.SceneComponent, "Coloration")
        shape(bp, "Band1", CYLINDER, "m_PH_emissive_turboWhite", vec(28.0, 0.0, 10.0), rot(pitch=-90.0), vec(0.32, 0.32, 0.05), coloration)
        shape(bp, "Band2", CYLINDER, "m_PH_emissive_turboWhite", vec(62.0, 0.0, 10.0), rot(pitch=-90.0), vec(0.31, 0.31, 0.05), coloration)

        _, explosive = add_component(bp, unreal.YSAutoShotAbility, "AutoShot")
        shotgun_auto = find_component(assets.load_asset(shotgun_path), unreal.YSAutoShotAbility)
        ys.copy_editable_properties(shotgun_auto, explosive)
        explosive.set_editor_property("display_name", unreal.Text("ExplosiveShot"))
        explosive.set_editor_property("shoot_system", unreal.YSShootSystem.EXPLOSIVE)
        explosive.set_editor_property("spread_fire", False)
        explosive.set_editor_property("bullets_per_burst", 2)
        explosive.set_editor_property("burst_duration", 0.2)
        explosive.set_editor_property("cooldown", 2.0)
        explosive.set_editor_property("damage", 20.0)
        explosive.set_editor_property("explosion_damage", 50.0)
        explosive.set_editor_property("projectile_class", assets.load_blueprint_class(explosive_ammo_path))
        explosive.set_editor_property("explosion_class", assets.load_blueprint_class(blast_path))
        explosive.set_editor_property("sticky_mine_class", assets.load_blueprint_class(sticky_path))
        explosive.set_editor_property("play_reload_anim", False)

        _, rocket = add_component(bp, unreal.YSRocketJumpAbility, "RocketJump")
        ys.copy_editable_properties(anchor_jump, rocket)
        rocket.set_editor_property("display_name", unreal.Text("Rocket jump"))
        rocket.set_editor_property("weapon_type", unreal.YSWeaponType.GUN)
        rocket.set_editor_property("explosion_class", assets.load_blueprint_class(cannoneer_bomb_path))
        rocket.set_editor_property("explosion_damage", 150.0)
        rocket.set_editor_property("step_back_duration", 0.0)
        rocket.set_editor_property("cooldown", 10.0)
        rocket.set_editor_property("reset_auto_attack_on_use", False)
        rocket.set_editor_property("icon", None)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)

        weapon = cdo(canon_path)
        shotgun_cdo = cdo(shotgun_path)
        weapon.set_editor_property("display_name", "HandCanon")
        weapon.set_editor_property("two_handed", True)
        for prop in ("override_hand_location", "hand_location", "override_hand_rotation", "hand_rotation"):
            weapon.set_editor_property(prop, shotgun_cdo.get_editor_property(prop))
        barrel = weapon.get_editor_property("mesh")
        barrel.set_editor_property("static_mesh", assets.load_asset(CYLINDER))
        barrel.set_material(0, assets.load_asset(f"{MAT}/m_PH_monochrome_LightBlack"))
        barrel.set_editor_property("relative_location", vec(44.0, 0.0, 10.0))
        barrel.set_editor_property("relative_rotation", rot(pitch=-90.0))
        barrel.set_editor_property("relative_scale3d", vec(0.28, 0.28, 0.88))
        barrel.set_collision_profile_name("NoCollision")
        weapon.get_editor_property("muzzle").set_editor_property("relative_location", vec(94.0, 0.0, 10.0))
        weapon.get_editor_property("off_hand_socket").set_editor_property("relative_location", vec(50.0, 0.0, -6.0))
        assets.save_asset(canon_path, only_if_is_dirty=False)
        log(f"créé : {canon_path} (deux mains ; AutoShot Explosif + RocketJump)")
    else:
        log(f"existe déjà : {canon_path}")

    # ------------------------------------------------------------------ 8) sets d'arme
    def make_set(name, display, right, left):
        path = f"{SETS_DIR}/{name}"
        if assets.does_asset_exist(path):
            log(f"existe déjà : {path}")
            return assets.load_asset(path)
        weapon_set = create_data_asset(name, SETS_DIR, unreal.YSWeaponSet)
        ys.copy_editable_properties(assets.load_asset(start_set_path), weapon_set)
        weapon_set.set_editor_property("display_name", display)
        weapon_set.set_editor_property("right_hand_weapon", right)
        weapon_set.set_editor_property("left_hand_weapon", left)
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"créé : {path}")
        return weapon_set

    harq_set = make_set("DA_WeaponSet_Harquebusier", "Harquebuse", assets.load_blueprint_class(harq_sniper_path), None)
    artilleur_set = make_set("DA_WeaponSet_Artilleur", "HandCanon", assets.load_blueprint_class(canon_path), None)
    battle_bomb_class = assets.load_blueprint_class(battle_bomb_path)
    saboteur_set = make_set("DA_WeaponSet_Saboteur", "Akimbo Battle Bomb", battle_bomb_class, battle_bomb_class)

    # ------------------------------------------------------------------ 9) upgrades de classe
    old_shotgun_up = find_path("DA_ClassUpgrade_Cannoneer2Shotgun", UPGRADES_DIR)
    old_sniper_up = find_path("DA_ClassUpgrade_Cannoneer2Sniper", UPGRADES_DIR)

    def make_class_upgrade(name, template, source_set, target_set, display, description):
        path = f"{CLASS_DIR}/{name}"
        if not assets.does_asset_exist(path):
            if template:
                assets.duplicate_asset(template, path)
            else:
                create_data_asset(name, CLASS_DIR, unreal.YSClassUpgrade)
            asset = assets.load_asset(path)
            if display:
                asset.set_editor_property("display_name", display)
            if description:
                asset.set_editor_property("description", description)
            log(f"créé : {path}")
        asset = assets.load_asset(path)
        asset.set_editor_property("source_weapon_set", source_set)
        asset.set_editor_property("target_weapon_set", target_set)
        assets.save_asset(path, only_if_is_dirty=False)
        return asset

    template = old_shotgun_up or find_path("DA_ClassUpgrade_Harquebusier2Shotgun", UPGRADES_DIR)
    harq2shotgun = make_class_upgrade("DA_ClassUpgrade_Harquebusier2Shotgun", old_shotgun_up, harq_set, assets.load_asset(shotgun_set_path), None, None)
    harq2sniper = make_class_upgrade("DA_ClassUpgrade_Harquebusier2Sniper", old_sniper_up, harq_set, assets.load_asset(sniper_set_path), None, None)
    start_set = assets.load_asset(start_set_path)
    can2artilleur = make_class_upgrade("DA_ClassUpgrade_Cannoneer2Artilleur", old_shotgun_up or f"{CLASS_DIR}/DA_ClassUpgrade_Harquebusier2Shotgun",
                                       start_set, artilleur_set, "Artilleur", "HandCanon à deux mains : tirs explosifs et Rocket jump")
    can2saboteur = make_class_upgrade("DA_ClassUpgrade_Cannoneer2Saboteur", old_sniper_up or f"{CLASS_DIR}/DA_ClassUpgrade_Harquebusier2Sniper",
                                      start_set, saboteur_set, "Saboteur", "Akimbo de Battle Bombs : bombes lancées en cloche et roulade à deux bombes")

    # ------------------------------------------------------------------ 10) upgrades d'arme (système de shards)
    def make_weapon_upgrade(key, cls, weapons):
        path = f"{WEAPON_UP_DIR}/DA_Up_Weapon_{key}"
        created = not assets.does_asset_exist(path)
        if created:
            asset = create_data_asset(f"DA_Up_Weapon_{key}", WEAPON_UP_DIR, cls)
            asset.set_editor_property("shard_system", True)
            asset.set_editor_property("unlock_shards", [random_shard, random_shard])
            asset.set_editor_property("bonus_shards", [random_shard, random_shard])
            for weight in ("unlock_weight", "bonus_weight", "super_weight"):
                asset.set_editor_property(weight, 1.0)
            # icône reprise d'une upgrade d'arme existante (à remplacer par Paul)
            icon_source = assets.load_asset(f"{WEAPON_UP_DIR}/DA_Up_Weapon_Mine")
            if icon_source and icon_source.get_editor_property("icon"):
                asset.set_editor_property("icon", icon_source.get_editor_property("icon"))
            log(f"créé : {path}")
        asset = assets.load_asset(path)
        asset.set_editor_property("required_weapon_classes", weapons)
        return asset

    gros_sel = make_weapon_upgrade("GrosSel", unreal.YSGrosSelUpgrade, [assets.load_blueprint_class(shotgun_path)])
    aiming = make_weapon_upgrade("Aiming", unreal.YSAimingUpgrade, [assets.load_blueprint_class(harq_sniper_path), assets.load_blueprint_class(sniper_path)])
    mine_shooter = make_weapon_upgrade("MineShooter", unreal.YSMineShooterUpgrade, [assets.load_blueprint_class(canon_path)])
    mine = assets.load_asset(f"{WEAPON_UP_DIR}/DA_Up_Weapon_Mine")
    gros_sel.set_editor_property("twin_upgrade", aiming)
    aiming.set_editor_property("twin_upgrade", gros_sel)
    mine_shooter.set_editor_property("twin_upgrade", mine)
    mine.set_editor_property("twin_upgrade", mine_shooter)
    for asset in (gros_sel, aiming, mine_shooter, mine):
        assets.save_loaded_asset(asset, only_if_is_dirty=False)

    # Ricochet : jamais sur le Dragon breath (tir en éventail de l'Ability du Shotgun)
    ricochet = assets.load_asset(f"{WEAPON_UP_DIR}/DA_Up_Weapon_Ricochet")
    excluded = list(ricochet.get_editor_property("excluded_action_classes"))
    if unreal.YSDragonBreathAbility.static_class() not in excluded:
        excluded.append(unreal.YSDragonBreathAbility.static_class())
        ricochet.set_editor_property("excluded_action_classes", excluded)
        assets.save_loaded_asset(ricochet, only_if_is_dirty=False)
        log("Ricochet : Dragon breath exclu")

    # ------------------------------------------------------------------ 11) personnages : pools, sets, passifs
    passive_path = f"{PASSIVE_DIR}/DA_Passive_MaitreCanonnier"
    if not assets.does_asset_exist(passive_path):
        passive = create_data_asset("DA_Passive_MaitreCanonnier", PASSIVE_DIR, unreal.YSPassive)
        passive.set_editor_property("display_name", "Maître canonnier")
        passive.set_editor_property("description", "A des upgrades en plus quand il est au canon du bateau (à venir)")
        assets.save_asset(passive_path, only_if_is_dirty=False)
        log(f"créé : {passive_path}")

    def set_pool(path, tail):
        character = cdo(path)
        pool = list(character.get_editor_property("upgrade_pool"))[:15]
        pool.extend(tail)
        character.set_editor_property("upgrade_pool", pool)
        return character

    harq = set_pool(harq_path, [harq2shotgun, harq2sniper, gros_sel, aiming])
    harq.set_editor_property("character_name", "Harquebusier")
    harq.set_editor_property("weapon_set", harq_set)
    harq.set_editor_property("passive", assets.load_asset(f"{PASSIVE_DIR}/DA_Passive_Marksman"))
    assets.save_asset(harq_path, only_if_is_dirty=False)

    can = set_pool(cannoneer_path, [can2artilleur, can2saboteur, mine, mine_shooter])
    can.set_editor_property("passive", assets.load_asset(passive_path))
    assets.save_asset(cannoneer_path, only_if_is_dirty=False)

    # anciennes upgrades de classe du Cannoneer : plus référencées
    for old in (old_shotgun_up, old_sniper_up):
        if old and assets.does_asset_exist(old):
            assets.delete_asset(old)
            log(f"supprimé : {old}")

    # ------------------------------------------------------------------ 12) cheveux en pétard du Cannoneer (cordes épaisses dressées)
    can_bp = assets.load_asset(cannoneer_path)
    if find_handle(can_bp, "HairSpike1") is None:
        head_handle = find_handle(can_bp, HEAD)
        if head_handle is None:
            raise RuntimeError("Head Feature introuvable sur le Cannoneer")
        head = lib.get_object(lib.get_data(head_handle))
        rope_src = next((t for t in templates(can_bp) if isinstance(t, unreal.YSDanglingChainComponent)), None)
        # racines sur le haut de la calotte (sphère de rayon 50 dans l'espace de Head Feature), aucune devant le visage
        spikes = [(0.0, 0.0)] + [(35.0, a) for a in (45.0, 135.0, 225.0, 315.0)] + [(62.0, a) for a in (60.0, 120.0, 180.0, 240.0, 300.0)]
        for i, (theta, phi) in enumerate(spikes):
            t, p = math.radians(theta), math.radians(phi)
            _, rope = add_component(can_bp, unreal.YSDanglingChainComponent, f"HairSpike{i + 1}", head_handle)
            if rope_src:
                ys.copy_editable_properties(rope_src, rope)
            rope.set_editor_property("relative_location", vec(46.0 * math.sin(t) * math.cos(p), 46.0 * math.sin(t) * math.sin(p), 46.0 * math.cos(t)))
            rope.set_editor_property("segment_count", 3 if theta > 0 else 4)
            rope.set_editor_property("segment_length", 11.0)
            rope.set_editor_property("segment_radius", 8.0)
            # gravité vers le haut : les cordes se dressent (en pétard) et partent en arrière en courant
            rope.set_editor_property("gravity", vec(0.0, 0.0, 900.0))
            rope.set_editor_property("trail_acceleration", 400.0)
            rope.set_editor_property("damping", 0.1)
            rope.set_editor_property("segment_material", head.get_material(0))
        log(f"Cannoneer : {len(spikes)} cordes HairSpike dressées sur la calotte")
        # tresses et lunettes (gardées par le Harquebusier) retirées après la copie de leurs réglages de corde
        removed = remove_where(can_bp, lambda n, o: n.startswith(("Braid", "Goggle")))
        log(f"Cannoneer : tresses et lunettes retirées ({removed} composants)")
        compile_save(can_bp, cannoneer_path)

    # ------------------------------------------------------------------ 13) sélection : Harquebusier entre Cannoneer et Quartermaster
    harq_class = assets.load_blueprint_class(harq_path)
    can_class = assets.load_blueprint_class(cannoneer_path)
    for path in GAME_MODES:
        gm = cdo(path)
        current = [c for c in gm.get_editor_property("selectable_characters") if c != harq_class]
        index = current.index(can_class) + 1 if can_class in current else len(current)
        current.insert(index, harq_class)
        gm.set_editor_property("selectable_characters", current)
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"{path} : " + ", ".join(c.get_name() for c in current if c))

    # ------------------------------------------------------------------ 14) ligne du Harquebusier dans DT_Chara_Upgrades (copie de celle du Cannoneer)
    try:
        table = assets.load_asset(f"{UPGRADES_DIR}/DT_Chara_Upgrades")
        rows = [str(r) for r in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)]
        if "Harquebusier" not in rows:
            data = json.loads(table.export_to_json_string() if hasattr(table, "export_to_json_string") else unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
            source = next((dict(r) for r in data if r.get("Name") == "Cannoneer"), None)
            if source is not None:
                source["Name"] = "Harquebusier"
                data.append(source)
                unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table, json.dumps(data))
                assets.save_loaded_asset(table, only_if_is_dirty=False)
                log("DT_Chara_Upgrades : ligne Harquebusier ajoutée (copie de Cannoneer)")
            else:
                log("DT_Chara_Upgrades : pas de ligne Cannoneer à copier")
    except Exception as table_error:
        log(f"DT_Chara_Upgrades : ligne Harquebusier non ajoutée ({table_error!r})")

    # ------------------------------------------------------------------ relecture
    for name, path in (("Harquebusier", harq_path), ("Cannoneer", cannoneer_path)):
        c = cdo(path)
        log(f"relu {name} : set {c.get_editor_property('weapon_set').get_name()}, passif {c.get_editor_property('passive').get_name()}, "
            f"pool {[u.get_name() if u else None for u in c.get_editor_property('upgrade_pool')][15:]}")
    for path in (harq_sniper_path, sniper_path, shotgun_path, canon_path, battle_bomb_path):
        bp = assets.load_asset(path)
        for t in templates(bp):
            if isinstance(t, unreal.YSAbilityComponent):
                line = f"relu {path.split('/')[-1]}.{t.get_name()} : {type(t).__name__} « {t.get_editor_property('display_name')} » recharge {t.get_editor_property('cooldown')}"
                if isinstance(t, unreal.YSCoupDPetoirAbility):
                    line += (f", {t.get_editor_property('shoot_system')} éventail {t.get_editor_property('spread_fire')} ({t.get_editor_property('shotgun_ammo_per_shot')}), "
                             f"rafale {t.get_editor_property('bullets_per_burst')} en {t.get_editor_property('burst_duration')} s, dégâts {t.get_editor_property('damage')}, "
                             f"pierce {t.get_editor_property('pierce_max_hits')}, AMMO {t.get_editor_property('projectile_class').get_name() if t.get_editor_property('projectile_class') else None}")
                if isinstance(t, unreal.YSRocketJumpAbility):
                    line += f", explosion {t.get_editor_property('explosion_damage')}"
                if t.get_editor_property("reset_auto_attack_on_use"):
                    line += f", rend l'attaque auto après {t.get_editor_property('auto_attack_reset_delay')} s"
                log(line)
        if find_component(bp, unreal.BoxComponent):
            log(f"   {path.split('/')[-1]} : Hitbox présente")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_harquebusier.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
