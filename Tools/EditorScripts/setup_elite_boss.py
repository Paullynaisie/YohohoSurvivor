"""
Élites et Boss des NME (2026-11-03) :
  - FoeCharacter_Skull_Boss = copie de FoeCharacter_Skull_Elite telle qu'elle était (Boss armor, bouclier, Boss Big Slash, PV x20, taille x2.5).
  - FoeCharacter_Skull_Elite devient un élite sans mécaniques de boss : Boss Armor décoché, PV x10 (au lieu de x20), taille x2,
    set DA_WeaponSet_NME_GreatSword (ancienne NME great sword, sans Boss Big Slash). Fait UNE fois (à la création du Boss).
  - FoeCharacter_Skull_Gun_Elite / FoeCharacter_Skull_Gun_Boss : enfants de FoeCharacter_Skull_Gun, avec ce qui distingue
    l'élite / le boss du Skull (propriétés du jeu qui diffèrent, sauf le set d'arme, le nom et la touche).
  - Boss Gun : /Game/Placeable/Weapons/Foe/BP_NME_BossPistol (enfant de BP_NME_Pistol + composant « Roll360shot »)
    et /Game/Core/WeaponSets/DA_WeaponSet_NME_BossPistol.
  - TestMap (BP_TestMapGameMode) : pavé numérique 6 Elite Skull, 7 Elite Gun, 8 Boss Skull, 9 Boss Gun ; Jolly Ship (7), Galion (8)
    et FoeCharacter_Skull_Elite_Test (8) déconnectés.
  - DA_I_StartIsland : vérifie que l'élite (pas le boss) apparaît la nuit.
Ne recrée rien qui existe (relançable).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_elite_boss.py"
Rapport : Saved/Logs/setup_elite_boss.txt
"""
import os
import unreal

FOE_DIR = "/Game/Placeable/Character/Foe"
SKULL = f"{FOE_DIR}/FoeCharacter_Skull"
ELITE = f"{FOE_DIR}/FoeCharacter_Skull_Elite"
ELITE_TEST = f"{FOE_DIR}/FoeCharacter_Skull_Elite_Test"
BOSS = f"{FOE_DIR}/FoeCharacter_Skull_Boss"
GUN = f"{FOE_DIR}/FoeCharacter_Skull_Gun"
GUN_ELITE_NAME = "FoeCharacter_Skull_Gun_Elite"
GUN_BOSS_NAME = "FoeCharacter_Skull_Gun_Boss"
WEAPON_DIR = "/Game/Placeable/Weapons/Foe"
PISTOL_BP = f"{WEAPON_DIR}/BP_NME_Pistol"
BOSS_PISTOL_NAME = "BP_NME_BossPistol"
BOSS_PISTOL_BP = f"{WEAPON_DIR}/{BOSS_PISTOL_NAME}"
SET_DIR = "/Game/Core/WeaponSets"
GREAT_SWORD_SET = f"{SET_DIR}/DA_WeaponSet_NME_GreatSword"
PISTOL_SET = f"{SET_DIR}/DA_WeaponSet_NME_Pistol"
BOSS_PISTOL_SET = f"{SET_DIR}/DA_WeaponSet_NME_BossPistol"
TEST_GM = "/Game/Core/GameModes/BP_TestMapGameMode"
START_ISLAND = "/Game/Core/Islands/DA_I_StartIsland"

# propriétés gardées propres à chaque NME (le reste de la différence Skull -> élite / boss est copié)
EXCLUDED = ["WeaponSet", "LeftHandWeaponClass", "RightHandWeaponClass", "FoeName", "NumpadSpawnKey"]

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
ys = unreal.YSEditorScriptLibrary
report = []


def log(msg):
    unreal.log(f"[setup_elite_boss] {msg}")
    report.append(msg)


def cls(path):
    return assets.load_blueprint_class(path)


def cdo(path):
    return unreal.get_default_object(cls(path))


def save(path):
    assets.save_asset(path, only_if_is_dirty=False)


def child_blueprint(parent_path, name, folder):
    path = f"{folder}/{name}"
    if assets.does_asset_exist(path):
        log(f"existe déjà : {path}")
        return assets.load_asset(path), False
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", cls(parent_path))
    bp = tools.create_asset(name, folder, unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    log(f"créé : {path} (enfant de {parent_path})")
    return bp, True


def components(bp):
    result = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if obj:
            result.append((handle, obj))
    return result


def describe(path):
    o = cdo(path)
    return (f"{path.split('/')[-1]} : « {o.get_editor_property('foe_name')} », élite {o.get_editor_property('is_elite')}, "
            f"boss armor {o.get_editor_property('boss_armor')}, PV x{o.get_editor_property('health_multiplier')}, "
            f"dégâts x{o.get_editor_property('damage_multiplier')}, taille x{o.get_editor_property('size_multiplier')}, "
            f"set {o.get_editor_property('weapon_set').get_name() if o.get_editor_property('weapon_set') else None}, "
            f"touche {o.get_editor_property('numpad_spawn_key')}")


try:
    # 1. Boss = copie de l'élite actuel ; l'élite perd ses mécaniques de boss (une seule fois, à la création du Boss)
    if assets.does_asset_exist(BOSS):
        log(f"existe déjà : {BOSS} (élite déjà modifié, non retouché)")
    else:
        assets.duplicate_asset(ELITE, BOSS)
        boss = cdo(BOSS)
        boss.set_editor_property("foe_name", "Foe Skull Boss")
        boss.set_editor_property("numpad_spawn_key", 8)
        save(BOSS)
        log(f"créé : {BOSS} (copie de l'élite)")

        elite = cdo(ELITE)
        elite.set_editor_property("boss_armor", False)
        elite.set_editor_property("health_multiplier", elite.get_editor_property("health_multiplier") / 2.0)
        elite.set_editor_property("size_multiplier", 2.0)
        elite.set_editor_property("weapon_set", assets.load_asset(GREAT_SWORD_SET))
        elite.set_editor_property("numpad_spawn_key", 6)
        save(ELITE)
        log(f"{ELITE} : sans Boss armor, PV / 2, taille x2, set {GREAT_SWORD_SET}, touche 6")

    # 2. arme du Boss Gun : pistolet de NME + Roll360shot
    boss_pistol, _ = child_blueprint(PISTOL_BP, BOSS_PISTOL_NAME, WEAPON_DIR)
    roll = next((obj for _, obj in components(boss_pistol) if isinstance(obj, unreal.YSFoeRoll360ShotAction)), None)
    if roll is None:
        root = subsystem.k2_gather_subobject_data_for_blueprint(boss_pistol)[0]
        params = unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.YSFoeRoll360ShotAction, blueprint_context=boss_pistol)
        handle, fail = subsystem.add_new_subobject(params)
        if not lib.is_handle_valid(handle):
            raise RuntimeError(f"ajout du Roll360shot impossible : {fail}")
        subsystem.rename_subobject(handle, unreal.Text("Roll360shot"))
        log("composant Roll360shot ajouté (réglages par défaut du C++)")
    unreal.BlueprintEditorLibrary.compile_blueprint(boss_pistol)
    cdo(BOSS_PISTOL_BP).set_editor_property("display_name", "NME boss pistol")
    save(BOSS_PISTOL_BP)

    if assets.does_asset_exist(BOSS_PISTOL_SET):
        log(f"existe déjà : {BOSS_PISTOL_SET}")
    else:
        assets.duplicate_asset(PISTOL_SET, BOSS_PISTOL_SET)
        boss_set = assets.load_asset(BOSS_PISTOL_SET)
        boss_set.set_editor_property("right_hand_weapon", cls(BOSS_PISTOL_BP))
        boss_set.set_editor_property("display_name", "NME boss pistol")
        save(BOSS_PISTOL_SET)
        log(f"créé : {BOSS_PISTOL_SET} (copie de {PISTOL_SET}, main droite {BOSS_PISTOL_NAME})")

    # 3. Elite Gun / Boss Gun : Skull Gun + ce qui distingue l'élite / le boss du Skull
    for name, source, foe_name, key, weapon_set in [
        (GUN_ELITE_NAME, ELITE, "Skull Gun Elite", 7, None),
        (GUN_BOSS_NAME, BOSS, "Skull Gun Boss", 9, BOSS_PISTOL_SET),
    ]:
        path = f"{FOE_DIR}/{name}"
        _, created = child_blueprint(GUN, name, FOE_DIR)
        if created:
            target = cdo(path)
            copied = ys.copy_changed_properties(cdo(source), cdo(SKULL), target, EXCLUDED)
            log(f"{name} : copié depuis {source.split('/')[-1]} :\n  " + "\n  ".join(copied.strip().splitlines()))
            target.set_editor_property("foe_name", foe_name)
            target.set_editor_property("numpad_spawn_key", key)
            if weapon_set:
                target.set_editor_property("weapon_set", assets.load_asset(weapon_set))
            save(path)

    # 4. pavé numérique de TestMap
    gm = cdo(TEST_GM)
    gm.set_editor_property("numpad_jolly_ship_key", -1)
    gm.set_editor_property("numpad_galion_key", -1)
    classes = list(gm.get_editor_property("numpad_foe_classes"))
    for path in [ELITE, f"{FOE_DIR}/{GUN_ELITE_NAME}", BOSS, f"{FOE_DIR}/{GUN_BOSS_NAME}"]:
        c = cls(path)
        if c not in classes:
            classes.append(c)
    gm.set_editor_property("numpad_foe_classes", classes)
    save(TEST_GM)
    if assets.does_asset_exist(ELITE_TEST) and cdo(ELITE_TEST).get_editor_property("numpad_spawn_key") in (6, 7, 8, 9):
        cdo(ELITE_TEST).set_editor_property("numpad_spawn_key", -1)
        save(ELITE_TEST)
        log(f"{ELITE_TEST} : touche retirée")

    # relecture
    for path in [SKULL, ELITE, BOSS, GUN, f"{FOE_DIR}/{GUN_ELITE_NAME}", f"{FOE_DIR}/{GUN_BOSS_NAME}"]:
        log("relu : " + describe(path))
    gm = cdo(TEST_GM)
    log(f"relu : TestMap jolly {gm.get_editor_property('numpad_jolly_ship_key')}, galion {gm.get_editor_property('numpad_galion_key')}")
    keys = {}
    for c in gm.get_editor_property("numpad_foe_classes"):
        if c:
            k = unreal.get_default_object(c).get_editor_property("numpad_spawn_key")
            keys.setdefault(k, []).append(c.get_name())
    log(f"relu : touches {dict(sorted(keys.items()))}")
    for _, obj in components(assets.load_asset(BOSS_PISTOL_BP)):
        if isinstance(obj, unreal.YSFoeRoll360ShotAction):
            log(f"relu : Roll360shot roulade {obj.get_editor_property('roll_distance')} cm / {obj.get_editor_property('roll_duration')} s, "
                f"{obj.get_editor_property('shot_count')} balles / {obj.get_editor_property('angle_step')}° / {obj.get_editor_property('shot_interval')} s, "
                f"dégâts x{obj.get_editor_property('damage_multiplier')}, échelle x{obj.get_editor_property('bullet_scale')}")

    island = assets.load_asset(START_ISLAND)
    for ev in island.get_editor_property("night_events"):
        for f in ev.get_editor_property("foes"):
            fc = f.get_editor_property("foe_class")
            if fc and unreal.get_default_object(fc).get_editor_property("is_elite"):
                log(f"relu : nuit de DA_I_StartIsland à {ev.get_editor_property('time')} s : {fc.get_name()} "
                    f"(boss armor {unreal.get_default_object(fc).get_editor_property('boss_armor')})")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_elite_boss.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
