"""
Zone de rachat du Brooker :
  - /Game/Art/Materials/M_BrookerZone : matériau translucide éclairé (couleur de base Color, lueur Color x Glow), opacité = FillOpacity à l'intérieur + EdgeOpacity
    sur le bord (distance au centre de l'objet / Radius ; largeur EdgeWidth en cm). Paramètres réglés par AYSBrookerNPC
    (Brooker|Zone) : Color, Glow, FillOpacity, EdgeOpacity, Radius (= AcceptRange), EdgeWidth.
  - BP_NPC_Brooker : ZoneMaterial = M_BrookerZone
  - GameMap : le coffre de test (Chest_Test) reprend la valeur du Blueprint (sa valeur d'instance était à 0),
    les Brookers placés reprennent le matériau de zone du Blueprint
Ne recrée pas le matériau s'il existe déjà.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_brooker_zone.py"
Rapport : Saved/Logs/setup_brooker_zone.txt
"""
import os
import unreal

MATERIAL_DIR = "/Game/Art/Materials"
MATERIAL_NAME = "M_BrookerZone"
MATERIAL_PATH = f"{MATERIAL_DIR}/{MATERIAL_NAME}"
BROOKER_BP = "/Game/Placeable/NPC/BP_NPC_Brooker"
CHEST_BP = "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_Chest"
LEVEL_PATH = "/Game/Maps/GameMap"
# reconstruit le matériau (à remettre à False une fois réglé par Paul)
REBUILD_MATERIAL = False

assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
report = []


def log(msg):
    unreal.log(f"[setup_brooker_zone] {msg}")
    report.append(msg)


def node(material, cls, x, y, **props):
    expression = mel.create_material_expression(material, cls, x, y)
    for key, value in props.items():
        expression.set_editor_property(key, value)
    return expression


def build_material(material):
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    # éclairé comme le sol (une couleur émissive seule est trop sombre avec l'exposition de la scène)
    material.set_editor_property("translucency_lighting_mode", unreal.TranslucencyLightingMode.TLM_SURFACE)
    material.set_editor_property("two_sided", True)

    color = node(material, unreal.MaterialExpressionVectorParameter, -600, -200, parameter_name="Color", default_value=unreal.LinearColor(1.0, 0.75, 0.1, 1.0))
    glow = node(material, unreal.MaterialExpressionScalarParameter, -600, -80, parameter_name="Glow", default_value=0.3)
    emissive = node(material, unreal.MaterialExpressionMultiply, -300, -150)
    mel.connect_material_expressions(color, "", emissive, "A")
    mel.connect_material_expressions(glow, "", emissive, "B")
    fill = node(material, unreal.MaterialExpressionScalarParameter, -600, 0, parameter_name="FillOpacity", default_value=0.12)
    edge = node(material, unreal.MaterialExpressionScalarParameter, -600, 100, parameter_name="EdgeOpacity", default_value=0.6)
    radius = node(material, unreal.MaterialExpressionScalarParameter, -1200, 300, parameter_name="Radius", default_value=2500.0)
    edge_width = node(material, unreal.MaterialExpressionScalarParameter, -1200, 450, parameter_name="EdgeWidth", default_value=60.0)
    world = node(material, unreal.MaterialExpressionWorldPosition, -1600, 200)
    center = node(material, unreal.MaterialExpressionObjectPositionWS, -1600, 320)

    # distance au centre (cm) -> bord : saturate((Distance - (Radius - EdgeWidth)) / EdgeWidth)
    distance = node(material, unreal.MaterialExpressionDistance, -1300, 200)
    mel.connect_material_expressions(world, "", distance, "A")
    mel.connect_material_expressions(center, "", distance, "B")
    inner = node(material, unreal.MaterialExpressionSubtract, -1000, 350)
    mel.connect_material_expressions(radius, "", inner, "A")
    mel.connect_material_expressions(edge_width, "", inner, "B")
    from_inner = node(material, unreal.MaterialExpressionSubtract, -800, 250)
    mel.connect_material_expressions(distance, "", from_inner, "A")
    mel.connect_material_expressions(inner, "", from_inner, "B")
    ratio = node(material, unreal.MaterialExpressionDivide, -600, 300)
    mel.connect_material_expressions(from_inner, "", ratio, "A")
    mel.connect_material_expressions(edge_width, "", ratio, "B")
    edge_mask = node(material, unreal.MaterialExpressionSaturate, -400, 300)
    mel.connect_material_expressions(ratio, "", edge_mask, "")

    edge_part = node(material, unreal.MaterialExpressionMultiply, -250, 150)
    mel.connect_material_expressions(edge, "", edge_part, "A")
    mel.connect_material_expressions(edge_mask, "", edge_part, "B")
    opacity = node(material, unreal.MaterialExpressionAdd, -100, 50)
    mel.connect_material_expressions(fill, "", opacity, "A")
    mel.connect_material_expressions(edge_part, "", opacity, "B")

    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(emissive, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    # vérification : chaque entrée des nœuds de calcul est branchée
    for expression in mel.get_material_expressions(material):
        missing = [i for i in mel.get_inputs_for_material_expression(material, expression) if i is None]
        if missing:
            raise RuntimeError(f"{expression.get_name()} : entrée non branchée")
    mel.recompile_material(material)
    assets.save_asset(MATERIAL_PATH, only_if_is_dirty=False)


try:
    if not assets.does_asset_exist(MATERIAL_PATH):
        build_material(unreal.AssetToolsHelpers.get_asset_tools().create_asset(MATERIAL_NAME, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew()))
        log(f"créé : {MATERIAL_PATH}")
    elif REBUILD_MATERIAL:
        # supprimé puis recréé (vider le graphe sur place laisse des nœuds)
        brooker_cdo = unreal.get_default_object(assets.load_blueprint_class(BROOKER_BP))
        brooker_cdo.set_editor_property("zone_material", None)
        assets.save_asset(BROOKER_BP, only_if_is_dirty=False)
        assets.delete_asset(MATERIAL_PATH)
        build_material(unreal.AssetToolsHelpers.get_asset_tools().create_asset(MATERIAL_NAME, MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew()))
        log(f"recréé : {MATERIAL_PATH}")
    else:
        log(f"existe déjà : {MATERIAL_PATH}")

    brooker_cdo = unreal.get_default_object(assets.load_blueprint_class(BROOKER_BP))
    brooker_cdo.set_editor_property("zone_material", assets.load_asset(MATERIAL_PATH))
    assets.save_asset(BROOKER_BP, only_if_is_dirty=False)

    # coffre de test : même valeur que son Blueprint
    chest_value = unreal.get_default_object(assets.load_blueprint_class(CHEST_BP)).get_editor_property("doubloon_value")
    levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    if not levels.load_level(LEVEL_PATH):
        raise RuntimeError(f"impossible d'ouvrir {LEVEL_PATH}")
    changed = False
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.YSInteractiveObject) and actor.get_editor_property("doubloon_value") != chest_value:
            log(f"{actor.get_actor_label()} : {actor.get_editor_property('doubloon_value')} -> {chest_value} doublons")
            actor.set_editor_property("doubloon_value", chest_value)
            changed = True
    # Brookers placés : même matériau de zone que le Blueprint (une référence vidée reste sinon en override)
    zone_material = assets.load_asset(MATERIAL_PATH)
    for actor in actors.get_all_level_actors():
        if isinstance(actor, unreal.YSBrookerNPC) and actor.get_editor_property("zone_material") != zone_material:
            log(f"{actor.get_actor_label()} : ZoneMaterial {actor.get_editor_property('zone_material')} -> {MATERIAL_NAME}")
            actor.set_editor_property("zone_material", zone_material)
            changed = True
    if changed:
        levels.save_current_level()

    check = unreal.get_default_object(assets.load_blueprint_class(BROOKER_BP))
    log(f"relu : ZoneMaterial du Brooker = {check.get_editor_property('zone_material').get_name()}, zone {check.get_editor_property('accept_range')} cm")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_brooker_zone.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
