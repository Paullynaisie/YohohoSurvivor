"""
Crée les armes et équipe le joueur :
  - /Game/Placeable/Weapons/BP_Arme      : arme de base (parent AYSWeapon)
  - /Game/Placeable/Weapons/BP_Pistolet  : enfant de BP_Arme, cube + action « CoupDPetoir »
                                            (réglages repris de l'ancien Coup d'Petoir du personnage)
  - /Game/Placeable/Weapons/BP_Epee      : enfant de BP_Arme, lame en cube, pas encore d'action
  - PlayerCharacter_mouss.LeftHandWeaponClass = BP_Pistolet

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_weapons.py"
Rapport : Saved/Logs/setup_weapons.txt
"""
import os
import unreal

WEAPONS_DIR = "/Game/Placeable/Weapons"
PLAYER_BP = "/Game/Placeable/Character/PlayerCharacter_mouss"
CUBE = "/Engine/BasicShapes/Cube"
GRID_MATERIAL = "/Engine/EngineMaterials/WorldGridMaterial"
PETOIR_AMMO = "/Game/Placeable/AMMO/PetoirAMMO"

assets = unreal.EditorAssetLibrary
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_weapons] {msg}")
    report.append(msg)


def get_or_create_blueprint(name, parent_class):
    path = f"{WEAPONS_DIR}/{name}"
    if assets.does_asset_exist(path):
        log(f"existe déjà : {path}")
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", parent_class)
        unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, WEAPONS_DIR, unreal.Blueprint, factory)
        log(f"créé : {path}")
    bp = assets.load_asset(path)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    return bp, path


def setup_mesh(path, location, scale):
    cdo = unreal.get_default_object(assets.load_blueprint_class(path))
    mesh = cdo.get_editor_property("Mesh")
    mesh.set_static_mesh(assets.load_asset(CUBE))
    mesh.set_material(0, assets.load_asset(GRID_MATERIAL))
    mesh.set_editor_property("relative_location", location)
    mesh.set_editor_property("relative_scale3d", scale)
    return cdo


def add_action(bp, component_class, name):
    """Ajoute un composant d'action au Blueprint (s'il n'existe pas) et le retourne."""
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = sds.k2_gather_subobject_data_for_blueprint(bp)
    for handle in handles:
        if str(lib.get_variable_name(lib.get_data(handle))) == name:
            return lib.get_object(lib.get_data(handle))
    params = unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=component_class, blueprint_context=bp)
    handle, fail_reason = sds.add_new_subobject(params)
    if not lib.is_handle_valid(handle):
        raise RuntimeError(f"ajout de {name} impossible : {fail_reason}")
    sds.rename_subobject(handle, unreal.Text(name))
    return lib.get_object(lib.get_data(handle))


try:
    base_bp, base_path = get_or_create_blueprint("BP_Arme", unreal.YSWeapon)
    assets.save_asset(base_path, only_if_is_dirty=False)
    base_class = assets.load_blueprint_class(base_path)

    # pistolet : même visuel que l'ancien pistolet du personnage, bouche au bout du canon
    pistol_bp, pistol_path = get_or_create_blueprint("BP_Pistolet", base_class)
    pistol_cdo = setup_mesh(pistol_path, unreal.Vector(15, 0, 0), unreal.Vector(0.25, 0.06, 0.1))
    pistol_cdo.get_editor_property("Muzzle").set_editor_property("relative_location", unreal.Vector(30, 0, 0))
    action = add_action(pistol_bp, unreal.YSCoupDPetoirAbility, "CoupDPetoir")
    action.set_editor_property("projectile_class", assets.load_blueprint_class(PETOIR_AMMO))
    action.set_editor_property("damage", 5.0)
    action.set_editor_property("cooldown", 1.0)
    unreal.BlueprintEditorLibrary.compile_blueprint(pistol_bp)
    assets.save_asset(pistol_path, only_if_is_dirty=False)
    log("BP_Pistolet : action CoupDPetoir (PetoirAMMO, 5 dégâts, recharge 1 s)")

    # épée : longue lame pointée vers l'avant
    sword_bp, sword_path = get_or_create_blueprint("BP_Epee", base_class)
    sword_cdo = setup_mesh(sword_path, unreal.Vector(40, 0, 0), unreal.Vector(0.7, 0.03, 0.08))
    sword_cdo.get_editor_property("Muzzle").set_editor_property("relative_location", unreal.Vector(75, 0, 0))
    assets.save_asset(sword_path, only_if_is_dirty=False)

    player_bp = assets.load_asset(PLAYER_BP)
    unreal.BlueprintEditorLibrary.compile_blueprint(player_bp)
    player_cdo = unreal.get_default_object(assets.load_blueprint_class(PLAYER_BP))
    player_cdo.set_editor_property("left_hand_weapon_class", assets.load_blueprint_class(pistol_path))
    assets.save_asset(PLAYER_BP, only_if_is_dirty=False)
    log("PlayerCharacter_mouss : main gauche = BP_Pistolet")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_weapons.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
