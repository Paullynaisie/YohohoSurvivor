"""
Relief des îles divisé par deux (demande de Paul du 2026-11-12) : dans chaque DataAsset d'île (UYSIslandData, tous dossiers),
Generation.PlateauHeight et Generation.PlateauNoise x0.5. Les valeurs par défaut du C++ ont été divisées de même (175 / 15, 12.5).
Rapporte aussi les paramètres scalaires du matériau du sol (altitude de la jungle à vérifier).

Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/halve_island_relief_2026_11_12.py"
Rapport : Saved/Logs/halve_island_relief_2026_11_12.txt
"""
import os
import unreal

FACTOR = 0.5
report = []


def log(msg):
    unreal.log(f"[halve_island_relief] {msg}")
    report.append(msg)


registry = unreal.AssetRegistryHelpers.get_asset_registry()
class_path = unreal.TopLevelAssetPath("/Script/YohohoSurvivor", "YSIslandData")
found = registry.get_assets_by_class(class_path, True)
log(f"{len(found)} DataAsset(s) d'île")
materials = set()
for data in found:
    path = str(data.package_name)
    asset = unreal.EditorAssetLibrary.load_asset(path)
    rules = asset.get_editor_property("generation")
    height = rules.get_editor_property("plateau_height")
    noise = rules.get_editor_property("plateau_noise")
    new_noise = unreal.Vector2D(noise.x * FACTOR, noise.y * FACTOR)
    rules.set_editor_property("plateau_height", height * FACTOR)
    rules.set_editor_property("plateau_noise", new_noise)
    asset.set_editor_property("generation", rules)
    saved = unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)
    check = unreal.EditorAssetLibrary.load_asset(path).get_editor_property("generation")
    log(f"{path} : plateau {height:.1f} -> {check.get_editor_property('plateau_height'):.1f}, ondulation ({noise.x:.1f}, {noise.y:.1f}) -> "
        f"({check.get_editor_property('plateau_noise').x:.1f}, {check.get_editor_property('plateau_noise').y:.1f}), "
        f"falaise {rules.get_editor_property('cliff_width'):.0f}, fond {rules.get_editor_property('sea_floor'):.0f}, sauvé {saved}")
    material = rules.get_editor_property("ground_material")
    if material:
        materials.add(material.get_path_name())

mel = unreal.MaterialEditingLibrary
for material_path in sorted(materials):
    material = unreal.load_asset(material_path)
    values = []
    try:
        for name in mel.get_scalar_parameter_names(material):
            if isinstance(material, unreal.MaterialInstance):
                value = mel.get_material_instance_scalar_parameter_value(material, name)
            else:
                value = mel.get_material_default_scalar_parameter_value(material, name)
            values.append(f"{name}={value:g}")
    except Exception as error:
        values.append(f"(lecture impossible : {error})")
    log(f"matériau du sol {material_path} : {', '.join(values) if values else 'aucun paramètre scalaire'}")

path = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "halve_island_relief_2026_11_12.txt")
with open(path, "w", encoding="utf-8") as file:
    file.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
