"""
Vérification (lecture seule) : redirecteurs restants et assets de /Game qui dépendent d'un package /Game absent.

Lancement :
  UnrealEditor-Cmd.exe YohohoSurvivor.uproject -run=pythonscript -script="Tools/EditorScripts/check_broken_references.py" -unattended
Rapport : Saved/Logs/check_broken_references.txt
"""
import os
import unreal

registry = unreal.AssetRegistryHelpers.get_asset_registry()
report = []


def log(msg):
    unreal.log(f"[check_broken_references] {msg}")
    report.append(msg)


try:
    registry.search_all_assets(True)
    all_assets = registry.get_assets_by_path("/Game", recursive=True, include_only_on_disk_assets=True)
    packages = {str(a.package_name) for a in all_assets}
    redirectors = sorted({str(a.package_name) for a in all_assets if str(a.asset_class_path.asset_name) == "ObjectRedirector"})
    log(f"redirecteurs : {redirectors}")
    options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True,
                                                    include_searchable_names=False, include_soft_management_references=False,
                                                    include_hard_management_references=False)
    broken = 0
    for package in sorted(packages):
        for dep in registry.get_dependencies(package, options) or []:
            dep = str(dep)
            if dep.startswith("/Game/") and dep not in packages:
                log(f"CASSÉ : {package} -> {dep}")
                broken += 1
    log(f"{broken} référence(s) cassée(s)")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "check_broken_references.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
