"""
Personnalisation à l'écran de sélection (BP_TestGameMode, catégorie Character Select) :
  - couleurs Rouge / Bleu / Vert / Violet / Rose / Jaune / Cyan / Noir : matériaux m_PH_monochrome_Red / Blue / Green /
    Purple / Pink / Yellow / Cyan / Black
    (appliqués au composant « Head Feature » du personnage et à ses sous-composants)
  - visages : textures T_MoussFace_1 à 8 (decal du visage)
Couleurs : écrites seulement si la liste est vide ; visages : ceux qui manquent sont ajoutés à la fin
(les réglages de Paul sont conservés).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_customization.py"
Rapport : Saved/Logs/setup_customization.txt
"""
import os
import unreal

GAME_MODE_BP = "/Game/Core/GameModes/BP_TestGameMode"
MATERIAL_DIR = "/Game/Art/Materials/PH_materials"
FACE_DIR = "/Game/Art/Textures/CharacterFaces"
COLORS = [
    # (nom, couleur du bouton, matériau)
    ("Rouge", unreal.LinearColor(0.85, 0.08, 0.08, 1.0), "m_PH_monochrome_Red"),
    ("Bleu", unreal.LinearColor(0.1, 0.3, 0.95, 1.0), "m_PH_monochrome_Blue"),
    ("Vert", unreal.LinearColor(0.1, 0.75, 0.2, 1.0), "m_PH_monochrome_Green"),
    ("Violet", unreal.LinearColor(0.55, 0.15, 0.85, 1.0), "m_PH_monochrome_Purple"),
    ("Rose", unreal.LinearColor(1.0, 0.4, 0.7, 1.0), "m_PH_monochrome_Pink"),
    ("Jaune", unreal.LinearColor(0.95, 0.85, 0.1, 1.0), "m_PH_monochrome_Yellow"),
    ("Cyan", unreal.LinearColor(0.1, 0.85, 0.9, 1.0), "m_PH_monochrome_Cyan"),
    ("Noir", unreal.LinearColor(0.04, 0.04, 0.04, 1.0), "m_PH_monochrome_Black"),
]
FACES = ["T_MoussFace_1", "T_MoussFace_2", "T_MoussFace_3", "T_MoussFace_4", "T_MoussFace_5", "T_MoussFace_6", "T_MoussFace_7", "T_MoussFace_8"]

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_customization] {msg}")
    report.append(msg)


try:
    colors = []
    for name, button_color, material_name in COLORS:
        material = assets.load_asset(f"{MATERIAL_DIR}/{material_name}")
        if material is None:
            raise RuntimeError(f"matériau introuvable : {material_name}")
        color = unreal.YSCustomizationColor()
        color.set_editor_property("name", name)
        color.set_editor_property("button_color", button_color)
        color.set_editor_property("material", material)
        colors.append(color)

    gm = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP))
    if len(gm.get_editor_property("customization_colors")) == 0:
        gm.set_editor_property("customization_colors", colors)
        log("couleurs écrites")

    # visages : les existants gardent leur ordre, ceux qui manquent sont ajoutés à la fin
    faces = [f for f in gm.get_editor_property("customization_faces") if f]
    for face_name in FACES:
        face = assets.load_asset(f"{FACE_DIR}/{face_name}")
        if face is None:
            raise RuntimeError(f"texture introuvable : {face_name}")
        if face not in faces:
            faces.append(face)
            log(f"visage ajouté : {face_name}")
    gm.set_editor_property("customization_faces", faces)
    assets.save_asset(GAME_MODE_BP, only_if_is_dirty=False)

    check = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP))
    log(f"couleurs relues : {[(str(c.get_editor_property('name')), c.get_editor_property('material').get_name()) for c in check.get_editor_property('customization_colors')]}")
    log(f"visages relus : {[f.get_name() for f in check.get_editor_property('customization_faces')]}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_customization.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
