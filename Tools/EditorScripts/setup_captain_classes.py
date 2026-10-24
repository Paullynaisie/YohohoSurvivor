"""
Upgrades de classe du Captain (set de départ DA_WeaponSet_Captain : Captain Gun + Crochet du capitaine) :
  - « Corsaire » (DA_WeaponSet_Corsaire) : droite /Game/Placeable/Weapons/Player/BP_CaptainSword (copie de BP_Epee, « Captain Sword » :
    son Auto swing marque le NME touché, et les minions font des critiques sur ce NME), gauche le Crochet du capitaine (inchangé)
  - « Cursed Captain » (DA_WeaponSet_CursedCaptain) : droite BP_CursedCaptainGun (copie de BP_CaptainGun, « Cursed Captain Gun » :
    balle /Game/Placeable/AMMO/CursedCaptainAMMO = enfant de CaptainAMMO, violette, Mark Spawns Minion : à la mort du NME marqué un minion
    apparaît à sa place s'il reste de la place) ; gauche BP_CursedHook (copie du Crochet, « Cursed Hook » : Summon the crew remplacé par
    « GoMinions » = Allez-y mes minions !, mêmes réglages)
  - Abilities des minions (jamais utilisées d'elles-mêmes, forcées par Allez-y mes minions !) : « DashingSlash » sur BP_MinionSword
    (réglages du Dashing Slash de BP_BattleSword), « RollingShot » sur BP_MinionAutoGun (réglages du Rolling shot de BP_Pistolet)
  - /Game/Core/Upgrades/DA_ClassUpgrade_Captain2Corsaire, DA_ClassUpgrade_Captain2CursedCaptain et DA_Upgrade_AllMyMinions
    (« You're all my minions », poids copiés de DA_Upgrade_Mine), ajoutés à la fin du pool du Captain (les index existants ne bougent pas)
Ne recrée rien qui existe déjà (réglages de Paul conservés).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_captain_classes.py"
Rapport : Saved/Logs/setup_captain_classes.txt
"""
import os
import unreal

AMMO_DIR = "/Game/Placeable/AMMO"
CAPTAIN_AMMO = f"{AMMO_DIR}/CaptainAMMO"
CURSED_AMMO_NAME = "CursedCaptainAMMO"
CURSED_AMMO = f"{AMMO_DIR}/{CURSED_AMMO_NAME}"
WEAPON_DIR = "/Game/Placeable/Weapons/Player"
SWORD = f"{WEAPON_DIR}/BP_Epee"
BATTLE_SWORD = f"{WEAPON_DIR}/BP_BattleSword"
PISTOL = f"{WEAPON_DIR}/BP_Pistolet"
CAPTAIN_GUN = f"{WEAPON_DIR}/BP_CaptainGun"
HOOK = f"{WEAPON_DIR}/BP_CaptainHook"
MINION_GUN = f"{WEAPON_DIR}/BP_MinionAutoGun"
MINION_SWORD = f"{WEAPON_DIR}/BP_MinionSword"
CAPTAIN_SWORD_NAME, CURSED_GUN_NAME, CURSED_HOOK_NAME = "BP_CaptainSword", "BP_CursedCaptainGun", "BP_CursedHook"
CAPTAIN_SWORD, CURSED_GUN, CURSED_HOOK = (f"{WEAPON_DIR}/{n}" for n in (CAPTAIN_SWORD_NAME, CURSED_GUN_NAME, CURSED_HOOK_NAME))
SETS_DIR = "/Game/Core/WeaponSets"
CAPTAIN_SET = f"{SETS_DIR}/DA_WeaponSet_Captain"
UPGRADES_DIR = "/Game/Core/Upgrades"
MINE_UPGRADE = f"{UPGRADES_DIR}/DA_Upgrade_Mine"
ALL_MY_MINIONS_NAME = "DA_Upgrade_AllMyMinions"
ALL_MY_MINIONS = f"{UPGRADES_DIR}/{ALL_MY_MINIONS_NAME}"
CAPTAIN = "/Game/Placeable/Character/Player/PlayerCharacter_Captain"
MAT = "/Game/Art/Materials/PH_materials"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
ys = unreal.YSEditorScriptLibrary
report = []


def log(msg):
    unreal.log(f"[setup_captain_classes] {msg}")
    report.append(msg)


def handles(bp):
    return subsystem.k2_gather_subobject_data_for_blueprint(bp)


def templates(bp):
    result = []
    for h in handles(bp):
        obj = lib.get_object(lib.get_data(h))
        if obj and obj not in result:
            result.append(obj)
    return result


def action_of(bp, cls):
    return next((t for t in templates(bp) if type(t) == cls), None)


def add_component(bp, cls, name):
    handle, fail = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=handles(bp)[0], new_class=cls, blueprint_context=bp))
    if not fail.is_empty():
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    subsystem.rename_subobject(handle, unreal.Text(name))
    return lib.get_object(lib.get_data(handle))


def delete_component(bp, name):
    for h in handles(bp):
        if str(lib.get_variable_name(lib.get_data(h))) == name:
            subsystem.delete_subobject(handles(bp)[0], h, bp)
            return True
    return False


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def save(path):
    assets.save_asset(path, only_if_is_dirty=False)


def duplicate_weapon(name, source, display, setup):
    path = f"{WEAPON_DIR}/{name}"
    if assets.does_asset_exist(path):
        log(f"existe déjà : {path}")
        return
    tools.duplicate_asset(name, WEAPON_DIR, assets.load_asset(source))
    bp = assets.load_asset(path)
    setup(bp)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo(path).set_editor_property("display_name", display)
    save(path)
    log(f"créé : {path} (copie de {source.split('/')[-1]}, « {display} »)")


def create_data_asset(name, folder, cls):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    return tools.create_asset(name, folder, cls, factory)


def create_weapon_set(name, display, right, left):
    path = f"{SETS_DIR}/{name}"
    if assets.does_asset_exist(path):
        log(f"existe déjà : {path}")
        return assets.load_asset(path)
    weapon_set = create_data_asset(name, SETS_DIR, unreal.YSWeaponSet)
    # PV / pushback du set de départ du Captain
    ys.copy_editable_properties(assets.load_asset(CAPTAIN_SET), weapon_set)
    weapon_set.set_editor_property("display_name", display)
    weapon_set.set_editor_property("right_hand_weapon", assets.load_blueprint_class(right))
    weapon_set.set_editor_property("left_hand_weapon", assets.load_blueprint_class(left))
    save(path)
    log(f"créé : {path} ({display})")
    return weapon_set


def add_minion_ability(path, name, cls, source_path):
    bp = assets.load_asset(path)
    if action_of(bp, cls):
        log(f"{path.split('/')[-1]} a déjà {cls.__name__}")
        return
    source = action_of(assets.load_asset(source_path), cls)
    if source is None:
        raise RuntimeError(f"{source_path} sans {cls.__name__}")
    action = add_component(bp, cls, name)
    ys.copy_editable_properties(source, action)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(path)
    log(f"{path.split('/')[-1]} : {name} ajouté (réglages de {source_path.split('/')[-1]}.{source.get_name()})")


try:
    # 1) Cursed Captain AMMO : balle du Captain Gun, violette, minion à la mort du NME marqué
    if not assets.does_asset_exist(CURSED_AMMO):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", assets.load_blueprint_class(CAPTAIN_AMMO))
        tools.create_asset(CURSED_AMMO_NAME, AMMO_DIR, unreal.Blueprint, factory)
        ammo = cdo(CURSED_AMMO)
        ammo.set_editor_property("mark_spawns_minion", True)
        ammo.get_editor_property("mesh").set_material(0, assets.load_asset(f"{MAT}/m_PH_monochrome_Purple"))
        save(CURSED_AMMO)
        log(f"créé : {CURSED_AMMO} (enfant de CaptainAMMO, Mark Spawns Minion)")
    else:
        log(f"existe déjà : {CURSED_AMMO}")

    # 2) armes
    def captain_sword(bp):
        swing = action_of(bp, unreal.YSAutoSwingAbility)
        swing.set_editor_property("marks_target", True)
        swing.set_editor_property("mark_crits_minions", True)

    def cursed_gun(bp):
        shot = action_of(bp, unreal.YSAutoShotAbility)
        shot.set_editor_property("projectile_class", assets.load_blueprint_class(CURSED_AMMO))

    def cursed_hook(bp):
        summon = action_of(bp, unreal.YSSummonCrewAbility)
        go = add_component(bp, unreal.YSGoMinionsAbility, "GoMinions")
        ys.copy_editable_properties(summon, go)
        go.set_editor_property("display_name", "Allez-y mes minions !")
        if not delete_component(bp, "SummonCrew"):
            raise RuntimeError("composant SummonCrew introuvable dans la copie du Crochet")

    duplicate_weapon(CAPTAIN_SWORD_NAME, SWORD, "Captain Sword", captain_sword)
    duplicate_weapon(CURSED_GUN_NAME, CAPTAIN_GUN, "Cursed Captain Gun", cursed_gun)
    duplicate_weapon(CURSED_HOOK_NAME, HOOK, "Cursed Hook", cursed_hook)

    # 3) Abilities des minions (forcées seulement)
    add_minion_ability(MINION_SWORD, "DashingSlash", unreal.YSDashingSlashAbility, BATTLE_SWORD)
    add_minion_ability(MINION_GUN, "RollingShot", unreal.YSRollingShotAbility, PISTOL)

    # 4) sets
    corsaire_set = create_weapon_set("DA_WeaponSet_Corsaire", "Corsaire", CAPTAIN_SWORD, HOOK)
    cursed_set = create_weapon_set("DA_WeaponSet_CursedCaptain", "Cursed Captain", CURSED_GUN, CURSED_HOOK)

    # 5) upgrades de classe + You're all my minions, à la fin du pool du Captain
    class_upgrades = [
        ("DA_ClassUpgrade_Captain2Corsaire", "Corsaire", corsaire_set,
         "Captain Sword : ses coups marquent le NME touché,\nles minions font des critiques sur lui\n+ Crochet du capitaine"),
        ("DA_ClassUpgrade_Captain2CursedCaptain", "Cursed Captain", cursed_set,
         "Cursed Captain Gun : le NME marqué qui meurt devient un minion\nCursed Hook : Allez-y mes minions !\nforce l'Ability des minions"),
    ]
    pool_additions = []
    for name, display, target, description in class_upgrades:
        path = f"{UPGRADES_DIR}/{name}"
        if not assets.does_asset_exist(path):
            upgrade = create_data_asset(name, UPGRADES_DIR, unreal.YSClassUpgrade)
            upgrade.set_editor_property("display_name", display)
            upgrade.set_editor_property("description", description)
            upgrade.set_editor_property("source_weapon_set", assets.load_asset(CAPTAIN_SET))
            upgrade.set_editor_property("target_weapon_set", target)
            save(path)
            log(f"créé : {path} (Captain -> {display})")
        pool_additions.append(assets.load_asset(path))

    if not assets.does_asset_exist(ALL_MY_MINIONS):
        upgrade = create_data_asset(ALL_MY_MINIONS_NAME, UPGRADES_DIR, unreal.YSAllMyMinionsUpgrade)
        mine = assets.load_asset(MINE_UPGRADE)
        for prop in ("unlock_weight", "bonus_weight", "super_weight", "max_bonus_stacks"):
            upgrade.set_editor_property(prop, mine.get_editor_property(prop))
        save(ALL_MY_MINIONS)
        log(f"créé : {ALL_MY_MINIONS} (poids de DA_Upgrade_Mine)")
    pool_additions.append(assets.load_asset(ALL_MY_MINIONS))

    captain = cdo(CAPTAIN)
    pool = list(captain.get_editor_property("upgrade_pool"))
    for upgrade in pool_additions:
        if upgrade not in pool:
            pool.append(upgrade)
            log(f"{upgrade.get_name()} ajoutée au pool du Captain (index {len(pool) - 1})")
    captain.set_editor_property("upgrade_pool", pool)
    save(CAPTAIN)

    # relecture
    for path in (CAPTAIN_SWORD, CURSED_GUN, CURSED_HOOK, MINION_SWORD, MINION_GUN):
        for t in templates(assets.load_asset(path)):
            if isinstance(t, unreal.YSAbilityComponent):
                extra = ""
                if isinstance(t, unreal.YSAutoSwingAbility):
                    extra = f", dégâts {t.get_editor_property('damage')}, marque {t.get_editor_property('marks_target')} / critiques minions {t.get_editor_property('mark_crits_minions')}"
                if isinstance(t, unreal.YSCoupDPetoirAbility):
                    extra = f", dégâts {t.get_editor_property('damage')}, AMMO {t.get_editor_property('projectile_class').get_name()}"
                if isinstance(t, unreal.YSSummonCrewAbility):
                    extra = f", {t.get_editor_property('base_max_minions')} minions max, {len(t.get_editor_property('minion_weapon_sets'))} sets"
                log(f"relu {path.split('/')[-1]}.{t.get_name()} : {t.get_class().get_name()} « {t.get_editor_property('display_name')} », recharge {t.get_editor_property('cooldown')}{extra}")
    log(f"relu CursedCaptainAMMO : marque {cdo(CURSED_AMMO).get_editor_property('marks_target')}, minion à la mort {cdo(CURSED_AMMO).get_editor_property('mark_spawns_minion')}")
    log("relu pool du Captain : " + ", ".join(f"{i} {u.get_name()}" for i, u in enumerate(cdo(CAPTAIN).get_editor_property("upgrade_pool")) if u))
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_captain_classes.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
