"""
Retours de Paul du 2026-11-13 (Kraken, Davy Jones, MoveActions) : valeurs posées dans les Blueprints. Relançable (valeurs absolues).
  - MoveActions (composants C++ de chaque personnage joueur) : Run x1.5 (au lieu de 1.25), Booster de moral 5 s (au lieu de 3) ;
    DA_Up_MoveAction_Run : Run ++ = +0.1 (au lieu de 0.05) ; textes des cartes mis à jour (+50 %, +10 %, 5 s).
  - FoeCharacter_KrakenTentacle_Melee : dégâts subis x0.25 depuis les joueurs (x1 depuis le bateau), taille x1.5 (Tentacle Scale ; x3 jugé trop grand par Paul),
    sortie à 8 m du bord de la coque (sa base fait maintenant ~7 m de rayon).
  - FoeCharacter_KrakenTentacle_Range : taille x1.5.
  - FoeCharacter_KrakenTentacle_DavyJones : attaque 2x moins souvent (Recover Duration 2 -> 4, Attack Delay Range 1..3 -> 2..6).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="<chemin absolu>/Tools/EditorScripts/setup_retours_2026_11_13.py"
Rapport : Saved/Logs/setup_retours_2026_11_13.txt
"""
import os
import unreal

CHARACTERS_DIR = "/Game/Placeable/Character/Player"
CHARACTERS = ["PlayerCharacter_mouss", "PlayerCharacter_Cannoneer", "PlayerCharacter_Harquebusier", "PlayerCharacter_Quartermaster",
              "PlayerCharacter_Captain", "PlayerCharacter_JohnnyPlaytest", "PlayerCharacter_HiredMouss"]
MOVE_DIR = "/Game/Core/Upgrades/Up_Character/Up_MoveActions"
KRAKEN_DIR = "/Game/Placeable/Character/Foe/Kraken"

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_retours_2026_11_13] {msg}")
    report.append(msg)


def replace_text(asset, prop, replacements):
    text = str(asset.get_editor_property(prop))
    new = text
    for old, value in replacements:
        new = new.replace(old, value)
    if new != text:
        asset.set_editor_property(prop, unreal.Text(new))
        log(f"  {asset.get_name()}.{prop} : {text!r} -> {new!r}")


def set_foe(name, values):
    path = f"{KRAKEN_DIR}/{name}"
    bp = assets.load_asset(path)
    cdo = unreal.get_default_object(bp.generated_class())
    for prop, value in values.items():
        cdo.set_editor_property(prop, value)
    assets.save_asset(path, only_if_is_dirty=False)
    log(f"{name} : " + ", ".join(f"{p} = {cdo.get_editor_property(p)}" for p in values))


try:
    for name in CHARACTERS:
        path = f"{CHARACTERS_DIR}/{name}"
        bp = assets.load_asset(path)
        cdo = unreal.get_default_object(bp.generated_class())
        run = cdo.get_editor_property("RunMoveAction")
        boost = cdo.get_editor_property("MoraleBoostMoveAction")
        run.set_editor_property("run_speed_multiplier", 1.5)
        boost.set_editor_property("duration", 5.0)
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"{name} : Run x{run.get_editor_property('run_speed_multiplier')}, Booster {boost.get_editor_property('duration')} s")

    run_da = assets.load_asset(f"{MOVE_DIR}/DA_Up_MoveAction_Run")
    run_da.set_editor_property("bonus_silver_value", 0.1)
    run_da.set_editor_property("bonus_gold_value", 0.1)
    replace_text(run_da, "unlock_description", [("+25 %", "+50 %")])
    replace_text(run_da, "bonus_description", [("+5 %", "+10 %")])
    assets.save_asset(run_da.get_path_name().split(".")[0], only_if_is_dirty=False)
    log(f"DA_Up_MoveAction_Run : Run ++ {run_da.get_editor_property('bonus_gold_value')}")

    boost_da = assets.load_asset(f"{MOVE_DIR}/DA_Up_MoveAction_MoraleBoost")
    replace_text(boost_da, "unlock_description", [("pendant 3 s", "pendant 5 s")])
    assets.save_asset(boost_da.get_path_name().split(".")[0], only_if_is_dirty=False)

    set_foe("FoeCharacter_KrakenTentacle_Melee", {"player_dmg_multiplier": 0.25, "boat_dmg_multiplier": 1.0, "tentacle_scale": 1.5, "emerge_distance": 800.0})
    set_foe("FoeCharacter_KrakenTentacle_Range", {"tentacle_scale": 1.5})
    set_foe("FoeCharacter_KrakenTentacle_DavyJones", {"recover_duration": 4.0, "attack_delay_range": unreal.Vector2D(2.0, 6.0), "tentacle_scale": 1.0})
    log("OK")
except Exception as error:
    import traceback
    log(f"ERREUR : {error}\n{traceback.format_exc()}")

with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_retours_2026_11_13.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
