"""
Met en place la visée à la souris et le strafe du joueur :
  - /Game/Placeable/Character/Anims/BS_Player_Strafe : BlendSpace 2D (X = direction, Y = vitesse)
    avec les animations Unarmed du mannequin (idle, marche et course dans 8 directions)
  - PlayerCharacter_mouss.StrafeBlendSpace = BS_Player_Strafe
  - BP_TestGameMode.PlayerControllerClass = AYSPlayerController (curseur + visée)

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_strafe.py"
Rapport : Saved/Logs/setup_strafe.txt
"""
import os
import unreal

BS_DIR, BS_NAME = "/Game/Placeable/Character/Anims", "BS_Player_Strafe"
SKELETON = "/Game/PresetProject/Characters/Mannequins/Meshes/SK_Mannequin"
ANIMS = "/Game/PresetProject/Characters/Mannequins/Anims/Unarmed"
PLAYER_BP = "/Game/Placeable/Character/PlayerCharacter_mouss"
GAME_MODE_BP = "/Game/Core/GameModes/BP_TestGameMode"

WALK_SPEED, JOG_SPEED, MAX_SPEED = 200.0, 600.0, 600.0

# direction (degrés, 0 = avant, 90 = droite) -> suffixe des animations
DIRECTIONS = {
    -180: "Bwd", -135: "Bwd_Left", -90: "Left", -45: "Fwd_Left", 0: "Fwd",
    45: "Fwd_Right", 90: "Right", 135: "Bwd_Right", 180: "Bwd",
}

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_strafe] {msg}")
    report.append(msg)


def load(path):
    asset = assets.load_asset(path)
    if asset is None:
        raise RuntimeError(f"asset introuvable : {path}")
    return asset


def build_blend_space():
    path = f"{BS_DIR}/{BS_NAME}"
    if assets.does_asset_exist(path):
        bs = load(path)
        log(f"existe déjà, mis à jour : {path}")
    else:
        factory = unreal.BlendSpaceFactoryNew()
        factory.set_editor_property("target_skeleton", load(SKELETON))
        bs = unreal.AssetToolsHelpers.get_asset_tools().create_asset(BS_NAME, BS_DIR, unreal.BlendSpace, factory)
        log(f"créé : {path}")

    lib = unreal.YSEditorScriptLibrary
    lib.set_blend_space_axis(bs, 0, "Direction", -180.0, 180.0, 8, True)
    lib.set_blend_space_axis(bs, 1, "Speed", 0.0, MAX_SPEED, 6, False)

    anims, positions = [], []
    idle = load(f"{ANIMS}/MM_Idle")
    for direction, suffix in DIRECTIONS.items():
        anims.append(idle)
        positions.append(unreal.Vector(direction, 0, 0))
        anims.append(load(f"{ANIMS}/Walk/MF_Unarmed_Walk_{suffix}"))
        positions.append(unreal.Vector(direction, WALK_SPEED, 0))
        anims.append(load(f"{ANIMS}/Jog/MF_Unarmed_Jog_{suffix}"))
        positions.append(unreal.Vector(direction, JOG_SPEED, 0))
    added = lib.set_blend_space_samples(bs, anims, positions)
    log(f"samples ajoutés : {added}/{len(anims)}")

    assets.save_asset(path, only_if_is_dirty=False)
    return bs


def set_blueprint_default(bp_path, prop, value):
    bp = load(bp_path)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(assets.load_blueprint_class(bp_path))
    cdo.set_editor_property(prop, value)
    assets.save_asset(bp_path, only_if_is_dirty=False)
    log(f"{bp_path}.{prop} = {value.get_name()}")


try:
    blend_space = build_blend_space()
    set_blueprint_default(PLAYER_BP, "strafe_blend_space", blend_space)
    set_blueprint_default(GAME_MODE_BP, "player_controller_class", unreal.YSPlayerController.static_class())
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_strafe.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
