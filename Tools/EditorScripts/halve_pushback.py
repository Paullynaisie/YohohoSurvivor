"""
PUSHBACK DIVISÉ PAR 2 (demande de Paul, 2026-10-24, avec l'ajout du pushback vertical) — script à NE LANCER QU'UNE FOIS
(déjà lancé : le relancer diviserait encore par 2). Lancé AVANT le changement des valeurs par défaut C++ (200 / 1000 -> 100 / 500).
  - personnages joueurs et NME (/Game/Placeable/Character, récursif) : Pushback Min Force / Pushback Max Force
  - sets d'arme (/Game/Core/WeaponSets) : Pushback Min Force / Pushback Max Force
  - Mine repulsante (DA_Upgrade_Mine, DA_Upgrade_MineThrower) : Bonus Silver / Gold Value (pushback ajouté)
Les ratios de dégâts (Pushback Min / Max Damage Ratio) ne changent pas.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/halve_pushback.py"
Rapport : Saved/Logs/halve_pushback.txt
"""
import os
import unreal

assets = unreal.EditorAssetLibrary
report = []
FORCES = ("pushback_min_force", "pushback_max_force")
MINE_UPGRADES = ["/Game/Core/Upgrades/DA_Upgrade_Mine", "/Game/Core/Upgrades/DA_Upgrade_MineThrower"]


def log(msg):
    unreal.log(f"[halve_pushback] {msg}")
    report.append(msg)


def halve(obj, props, label, path):
    values = []
    for prop in props:
        old = obj.get_editor_property(prop)
        obj.set_editor_property(prop, old * 0.5)
        values.append(f"{prop} {old:g} -> {old * 0.5:g}")
    assets.save_asset(path, only_if_is_dirty=False)
    log(f"{label} : " + ", ".join(values))


try:
    for path in assets.list_assets("/Game/Placeable/Character", recursive=True, include_folder=False):
        path = path.split(".")[0]
        asset = assets.load_asset(path)
        if not isinstance(asset, unreal.Blueprint):
            continue
        generated = assets.load_blueprint_class(path)
        default = unreal.get_default_object(generated) if generated else None
        if isinstance(default, (unreal.YSPlayerCharacter, unreal.YSFoeCharacter)):
            halve(default, FORCES, path.split("/")[-1], path)

    for path in assets.list_assets("/Game/Core/WeaponSets", recursive=True, include_folder=False):
        path = path.split(".")[0]
        weapon_set = assets.load_asset(path)
        if isinstance(weapon_set, unreal.YSWeaponSet):
            halve(weapon_set, FORCES, path.split("/")[-1], path)

    for path in MINE_UPGRADES:
        halve(assets.load_asset(path), ("bonus_silver_value", "bonus_gold_value"), path.split("/")[-1], path)
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "halve_pushback.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
