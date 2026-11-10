"""
Lecture seule (2026-11-10) : lignes de DT_CustomizationColors, matériaux émissifs m_PH_emissive_*, armes du joueur avec un Auto swing
(et son réglage Slash Trail). Rapport : Saved/Logs/inspect_player_colors.txt
  UnrealEditor-Cmd.exe YohohoSurvivor.uproject -run=pythonscript -script="<projet>/Tools/EditorScripts/inspect_player_colors.py" -unattended
"""
import os
import unreal

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    report.append(str(msg))


try:
    table = assets.load_asset("/Game/Core/Customization/DT_CustomizationColors")
    names = unreal.DataTableFunctionLibrary.get_data_table_row_names(table)
    log(f"DT_CustomizationColors : {[str(n) for n in names]}")
    for column in ("Name", "ButtonColor", "Material"):
        log(f"  {column} : {unreal.DataTableFunctionLibrary.get_data_table_column_as_string(table, column)}")

    for path in assets.list_assets("/Game/Art/Materials/PH_materials", recursive=False):
        if "emissive" not in path:
            continue
        mat = assets.load_asset(path)
        info = f"{mat.get_name()} : {mat.get_class().get_name()}"
        if isinstance(mat, unreal.MaterialInstance):
            parent = mat.get_editor_property("parent")
            info += f", parent {parent.get_path_name() if parent else None}"
            for p in mat.get_editor_property("vector_parameter_values"):
                info += f", {p.parameter_info.name}={p.parameter_value}"
            for p in mat.get_editor_property("scalar_parameter_values"):
                info += f", {p.parameter_info.name}={p.parameter_value}"
        else:
            info += f", vecteurs {unreal.MaterialEditingLibrary.get_vector_parameter_names(mat)}, scalaires {unreal.MaterialEditingLibrary.get_scalar_parameter_names(mat)}"
            info += f", {unreal.MaterialEditingLibrary.get_num_material_expressions(mat)} noeuds, couleurs {[(round(c.r,3), round(c.g,3), round(c.b,3)) for c in unreal.YSEditorScriptLibrary.get_material_color_constants(mat)]}"
        log(info)

    for root in ("/Game/Placeable/Weapons", "/Game/Placeable/Character/Summon"):
        for path in assets.list_assets(root, recursive=True):
            data = unreal.EditorAssetLibrary.find_asset_data(path)
            if str(data.asset_class_path.asset_name) != "Blueprint":
                continue
            bp = assets.load_asset(path)
            gen = bp.generated_class() if bp else None
            if not gen:
                continue
            for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
                obj = lib.get_object(lib.get_data(handle))
                if isinstance(obj, unreal.YSAutoSwingAbility):
                    tags = obj.get_editor_property("HitTags")
                    log(f"SWING {path} . {obj.get_name()} ({obj.get_class().get_name()}) trail={obj.get_editor_property('bSlashTrail')} "
                        f"mat={obj.get_editor_property('SlashMaterial')} tags={tags}")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "inspect_player_colors.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
