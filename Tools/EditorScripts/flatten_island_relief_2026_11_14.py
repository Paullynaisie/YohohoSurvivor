"""
Relief des îles encore aplati (demande de Paul du 2026-11-14 : « un peu moins de hauteur ») : x0.6 après la division par deux du
2026-11-12. Dans chaque DataAsset d'île (UYSIslandData, tous dossiers) : Generation.PlateauHeight 175 -> 105 et
Generation.PlateauNoise (15, 12.5) -> (9, 7.5) ; matériau du sol M_IslandGround_Cliff (altitude monde) à la même échelle :
SandTop 132.5 -> 79.5, BlendHeight 12.5 -> 7.5. Les valeurs par défaut du C++ ont été changées de même.
Relançable sans danger : une valeur qui n'est plus l'ancienne (déjà aplatie, ou réglée à la main) n'est pas touchée (rapportée).

Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/flatten_island_relief_2026_11_14.py"
Rapport : Saved/Logs/flatten_island_relief_2026_11_14.txt
"""
import os
import unreal

FACTOR = 0.6
OLD_HEIGHT = 175.0
OLD_NOISE = (15.0, 12.5)
MATERIAL = "/Game/Art/Materials/M_IslandGround_Cliff"
OLD_MATERIAL = {"SandTop": 132.5, "BlendHeight": 12.5}
report = []


def log(msg):
    unreal.log(f"[flatten_island_relief] {msg}")
    report.append(msg)


def near(a, b):
    return abs(a - b) < 0.01


registry = unreal.AssetRegistryHelpers.get_asset_registry()
class_path = unreal.TopLevelAssetPath("/Script/YohohoSurvivor", "YSIslandData")
found = registry.get_assets_by_class(class_path, True)
log(f"{len(found)} DataAsset(s) d'île")
for data in found:
    path = str(data.package_name)
    asset = unreal.EditorAssetLibrary.load_asset(path)
    rules = asset.get_editor_property("generation")
    height = rules.get_editor_property("plateau_height")
    noise = rules.get_editor_property("plateau_noise")
    changed = False
    if near(height, OLD_HEIGHT):
        rules.set_editor_property("plateau_height", height * FACTOR)
        changed = True
    else:
        log(f"{path} : plateau {height:.1f} != {OLD_HEIGHT} -> pas touché")
    if near(noise.x, OLD_NOISE[0]) and near(noise.y, OLD_NOISE[1]):
        rules.set_editor_property("plateau_noise", unreal.Vector2D(noise.x * FACTOR, noise.y * FACTOR))
        changed = True
    else:
        log(f"{path} : ondulation ({noise.x:.1f}, {noise.y:.1f}) != {OLD_NOISE} -> pas touchée")
    if not changed:
        continue
    asset.set_editor_property("generation", rules)
    saved = unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    check = unreal.EditorAssetLibrary.load_asset(path).get_editor_property("generation")
    log(f"{path} : plateau {height:.1f} -> {check.get_editor_property('plateau_height'):.1f}, ondulation ({noise.x:.1f}, {noise.y:.1f}) -> "
        f"({check.get_editor_property('plateau_noise').x:.1f}, {check.get_editor_property('plateau_noise').y:.1f}), sauvé {saved}")

mel = unreal.MaterialEditingLibrary
material = unreal.EditorAssetLibrary.load_asset(MATERIAL)
material_changed = False
for name, old in OLD_MATERIAL.items():
    before = mel.get_material_default_scalar_parameter_value(material, name)
    if not near(before, old):
        log(f"{name} : {before:g} != {old:g} -> pas touché")
        continue
    ok = unreal.YSEditorScriptLibrary.set_material_scalar_parameter_default(material, name, before * FACTOR)
    after = mel.get_material_default_scalar_parameter_value(material, name)
    material_changed = material_changed or ok
    log(f"{name} : {before:g} -> {after:g} ({'ok' if ok else 'NŒUD INTROUVABLE'})")
if material_changed:
    mel.recompile_material(material)
    log(f"matériau sauvé {unreal.EditorAssetLibrary.save_asset(MATERIAL, only_if_is_dirty=False)}")

path = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "flatten_island_relief_2026_11_14.txt")
with open(path, "w", encoding="utf-8") as file:
    file.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
