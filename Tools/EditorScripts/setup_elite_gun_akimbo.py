"""
Elite Gun / Boss Gun en akimbo (2026-11-03) :
  - /Game/Placeable/Weapons/Foe/BP_NME_ElitePistol : enfant de BP_NME_Pistol, NME shot à 2 balles par pistolet, 0.1 s entre
    toutes les balles (Shots Per Gun / Burst Shot Interval).
  - DA_WeaponSet_NME_ElitePistol (copie de DA_WeaponSet_NME_Pistol) : ElitePistol dans les deux mains -> FoeCharacter_Skull_Gun_Elite.
  - DA_WeaponSet_NME_BossPistol : main gauche = ElitePistol ; BP_NME_BossPistol : NME shot à 2 balles par pistolet, 0.1 s ;
    Roll360shot à 0.05 s entre les balles.
Relançable (remet ces valeurs).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_elite_gun_akimbo.py"
Rapport : Saved/Logs/setup_elite_gun_akimbo.txt
"""
import os
import unreal

FOE_DIR = "/Game/Placeable/Character/Foe"
WEAPON_DIR = "/Game/Placeable/Weapons/Foe"
PISTOL_BP = f"{WEAPON_DIR}/BP_NME_Pistol"
ELITE_PISTOL_NAME = "BP_NME_ElitePistol"
ELITE_PISTOL_BP = f"{WEAPON_DIR}/{ELITE_PISTOL_NAME}"
BOSS_PISTOL_BP = f"{WEAPON_DIR}/BP_NME_BossPistol"
SET_DIR = "/Game/Core/WeaponSets"
PISTOL_SET = f"{SET_DIR}/DA_WeaponSet_NME_Pistol"
ELITE_SET = f"{SET_DIR}/DA_WeaponSet_NME_ElitePistol"
BOSS_SET = f"{SET_DIR}/DA_WeaponSet_NME_BossPistol"
GUN_ELITE = f"{FOE_DIR}/FoeCharacter_Skull_Gun_Elite"
GUN_BOSS = f"{FOE_DIR}/FoeCharacter_Skull_Gun_Boss"

SHOTS_PER_GUN = 2
BURST_INTERVAL = 0.1
ROLL360_INTERVAL = 0.05

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_elite_gun_akimbo] {msg}")
    report.append(msg)


def cls(path):
    return assets.load_blueprint_class(path)


def cdo(path):
    return unreal.get_default_object(cls(path))


def save(path):
    assets.save_asset(path, only_if_is_dirty=False)


def own_component(bp, component_class):
    """Composant ajouté dans CE Blueprint (pas hérité)."""
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(handle)
        obj = lib.get_object(data)
        if obj and isinstance(obj, component_class) and not lib.is_inherited_component(data):
            return obj
    return None


def shot_template(bp):
    """NME shot du Blueprint : pour un composant hérité, le gabarit propre à ce Blueprint (override), pas celui du parent."""
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(handle)
        obj = lib.get_object_for_blueprint(data, bp)
        if obj and isinstance(obj, unreal.YSFoeShotAction):
            return obj
    return None


def set_shot(path):
    bp = assets.load_asset(path)
    shot = shot_template(bp)
    if shot is None:
        raise RuntimeError(f"{path} : composant Shot introuvable")
    shot.set_editor_property("shots_per_gun", SHOTS_PER_GUN)
    shot.set_editor_property("burst_shot_interval", BURST_INTERVAL)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    save(path)
    log(f"{path.split('/')[-1]} : NME shot {SHOTS_PER_GUN} balles par pistolet, {BURST_INTERVAL} s")


try:
    # pistolet de l'élite
    if assets.does_asset_exist(ELITE_PISTOL_BP):
        log(f"existe déjà : {ELITE_PISTOL_BP}")
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", cls(PISTOL_BP))
        bp = tools.create_asset(ELITE_PISTOL_NAME, WEAPON_DIR, unreal.Blueprint, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        log(f"créé : {ELITE_PISTOL_BP} (enfant de BP_NME_Pistol)")
    cdo(ELITE_PISTOL_BP).set_editor_property("display_name", "NME elite pistol")
    set_shot(ELITE_PISTOL_BP)
    set_shot(BOSS_PISTOL_BP)

    # Roll360shot du Boss Gun : 0.05 s entre les balles
    boss_pistol = assets.load_asset(BOSS_PISTOL_BP)
    roll = own_component(boss_pistol, unreal.YSFoeRoll360ShotAction)
    if roll is None:
        raise RuntimeError("Roll360shot introuvable dans BP_NME_BossPistol")
    roll.set_editor_property("shot_interval", ROLL360_INTERVAL)
    unreal.BlueprintEditorLibrary.compile_blueprint(boss_pistol)
    save(BOSS_PISTOL_BP)
    log(f"Roll360shot : {ROLL360_INTERVAL} s entre les balles")

    # sets
    if not assets.does_asset_exist(ELITE_SET):
        assets.duplicate_asset(PISTOL_SET, ELITE_SET)
        log(f"créé : {ELITE_SET} (copie de {PISTOL_SET})")
    elite_set = assets.load_asset(ELITE_SET)
    elite_set.set_editor_property("right_hand_weapon", cls(ELITE_PISTOL_BP))
    elite_set.set_editor_property("left_hand_weapon", cls(ELITE_PISTOL_BP))
    elite_set.set_editor_property("display_name", "NME akimbo elite pistol")
    save(ELITE_SET)

    boss_set = assets.load_asset(BOSS_SET)
    boss_set.set_editor_property("left_hand_weapon", cls(ELITE_PISTOL_BP))
    boss_set.set_editor_property("display_name", "NME akimbo boss pistol")
    save(BOSS_SET)

    cdo(GUN_ELITE).set_editor_property("weapon_set", elite_set)
    save(GUN_ELITE)

    # relecture
    for path in [ELITE_PISTOL_BP, BOSS_PISTOL_BP, PISTOL_BP]:
        shot = shot_template(assets.load_asset(path))
        log(f"relu : {path.split('/')[-1]} Shot {shot.get_name()} ({shot.get_outer().get_name()}) "
            f"{shot.get_editor_property('shots_per_gun')} balles / {shot.get_editor_property('burst_shot_interval')} s")
    for path in [GUN_ELITE, GUN_BOSS]:
        s = cdo(path).get_editor_property("weapon_set")
        log(f"relu : {path.split('/')[-1]} set {s.get_name()} : droite {s.get_editor_property('right_hand_weapon').get_name()}, "
            f"gauche {s.get_editor_property('left_hand_weapon').get_name()}")
    log(f"relu : Roll360shot {own_component(assets.load_asset(BOSS_PISTOL_BP), unreal.YSFoeRoll360ShotAction).get_editor_property('shot_interval')} s")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_elite_gun_akimbo.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
