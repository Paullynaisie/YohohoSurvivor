"""
Vérification (lecture seule) : écrit toutes les propriétés lisibles des assets listés (composants des Blueprints compris)
dans un fichier texte, pour comparer deux versions d'assets (ex. avant / après les retouches de Paul).

  UnrealEditor-Cmd.exe YohohoSurvivor.uproject -run=pythonscript -script="Tools/EditorScripts/dump_asset_values.py" -unattended
Paramètres par variables d'environnement : YS_DUMP_ASSETS (chemins séparés par ;), YS_DUMP_OUT (fichier de sortie).
"""
import os
import unreal

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary

PATHS = [p for p in os.environ.get("YS_DUMP_ASSETS", "").split(";") if p]
OUT = os.environ.get("YS_DUMP_OUT", os.path.join(unreal.Paths.project_saved_dir(), "Logs", "dump_asset_values.txt"))
SKIP = {"class_", "outer", "world", "static_class"}
lines = []


def fmt(value):
    if isinstance(value, unreal.Object):
        return value.get_path_name()
    if isinstance(value, (list, tuple, unreal.Array)):
        return "[" + ", ".join(fmt(v) for v in value) + "]"
    if isinstance(value, unreal.Map):
        return "{" + ", ".join(f"{fmt(k)}: {fmt(v)}" for k, v in value.items()) + "}"
    return str(value)


def dump(prefix, obj):
    for name in sorted(dir(obj)):
        if name.startswith("_") or name in SKIP:
            continue
        try:
            value = obj.get_editor_property(name)
        except Exception:
            continue
        if callable(value):
            continue
        lines.append(f"{prefix}.{name} = {fmt(value)}")


for path in PATHS:
    asset = assets.load_asset(path)
    if isinstance(asset, unreal.Blueprint):
        dump(path + ":CDO", unreal.get_default_object(assets.load_blueprint_class(path)))
        for h in subsystem.k2_gather_subobject_data_for_blueprint(asset):
            data = lib.get_data(h)
            obj = lib.get_object(data)
            if obj is not None and not isinstance(obj, unreal.Actor):
                dump(f"{path}:{lib.get_variable_name(data)}", obj)
    elif asset is not None:
        dump(path, asset)
    else:
        lines.append(f"{path} : introuvable")

with open(OUT, "w", encoding="utf-8") as f:
    f.write("\n".join(lines) + "\n")
unreal.log(f"[dump_asset_values] {len(lines)} lignes -> {OUT}")
