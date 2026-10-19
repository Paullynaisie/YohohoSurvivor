"""
Met en place le Auto swing :
  - BP_Epee : composant « AutoSwing » (attaque auto) + « Hitbox » (UBoxComponent couvrant la lame,
    sans collision propre : la capacité l'interroge pendant la coupe)
  - PlayerCharacter_mouss : main droite = BP_Epee (main gauche inchangée)

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_sabre.py"
Rapport : Saved/Logs/setup_sabre.txt
"""
import os
import unreal

SWORD_BP = "/Game/Placeable/Weapons/BP_Epee"
PLAYER_BP = "/Game/Placeable/Character/PlayerCharacter_mouss"
# la lame de BP_Epee monte le long de l'axe Z de l'arme (≈ -15 à 165 cm), tranchant côté X
HITBOX_LOCATION = unreal.Vector(25.0, 0.0, 75.0)
HITBOX_EXTENT = unreal.Vector(25.0, 8.0, 90.0)

assets = unreal.EditorAssetLibrary
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_sabre] {msg}")
    report.append(msg)


def get_or_add_component(bp, component_class, name, parent_name=None):
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = sds.k2_gather_subobject_data_for_blueprint(bp)
    parent = handles[0]
    for handle in handles:
        variable = str(lib.get_variable_name(lib.get_data(handle)))
        if variable == name:
            log(f"{name} existe déjà")
            return lib.get_object(lib.get_data(handle))
        if parent_name and variable == parent_name:
            parent = handle
    params = unreal.AddNewSubobjectParams(parent_handle=parent, new_class=component_class, blueprint_context=bp)
    handle, fail_reason = sds.add_new_subobject(params)
    if not lib.is_handle_valid(handle):
        raise RuntimeError(f"ajout de {name} impossible : {fail_reason}")
    sds.rename_subobject(handle, unreal.Text(name))
    log(f"{name} ajouté")
    return lib.get_object(lib.get_data(handle))


try:
    sword = assets.load_asset(SWORD_BP)
    get_or_add_component(sword, unreal.YSAutoSwingAbility, "AutoSwing")
    hitbox = get_or_add_component(sword, unreal.BoxComponent, "Hitbox", parent_name="Root")
    hitbox.set_editor_property("relative_location", HITBOX_LOCATION)
    hitbox.set_box_extent(HITBOX_EXTENT, False)
    hitbox.set_collision_enabled(unreal.CollisionEnabled.NO_COLLISION)
    hitbox.set_editor_property("hidden_in_game", True)
    unreal.BlueprintEditorLibrary.compile_blueprint(sword)
    assets.save_asset(SWORD_BP, only_if_is_dirty=False)

    player = assets.load_asset(PLAYER_BP)
    unreal.BlueprintEditorLibrary.compile_blueprint(player)
    cdo = unreal.get_default_object(assets.load_blueprint_class(PLAYER_BP))
    cdo.set_editor_property("right_hand_weapon_class", assets.load_blueprint_class(SWORD_BP))
    assets.save_asset(PLAYER_BP, only_if_is_dirty=False)
    log(f"PlayerCharacter_mouss : main droite = BP_Epee, main gauche = {cdo.get_editor_property('left_hand_weapon_class').get_name()}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_sabre.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
