"""
Barre de vie des boss (2026-11-02), relançable : plus sous l'élite (seulement le HUD, bas de l'écran au milieu), sauf le capitaine de Galion
qui n'a pas de barre de HUD (il garderait sinon ses PV invisibles).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_boss_ui_2026_11_02.py"
Rapport : Saved/Logs/setup_boss_ui_2026_11_02.txt
"""
import os
import unreal

FOE_DIR = "/Game/Placeable/Character/Foe"
BAR_UNDER = {"FoeCharacter_Skull_Elite": False, "FoeCharacter_Galion_Captain": True}

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_boss_ui] {msg}")
    report.append(msg)


try:
    for name, under in BAR_UNDER.items():
        path = f"{FOE_DIR}/{name}"
        foe = unreal.get_default_object(assets.load_blueprint_class(path))
        foe.set_editor_property("elite_health_bar_under", under)
        assets.save_asset(path, only_if_is_dirty=False)
    for name in BAR_UNDER:
        foe = unreal.get_default_object(assets.load_blueprint_class(f"{FOE_DIR}/{name}"))
        log(f"relu : {name} barre sous lui {foe.get_editor_property('elite_health_bar_under')}, rage cap {foe.get_editor_property('rage_cap_health_ratio')}, "
            f"réduction {foe.get_editor_property('dmg_reduce_factor_health_ratio')}, rage max {foe.get_editor_property('max_rage')}")
    log("OK")
except Exception as error:
    import traceback
    log(f"ERREUR : {error}\n{traceback.format_exc()}")

with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_boss_ui_2026_11_02.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
