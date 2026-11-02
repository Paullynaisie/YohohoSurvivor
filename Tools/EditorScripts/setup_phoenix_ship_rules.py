"""
Plumes de phoenix et règles des bateaux (2026-11-02), relançable :
  - /Game/Core/UI/WBP_DeathScreen (enfant de YSDeathScreenWidget, réglages Layout) -> BP_YSPlayerController.
  - Coffre de boss (BP_InteractiveObject_Chest) : 1 plume ; coffre au trésor (son enfant) : 0.
  - Vagues en mer : Jolly Ships toutes les 20 s, Galions toutes les 120 s.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_phoenix_ship_rules.py"
Rapport : Saved/Logs/setup_phoenix_ship_rules.txt
"""
import os
import unreal

UI_DIR = "/Game/Core/UI"
DEATH_WIDGET = f"{UI_DIR}/WBP_DeathScreen"
PC_BP = "/Game/Core/Player/BP_YSPlayerController"
CHEST = "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_Chest"
TREASURE_CHEST = "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_TreasureChest"
GAME_MODES = ["/Game/Core/GameModes/BP_TestGameMode", "/Game/Core/GameModes/BP_TestMapGameMode"]

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
report = []


def log(msg):
    unreal.log(f"[setup_phoenix] {msg}")
    report.append(msg)


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


try:
    if not assets.does_asset_exist(DEATH_WIDGET):
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", unreal.YSDeathScreenWidget)
        tools.create_asset("WBP_DeathScreen", UI_DIR, unreal.WidgetBlueprint, factory)
        assets.save_asset(DEATH_WIDGET, only_if_is_dirty=False)
    cdo(PC_BP).set_editor_property("death_screen_class", assets.load_blueprint_class(DEATH_WIDGET))
    assets.save_asset(PC_BP, only_if_is_dirty=False)

    for path, feathers in ((CHEST, 1), (TREASURE_CHEST, 0)):
        cdo(path).set_editor_property("phoenix_feather_value", feathers)
        assets.save_asset(path, only_if_is_dirty=False)

    for path in GAME_MODES:
        mode = cdo(path)
        mode.set_editor_property("jolly_ship_wave_interval", 20.0)
        mode.set_editor_property("galion_wave_interval", 120.0)
        assets.save_asset(path, only_if_is_dirty=False)

    log(f"relu : écran {cdo(PC_BP).get_editor_property('death_screen_class').get_name()}, plumes coffre {cdo(CHEST).get_editor_property('phoenix_feather_value')}, "
        f"trésor {cdo(TREASURE_CHEST).get_editor_property('phoenix_feather_value')}")
    for path in GAME_MODES:
        mode = cdo(path)
        log(f"relu : {path} chaloupes {mode.get_editor_property('jolly_ship_wave_interval')} s, Galions {mode.get_editor_property('galion_wave_interval')} s, plumes {mode.get_editor_property('starting_phoenix_feathers')}")
    log("OK")
except Exception as error:
    import traceback
    log(f"ERREUR : {error}\n{traceback.format_exc()}")

with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_phoenix_ship_rules.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
