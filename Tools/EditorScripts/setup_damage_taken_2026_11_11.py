"""
Retours de Paul du 2026-11-11 (dégâts subis par les NME, Kraken, réparation) :
  - Player Dmg Multiplier / Boat Dmg Multiplier de chaque NME (dégâts d'un personnage / d'un poste de combat du bateau) :
      Kraken (4 morceaux) 0.65 / 1 ; élites, boss, capitaine de Galion 1 / 0.5 ; Nemesis 1 / 0.35 ;
      coque du Jolly Ship (bateau de NME, choix de Claude) 0.5 / 1 ; NME de base 1 / 0.8
    (postes des bateaux de NME 0.5 / 1 : valeurs par défaut du C++ d'AYSShipStation, posées aussi ici sur les postes)
  - Kraken : tirs de la tentacule à distance et de la tête finale à 1000 ; attente avant l'attaque 1..3 s (distance), 1 s (mêlée)
  - réparation : 5 % des PV max par seconde (postes : Espace ; core : E, par joueur)
Relançable (pose les valeurs, ne crée rien).

Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/setup_damage_taken_2026_11_11.py"
Rapport : Saved/Logs/setup_damage_taken_2026_11_11.txt
"""
import os
import unreal

FOE_DIR = "/Game/Placeable/Character/Foe"
SHIP_DIRS = ("/Game/Placeable/Ship", "/Game/Core/Ship")

KRAKEN = (0.65, 1.0)
BOSS = (1.0, 0.5)
NEMESIS = (1.0, 0.35)
FOE_SHIP = (0.5, 1.0)
BASE = (1.0, 0.8)

# par nom de Blueprint (le reste : élite -> BOSS, sinon BASE)
BY_NAME = {
    "FoeCharacter_Skull_Nemesis": NEMESIS,
    "FoeCharacter_Galion_Captain": BOSS,
    "FoeCharacter_JollyHull": FOE_SHIP,
}

KRAKEN_VALUES = {
    "FoeCharacter_KrakenTentacle_Range": {"InkDamage": 1000.0, "AttackDelayRange": unreal.Vector2D(1.0, 3.0)},
    "FoeCharacter_KrakenTentacle_Melee": {"AttackDelayRange": unreal.Vector2D(1.0, 1.0)},
    "FoeCharacter_KrakenHead_Final": {"ShotDamage": 1000.0},
}

REPAIR_PERCENT = 5.0

assets = unreal.EditorAssetLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
report = []


def log(msg):
    unreal.log(f"[setup_damage_taken] {msg}")
    report.append(str(msg))


def blueprints(path):
    for data in registry.get_assets_by_path(path, recursive=True):
        if str(data.asset_class_path.asset_name) != "Blueprint":
            continue
        package = str(data.package_name)
        cls = assets.load_blueprint_class(package)
        if cls:
            yield str(data.asset_name), package, cls, unreal.get_default_object(cls)


def save(package):
    if assets.save_asset(package, only_if_is_dirty=False):
        log(f"  sauvegardé : {package}")
    else:
        log(f"  ÉCHEC de la sauvegarde : {package}")


# ---------------------------------------------------------------- NME
for name, package, cls, cdo in blueprints(FOE_DIR):
    if not isinstance(cdo, unreal.YSFoeCharacter):
        continue
    if isinstance(cdo, unreal.YSKrakenPart):
        values = KRAKEN
    elif name in BY_NAME:
        values = BY_NAME[name]
    elif cdo.get_editor_property("bIsElite"):
        values = BOSS
    else:
        values = BASE
    cdo.set_editor_property("PlayerDmgMultiplier", values[0])
    cdo.set_editor_property("BoatDmgMultiplier", values[1])
    extra = KRAKEN_VALUES.get(name, {})
    for prop, value in extra.items():
        cdo.set_editor_property(prop, value)
    log(f"{name} : Player Dmg x{cdo.get_editor_property('PlayerDmgMultiplier'):.2f}, Boat Dmg x{cdo.get_editor_property('BoatDmgMultiplier'):.2f}"
        + "".join(f", {prop} {cdo.get_editor_property(prop)}" for prop in extra))
    if isinstance(cdo, unreal.YSKrakenPart):
        log(f"  Kraken Boss Armor {cdo.get_editor_property('bKrakenBossArmor')}")
    save(package)

# ---------------------------------------------------------------- postes et bateaux
for ship_dir in SHIP_DIRS:
    for name, package, cls, cdo in blueprints(ship_dir):
        changed = False
        if isinstance(cdo, unreal.YSShipStation):
            cdo.set_editor_property("RepairPercentPerSecond", REPAIR_PERCENT)
            cdo.set_editor_property("PlayerDmgMultiplier", FOE_SHIP[0])
            cdo.set_editor_property("BoatDmgMultiplier", FOE_SHIP[1])
            log(f"{name} (poste) : réparation {cdo.get_editor_property('RepairPercentPerSecond')} %/s, sur un bateau de NME Player Dmg x{FOE_SHIP[0]}, Boat Dmg x{FOE_SHIP[1]}")
            changed = True
        elif isinstance(cdo, unreal.YSPirateShip):
            cdo.set_editor_property("CoreRepairPercentPerSecond", REPAIR_PERCENT)
            log(f"{name} (bateau) : réparation du core {cdo.get_editor_property('CoreRepairPercentPerSecond')} %/s par joueur")
            if isinstance(cdo, unreal.YSFoeGalion):
                cdo.set_editor_property("CaptainPlayerDmgMultiplier", BOSS[0])
                cdo.set_editor_property("CaptainBoatDmgMultiplier", BOSS[1])
                log(f"  capitaines : Player Dmg x{BOSS[0]}, Boat Dmg x{BOSS[1]}")
            changed = True
        if changed:
            save(package)

out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_damage_taken_2026_11_11.txt")
with open(out, "w", encoding="utf-8") as f:
    f.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
