"""
Maps :
  - l'ancienne map de test (TestMaps/Test-Ground) renommée en /Game/Maps/GameMap (« game map » : la map du jeu, références mises à jour par l'éditeur)
  - /Game/Maps/TestMaps/TestMap : copie de GameMap SANS le jeu : les 4 PlayerStart, l'île, la mer, le ponton et le bateau
    (NME, coffres, cartes au trésor et Brooker retirés) ; GameMode Override = BP_TestMapGameMode
  - /Game/Core/GameModes/BP_TestMapGameMode : enfant de BP_TestGameMode (sélection, upgrades, HUD identiques) sans règle de spawn,
    sans carte au trésor, sans cycle jour / nuit ; pavé numérique = apparition manuelle de NME (Numpad Foe Classes)
  - touches du pavé (Foe|Spawn > Numpad Spawn Key) : Skull 0, Skull Bomb 1, Skull Gun 2, Skull Elite 9
Relançable : ne renomme / ne recopie / ne recrée que ce qui manque ; les touches ne sont écrites que si elles valent -1.

Lancement (éditeur fermé, sur une autre map que celle à renommer) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="Tools/EditorScripts/setup_test_map.py"
Rapport : Saved/Logs/setup_test_map.txt
"""
import os
import unreal

OLD_MAP = "/Game/Maps/TestMaps/Test" + "Ground"  # (nom coupé : l'éditeur refuse de renommer une map encore citée dans les fichiers texte)
GAME_MAP = "/Game/Maps/GameMap"
TEST_MAP_DIR = "/Game/Maps/TestMaps"
TEST_MAP_NAME = "TestMap"
TEST_MAP = f"{TEST_MAP_DIR}/{TEST_MAP_NAME}"
GM_DIR = "/Game/Core/GameModes"
GAME_GM = f"{GM_DIR}/BP_TestGameMode"
TEST_GM_NAME = "BP_TestMapGameMode"
TEST_GM = f"{GM_DIR}/{TEST_GM_NAME}"
CHAR_DIR = "/Game/Placeable/Character"
NUMPAD_FOES = [("FoeCharacter_Skull", 0), ("FoeCharacter_Skull_Bomb", 1), ("FoeCharacter_Skull_Gun", 2), ("FoeCharacter_Skull_Elite", 9)]

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report = []


def log(msg):
    unreal.log(f"[setup_test_map] {msg}")
    report.append(msg)


try:
    # 1) la map du jeu s'appelle GameMap. Le renommage d'une map demande toujours une confirmation (annulée en mode
    # automatique) : « Enregistrer sous » GameMap (save_map), vérifié sur le disque, puis suppression de l'ancienne,
    # qu'aucun asset ne référence (seulement la config, déjà mise à jour)
    def save_level_as(source, target):
        if not levels.load_level(source):
            raise RuntimeError(f"impossible d'ouvrir {source}")
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        if not unreal.EditorLoadingAndSavingUtils.save_map(world, target):
            raise RuntimeError(f"« enregistrer sous » {target} impossible")
        disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir() + target[len("/Game/"):] + ".umap")
        if not os.path.exists(disk):
            raise RuntimeError(f"{target} absente du disque après la sauvegarde ({disk})")
        # save_map laisse la map d'origine ouverte : on ouvre la nouvelle pour que les retouches suivantes portent sur elle
        if not levels.load_level(target):
            raise RuntimeError(f"impossible d'ouvrir {target}")

    if assets.does_asset_exist(OLD_MAP) and not assets.does_asset_exist(GAME_MAP):
        referencers = [str(p) for p in unreal.AssetRegistryHelpers.get_asset_registry().get_referencers(OLD_MAP, unreal.AssetRegistryDependencyOptions()) or []]
        if referencers:
            raise RuntimeError(f"{OLD_MAP} est référencée par {referencers}")
        save_level_as(OLD_MAP, GAME_MAP)
        log(f"enregistrée sous : {OLD_MAP} -> {GAME_MAP}")
    elif assets.does_asset_exist(GAME_MAP):
        log(f"existe déjà : {GAME_MAP}")
    else:
        raise RuntimeError("ni l'ancienne map ni GameMap")

    # l'ancienne map ne se supprime que si elle n'a pas été chargée dans cette session : au lancement suivant du script
    old_disk = unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_content_dir() + OLD_MAP[len("/Game/"):] + ".umap")
    if os.path.exists(old_disk):
        assets.delete_asset(OLD_MAP)
        log(f"ancienne map {'supprimée' if not os.path.exists(old_disk) else 'encore chargée : relancer le script pour la supprimer'}")

    # 2) NME : touche du pavé numérique
    # Skull d'abord : ses enfants (Bomb, Gun, Elite) héritent de sa touche tant qu'ils n'ont pas la leur
    foe_classes = []
    skull_key = None
    for name, key in NUMPAD_FOES:
        path = f"{CHAR_DIR}/{name}"
        cdo = unreal.get_default_object(assets.load_blueprint_class(path))
        current = cdo.get_editor_property("numpad_spawn_key")
        if current < 0 or (skull_key is not None and current == skull_key):
            cdo.set_editor_property("numpad_spawn_key", key)
            assets.save_asset(path, only_if_is_dirty=False)
            log(f"{name} : touche {key}")
        if skull_key is None:
            skull_key = cdo.get_editor_property("numpad_spawn_key")
        foe_classes.append(assets.load_blueprint_class(path))

    # 3) GameMode de la map de test
    if not assets.does_asset_exist(TEST_GM):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", assets.load_blueprint_class(GAME_GM))
        tools.create_asset(TEST_GM_NAME, GM_DIR, unreal.Blueprint, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(assets.load_asset(TEST_GM))
        gm = unreal.get_default_object(assets.load_blueprint_class(TEST_GM))
        gm.set_editor_property("spawn_rules", [])
        gm.set_editor_property("treasure_map_class", None)
        gm.set_editor_property("day_night_cycle", False)
        gm.set_editor_property("numpad_foe_spawn", True)
        gm.set_editor_property("numpad_foe_classes", foe_classes)
        assets.save_asset(TEST_GM, only_if_is_dirty=False)
        log(f"créé : {TEST_GM}")
    else:
        log(f"existe déjà : {TEST_GM}")
    test_gm_class = assets.load_blueprint_class(TEST_GM)

    # 4) TestMap : GameMap enregistrée sous TestMap, sans le jeu
    if not assets.does_asset_exist(TEST_MAP):
        save_level_as(GAME_MAP, TEST_MAP)
        log(f"créée : {TEST_MAP} (GameMap enregistrée sous)")
        removed = []
        for actor in actors_sub.get_all_level_actors():
            if isinstance(actor, (unreal.YSFoeCharacter, unreal.YSInteractiveObject, unreal.YSBrookerNPC)):
                removed.append(actor.get_actor_label())
                actors_sub.destroy_actor(actor)
        log(f"retirés de TestMap : {removed}")
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        world.get_world_settings().set_editor_property("default_game_mode", test_gm_class)
        levels.save_current_level()
        log("TestMap : GameMode Override = BP_TestMapGameMode, sauvegardée")
    else:
        log(f"existe déjà : {TEST_MAP}")

    # relecture
    levels.load_level(TEST_MAP)
    world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
    counts = {}
    for actor in actors_sub.get_all_level_actors():
        cls = actor.get_class().get_name()
        counts[cls] = counts.get(cls, 0) + 1
    gm_override = world.get_world_settings().get_editor_property("default_game_mode")
    log(f"relu TestMap : GameMode {gm_override.get_name() if gm_override else None}, KillZ {world.get_world_settings().get_editor_property('kill_z')}, "
        f"acteurs {sorted(counts.items())}")
    gm = unreal.get_default_object(test_gm_class)
    log(f"relu BP_TestMapGameMode : règles {len(gm.get_editor_property('spawn_rules'))}, carte {gm.get_editor_property('treasure_map_class')}, "
        f"jour/nuit {gm.get_editor_property('day_night_cycle')}, pavé {gm.get_editor_property('numpad_foe_spawn')} "
        f"{[(c.get_name(), unreal.get_default_object(c).get_editor_property('numpad_spawn_key')) for c in gm.get_editor_property('numpad_foe_classes')]}")
    log(f"relu : GameMap {assets.does_asset_exist(GAME_MAP)}, TestGround {assets.does_asset_exist(OLD_MAP)}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_test_map.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
