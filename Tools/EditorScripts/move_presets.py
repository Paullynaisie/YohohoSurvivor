"""
Range les assets du template Top Down dans /Game/PresetProject en passant par les outils
de l'éditeur (références mises à jour, acteurs externes des maps déplacés), puis nettoie les redirecteurs.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/move_presets.py"
Rapport : Saved/Logs/move_presets.txt
"""
import os
import unreal

FOLDERS = ["Characters", "Cursor", "LevelPrototyping", "TopDown", "Variant_Strategy", "Variant_TwinStick"]
DEST_ROOT = "/Game/PresetProject"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
registry = unreal.AssetRegistryHelpers.get_asset_registry()
report = []


def log(msg):
    unreal.log(f"[move_presets] {msg}")
    report.append(msg)


def assets_in(folder):
    return registry.get_assets(unreal.ARFilter(package_paths=[folder], recursive_paths=True))


def is_redirector(asset_data):
    return str(asset_data.asset_class_path.asset_name) == "ObjectRedirector"


def find_redirectors(folder):
    return [a for a in assets_in(folder) if is_redirector(a)]


def referencers(package_name):
    opts = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True)
    return [str(r) for r in (registry.get_referencers(package_name, opts) or []) if str(r) != str(package_name)]


def move_all():
    renames = []
    for folder in FOLDERS:
        src = f"/Game/{folder}"
        if not assets.does_directory_exist(src):
            log(f"absent, ignoré : {src}")
            continue
        for asset_data in assets_in(src):
            if is_redirector(asset_data):
                continue
            new_dir = DEST_ROOT + str(asset_data.package_path)[len("/Game"):]
            renames.append(unreal.AssetRenameData(asset_data.get_asset(), new_dir, str(asset_data.asset_name)))
    log(f"{len(renames)} assets à déplacer")
    ok = tools.rename_assets(renames)
    log(f"rename_assets : {'OK' if ok else 'ÉCHEC'}")
    assets.save_directory(DEST_ROOT, only_if_is_dirty=True, recursive=True)

    # le renommage a déjà réécrit les références ; on supprime les redirecteurs que plus rien n'utilise
    for folder in FOLDERS:
        for redirector in find_redirectors(f"/Game/{folder}"):
            package = str(redirector.package_name)
            users = referencers(package)
            if users:
                log(f"redirecteur conservé {package}, encore utilisé par {users}")
            else:
                assets.delete_asset(package)
    for folder in FOLDERS:
        src = f"/Game/{folder}"
        if assets.does_directory_exist(src):
            remaining = assets.list_assets(src, recursive=True, include_folder=False)
            if remaining:
                log(f"reste dans {src} : {list(remaining)}")
            else:
                assets.delete_directory(src)
                log(f"supprimé : {src}")


try:
    registry.wait_for_completion()
    move_all()
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "move_presets.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
