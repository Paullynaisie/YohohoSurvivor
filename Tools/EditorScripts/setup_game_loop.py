"""
Boucle de jeu de départ (GameMap seulement) :
  - coffres : BP_InteractiveObject_Chest 35 doublons, BP_InteractiveObject_TreasureChest 50
  - BP_TestGameMode (GameMap) : overtime de l'île de départ = circle spawns de FoeCharacter_Skull (niveau jour + 3, puis jour + 5
    30 s plus tard) ; bateau à l'objectif de doublons, fin de jeu sur le bateau, overtime : réglages par défaut du C++ (cochés)
  - BP_TestMapGameMode (TestMap) : rien de tout ça (bateau toujours là, pas de fin de jeu, pas d'overtime)
  - /Game/Core/UI/WBP_ShipBoarding (enfant de YSShipBoardingWidget, réglages Layout) choisi par BP_YSPlayerController
  - GameMap : palmiers, ponton et Brooker accrochés au Landscape « Island » (ils coulent avec l'île)
Relançable (ne recrée pas WBP_ShipBoarding, ne réaccroche pas ce qui l'est déjà).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_game_loop.py"
Rapport : Saved/Logs/setup_game_loop.txt
"""
import os
import unreal

CHEST = "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_Chest"
TREASURE_CHEST = "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_TreasureChest"
GAME_GM = "/Game/Core/GameModes/BP_TestGameMode"
TEST_GM = "/Game/Core/GameModes/BP_TestMapGameMode"
SKULL = "/Game/Placeable/Character/Foe/FoeCharacter_Skull"
PC_BP = "/Game/Core/Player/BP_YSPlayerController"
WIDGET_DIR = "/Game/Core/UI"
WIDGET_NAME = "WBP_ShipBoarding"
GAME_MAP = "/Game/Maps/GameMap"
ISLAND_LABEL = "Island"
ATTACHED_LABELS = ("Pier", "NPC_Brooker")

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
report = []


def log(msg):
    unreal.log(f"[setup_game_loop] {msg}")
    report.append(msg)


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


try:
    # coffres
    for path, value in ((CHEST, 35), (TREASURE_CHEST, 50)):
        cdo(path).set_editor_property("doubloon_value", value)
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"{path} : {cdo(path).get_editor_property('doubloon_value')} doublons")

    # GameMap : vagues d'overtime en Skull
    skull = assets.load_blueprint_class(SKULL)
    game = cdo(GAME_GM)
    waves = list(game.get_editor_property("overtime_waves"))
    for wave in waves:
        if len(wave.get_editor_property("enemy_classes")) == 0:
            wave.set_editor_property("enemy_classes", [skull])
    game.set_editor_property("overtime_waves", waves)
    assets.save_asset(GAME_GM, only_if_is_dirty=False)
    for i, wave in enumerate(cdo(GAME_GM).get_editor_property("overtime_waves")):
        log(f"BP_TestGameMode : overtime {i + 1} = {[c.get_name() for c in wave.get_editor_property('enemy_classes')]} "
            f"x{wave.get_editor_property('enemies_per_player')} / joueur, {wave.get_editor_property('delay')} s, niveau jour + {wave.get_editor_property('level_offset')}")
    for prop in ("ship_arrives_at_doubloon_goal", "ship_boarding_victory", "overtime", "overtime_day"):
        log(f"BP_TestGameMode : {prop} = {cdo(GAME_GM).get_editor_property(prop)}")

    # TestMap : rien de la boucle de jeu
    test = cdo(TEST_GM)
    for prop in ("ship_arrives_at_doubloon_goal", "ship_boarding_victory", "overtime"):
        test.set_editor_property(prop, False)
    assets.save_asset(TEST_GM, only_if_is_dirty=False)
    log(f"BP_TestMapGameMode : bateau à l'objectif {cdo(TEST_GM).get_editor_property('ship_arrives_at_doubloon_goal')}, "
        f"fin de jeu {cdo(TEST_GM).get_editor_property('ship_boarding_victory')}, overtime {cdo(TEST_GM).get_editor_property('overtime')}")

    # widget de fin de jeu
    widget_bp = f"{WIDGET_DIR}/{WIDGET_NAME}"
    if not assets.does_asset_exist(widget_bp):
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", unreal.YSShipBoardingWidget)
        created = tools.create_asset(WIDGET_NAME, WIDGET_DIR, unreal.WidgetBlueprint, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(created)
        assets.save_asset(widget_bp, only_if_is_dirty=False)
        log(f"créé : {widget_bp}")
    cdo(PC_BP).set_editor_property("ship_boarding_widget_class", assets.load_blueprint_class(widget_bp))
    assets.save_asset(PC_BP, only_if_is_dirty=False)
    log(f"BP_YSPlayerController : ShipBoardingWidgetClass = {cdo(PC_BP).get_editor_property('ship_boarding_widget_class').get_name()}")

    # GameMap : props de l'île accrochés au Landscape
    unreal.EditorLoadingAndSavingUtils.load_map(GAME_MAP)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
    island = next((a for a in actors if isinstance(a, unreal.Landscape) and a.get_actor_label() == ISLAND_LABEL), None)
    if not island:
        raise RuntimeError("Landscape « Island » introuvable")
    count = 0
    for actor in actors:
        is_prop = isinstance(actor, unreal.YSProp) or actor.get_actor_label() in ATTACHED_LABELS
        if not is_prop or actor == island:
            continue
        if actor.get_attach_parent_actor() == island:
            count += 1
            continue
        actor.modify()
        actor.attach_to_actor(island, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
        if actor.get_attach_parent_actor() == island:
            count += 1
        else:
            log(f"impossible d'accrocher {actor.get_actor_label()}")
    saved = unreal.EditorLoadingAndSavingUtils.save_map(unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world(), GAME_MAP)
    log(f"GameMap enregistrée : {saved}")
    log(f"GameMap : {count} acteur(s) accroché(s) à {island.get_actor_label()}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_game_loop.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
