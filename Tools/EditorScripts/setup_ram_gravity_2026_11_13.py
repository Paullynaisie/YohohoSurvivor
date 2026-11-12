"""
Retours de Paul du 2026-11-13 :
  - « À l'abordage » (éperonnage, clic gauche à la barre) : vitesse x1.5 (au lieu de x1.2 / 1.25), 1000 dégâts, sur tous les bateaux des joueurs
    (Blueprints d'AYSPirateShip de /Game/Placeable/Ship, sauf les bateaux de NME)
  - gravité des personnages x3 : propriété C++ 3C|Movement « Character Gravity Scale » (3 par défaut) ; le script LIT seulement la gravité
    actuelle des Blueprints de personnages (CMC + nouvelle propriété) pour vérifier qu'aucun n'avait une valeur à part
Relançable (valeurs fixes).

Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/setup_ram_gravity_2026_11_13.py"
Rapport : Saved/Logs/setup_ram_gravity_2026_11_13.txt
"""
import os
import unreal

SHIP_DIR = "/Game/Placeable/Ship"
PLAYER_DIR = "/Game/Placeable/Character/Player"
RAM_SPEED = 1.5
RAM_DAMAGE = 1000.0

assets = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
report = []


def log(msg):
    unreal.log(f"[setup_ram_gravity] {msg}")
    report.append(str(msg))


def blueprints(path):
    for data in registry.get_assets_by_path(path, recursive=True):
        if str(data.asset_class_path.asset_name) == "Blueprint":
            yield str(data.package_name)


def main():
    pirate = unreal.YSPirateShip.static_class()
    foe_classes = [unreal.YSFoeGalion.static_class(), unreal.YSFoeJollyShip.static_class()]
    log("== Bateaux : « À l'abordage »")
    for path in sorted(blueprints(SHIP_DIR)):
        cls = assets.load_blueprint_class(path)
        if not cls or not unreal.MathLibrary.class_is_child_of(cls, pirate):
            continue
        if any(unreal.MathLibrary.class_is_child_of(cls, foe) for foe in foe_classes):
            log(f"  {path} : bateau de NME, ignoré")
            continue
        cdo = unreal.get_default_object(cls)
        before = (cdo.get_editor_property("ram_speed_multiplier"), cdo.get_editor_property("ram_damage"))
        cdo.set_editor_property("ram_speed_multiplier", RAM_SPEED)
        cdo.set_editor_property("ram_damage", RAM_DAMAGE)
        unreal.BlueprintEditorLibrary.compile_blueprint(assets.load_asset(path))
        saved = assets.save_asset(path, only_if_is_dirty=False)
        after = (cdo.get_editor_property("ram_speed_multiplier"), cdo.get_editor_property("ram_damage"))
        log(f"  {path} : vitesse x{before[0]:.2f} -> x{after[0]:.2f}, dégâts {before[1]:.0f} -> {after[1]:.0f}{'' if saved else ' (ÉCHEC de la sauvegarde)'}")

    player = unreal.YSPlayerCharacter.static_class()
    log("== Personnages : gravité (lecture seule)")
    for path in sorted(blueprints("/Game/Placeable/Character")):
        cls = assets.load_blueprint_class(path)
        if not cls or not unreal.MathLibrary.class_is_child_of(cls, player):
            continue
        cdo = unreal.get_default_object(cls)
        try:
            cmc = cdo.get_editor_property("character_movement")
            cmc_gravity = f"{cmc.get_editor_property('gravity_scale'):.2f}"
        except Exception as error:  # noqa: BLE001
            cmc_gravity = f"? ({error})"
        log(f"  {path} : CMC Gravity Scale {cmc_gravity}, Character Gravity Scale {cdo.get_editor_property('character_gravity_scale'):.2f}")


try:
    main()
except Exception as error:  # noqa: BLE001
    log(f"ERREUR : {error}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_ram_gravity_2026_11_13.txt")
    with open(out, "w", encoding="utf-8") as file:
        file.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
