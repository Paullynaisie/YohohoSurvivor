"""
NME en DA « primitives » (FoeCharacter_Skull) :
  - mesh squelettique (mannequin) vidé ; le corps est le cylindre BodyMesh (C++, mêmes proportions que le joueur)
  - BodyMesh : m_PH_monochrome_Red
  - FaceDecal : M_FaceDecal_4

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_foe_primitive.py"
Rapport : Saved/Logs/setup_foe_primitive.txt
"""
import os
import unreal

FOE_BP = "/Game/Placeable/Character/FoeCharacter_Skull"
BODY_MATERIAL = "/Game/Art/Materials/PH_materials/m_PH_monochrome_Red"
FACE_DECAL = "/Game/Art/Materials/Decals/M_FaceDecal_4"

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_foe_primitive] {msg}")
    report.append(msg)


try:
    bp = assets.load_asset(FOE_BP)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo = unreal.get_default_object(assets.load_blueprint_class(FOE_BP))

    mesh = cdo.get_editor_property("Mesh")
    mesh.set_editor_property("skeletal_mesh_asset", None)
    mesh.set_editor_property("anim_class", None)

    body = cdo.get_editor_property("BodyMesh")
    body.set_material(0, assets.load_asset(BODY_MATERIAL))

    face = cdo.get_editor_property("FaceDecal")
    face.set_editor_property("decal_material", assets.load_asset(FACE_DECAL))
    face.set_editor_property("face_texture", None)

    assets.save_asset(FOE_BP, only_if_is_dirty=False)

    check = unreal.get_default_object(assets.load_blueprint_class(FOE_BP))
    log(f"BodyMesh : {check.get_editor_property('BodyMesh').get_material(0).get_name()}, "
        f"FaceDecal : {check.get_editor_property('FaceDecal').get_editor_property('decal_material').get_name()}, "
        f"mesh squelettique : {check.get_editor_property('Mesh').get_editor_property('skeletal_mesh_asset')}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_foe_primitive.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
