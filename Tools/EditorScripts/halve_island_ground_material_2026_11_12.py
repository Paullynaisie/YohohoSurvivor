"""
Suite de halve_island_relief_2026_11_12.py : le matériau du sol des îles colore selon l'altitude MONDE (sable sous SandTop, jungle
au-dessus, transition sur BlendHeight). Le plateau passant de 350 à 175, SandTop 265 -> 132.5 et BlendHeight 25 -> 12.5 (même rapport).
Ne pas relancer : il divise à chaque passage.

Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/halve_island_ground_material_2026_11_12.py"
Rapport : Saved/Logs/halve_island_ground_material_2026_11_12.txt
"""
import os
import unreal

MATERIAL = "/Game/Art/Materials/M_IslandGround_Cliff"
FACTOR = 0.5
report = []


def log(msg):
    unreal.log(f"[halve_island_ground_material] {msg}")
    report.append(msg)


mel = unreal.MaterialEditingLibrary
material = unreal.EditorAssetLibrary.load_asset(MATERIAL)
for name in ("SandTop", "BlendHeight"):
    before = mel.get_material_default_scalar_parameter_value(material, name)
    ok = unreal.YSEditorScriptLibrary.set_material_scalar_parameter_default(material, name, before * FACTOR)
    after = mel.get_material_default_scalar_parameter_value(material, name)
    log(f"{name} : {before:g} -> {after:g} ({'ok' if ok else 'NŒUD INTROUVABLE'})")
mel.recompile_material(material)
log(f"sauvé {unreal.EditorAssetLibrary.save_asset(MATERIAL, only_if_is_dirty=False)}")

path = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "halve_island_ground_material_2026_11_12.txt")
with open(path, "w", encoding="utf-8") as file:
    file.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
