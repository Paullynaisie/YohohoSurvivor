"""
Passage du joueur en DA « primitives » :
  - /Game/Art/Materials/Decals/M_FaceDecal : matériau de decal (paramètre texture FaceTexture, alpha = opacité)
  - PlayerCharacter_mouss : FaceDecal utilise M_FaceDecal, mesh squelettique vidé,
    pistolet replacé sur le flanc gauche (ses anciens réglages étaient relatifs à la main du mannequin)

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_primitive_character.py"
Rapport : Saved/Logs/setup_primitive_character.txt
"""
import os
import unreal

DECAL_DIR, DECAL_NAME = "/Game/Art/Materials/Decals", "M_FaceDecal"
PLACEHOLDER_TEXTURE = "/Engine/EngineResources/DefaultTexture"
PLAYER_BP = "/Game/Placeable/Character/PlayerCharacter_mouss"

assets = unreal.EditorAssetLibrary
mel = unreal.MaterialEditingLibrary
report = []


def log(msg):
    unreal.log(f"[setup_primitive_character] {msg}")
    report.append(msg)


def create_face_decal_material():
    path = f"{DECAL_DIR}/{DECAL_NAME}"
    if assets.does_asset_exist(path):
        log(f"existe déjà : {path}")
        return assets.load_asset(path)

    material = unreal.AssetToolsHelpers.get_asset_tools().create_asset(DECAL_NAME, DECAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("material_domain", unreal.MaterialDomain.MD_DEFERRED_DECAL)
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)

    texture = mel.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -400, 0)
    texture.set_editor_property("parameter_name", "FaceTexture")
    placeholder = assets.load_asset(PLACEHOLDER_TEXTURE)
    if placeholder:
        texture.set_editor_property("texture", placeholder)
    mel.connect_material_property(texture, "RGB", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(texture, "A", unreal.MaterialProperty.MP_OPACITY)

    mel.recompile_material(material)
    assets.save_asset(path, only_if_is_dirty=False)
    log(f"créé : {path} (texture par défaut : {placeholder.get_name() if placeholder else 'aucune'})")
    return material


def setup_player(decal_material):
    bp = assets.load_asset(PLAYER_BP)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(assets.load_blueprint_class(PLAYER_BP))

    cdo.get_editor_property("FaceDecal").set_editor_property("decal_material", decal_material)

    mesh = cdo.get_editor_property("Mesh")
    mesh.set_editor_property("skeletal_mesh_asset", None)

    pistol = cdo.get_editor_property("PistolMesh")
    pistol.set_editor_property("relative_location", unreal.Vector(20, -50, 0))
    pistol.set_editor_property("relative_rotation", unreal.Rotator(0, 0, 0))

    assets.save_asset(PLAYER_BP, only_if_is_dirty=False)
    log("PlayerCharacter_mouss : decal du visage, mesh squelettique vide, pistolet sur le flanc gauche")


try:
    setup_player(create_face_decal_material())
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_primitive_character.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
