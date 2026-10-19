"""
Skull Bomb (NME à distance) :
  - /Game/Art/Materials/M_FoeExplosion : translucide non éclairé (paramètres Color, Opacity) pour la sphère d'explosion
  - /Game/Art/Materials/M_FoeBomb : bombe (paramètres Color, Emissive), du noir au rouge lumineux pendant la mèche
  - FoeCharacter_Skull_Bomb (BP de Paul, enfant de FoeCharacter_Skull) : bRanged, matériaux de la bombe
    (disque d'impact = M_BrookerZone), Spawn Weight 0.25 (1 apparition sur 5 face au Skull à 1)
  - BP_TestGameMode : Skull Bomb ajouté aux NME de la règle de vagues
Ne recrée pas ce qui existe déjà.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_skull_bomb.py"
Rapport : Saved/Logs/setup_skull_bomb.txt
"""
import os
import unreal

MATERIALS_DIR = "/Game/Art/Materials"
EXPLOSION_MATERIAL = f"{MATERIALS_DIR}/M_FoeExplosion"
BOMB_MATERIAL = f"{MATERIALS_DIR}/M_FoeBomb"
ZONE_MATERIAL = f"{MATERIALS_DIR}/M_BrookerZone"
BOMB_BP = "/Game/Placeable/Character/FoeCharacter_Skull_Bomb"
GAME_MODE_BP = "/Game/Core/GameModes/BP_TestGameMode"
BOMB_SPAWN_WEIGHT = 0.25

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
report = []


def log(msg):
    unreal.log(f"[setup_skull_bomb] {msg}")
    report.append(msg)


try:
    # matériau de l'explosion
    if not assets.does_asset_exist(EXPLOSION_MATERIAL):
        material = tools.create_asset("M_FoeExplosion", MATERIALS_DIR, unreal.Material, unreal.MaterialFactoryNew())
        material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
        material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
        color = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -400, 0)
        color.set_editor_property("parameter_name", "Color")
        color.set_editor_property("default_value", unreal.LinearColor(1.0, 0.0, 0.0, 1.0))
        opacity = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -400, 200)
        opacity.set_editor_property("parameter_name", "Opacity")
        opacity.set_editor_property("default_value", 0.35)
        mel.connect_material_property(color, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
        mel.recompile_material(material)
        assets.save_asset(EXPLOSION_MATERIAL, only_if_is_dirty=False)
        log(f"créé : {EXPLOSION_MATERIAL}")
    else:
        log(f"existe déjà : {EXPLOSION_MATERIAL}")

    # matériau de la bombe : opaque éclairé, couleur + lueur (mèche du noir au rouge lumineux)
    if not assets.does_asset_exist(BOMB_MATERIAL):
        material = tools.create_asset("M_FoeBomb", MATERIALS_DIR, unreal.Material, unreal.MaterialFactoryNew())
        color = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -400, 0)
        color.set_editor_property("parameter_name", "Color")
        color.set_editor_property("default_value", unreal.LinearColor(0.02, 0.02, 0.02, 1.0))
        emissive = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -400, 200)
        emissive.set_editor_property("parameter_name", "Emissive")
        emissive.set_editor_property("default_value", unreal.LinearColor(0.0, 0.0, 0.0, 1.0))
        mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
        mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
        mel.recompile_material(material)
        assets.save_asset(BOMB_MATERIAL, only_if_is_dirty=False)
        log(f"créé : {BOMB_MATERIAL}")
    else:
        log(f"existe déjà : {BOMB_MATERIAL}")

    # Skull Bomb : à distance, matériaux de la bombe, 1 apparition sur 5 (réglages de Paul conservés au 2e passage)
    bomb_class = assets.load_blueprint_class(BOMB_BP)
    bomb_cdo = unreal.get_default_object(bomb_class)
    if not bomb_cdo.get_editor_property("ranged"):
        bomb_cdo.set_editor_property("ranged", True)
        bomb_cdo.set_editor_property("spawn_weight", BOMB_SPAWN_WEIGHT)
    if bomb_cdo.get_editor_property("landing_zone_material") is None:
        bomb_cdo.set_editor_property("landing_zone_material", assets.load_asset(ZONE_MATERIAL))
    if bomb_cdo.get_editor_property("explosion_material") is None:
        bomb_cdo.set_editor_property("explosion_material", assets.load_asset(EXPLOSION_MATERIAL))
    if bomb_cdo.get_editor_property("bomb_material") is None:
        bomb_cdo.set_editor_property("bomb_material", assets.load_asset(BOMB_MATERIAL))
    if str(bomb_cdo.get_editor_property("foe_name")) == "":
        bomb_cdo.set_editor_property("foe_name", "Skull Bomb")
    assets.save_asset(BOMB_BP, only_if_is_dirty=False)

    # vagues : Skull Bomb ajouté aux NME
    gm = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP))
    wave = next((r for r in gm.get_editor_property("spawn_rules") if r and isinstance(r, unreal.YSTimedWaveSpawnRule)), None)
    if wave is None:
        raise RuntimeError("pas de UYSTimedWaveSpawnRule dans SpawnRules")
    classes = list(wave.get_editor_property("enemy_classes"))
    if bomb_class not in classes:
        classes.append(bomb_class)
        wave.set_editor_property("enemy_classes", classes)
        assets.save_asset(GAME_MODE_BP, only_if_is_dirty=False)
        log("Skull Bomb ajouté aux vagues")

    # relecture
    check_bomb = unreal.get_default_object(assets.load_blueprint_class(BOMB_BP))
    check_rules = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP)).get_editor_property("spawn_rules")
    log(f"relu : Skull Bomb ranged {check_bomb.get_editor_property('ranged')}, portée {check_bomb.get_editor_property('attack_range')}, "
        f"poids {check_bomb.get_editor_property('spawn_weight')}, zone {check_bomb.get_editor_property('landing_zone_material').get_name()}, "
        f"explosion {check_bomb.get_editor_property('explosion_material').get_name()}, bombe {check_bomb.get_editor_property('bomb_material').get_name()}, sommet {check_bomb.get_editor_property('bomb_apex_height')}, vol {check_bomb.get_editor_property('bomb_flight_duration')} ; vagues "
        f"{[[c.get_name() for c in r.get_editor_property('enemy_classes')] for r in check_rules if r]}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_skull_bomb.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
