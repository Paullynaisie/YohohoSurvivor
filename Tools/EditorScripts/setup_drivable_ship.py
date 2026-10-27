"""
Bateau pilotable (barre, touche E) :
  - /Game/Placeable/Ship/BP_PirateShip : composant HelmStation (barre AYSShipHelm) placé au centre de la roue « HelmWheel »
  - /Game/Maps/TestMap : le bateau amarré au ponton devient pilotable (bDrivable) et n'est plus le bateau objectif (bObjectiveShip)
  - /Game/Maps/GameMap : le bateau objectif reste objectif, non pilotable (rien n'est changé)
Relançable.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_drivable_ship.py"
Rapport : Saved/Logs/setup_drivable_ship.txt
"""
import os
import unreal

SHIP_BP = "/Game/Placeable/Ship/BP_PirateShip"
TEST_MAP = "/Game/Maps/TestMap"
WHEEL = "HelmWheel"

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
report = []


def log(msg):
    unreal.log(f"[setup_drivable_ship] {msg}")
    report.append(msg)


try:
    bp = assets.load_asset(SHIP_BP)
    wheel = None
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if obj and obj.get_name().startswith(WHEEL):
            wheel = obj
    cdo = unreal.get_default_object(assets.load_blueprint_class(SHIP_BP))
    helm = cdo.get_editor_property("HelmStation")
    if wheel:
        location = wheel.get_editor_property("relative_location")
        helm.set_editor_property("relative_location", location)
        log(f"HelmStation au centre de la roue : {location}")
    else:
        log(f"pas de composant {WHEEL} : HelmStation garde {helm.get_editor_property('relative_location')}")
    assets.save_asset(SHIP_BP, only_if_is_dirty=False)

    levels.load_level(TEST_MAP)
    ships = [a for a in actors_sub.get_all_level_actors() if isinstance(a, unreal.YSPirateShip)]
    for ship in ships:
        ship.set_editor_property("bObjectiveShip", False)
        ship.set_editor_property("bDrivable", True)
        log(f"TestMap : {ship.get_actor_label()} pilotable, pas objectif ({ship.get_actor_location()}, yaw {ship.get_actor_rotation().yaw:.0f})")
    if not ships:
        log("TestMap : aucun bateau")
    levels.save_current_level()
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_drivable_ship.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
