"""
Gamemode final et sa map :
  - /Game/Core/GameModes/BP_FlowMod : enfant de AYSGameMode. Sélection, upgrades et HUD comme BP_TestMapGameMode (réglages copiés une fois),
    mais il ne fait rien : ni NME, ni jour / nuit, ni carte au trésor, ni bateau, ni overtime, ni pavé numérique
  - /Game/Maps/FlowMap : GameMap enregistrée sous, sans le jeu : lumières, ciel, 4 PlayerStart, mer (AYSOcean) et deadzone (KillZ) ;
    GameMode Override = BP_FlowMod. L'île se crée ensuite avec generate_island.py
Relançable : ne recrée rien qui existe.

Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/setup_flow_map.py"
Rapport : Saved/Logs/setup_flow_map.txt
"""
import os
import unreal

GM_DIR = "/Game/Core/GameModes"
FLOW_GM_NAME = "BP_FlowMod"
FLOW_GM = f"{GM_DIR}/{FLOW_GM_NAME}"
SOURCE_GM = f"{GM_DIR}/BP_TestMapGameMode"
GAME_MAP = "/Game/Maps/GameMap"
FLOW_MAP = "/Game/Maps/FlowMap"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report = []


def log(msg):
    unreal.log(f"[setup_flow_map] {msg}")
    report.append(msg)


try:
    # 1) gamemode
    if not assets.does_asset_exist(FLOW_GM):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.YSGameMode)
        bp = tools.create_asset(FLOW_GM_NAME, GM_DIR, unreal.Blueprint, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        gm = unreal.get_default_object(assets.load_blueprint_class(FLOW_GM))
        source = unreal.get_default_object(assets.load_blueprint_class(SOURCE_GM))
        copied = unreal.YSEditorScriptLibrary.copy_editable_properties(source, gm)
        for prop in ("player_controller_class", "default_pawn_class", "game_state_class", "player_state_class"):
            gm.set_editor_property(prop, source.get_editor_property(prop))
        gm.set_editor_property("spawn_rules", [])
        gm.set_editor_property("treasure_map_class", None)
        for prop in ("day_night_cycle", "numpad_foe_spawn", "galion_waves", "jolly_ship_waves", "random_harbor",
                     "ship_arrives_at_doubloon_goal", "ship_boarding_victory", "overtime"):
            gm.set_editor_property(prop, False)
        gm.set_editor_property("numpad_foe_classes", [])
        assets.save_asset(FLOW_GM, only_if_is_dirty=False)
        log(f"créé : {FLOW_GM} ({copied} réglages copiés de BP_TestMapGameMode)")
    else:
        log(f"existe déjà : {FLOW_GM}")
    flow_class = assets.load_blueprint_class(FLOW_GM)

    # 2) map
    if not assets.does_asset_exist(FLOW_MAP):
        if not levels.load_level(GAME_MAP):
            raise RuntimeError(f"impossible d'ouvrir {GAME_MAP}")
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        if not unreal.EditorLoadingAndSavingUtils.save_map(world, FLOW_MAP):
            raise RuntimeError(f"« enregistrer sous » {FLOW_MAP} impossible")
        # save_map laisse la map d'origine ouverte : on ouvre la nouvelle pour que les retouches portent sur elle
        if not levels.load_level(FLOW_MAP):
            raise RuntimeError(f"impossible d'ouvrir {FLOW_MAP}")
        keep = (unreal.DirectionalLight, unreal.SkyAtmosphere, unreal.SkyLight, unreal.PlayerStart, unreal.YSOcean)
        removed = 0
        for actor in actors_sub.get_all_level_actors():
            # les postes du bateau partent avec lui : on ne garde que ce qu'on veut, parents d'abord
            if isinstance(actor, unreal.YSPirateShip) and not isinstance(actor, keep):
                actors_sub.destroy_actor(actor)
                removed += 1
        for actor in actors_sub.get_all_level_actors():
            if not isinstance(actor, keep):
                actors_sub.destroy_actor(actor)
                removed += 1
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        world.get_world_settings().set_editor_property("default_game_mode", flow_class)
        if not levels.save_current_level():
            raise RuntimeError("sauvegarde de FlowMap impossible")
        log(f"créée : {FLOW_MAP} ({removed} acteurs du jeu retirés)")
    else:
        log(f"existe déjà : {FLOW_MAP}")

    # relecture
    levels.load_level(FLOW_MAP)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    settings = world.get_world_settings()
    counts = {}
    for actor in actors_sub.get_all_level_actors():
        name = actor.get_class().get_name()
        counts[name] = counts.get(name, 0) + 1
    override = settings.get_editor_property("default_game_mode")
    log(f"relu FlowMap : GameMode {override.get_name() if override else None}, KillZ {settings.get_editor_property('kill_z')}, acteurs {sorted(counts.items())}")
    gm = unreal.get_default_object(flow_class)
    log(f"relu BP_FlowMod : règles {len(gm.get_editor_property('spawn_rules'))}, jour/nuit {gm.get_editor_property('day_night_cycle')}, "
        f"persos {len(gm.get_editor_property('selectable_characters'))}, contrôleur {gm.get_editor_property('player_controller_class').get_name()}, "
        f"upgrades {gm.get_editor_property('chara_upgrades_table')}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_flow_map.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
