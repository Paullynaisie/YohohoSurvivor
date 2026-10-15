"""
Sélection du personnage :
  - PlayerCharacter_mouss2sabre / PlayerCharacter_mouss2pistol : enfants de PlayerCharacter_mouss
    (même dossier), avec deux épées / deux pistolets
  - noms affichés : « Mouss », « Mouss 2sabre », « Mouss 2pistol »
  - BP_TestGameMode reparenté sur AYSGameMode, liste des personnages proposés dans le menu

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_character_select.py"
Rapport : Saved/Logs/setup_character_select.txt
"""
import os
import unreal

CHAR_DIR = "/Game/Placeable/Character"
BASE = f"{CHAR_DIR}/PlayerCharacter_mouss"
SWORD = "/Game/Placeable/Weapons/BP_Epee"
PISTOL = "/Game/Placeable/Weapons/BP_Pistolet"
GAME_MODE_BP = "/Game/Core/GameModes/BP_TestGameMode"

# nom de l'asset, nom affiché, arme gauche, arme droite (None = inchangé, hérité)
VARIANTS = [
    ("PlayerCharacter_mouss2sabre", "Mouss 2sabre", SWORD, SWORD),
    ("PlayerCharacter_mouss2pistol", "Mouss 2pistol", PISTOL, PISTOL),
]

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_character_select] {msg}")
    report.append(msg)


def cdo_of(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


try:
    base_bp = assets.load_asset(BASE)
    unreal.BlueprintEditorLibrary.compile_blueprint(base_bp)
    cdo_of(BASE).set_editor_property("character_name", unreal.Text("Mouss"))
    assets.save_asset(BASE, only_if_is_dirty=False)
    base_class = assets.load_blueprint_class(BASE)

    selectable = [base_class]
    for name, display, left, right in VARIANTS:
        path = f"{CHAR_DIR}/{name}"
        if not assets.does_asset_exist(path):
            factory = unreal.BlueprintFactory()
            factory.set_editor_property("parent_class", base_class)
            unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, CHAR_DIR, unreal.Blueprint, factory)
            log(f"créé : {path}")
        bp = assets.load_asset(path)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        cdo = cdo_of(path)
        cdo.set_editor_property("character_name", unreal.Text(display))
        cdo.set_editor_property("left_hand_weapon_class", assets.load_blueprint_class(left))
        cdo.set_editor_property("right_hand_weapon_class", assets.load_blueprint_class(right))
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"{name} : « {display} », gauche {left.rsplit('/', 1)[1]}, droite {right.rsplit('/', 1)[1]}")
        selectable.append(assets.load_blueprint_class(path))

    gm_bp = assets.load_asset(GAME_MODE_BP)
    unreal.BlueprintEditorLibrary.reparent_blueprint(gm_bp, unreal.YSGameMode.static_class())
    unreal.BlueprintEditorLibrary.compile_blueprint(gm_bp)
    gm = cdo_of(GAME_MODE_BP)
    gm.set_editor_property("selectable_characters", selectable)
    gm.set_editor_property("game_state_class", unreal.YSGameState.static_class())
    gm.set_editor_property("player_state_class", unreal.YSPlayerState.static_class())
    gm.set_editor_property("player_controller_class", unreal.YSPlayerController.static_class())
    assets.save_asset(GAME_MODE_BP, only_if_is_dirty=False)
    log(f"BP_TestGameMode : parent YSGameMode, {len(selectable)} personnages proposés")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_character_select.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
