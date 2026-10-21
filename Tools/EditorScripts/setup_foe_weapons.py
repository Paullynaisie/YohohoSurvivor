"""
Armes des NME :
  - /Game/Placeable/Weapons/Foe/BP_NME_Sword : copie VISUELLE de BP_Epee (« NME sword ») : actions (Auto swing...) retirées,
    Replicates décoché (arme de NME : créée localement sur chaque machine, aucun coût réseau) ; comportement à venir
  - /Game/Core/WeaponSets/DA_WeaponSet_NME_Sword : set « NME sword » (main droite seulement)
  - FoeCharacter_Skull : ce set, 50 % de chance d'être gaucher ; Skull Bomb / Skull Elite (enfants de Skull) : pas de set
Ne recrée pas ce qui existe déjà et ne réécrase pas les réglages des NME déjà faits.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_foe_weapons.py"
Rapport : Saved/Logs/setup_foe_weapons.txt
"""
import os
import unreal

FOE_WEAPONS_DIR = "/Game/Placeable/Weapons/Foe"
SWORD_BP = "/Game/Placeable/Weapons/BP_Epee"
NME_SWORD = "BP_NME_Sword"
SETS_DIR = "/Game/Core/WeaponSets"
NME_SET = "DA_WeaponSet_NME_Sword"
CHAR_DIR = "/Game/Placeable/Character"
SKULL = f"{CHAR_DIR}/FoeCharacter_Skull"
NO_SET_FOES = [f"{CHAR_DIR}/FoeCharacter_Skull_Bomb", f"{CHAR_DIR}/FoeCharacter_Skull_Elite"]
SKULL_LEFT_HANDED_CHANCE = 0.5

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_foe_weapons] {msg}")
    report.append(msg)


try:
    # arme : copie visuelle de l'épée, sans action, non répliquée
    sword_path = f"{FOE_WEAPONS_DIR}/{NME_SWORD}"
    if not assets.does_asset_exist(sword_path):
        tools.duplicate_asset(NME_SWORD, FOE_WEAPONS_DIR, assets.load_asset(SWORD_BP))
        sword = assets.load_asset(sword_path)
        handles = subsystem.k2_gather_subobject_data_for_blueprint(sword)
        root = handles[0]
        removed = []
        for handle in handles:
            obj = lib.get_object(lib.get_data(handle))
            if isinstance(obj, unreal.YSAbilityComponent):
                removed.append(obj.get_name())
                subsystem.delete_subobject(root, handle, sword)
        unreal.BlueprintEditorLibrary.compile_blueprint(sword)
        sword_cdo = unreal.get_default_object(assets.load_blueprint_class(sword_path))
        sword_cdo.set_editor_property("replicates", False)
        sword_cdo.set_editor_property("display_name", "NME sword")
        assets.save_asset(sword_path, only_if_is_dirty=False)
        log(f"créé : {sword_path} (copie de BP_Epee, actions retirées : {removed or 'aucune'})")
    else:
        log(f"existe déjà : {sword_path}")
    sword_class = assets.load_blueprint_class(sword_path)

    # set d'arme : NME sword en main droite
    set_path = f"{SETS_DIR}/{NME_SET}"
    if not assets.does_asset_exist(set_path):
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.YSWeaponSet)
        weapon_set = tools.create_asset(NME_SET, SETS_DIR, unreal.YSWeaponSet, factory)
        weapon_set.set_editor_property("display_name", "NME sword")
        weapon_set.set_editor_property("right_hand_weapon", sword_class)
        assets.save_asset(set_path, only_if_is_dirty=False)
        log(f"créé : {set_path}")
    weapon_set = assets.load_asset(set_path)

    # Skull : le set, 50 % gaucher ; ses enfants (Bomb, Elite) : pas encore de set
    skull_cdo = unreal.get_default_object(assets.load_blueprint_class(SKULL))
    if skull_cdo.get_editor_property("weapon_set") is None:
        skull_cdo.set_editor_property("weapon_set", weapon_set)
        skull_cdo.set_editor_property("left_handed_chance", SKULL_LEFT_HANDED_CHANCE)
        assets.save_asset(SKULL, only_if_is_dirty=False)
        log("FoeCharacter_Skull : set NME sword, 50 % gaucher")
    for path in NO_SET_FOES:
        cdo = unreal.get_default_object(assets.load_blueprint_class(path))
        if cdo.get_editor_property("weapon_set") == weapon_set:
            cdo.set_editor_property("weapon_set", None)
            cdo.set_editor_property("left_handed_chance", 0.0)
            assets.save_asset(path, only_if_is_dirty=False)
            log(f"{path.split('/')[-1]} : pas de set (hérité de Skull, retiré)")

    # relecture
    check_sword = unreal.get_default_object(assets.load_blueprint_class(sword_path))
    check = {p.split("/")[-1]: unreal.get_default_object(assets.load_blueprint_class(p)) for p in [SKULL] + NO_SET_FOES}
    log(f"relu : NME sword replicates {check_sword.get_editor_property('replicates')}, nom {check_sword.get_editor_property('display_name')} ; "
        + " ; ".join(f"{n} set {c.get_editor_property('weapon_set').get_name() if c.get_editor_property('weapon_set') else None} gaucher {c.get_editor_property('left_handed_chance')}" for n, c in check.items()))
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_foe_weapons.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
