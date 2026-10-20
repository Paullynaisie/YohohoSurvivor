"""
Personnalisation à l'écran de sélection : deux Data Tables que Paul remplit lui-même (une ligne = un bouton, dans l'ordre des lignes) :
  - /Game/Core/Customization/DT_CustomizationColors (FYSCustomizationColor : Name, Button Color, Material appliqué au
    composant « Head Feature » du personnage et à ses sous-composants)
  - /Game/Core/Customization/DT_CustomizationFaces (FYSCustomizationFace : Face = texture du decal de visage)
  - BP_TestGameMode (catégorie Character Select) : Customization Color Table / Customization Face Table
À la création, les tables reprennent les anciennes listes de BP_TestGameMode (Customization Colors / Faces, ensuite vidées),
sinon les valeurs par défaut ci-dessous. Une table qui a déjà des lignes n'est jamais modifiée (réglages de Paul conservés).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_customization.py"
Rapport : Saved/Logs/setup_customization.txt
"""
import os
import unreal

GAME_MODE_BP = "/Game/Core/GameModes/BP_TestGameMode"
TABLE_DIR = "/Game/Core/Customization"
COLOR_TABLE = "DT_CustomizationColors"
FACE_TABLE = "DT_CustomizationFaces"
MATERIAL_DIR = "/Game/Art/Materials/PH_materials"
FACE_DIR = "/Game/Art/Textures/CharacterFaces"
DEFAULT_COLORS = [
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
DEFAULT_FACES = [f"T_MoussFace_{i}" for i in range(1, 11)]

assets = unreal.EditorAssetLibrary
tables = unreal.DataTableFunctionLibrary
editor_lib = unreal.YSEditorScriptLibrary
report = []


def log(msg):
    unreal.log(f"[setup_customization] {msg}")
    report.append(msg)


def get_or_create_table(name, struct):
    path = f"{TABLE_DIR}/{name}"
    if assets.does_asset_exist(path):
        return assets.load_asset(path), False
    factory = unreal.DataTableFactory()
    factory.set_editor_property("struct", struct)
    table = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, TABLE_DIR, unreal.DataTable, factory)
    log(f"créée : {path}")
    return table, True


def row_names(table):
    return [str(n) for n in tables.get_data_table_row_names(table)]


try:
    gm = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP))
    color_table, _ = get_or_create_table(COLOR_TABLE, unreal.YSCustomizationColor.static_struct())
    face_table, _ = get_or_create_table(FACE_TABLE, unreal.YSCustomizationFace.static_struct())

    # couleurs : ancienne liste du GameMode, sinon valeurs par défaut
    if not row_names(color_table):
        colors = list(gm.get_editor_property("customization_colors"))
        if not colors:
            for name, button_color, material_name in DEFAULT_COLORS:
                color = unreal.YSCustomizationColor()
                color.set_editor_property("name", name)
                color.set_editor_property("button_color", button_color)
                color.set_editor_property("material", assets.load_asset(f"{MATERIAL_DIR}/{material_name}"))
                colors.append(color)
        for index, color in enumerate(colors):
            row = str(color.get_editor_property("name")) or f"Color_{index + 1:02d}"
            if not editor_lib.add_customization_color_row(color_table, row, color):
                raise RuntimeError(f"ligne de couleur {row} refusée")
        assets.save_asset(f"{TABLE_DIR}/{COLOR_TABLE}", only_if_is_dirty=False)
        log(f"{COLOR_TABLE} : {len(colors)} couleur(s) écrites")
    else:
        log(f"{COLOR_TABLE} : déjà remplie, inchangée")

    # visages : ancienne liste du GameMode, sinon T_MoussFace_1 à 10
    if not row_names(face_table):
        faces = [f for f in gm.get_editor_property("customization_faces") if f]
        if not faces:
            faces = [assets.load_asset(f"{FACE_DIR}/{name}") for name in DEFAULT_FACES]
        for face in faces:
            if not editor_lib.add_customization_face_row(face_table, face.get_name(), face):
                raise RuntimeError(f"ligne de visage {face.get_name()} refusée")
        assets.save_asset(f"{TABLE_DIR}/{FACE_TABLE}", only_if_is_dirty=False)
        log(f"{FACE_TABLE} : {len(faces)} visage(s) écrits")
    else:
        log(f"{FACE_TABLE} : déjà remplie, inchangée")

    # GameMode : les tables remplacent les listes (vidées)
    gm.set_editor_property("customization_color_table", color_table)
    gm.set_editor_property("customization_face_table", face_table)
    gm.set_editor_property("customization_colors", [])
    gm.set_editor_property("customization_faces", [])
    assets.save_asset(GAME_MODE_BP, only_if_is_dirty=False)

    check = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP))
    log(f"BP_TestGameMode : tables {check.get_editor_property('customization_color_table').get_name()} / "
        f"{check.get_editor_property('customization_face_table').get_name()}, listes {len(check.get_editor_property('customization_colors'))} / "
        f"{len(check.get_editor_property('customization_faces'))}")
    log(f"couleurs relues : {row_names(assets.load_asset(f'{TABLE_DIR}/{COLOR_TABLE}'))}")
    log(f"visages relus : {row_names(assets.load_asset(f'{TABLE_DIR}/{FACE_TABLE}'))}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_customization.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
