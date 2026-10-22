"""
Attaques des NME :
  - BP_NME_Sword : composant « AutoSwing » (UYSAutoSwingAbility) = l'Auto swing de BP_Epee (découpage de l'action, poses
    et timings de l'animation copiés), HitTags Team.Player (touche les joueurs, jamais les NME) ; déclenché par la hitbox
    de contact du NME (AYSFoeCharacter), dégâts = ContactDamage du NME
  - /Game/Placeable/AMMO/NME_GunAMMO : enfant de PetoirAMMO, HitTags Team.Player, 2x plus lente, plus grosse, émissif rouge
  - /Game/Placeable/Weapons/Foe/BP_NME_Pistol : copie visuelle de BP_Pistolet (mêmes réglages de position de main),
    coloration turboGreen -> m_PH_emissive_turboRed, actions retirées, non répliquée
  - /Game/Core/WeaponSets/DA_WeaponSet_NME_Pistol : set « NME pistol » (main droite)
  - /Game/Placeable/Character/FoeCharacter_Skull_Gun : copie de FoeCharacter_Skull_Bomb (mêmes stats et même comportement :
    court jusqu'à Attack Range puis attaque à distance), Ranged Attack = Shot, NME pistol, 50 % gaucher
  - BP_TestGameMode : Skull Gun ajouté aux NME des vagues (même Spawn Weight que Skull Bomb : ~1 sur 5)
Ne recrée pas ce qui existe (réglages de Paul conservés).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_foe_attacks.py"
Rapport : Saved/Logs/setup_foe_attacks.txt
"""
import os
import unreal

WEAPONS = "/Game/Placeable/Weapons"
FOE_WEAPONS = f"{WEAPONS}/Foe"
SWORD_BP = f"{WEAPONS}/BP_Epee"
PISTOL_BP = f"{WEAPONS}/BP_Pistolet"
NME_SWORD = f"{FOE_WEAPONS}/BP_NME_Sword"
NME_PISTOL_NAME = "BP_NME_Pistol"
NME_PISTOL = f"{FOE_WEAPONS}/{NME_PISTOL_NAME}"
AMMO_DIR = "/Game/Placeable/AMMO"
BASE_AMMO = f"{AMMO_DIR}/PetoirAMMO"
GUN_AMMO_NAME = "NME_GunAMMO"
GUN_AMMO = f"{AMMO_DIR}/{GUN_AMMO_NAME}"
SETS_DIR = "/Game/Core/WeaponSets"
PISTOL_SET_NAME = "DA_WeaponSet_NME_Pistol"
PISTOL_SET = f"{SETS_DIR}/{PISTOL_SET_NAME}"
CHAR_DIR = "/Game/Placeable/Character"
SKULL = f"{CHAR_DIR}/FoeCharacter_Skull"
SKULL_BOMB = f"{CHAR_DIR}/FoeCharacter_Skull_Bomb"
SKULL_GUN_NAME = "FoeCharacter_Skull_Gun"
SKULL_GUN = f"{CHAR_DIR}/{SKULL_GUN_NAME}"
GAME_MODE_BP = "/Game/Core/GameModes/BP_TestGameMode"
MAT = "/Game/Art/Materials/PH_materials"
TURBO_GREEN = "m_PH_emissive_turboGreen"
TURBO_RED = f"{MAT}/m_PH_emissive_turboRed"
SWING_PROPS = ["auto_atk_cooldown", "cooldown", "akimbo_fire_delay", "melee_system", "windup_duration", "swing_duration", "recover_duration",
               "windup_hand_location", "windup_hand_rotation", "swing_end_hand_location", "swing_end_hand_rotation", "circular_swing",
               "windup_ease_exponent", "swing_ease_exponent", "recover_ease_exponent", "show_hitbox", "hitbox_color", "icon_tint"]
AMMO_SPEED_FACTOR = 0.5   # 2x plus lente que la balle du joueur
AMMO_SIZE_FACTOR = 1.5    # plus grosse
LEFT_HANDED_CHANCE = 0.5

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_foe_attacks] {msg}")
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
    player_tags = cdo(SKULL).get_editor_property("contact_hit_tags")  # Team.Player

    # 1) NME sword : Auto swing de l'épée du joueur, qui touche les joueurs
    sword = assets.load_asset(NME_SWORD)
    swing = next((obj for _, obj in components(sword) if isinstance(obj, unreal.YSAutoSwingAbility)), None)
    if swing is None:
        source = next(obj for _, obj in components(assets.load_asset(SWORD_BP)) if isinstance(obj, unreal.YSAutoSwingAbility))
        root = subsystem.k2_gather_subobject_data_for_blueprint(sword)[0]
        handle, fail = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.YSAutoSwingAbility, blueprint_context=sword))
        if not fail.is_empty():
            raise RuntimeError(f"ajout de l'Auto swing impossible : {fail}")
        subsystem.rename_subobject(handle, unreal.Text("AutoSwing"))
        swing = lib.get_object(lib.get_data(handle))
        for prop in SWING_PROPS:
            swing.set_editor_property(prop, source.get_editor_property(prop))
        swing.set_editor_property("hit_tags", player_tags)
        swing.set_editor_property("display_name", "NME swing")
        unreal.BlueprintEditorLibrary.compile_blueprint(sword)
        assets.save_asset(NME_SWORD, only_if_is_dirty=False)
        log(f"BP_NME_Sword : Auto swing ajouté (copié de BP_Epee, touche Team.Player)")
    else:
        log("BP_NME_Sword : Auto swing déjà là")

    # 2) balle des NME
    if not assets.does_asset_exist(GUN_AMMO):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", assets.load_blueprint_class(BASE_AMMO))
        tools.create_asset(GUN_AMMO_NAME, AMMO_DIR, unreal.Blueprint, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(assets.load_asset(GUN_AMMO))
        base = cdo(BASE_AMMO)
        ammo = cdo(GUN_AMMO)
        ammo.set_editor_property("speed", base.get_editor_property("speed") * AMMO_SPEED_FACTOR)
        ammo.set_editor_property("hit_tags", player_tags)
        collision = ammo.get_editor_property("collision")
        collision.set_editor_property("sphere_radius", base.get_editor_property("collision").get_editor_property("sphere_radius") * AMMO_SIZE_FACTOR)
        mesh = ammo.get_editor_property("mesh")
        mesh.set_editor_property("relative_scale3d", base.get_editor_property("mesh").get_editor_property("relative_scale3d") * AMMO_SIZE_FACTOR)
        mesh.set_material(0, assets.load_asset(TURBO_RED))
        assets.save_asset(GUN_AMMO, only_if_is_dirty=False)
        log(f"créé : {GUN_AMMO}")
    else:
        log(f"existe déjà : {GUN_AMMO}")

    # 3) NME pistol : copie visuelle du pistolet du joueur, coloration rouge émissive
    if not assets.does_asset_exist(NME_PISTOL):
        tools.duplicate_asset(NME_PISTOL_NAME, FOE_WEAPONS, assets.load_asset(PISTOL_BP))
        pistol = assets.load_asset(NME_PISTOL)
        handles = subsystem.k2_gather_subobject_data_for_blueprint(pistol)
        root = handles[0]
        recolored = 0
        for handle in handles:
            obj = lib.get_object(lib.get_data(handle))
            if isinstance(obj, unreal.YSAbilityComponent):
                subsystem.delete_subobject(root, handle, pistol)
            elif isinstance(obj, unreal.StaticMeshComponent):
                for index, material in enumerate(obj.get_materials()):
                    if material and material.get_name() == TURBO_GREEN:
                        obj.set_material(index, assets.load_asset(TURBO_RED))
                        recolored += 1
        unreal.BlueprintEditorLibrary.compile_blueprint(pistol)
        pistol_cdo = cdo(NME_PISTOL)
        pistol_cdo.set_editor_property("replicates", False)
        pistol_cdo.set_editor_property("display_name", "NME pistol")
        assets.save_asset(NME_PISTOL, only_if_is_dirty=False)
        log(f"créé : {NME_PISTOL} ({recolored} forme(s) recolorée(s) en turboRed)")
    else:
        log(f"existe déjà : {NME_PISTOL}")

    # 4) set d'arme
    if not assets.does_asset_exist(PISTOL_SET):
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.YSWeaponSet)
        weapon_set = tools.create_asset(PISTOL_SET_NAME, SETS_DIR, unreal.YSWeaponSet, factory)
        weapon_set.set_editor_property("display_name", "NME pistol")
        weapon_set.set_editor_property("right_hand_weapon", assets.load_blueprint_class(NME_PISTOL))
        assets.save_asset(PISTOL_SET, only_if_is_dirty=False)
        log(f"créé : {PISTOL_SET}")
    weapon_set = assets.load_asset(PISTOL_SET)

    # 5) Skull Gun : mêmes stats et comportement que Skull Bomb, tir au lieu de la bombe
    if not assets.does_asset_exist(SKULL_GUN):
        tools.duplicate_asset(SKULL_GUN_NAME, CHAR_DIR, assets.load_asset(SKULL_BOMB))
        unreal.BlueprintEditorLibrary.compile_blueprint(assets.load_asset(SKULL_GUN))
        # l'attaque (tir) est celle de son arme : action « Shot » de BP_NME_Pistol (setup_foe_weapon_attacks.py)
        gun = cdo(SKULL_GUN)
        gun.set_editor_property("foe_name", "Skull Gun")
        gun.set_editor_property("weapon_set", weapon_set)
        gun.set_editor_property("left_handed_chance", LEFT_HANDED_CHANCE)
        assets.save_asset(SKULL_GUN, only_if_is_dirty=False)
        log(f"créé : {SKULL_GUN} (copie de Skull Bomb)")
    else:
        log(f"existe déjà : {SKULL_GUN}")
    gun_class = assets.load_blueprint_class(SKULL_GUN)

    # 6) vagues : Skull Gun parmi les NME
    gm = cdo(GAME_MODE_BP)
    wave = next((r for r in gm.get_editor_property("spawn_rules") if r and isinstance(r, unreal.YSTimedWaveSpawnRule)), None)
    if wave is None:
        raise RuntimeError("pas de règle « Vagues jour / nuit » dans BP_TestGameMode")
    classes = list(wave.get_editor_property("enemy_classes"))
    if gun_class not in classes:
        classes.append(gun_class)
        wave.set_editor_property("enemy_classes", classes)
        assets.save_asset(GAME_MODE_BP, only_if_is_dirty=False)
        log("BP_TestGameMode : Skull Gun ajouté aux vagues")

    # relecture
    check_swing = next(obj for _, obj in components(assets.load_asset(NME_SWORD)) if isinstance(obj, unreal.YSAutoSwingAbility))
    log(f"relu : NME swing windup {check_swing.get_editor_property('windup_duration')} swing {check_swing.get_editor_property('swing_duration')} "
        f"recover {check_swing.get_editor_property('recover_duration')} CD {check_swing.get_editor_property('auto_atk_cooldown')} tags {check_swing.get_editor_property('hit_tags')}")
    ammo = cdo(GUN_AMMO)
    log(f"relu : NME_GunAMMO vitesse {ammo.get_editor_property('speed')} rayon {ammo.get_editor_property('collision').get_editor_property('sphere_radius')} "
        f"échelle {ammo.get_editor_property('mesh').get_editor_property('relative_scale3d')} tags {ammo.get_editor_property('hit_tags')}")
    pistol_cdo = cdo(NME_PISTOL)
    log(f"relu : NME pistol replicates {pistol_cdo.get_editor_property('replicates')} main {pistol_cdo.get_editor_property('hand_location')} "
        f"actions {[o.get_name() for _, o in components(assets.load_asset(NME_PISTOL)) if isinstance(o, unreal.YSAbilityComponent)]} "
        f"matériaux {sorted(set(m.get_name() for _, o in components(assets.load_asset(NME_PISTOL)) if isinstance(o, unreal.StaticMeshComponent) for m in o.get_materials() if m))}")
    gun = cdo(SKULL_GUN)
    log("relu : Skull Gun " + ", ".join(f"{p} {gun.get_editor_property(p)}" for p in
        ["foe_name", "base_health", "health_per_level", "move_speed", "spawn_weight", "left_handed_chance"]) + f", set {gun.get_editor_property('weapon_set').get_name()}")
    log(f"relu : vagues {[c.get_name() for c in wave.get_editor_property('enemy_classes')]}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_foe_attacks.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
