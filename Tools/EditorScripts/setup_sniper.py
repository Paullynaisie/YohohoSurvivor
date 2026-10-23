"""
Sniper du Cannoneer + upgrades de classe du Cannoneer :
  - /Game/Placeable/AMMO/SniperAMMO (enfant de PetoirAMMO) : balle allongée bleue, rapide (4000 cm/s, 0.6 s de vie)
  - /Game/Placeable/Weapons/Player/BP_Sniper (enfant de BP_Arme, À DEUX MAINS : OffHandSocket sous le fût) :
      « AutoShot » = Auto shot (réglages de celui du Battle Petoir) en shoot system Sniper : 1 balle qui touche 2 NME, 40 dégâts,
      recharge 2 s, visée assistée 1500 cm / 5°
      « BombThrower » = lance BP_CannoneerBomb en cloche vers le curseur (1 s de vol, 1000 cm max ; dégâts de la Roll and Bomb de BP_Bomb)
  - /Game/Core/WeaponSets/DA_WeaponSet_Sniper (main droite ; PV et pushback repris du set de départ du Cannoneer)
  - upgrades de classe DA_ClassUpgrade_Cannoneer2Shotgun / DA_ClassUpgrade_Cannoneer2Sniper (set AutoPetoir & Bomb -> Shotgun / Sniper),
    ajoutées à la fin du pool du Cannoneer
Ne recrée rien qui existe déjà.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_sniper.py"
Rapport : Saved/Logs/setup_sniper.txt
"""
import os
import unreal

AMMO_DIR = "/Game/Placeable/AMMO"
PETOIR_AMMO = f"{AMMO_DIR}/PetoirAMMO"
SNIPER_AMMO_NAME = "SniperAMMO"
SNIPER_AMMO = f"{AMMO_DIR}/{SNIPER_AMMO_NAME}"
BOMB_AMMO = f"{AMMO_DIR}/BP_CannoneerBomb"
WEAPONS = "/Game/Placeable/Weapons/Player"
BASE_WEAPON = "/Game/Placeable/Weapons/BP_Arme"
BATTLE_PETOIR = f"{WEAPONS}/BP_BattlePetoir"
BOMB_WEAPON = f"{WEAPONS}/BP_Bomb"
SNIPER_NAME = "BP_Sniper"
SNIPER = f"{WEAPONS}/{SNIPER_NAME}"
SETS = "/Game/Core/WeaponSets"
START_SET = f"{SETS}/DA_WeaponSet_AutoPetoirBomb"
SHOTGUN_SET = f"{SETS}/DA_WeaponSet_Shotgun"
SNIPER_SET_NAME = "DA_WeaponSet_Sniper"
SNIPER_SET = f"{SETS}/{SNIPER_SET_NAME}"
UPGRADES = "/Game/Core/Upgrades"
CLASS_UPGRADES = [
    ("DA_ClassUpgrade_Cannoneer2Shotgun", "Shotgun", SHOTGUN_SET, "Shotgun à deux mains : 5 AMMO en éventail\nAbility : Roll and Bomb"),
    ("DA_ClassUpgrade_Cannoneer2Sniper", "Sniper", SNIPER_SET, "Sniper à deux mains : la balle traverse les NME\nAbility : BombThrower"),
]
CANNONEER = "/Game/Placeable/Character/Player/PlayerCharacter_Cannoneer"
MAT = "/Game/Art/Materials/PH_materials"
SPHERE = "/Engine/BasicShapes/Sphere"
CYLINDER = "/Engine/BasicShapes/Cylinder"
CUBE = "/Engine/BasicShapes/Cube"

AUTO_SHOT_PROPS = ["cooldown", "damage", "assist_aim_distance", "assist_aim_angle", "recoil_kick_duration", "recoil_recover_duration",
                   "recoil_kick_ease_exponent", "recoil_recover_ease_exponent", "fit_recoil_to_fire_rate", "akimbo_fire_delay", "icon",
                   "icon_tint", "weapon_type"]
HEALTH_PUSHBACK = ["override_max_health", "max_health", "override_pushback", "pushback_min_damage_ratio", "pushback_max_damage_ratio",
                   "pushback_min_force", "pushback_max_force"]

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_sniper] {msg}")
    report.append(msg)


def vec(x, y, z):
    return unreal.Vector(x, y, z)


def rot(pitch=0.0, yaw=0.0, roll=0.0):
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


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


def shape(bp, name, mesh, material, location, rotation, scale):
    comp = add_component(bp, unreal.StaticMeshComponent, name)
    comp.set_editor_property("static_mesh", assets.load_asset(mesh))
    comp.set_material(0, assets.load_asset(f"{MAT}/{material}"))
    comp.set_editor_property("relative_location", location)
    comp.set_editor_property("relative_rotation", rotation)
    comp.set_editor_property("relative_scale3d", scale)
    comp.set_collision_profile_name("NoCollision")
    return comp


def copy_props(src, dst, names):
    for n in names:
        dst.set_editor_property(n, src.get_editor_property(n))


def save(path):
    assets.save_asset(path, only_if_is_dirty=False)


def create_bp(name, folder, parent_class):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    return tools.create_asset(name, folder, unreal.Blueprint, factory)


def create_data_asset(name, folder, cls):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    return tools.create_asset(name, folder, cls, factory)


try:
    # 1) SniperAMMO
    if not assets.does_asset_exist(SNIPER_AMMO):
        create_bp(SNIPER_AMMO_NAME, AMMO_DIR, assets.load_blueprint_class(PETOIR_AMMO))
        ammo = cdo(SNIPER_AMMO)
        ammo.set_editor_property("speed", 4000.0)
        ammo.set_editor_property("life_time", 0.6)
        mesh = ammo.get_editor_property("mesh")
        mesh.set_editor_property("relative_scale3d", vec(0.9, 0.2, 0.2))
        mesh.set_material(0, assets.load_asset(f"{MAT}/m_PH_emissive_blue"))
        save(SNIPER_AMMO)
        log(f"créé : {SNIPER_AMMO} (4000 cm/s, 0.6 s, balle allongée bleue)")
    else:
        log(f"existe déjà : {SNIPER_AMMO}")

    # 2) Sniper : arme à deux mains, Auto shot en Sniper + BombThrower
    if not assets.does_asset_exist(SNIPER):
        bp = create_bp(SNIPER_NAME, WEAPONS, assets.load_blueprint_class(BASE_WEAPON))
        shape(bp, "Receiver", CUBE, "m_PH_monochrome_LightBlack", vec(10.0, 0.0, 4.0), rot(), vec(0.24, 0.09, 0.11))
        shape(bp, "Barrel", CYLINDER, "m_PH_monochrome_LightBlack", vec(55.0, 0.0, 6.0), rot(pitch=-90.0), vec(0.05, 0.05, 0.85))
        shape(bp, "MuzzleBrake", CYLINDER, "m_PH_monochrome_Grey", vec(96.0, 0.0, 6.0), rot(pitch=-90.0), vec(0.08, 0.08, 0.07))
        shape(bp, "ForeEnd", CUBE, "m_PH_monochrome_Brown", vec(34.0, 0.0, 2.5), rot(), vec(0.26, 0.08, 0.07))
        shape(bp, "Scope", CYLINDER, "m_PH_monochrome_Black", vec(14.0, 0.0, 14.0), rot(pitch=-90.0), vec(0.07, 0.07, 0.3))
        shape(bp, "ScopeLensFront", CYLINDER, "m_PH_emissive_blue", vec(29.5, 0.0, 14.0), rot(pitch=-90.0), vec(0.09, 0.09, 0.02))
        shape(bp, "ScopeLensBack", CYLINDER, "m_PH_monochrome_Grey", vec(-1.5, 0.0, 14.0), rot(pitch=-90.0), vec(0.08, 0.08, 0.02))
        shape(bp, "ScopeMount", CUBE, "m_PH_monochrome_Grey", vec(14.0, 0.0, 9.5), rot(), vec(0.12, 0.03, 0.06))

        battle_shot = component(BATTLE_PETOIR, unreal.YSAutoShotAbility)
        shot = add_component(bp, unreal.YSAutoShotAbility, "AutoShot")
        copy_props(battle_shot, shot, AUTO_SHOT_PROPS)
        shot.set_editor_property("display_name", "Sniper")
        shot.set_editor_property("shoot_system", unreal.YSShootSystem.SNIPER)
        shot.set_editor_property("bullets_per_burst", 1)
        shot.set_editor_property("burst_duration", 0.0)
        shot.set_editor_property("sniper_max_hits", 2)
        shot.set_editor_property("damage", 40.0)
        shot.set_editor_property("cooldown", 2.0)
        shot.set_editor_property("assist_aim_distance", 1500.0)
        shot.set_editor_property("assist_aim_angle", 5.0)
        shot.set_editor_property("projectile_class", assets.load_blueprint_class(SNIPER_AMMO))
        shot.set_editor_property("recoil_pitch", 25.0)
        shot.set_editor_property("recoil_kick_back", 30.0)
        shot.set_editor_property("play_reload_anim", False)

        roll_bomb = component(BOMB_WEAPON, unreal.YSRollAndBombAbility)
        thrower = add_component(bp, unreal.YSBombThrowerAbility, "BombThrower")
        thrower.set_editor_property("bomb_class", assets.load_blueprint_class(BOMB_AMMO))
        if roll_bomb:
            thrower.set_editor_property("bomb_damage", roll_bomb.get_editor_property("bomb_damage"))
            thrower.set_editor_property("cooldown", roll_bomb.get_editor_property("cooldown"))
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)

        weapon = cdo(SNIPER)
        weapon.set_editor_property("display_name", "Sniper")
        weapon.set_editor_property("two_handed", True)
        weapon.set_editor_property("override_hand_location", True)
        weapon.set_editor_property("hand_location", vec(30.0, 28.0, -5.0))
        weapon.set_editor_property("override_hand_rotation", True)
        weapon.set_editor_property("hand_rotation", rot())
        stock = weapon.get_editor_property("mesh")
        stock.set_editor_property("static_mesh", assets.load_asset(CUBE))
        stock.set_material(0, assets.load_asset(f"{MAT}/m_PH_monochrome_Brown"))
        stock.set_editor_property("relative_location", vec(-14.0, 0.0, -1.0))
        stock.set_editor_property("relative_rotation", rot(pitch=-10.0))
        stock.set_editor_property("relative_scale3d", vec(0.32, 0.08, 0.14))
        stock.set_collision_profile_name("NoCollision")
        weapon.get_editor_property("muzzle").set_editor_property("relative_location", vec(100.0, 0.0, 6.0))
        weapon.get_editor_property("off_hand_socket").set_editor_property("relative_location", vec(34.0, 0.0, -2.0))
        save(SNIPER)
        log(f"créé : {SNIPER} (AutoShot Sniper 1 balle x 40, 2 NME, recharge 2 s, 1500 cm / 5° ; "
            f"BombThrower {thrower.get_editor_property('bomb_damage')} dégâts, recharge {thrower.get_editor_property('cooldown')} s)")
    else:
        log(f"existe déjà : {SNIPER}")

    # 3) set Sniper
    if not assets.does_asset_exist(SNIPER_SET):
        weapon_set = create_data_asset(SNIPER_SET_NAME, SETS, unreal.YSWeaponSet)
        weapon_set.set_editor_property("display_name", "Sniper")
        weapon_set.set_editor_property("right_hand_weapon", assets.load_blueprint_class(SNIPER))
        copy_props(assets.load_asset(START_SET), weapon_set, HEALTH_PUSHBACK)
        save(SNIPER_SET)
        log(f"créé : {SNIPER_SET} (PV {weapon_set.get_editor_property('max_health')})")
    else:
        log(f"existe déjà : {SNIPER_SET}")

    # 4) upgrades de classe du Cannoneer, à la fin de son pool
    character = cdo(CANNONEER)
    pool = list(character.get_editor_property("upgrade_pool"))
    for name, display, target, description in CLASS_UPGRADES:
        path = f"{UPGRADES}/{name}"
        if not assets.does_asset_exist(path):
            upgrade = create_data_asset(name, UPGRADES, unreal.YSClassUpgrade)
            upgrade.set_editor_property("display_name", display)
            upgrade.set_editor_property("description", description)
            upgrade.set_editor_property("source_weapon_set", assets.load_asset(START_SET))
            upgrade.set_editor_property("target_weapon_set", assets.load_asset(target))
            save(path)
            log(f"créé : {path} (AutoPetoir & Bomb -> {display})")
        upgrade = assets.load_asset(path)
        if upgrade not in pool:
            pool.append(upgrade)
            log(f"{name} ajoutée au pool du Cannoneer (index {len(pool) - 1})")
    character.set_editor_property("upgrade_pool", pool)
    save(CANNONEER)

    # relecture
    for t in templates(assets.load_asset(SNIPER)):
        if isinstance(t, unreal.YSAbilityComponent):
            log(f"relu BP_Sniper.{t.get_name()} : {t.get_class().get_name()} recharge {t.get_editor_property('cooldown')}")
    log("relu pool du Cannoneer : " + ", ".join(f"{i} {u.get_name()}" for i, u in enumerate(cdo(CANNONEER).get_editor_property("upgrade_pool")) if u))
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_sniper.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
