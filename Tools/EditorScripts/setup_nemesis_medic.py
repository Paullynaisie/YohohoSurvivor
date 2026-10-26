"""
2026-10-27 :
  - Skull Nemesis : /Game/Placeable/Character/Foe/FoeCharacter_Skull_Nemesis (BP enfant de FoeCharacter_Skull, créé s'il
    n'existe pas) : corps violet (m_PH_monochrome_Purple), set d'arme DA_WeaponSet_NME_GreatSword, « Foe Skull Nemesis »,
    touche 3 du pavé numérique (ajouté aux NME de BP_TestMapGameMode)
  - nuit (règle « Vagues jour / nuit » de BP_TestGameMode) : début = élite + circle spawn de Skull (inchangé) ; 30 s = circle
    spawn de Skull Gun ; 60 s = circle spawn de Skull Nemesis ; puis toutes les 30 s des Nemesis 2 niveaux plus haut que le
    précédent (plus de circle spawn à 50 % des PV de l'élite)
  - overtime : les circle spawns de Skull (niveau bonus) font apparaître des Skull Nemesis
  - Medic : /Game/Placeable/Character/NPC/BP_NPC_Medic = copie de BP_NPC_Brooker reparentée sur YSMedicNPC, tout ce qui
    était jaune en vert (matériaux Yellow -> Green, couleurs de la bulle et de la zone) ; placé dans GameMap et TestMap
    (accroché à l'île, il coule avec elle en overtime) s'il n'y en a pas déjà un
  - armure des personnages joueurs : Min Damage After Armor = 1 (l'armure ne ramène plus un coup à 0)
Relançable (ne recrée rien qui existe, valeurs absolues).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_nemesis_medic.py"
Rapport : Saved/Logs/setup_nemesis_medic.txt
"""
import os
import unreal

FOE_DIR = "/Game/Placeable/Character/Foe"
SKULL_BP = f"{FOE_DIR}/FoeCharacter_Skull"
SKULL_GUN_BP = f"{FOE_DIR}/FoeCharacter_Skull_Gun"
NEMESIS_NAME = "FoeCharacter_Skull_Nemesis"
NEMESIS_BP = f"{FOE_DIR}/{NEMESIS_NAME}"
NEMESIS_KEY = 3
PURPLE = "/Game/Art/Materials/PH_materials/m_PH_monochrome_Purple"
GREAT_SWORD_SET_NAME = "DA_WeaponSet_NME_GreatSword"
GAME_GM = "/Game/Core/GameModes/BP_TestGameMode"
TEST_GM = "/Game/Core/GameModes/BP_TestMapGameMode"
NPC_DIR = "/Game/Placeable/Character/NPC"
BROOKER_NAME = "BP_NPC_Brooker"
MEDIC_NAME = "BP_NPC_Medic"
MEDIC_BP = f"{NPC_DIR}/{MEDIC_NAME}"
GREEN = unreal.LinearColor(0.15, 0.85, 0.25, 1.0)
MAPS = ["/Game/Maps/GameMap", "/Game/Maps/TestMap"]
# à l'est du centre de l'île (spawns des joueurs au centre), posé au sol par le Medic à son BeginPlay
MEDIC_LOCATION = unreal.Vector(1500.0, 1500.0, 1200.0)
NIGHT_EVENTS = [(30.0, SKULL_GUN_BP), (60.0, NEMESIS_BP)]
REPEAT_INTERVAL = 30.0
REPEAT_LEVEL_STEP = 2

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
report = []


def log(msg):
    unreal.log(f"[setup_nemesis_medic] {msg}")
    report.append(msg)


def find_asset(name, root="/Game"):
    """chemin de l'asset de ce nom dans les sous-dossiers (Paul range les assets)"""
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for data in registry.get_assets_by_path(root, recursive=True):
        if str(data.asset_name) == name:
            return str(data.package_name)
    return None


def components(bp):
    return [lib.get_object(lib.get_data(h)) for h in subsystem.k2_gather_subobject_data_for_blueprint(bp) if lib.get_object(lib.get_data(h))]


def setup_nemesis():
    skull_class = assets.load_blueprint_class(SKULL_BP)
    if assets.does_asset_exist(NEMESIS_BP):
        log(f"existe déjà : {NEMESIS_BP}")
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", skull_class)
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NEMESIS_NAME, FOE_DIR, unreal.Blueprint, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assets.save_asset(NEMESIS_BP, only_if_is_dirty=False)
        log(f"créé : {NEMESIS_BP}")

    set_path = find_asset(GREAT_SWORD_SET_NAME)
    great_sword_set = assets.load_asset(set_path) if set_path else None
    if not great_sword_set:
        raise RuntimeError(f"{GREAT_SWORD_SET_NAME} introuvable")

    nemesis = unreal.get_default_object(assets.load_blueprint_class(NEMESIS_BP))
    nemesis.set_editor_property("foe_name", "Foe Skull Nemesis")
    nemesis.set_editor_property("weapon_set", great_sword_set)
    nemesis.set_editor_property("numpad_spawn_key", NEMESIS_KEY)
    body = nemesis.get_editor_property("BodyMesh")
    body.set_material(0, assets.load_asset(PURPLE))
    assets.save_asset(NEMESIS_BP, only_if_is_dirty=False)

    check = unreal.get_default_object(assets.load_blueprint_class(NEMESIS_BP))
    log(f"Nemesis relu : {check.get_editor_property('foe_name')}, set {check.get_editor_property('weapon_set').get_name()}, "
        f"corps {check.get_editor_property('BodyMesh').get_material(0).get_name()}, touche {check.get_editor_property('numpad_spawn_key')}")
    return assets.load_blueprint_class(NEMESIS_BP)


def make_circle(template, foe_class):
    circle = unreal.YSCircleSpawn()
    circle.set_editor_property("enabled", True)
    circle.set_editor_property("enemy_classes", [foe_class])
    circle.set_editor_property("enemies_per_player", template.get_editor_property("enemies_per_player"))
    circle.set_editor_property("max_enemies", template.get_editor_property("max_enemies"))
    return circle


def setup_night(nemesis_class):
    skull_class = assets.load_blueprint_class(SKULL_BP)
    gm = unreal.get_default_object(assets.load_blueprint_class(GAME_GM))
    wave = next((r for r in gm.get_editor_property("spawn_rules") if r and isinstance(r, unreal.YSTimedWaveSpawnRule)), None)
    if wave is None:
        raise RuntimeError("pas de règle de vagues dans SpawnRules de BP_TestGameMode")

    # même nombre de NME que le circle spawn du début de nuit
    template = wave.get_editor_property("night_start_circle_spawn")
    events = []
    for delay, path in NIGHT_EVENTS:
        event = unreal.YSNightCircleEvent()
        event.set_editor_property("delay", delay)
        event.set_editor_property("circle", make_circle(template, assets.load_blueprint_class(path)))
        events.append(event)
    wave.set_editor_property("night_circle_events", events)
    wave.set_editor_property("repeat_last_night_circle", True)
    wave.set_editor_property("night_circle_repeat_interval", REPEAT_INTERVAL)
    wave.set_editor_property("night_circle_repeat_level_step", REPEAT_LEVEL_STEP)

    # overtime : Skull -> Skull Nemesis
    waves = []
    for overtime in gm.get_editor_property("overtime_waves"):
        classes = [nemesis_class if c == skull_class else c for c in overtime.get_editor_property("enemy_classes")]
        overtime.set_editor_property("enemy_classes", classes)
        waves.append(overtime)
    gm.set_editor_property("overtime_waves", waves)
    assets.save_asset(GAME_GM, only_if_is_dirty=False)

    check_gm = unreal.get_default_object(assets.load_blueprint_class(GAME_GM))
    check = next(r for r in check_gm.get_editor_property("spawn_rules") if r and isinstance(r, unreal.YSTimedWaveSpawnRule))
    start = check.get_editor_property("night_start_circle_spawn")
    log(f"nuit, début : {[c.get_name() for c in start.get_editor_property('enemy_classes') if c]} x {start.get_editor_property('enemies_per_player')} / joueur")
    for event in check.get_editor_property("night_circle_events"):
        circle = event.get_editor_property("circle")
        log(f"nuit, {event.get_editor_property('delay')} s : {[c.get_name() for c in circle.get_editor_property('enemy_classes') if c]} "
            f"x {circle.get_editor_property('enemies_per_player')} / joueur")
    log(f"répétition : {check.get_editor_property('repeat_last_night_circle')}, toutes les {check.get_editor_property('night_circle_repeat_interval')} s, "
        f"+{check.get_editor_property('night_circle_repeat_level_step')} niveaux")
    for overtime in check_gm.get_editor_property("overtime_waves"):
        log(f"overtime : délai {overtime.get_editor_property('delay')} s, niveau +{overtime.get_editor_property('level_offset')}, "
            f"{[c.get_name() for c in overtime.get_editor_property('enemy_classes') if c]}")

    test_gm = unreal.get_default_object(assets.load_blueprint_class(TEST_GM))
    numpad = [c for c in test_gm.get_editor_property("numpad_foe_classes") if c]
    if nemesis_class not in numpad:
        numpad.append(nemesis_class)
        test_gm.set_editor_property("numpad_foe_classes", numpad)
        assets.save_asset(TEST_GM, only_if_is_dirty=False)
    log(f"pavé numérique (TestMap) : {[c.get_name() for c in unreal.get_default_object(assets.load_blueprint_class(TEST_GM)).get_editor_property('numpad_foe_classes') if c]}")


def setup_medic():
    if assets.does_asset_exist(MEDIC_BP):
        log(f"existe déjà : {MEDIC_BP}")
    else:
        brooker_path = find_asset(BROOKER_NAME)
        if not brooker_path:
            raise RuntimeError(f"{BROOKER_NAME} introuvable")
        if not assets.duplicate_asset(brooker_path, MEDIC_BP):
            raise RuntimeError(f"copie de {brooker_path} impossible")
        log(f"copié : {brooker_path} -> {MEDIC_BP}")

    bp = assets.load_asset(MEDIC_BP)
    if bp.generated_class() and not unreal.MathLibrary.class_is_child_of(bp.generated_class(), unreal.YSMedicNPC):
        unreal.BlueprintEditorLibrary.reparent_blueprint(bp, unreal.YSMedicNPC)
        log("reparenté sur YSMedicNPC")
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)

    # tout ce qui était jaune en vert
    for component in components(bp):
        if not isinstance(component, unreal.MeshComponent):
            continue
        for index in range(component.get_num_materials()):
            material = component.get_material(index)
            if material and "Yellow" in material.get_name():
                green_path = material.get_path_name().split(".")[0].replace("Yellow", "Green")
                green = assets.load_asset(green_path) if assets.does_asset_exist(green_path) else None
                if green:
                    component.set_material(index, green)
                    log(f"{component.get_name()} : {material.get_name()} -> {green.get_name()}")
                else:
                    log(f"ATTENTION {component.get_name()} : pas de version verte de {material.get_name()}")

    medic = unreal.get_default_object(bp.generated_class())
    medic.set_editor_property("brooker_name", "Medic")
    medic.set_editor_property("indicator_color", GREEN)
    medic.set_editor_property("zone_color", GREEN)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(MEDIC_BP, only_if_is_dirty=False)
    check = unreal.get_default_object(assets.load_blueprint_class(MEDIC_BP))
    log(f"Medic relu : {check.get_editor_property('brooker_name')}, bulle {check.get_editor_property('indicator_color')}, "
        f"zone {check.get_editor_property('zone_color')}, rayon {check.get_editor_property('accept_range')}")
    return assets.load_blueprint_class(MEDIC_BP)


def place_medic(level, medic_class):
    if not levels.load_level(level):
        raise RuntimeError(f"impossible d'ouvrir {level}")
    existing = [a for a in actors_sub.get_all_level_actors() if isinstance(a, unreal.YSMedicNPC)]
    if existing:
        log(f"{level} : Medic déjà placé ({existing[0].get_actor_label()})")
        return
    medic = actors_sub.spawn_actor_from_class(medic_class, MEDIC_LOCATION, unreal.Rotator(roll=0.0, pitch=0.0, yaw=180.0))
    medic.set_actor_label("Medic")
    medic.set_folder_path("Island")
    landscape = next((a for a in actors_sub.get_all_level_actors() if isinstance(a, unreal.LandscapeProxy)), None)
    if landscape:
        medic.attach_to_actor(landscape, "", unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, unreal.AttachmentRule.KEEP_WORLD, False)
    if not levels.save_current_level():
        raise RuntimeError(f"sauvegarde de {level} impossible")
    log(f"{level} : Medic placé en {MEDIC_LOCATION}, accroché à {landscape.get_actor_label() if landscape else 'rien'}")


def setup_armor():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for data in registry.get_assets_by_path("/Game/Placeable/Character", recursive=True):
        if str(data.asset_class_path.asset_name) != "Blueprint":
            continue
        path = str(data.package_name)
        cls = assets.load_blueprint_class(path)
        if not cls or not unreal.MathLibrary.class_is_child_of(cls, unreal.YSPlayerCharacter):
            continue
        cdo = unreal.get_default_object(cls)
        old = cdo.get_editor_property("min_damage_after_armor")
        if old != 1.0:
            cdo.set_editor_property("min_damage_after_armor", 1.0)
            assets.save_asset(path, only_if_is_dirty=False)
        log(f"{data.asset_name} : Min Damage After Armor {old} -> 1, Min Damage Ratio After Armor {cdo.get_editor_property('min_damage_ratio_after_armor')}")


try:
    nemesis_class = setup_nemesis()
    setup_night(nemesis_class)
    medic_class = setup_medic()
    for level in MAPS:
        place_medic(level, medic_class)
    setup_armor()
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_nemesis_medic.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
