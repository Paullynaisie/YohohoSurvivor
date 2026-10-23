"""
Nettoyage après un rangement d'assets : chaque map / asset qui passe encore par un redirecteur est réenregistré
(il pointe alors directement vers l'asset déplacé), puis les redirecteurs devenus inutiles sont supprimés.
Équivalent de « Update Redirector References » (non exposé en Python en UE 5.8).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/fix_redirectors.py"
Rapport : Saved/Logs/fix_redirectors.txt
"""
import os
import unreal

assets = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
report = []


def log(msg):
    unreal.log(f"[fix_redirectors] {msg}")
    report.append(msg)


try:
    registry.search_all_assets(True)
    all_assets = registry.get_assets_by_path("/Game", recursive=True, include_only_on_disk_assets=True)
    redirectors = sorted({str(a.package_name) for a in all_assets if str(a.asset_class_path.asset_name) == "ObjectRedirector"})
    log(f"redirecteurs : {redirectors}")

    options = unreal.AssetRegistryDependencyOptions(include_soft_package_references=True, include_hard_package_references=True,
                                                    include_searchable_names=False, include_soft_management_references=False,
                                                    include_hard_management_references=False)
    referencers = set()
    for redirector in redirectors:
        for ref in registry.get_referencers(redirector, options) or []:
            if str(ref) not in redirectors:
                referencers.add(str(ref))
    log(f"à réenregistrer : {sorted(referencers)}")

    maps = [p for p in referencers if any(str(a.asset_class_path.asset_name) == "World" for a in registry.get_assets_by_package_name(p))]
    for path in sorted(referencers):
        if path in maps:
            world = unreal.EditorLoadingAndSavingUtils.load_map(path)
            unreal.EditorLoadingAndSavingUtils.save_map(world, path)
            log(f"map réenregistrée : {path}")
        else:
            assets.load_asset(path)
            assets.save_asset(path, only_if_is_dirty=False)
            log(f"asset réenregistré : {path}")
    # quitter la map chargée pour pouvoir supprimer les redirecteurs qu'elle tenait
    unreal.EditorLoadingAndSavingUtils.new_blank_map(False)

    registry.search_all_assets(True)
    for redirector in redirectors:
        refs = [str(r) for r in (registry.get_referencers(redirector, options) or [])]
        if refs:
            log(f"GARDÉ (encore référencé par {refs}) : {redirector}")
            continue
        ok = assets.delete_asset(redirector)
        log(f"{'supprimé' if ok else 'ÉCHEC suppression'} : {redirector}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "fix_redirectors.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
