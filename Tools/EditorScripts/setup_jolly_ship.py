"""
Bateau, côté joueurs (2026-10-30) :
  - /Game/Placeable/Character/Foe/FoeCharacter_JollyHull : coque du NME Jolly Ship (PV du bateau, invisible, barre de vie de Skull)
  - /Game/Placeable/Ship/BP_NME_JollyShip : copie de BP_Longboat reparentée sur AYSFoeJollyShip ; équipage : pilote Skull, places Skull Gun
  - BP_TestGameMode : Jolly Ship Class ; BP_TestMapGameMode : pas de vagues de Jolly Ships (pavé numérique 7 seulement)
  - GameMap et TestMap : AYSShipPath « tour de l'île » (mode playtest des bateaux), branché sur les bateaux pilotables de la map (case
    Playtest Mode laissée décochée)
Ne recrée rien qui existe.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_jolly_ship.py"
Rapport : Saved/Logs/setup_jolly_ship.txt
"""
import os
import unreal

FOE_DIR = "/Game/Placeable/Character/Foe"
HULL_BP = f"{FOE_DIR}/FoeCharacter_JollyHull"
SKULL = f"{FOE_DIR}/FoeCharacter_Skull"
SKULL_GUN = f"{FOE_DIR}/FoeCharacter_Skull_Gun"
LONGBOAT_BP = "/Game/Placeable/Ship/BP_Longboat"
JOLLY_BP = "/Game/Placeable/Ship/BP_NME_JollyShip"
NME_PISTOL = "/Game/Placeable/Weapons/Foe/BP_NME_Pistol"
GUN_AMMO = "/Game/Placeable/AMMO/NME_GunAMMO"
TEST_GAME_MODE = "/Game/Core/GameModes/BP_TestGameMode"
TEST_MAP_GAME_MODE = "/Game/Core/GameModes/BP_TestMapGameMode"
MAPS = ["/Game/Maps/GameMap", "/Game/Maps/TestMap"]

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
report = []


def log(msg):
    unreal.log(f"[setup_jolly_ship] {msg}")
    report.append(msg)


def components(bp):
    out = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if obj:
            out[obj.get_name().replace("_GEN_VARIABLE", "")] = obj
    return out


try:
    # --- coque
    if not assets.does_asset_exist(HULL_BP):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.YSFoeJollyHull)
        tools.create_asset("FoeCharacter_JollyHull", FOE_DIR, unreal.Blueprint, factory)
        hull_cdo = unreal.get_default_object(assets.load_blueprint_class(HULL_BP))
        skull_cdo = unreal.get_default_object(assets.load_blueprint_class(SKULL))
        widget = skull_cdo.get_editor_property("HealthBar").get_editor_property("widget_class")
        hull_cdo.get_editor_property("HealthBar").set_editor_property("widget_class", widget)
        assets.save_asset(HULL_BP, only_if_is_dirty=False)
        log(f"créé : {HULL_BP} (barre de vie : {widget.get_name() if widget else 'aucune'})")
    else:
        log(f"existe déjà : {HULL_BP}")

    # --- chaloupe
    if not assets.does_asset_exist(JOLLY_BP):
        assets.duplicate_asset(LONGBOAT_BP, JOLLY_BP)
        jolly = assets.load_asset(JOLLY_BP)
        unreal.BlueprintEditorLibrary.reparent_blueprint(jolly, unreal.YSFoeJollyShip)
        unreal.BlueprintEditorLibrary.compile_blueprint(jolly)
        jolly_cdo = unreal.get_default_object(assets.load_blueprint_class(JOLLY_BP))
        jolly_cdo.set_editor_property("HullClass", assets.load_blueprint_class(HULL_BP))
        jolly_cdo.set_editor_property("PilotFoeClasses", [assets.load_blueprint_class(SKULL)])
        gun = assets.load_blueprint_class(SKULL_GUN)
        jolly_cdo.set_editor_property("LeftSeatFoeClasses", [gun])
        jolly_cdo.set_editor_property("RightSeatFoeClasses", [gun])
        assets.save_asset(JOLLY_BP, only_if_is_dirty=False)
        log(f"créé : {JOLLY_BP} (copie de BP_Longboat)")
    else:
        log(f"existe déjà : {JOLLY_BP}")
    jolly_class = assets.load_blueprint_class(JOLLY_BP)

    # --- GameModes
    game_mode_cdo = unreal.get_default_object(assets.load_blueprint_class(TEST_GAME_MODE))
    game_mode_cdo.set_editor_property("JollyShipClass", jolly_class)
    assets.save_asset(TEST_GAME_MODE, only_if_is_dirty=False)
    test_mode_cdo = unreal.get_default_object(assets.load_blueprint_class(TEST_MAP_GAME_MODE))
    test_mode_cdo.set_editor_property("bJollyShipWaves", False)
    assets.save_asset(TEST_MAP_GAME_MODE, only_if_is_dirty=False)
    log("GameModes : Jolly Ship Class réglée, pas de vagues sur TestMap")

    # --- réglages utiles à connaître
    shot = components(assets.load_asset(NME_PISTOL)).get("Shot")
    if shot:
        log(f"NME pistol : Attack Range {shot.get_editor_property('AttackRange'):.0f}, Attack Interval {shot.get_editor_property('AttackInterval'):.1f}")
    ammo_cdo = unreal.get_default_object(assets.load_blueprint_class(GUN_AMMO))
    log(f"NME_GunAMMO : Speed {ammo_cdo.get_editor_property('Speed'):.0f}, Life Time {ammo_cdo.get_editor_property('LifeTime'):.2f}")

    # --- trajet du mode playtest
    for map_path in MAPS:
        levels.load_level(map_path)
        all_actors = actors_sub.get_all_level_actors()
        paths = [a for a in all_actors if isinstance(a, unreal.YSShipPath)]
        if paths:
            path = paths[0]
            log(f"{map_path} : trajet déjà là ({path.get_actor_label()})")
        else:
            path = actors_sub.spawn_actor_from_class(unreal.YSShipPath, unreal.Vector(0.0, 0.0, 0.0))
            path.set_actor_label("ShipPath_IslandTour")
            path.build_around_island()
            log(f"{map_path} : trajet créé")
        spline = path.get_component_by_class(unreal.SplineComponent)
        log(f"{map_path} : {spline.get_number_of_spline_points()} points, {spline.get_spline_length():.0f} cm")
        for ship in all_actors:
            if isinstance(ship, unreal.YSPirateShip):
                ship.set_editor_property("PlaytestPath", path)
                log(f"{map_path} : {ship.get_actor_label()} -> trajet branché (Playtest Mode : {ship.get_editor_property('bPlaytestMode')})")
        levels.save_current_level()
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_jolly_ship.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
