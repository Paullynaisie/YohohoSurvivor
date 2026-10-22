"""
Arme de l'élite : /Game/Placeable/Weapons/Foe/BP_NME_GreatSword (« NME great sword »), arme À DEUX MAINS :
  - copie de BP_NME_Sword : même visuel de départ (manche allongé vers le bas pour la seconde main, hitbox recentrée sur la poignée) et même Auto swing
    (découpage, poses et timings de l'animation, HitTags Team.Player)
  - Two Handed coché ; socket OffHandSocket sur le manche, sous la main qui tient l'arme : l'autre main du NME s'y place
    toujours, même pendant le swing (qui n'anime que la main qui tient l'arme)
  - /Game/Core/WeaponSets/DA_WeaponSet_NME_GreatSword (main droite seulement) -> FoeCharacter_Skull_Elite (50 % gaucher)
Ne recrée pas ce qui existe (réglages de Paul conservés).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_foe_great_sword.py"
Rapport : Saved/Logs/setup_foe_great_sword.txt
"""
import os
import unreal

FOE_WEAPONS = "/Game/Placeable/Weapons/Foe"
NME_SWORD = f"{FOE_WEAPONS}/BP_NME_Sword"
GREAT_NAME = "BP_NME_GreatSword"
GREAT = f"{FOE_WEAPONS}/{GREAT_NAME}"
SETS_DIR = "/Game/Core/WeaponSets"
SET_NAME = "DA_WeaponSet_NME_GreatSword"
SET_PATH = f"{SETS_DIR}/{SET_NAME}"
ELITE = "/Game/Placeable/Character/FoeCharacter_Skull_Elite"
HANDLE_LOCATION = unreal.Vector(0.0, 0.0, -15.0)   # manche (Mesh, cube de 100) prolongé vers le bas : de -55 à +25
HANDLE_SCALE = unreal.Vector(0.1, 0.1, 0.8)
OFF_HAND_SOCKET = unreal.Vector(0.0, 0.0, -35.0)  # seconde main, sous la main qui tient l'arme (origine)
# hitbox centrée près de la poignée : l'élite est x3, une hitbox qui commence loin de la main passe au-delà du joueur collé à lui
HITBOX_LOCATION = unreal.Vector(25.0, 0.0, 40.0)
HITBOX_EXTENT = unreal.Vector(60.0, 60.0, 100.0)
LEFT_HANDED_CHANCE = 0.5

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_foe_great_sword] {msg}")
    report.append(msg)


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


try:
    if not assets.does_asset_exist(GREAT):
        tools.duplicate_asset(GREAT_NAME, FOE_WEAPONS, assets.load_asset(NME_SWORD))
        unreal.BlueprintEditorLibrary.compile_blueprint(assets.load_asset(GREAT))
        weapon = cdo(GREAT)
        weapon.set_editor_property("display_name", "NME great sword")
        weapon.set_editor_property("two_handed", True)
        weapon.set_editor_property("replicates", False)
        mesh = weapon.get_editor_property("mesh")
        mesh.set_editor_property("relative_location", HANDLE_LOCATION)
        mesh.set_editor_property("relative_scale3d", HANDLE_SCALE)
        weapon.get_editor_property("off_hand_socket").set_editor_property("relative_location", OFF_HAND_SOCKET)
        for handle in subsystem.k2_gather_subobject_data_for_blueprint(assets.load_asset(GREAT)):
            box = lib.get_object(lib.get_data(handle))
            if isinstance(box, unreal.BoxComponent):
                box.set_editor_property("relative_location", HITBOX_LOCATION)
                box.set_editor_property("box_extent", HITBOX_EXTENT)
        unreal.BlueprintEditorLibrary.compile_blueprint(assets.load_asset(GREAT))
        assets.save_asset(GREAT, only_if_is_dirty=False)
        log(f"créé : {GREAT} (copie de BP_NME_Sword, deux mains)")
    else:
        log(f"existe déjà : {GREAT}")

    if not assets.does_asset_exist(SET_PATH):
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.YSWeaponSet)
        weapon_set = tools.create_asset(SET_NAME, SETS_DIR, unreal.YSWeaponSet, factory)
        weapon_set.set_editor_property("display_name", "NME great sword")
        weapon_set.set_editor_property("right_hand_weapon", assets.load_blueprint_class(GREAT))
        assets.save_asset(SET_PATH, only_if_is_dirty=False)
        log(f"créé : {SET_PATH}")

    elite = cdo(ELITE)
    if elite.get_editor_property("weapon_set") is None:
        elite.set_editor_property("weapon_set", assets.load_asset(SET_PATH))
        elite.set_editor_property("left_handed_chance", LEFT_HANDED_CHANCE)
        assets.save_asset(ELITE, only_if_is_dirty=False)
        log("FoeCharacter_Skull_Elite : set NME great sword, 50 % gaucher")

    # relecture
    weapon = cdo(GREAT)
    swing = next(lib.get_object(lib.get_data(h)) for h in subsystem.k2_gather_subobject_data_for_blueprint(assets.load_asset(GREAT))
                 if isinstance(lib.get_object(lib.get_data(h)), unreal.YSAutoSwingAbility))
    log(f"relu : {weapon.get_editor_property('display_name')} deux mains {weapon.get_editor_property('two_handed')} socket "
        f"{weapon.get_editor_property('off_hand_socket').get_editor_property('relative_location')} replicates {weapon.get_editor_property('replicates')} ; "
        f"swing {swing.get_editor_property('windup_duration')} / {swing.get_editor_property('swing_duration')} / {swing.get_editor_property('recover_duration')} "
        f"CD {swing.get_editor_property('auto_atk_cooldown')} tags {swing.get_editor_property('hit_tags')}")
    elite = cdo(ELITE)
    log(f"relu : élite set {elite.get_editor_property('weapon_set').get_name()} gaucher {elite.get_editor_property('left_handed_chance')}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_foe_great_sword.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
