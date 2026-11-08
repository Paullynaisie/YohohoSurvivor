"""
Ajustements du 2026-11-08 (relançable : valeurs recalculées depuis les références, jamais multipliées deux fois) :
  - coffres (BP_InteractiveObject_Chest, hérité par le coffre au trésor et le coffre du boss) : flottent dans la mer
  - élites (Skull Elite, Skull Gun Elite, Skull Elite Test) : Boss armor (réduction des dégâts) sans bouclier ni rage
  - bateaux des joueurs (étapes du GameBoat après la chaloupe de départ, chaloupe de la chalouperie) : vitesses et accélérations x SHIP_SPEED
    (référence : BP_PirateShip pour les bateaux à pont, classe C++ AYSLongboat pour la chaloupe)
Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/setup_tweaks_2026_11_08.py"
Rapport : Saved/Logs/setup_tweaks_2026_11_08.txt
"""
import os
import unreal

CHEST = "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_Chest"
CHILD_CHESTS = ("/Game/Placeable/InteractiveObjects/BP_InteractiveObject_TreasureChest", "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_BossChest")
ELITES = ("/Game/Placeable/Character/Foe/Elites/FoeCharacter_Skull_Elite", "/Game/Placeable/Character/Foe/Elites/FoeCharacter_Skull_Gun_Elite",
          "/Game/Placeable/Character/Foe/Elites/FoeCharacter_Skull_Elite_Test")
BOSSES = ("/Game/Placeable/Character/Foe/Boss/FoeCharacter_Skull_Boss", "/Game/Placeable/Character/Foe/Boss/FoeCharacter_Skull_Gun_Boss")
SHIP_DIR = "/Game/Placeable/Ship"
PIRATE = f"{SHIP_DIR}/BP_PirateShip"
DECK_SHIPS = ("BP_GameBoat_Sloop", "BP_GameBoat_Slooplus", "BP_GameBoat_Brigantin", "BP_GameBoat_Galion")
LONGBOAT = f"{SHIP_DIR}/BP_Longboat"
SHIP_SPEED = 1.5
SPEED_PROPS = ("MaxForwardSpeed", "MaxReverseSpeed", "ForwardAcceleration", "ReverseAcceleration")

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_tweaks_2026_11_08] {msg}")
    report.append(msg)


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def save(path):
    unreal.BlueprintEditorLibrary.compile_blueprint(assets.load_asset(path))
    assets.save_asset(path, only_if_is_dirty=False)


def chests():
    cdo(CHEST).set_editor_property("bFloatsOnWater", True)
    save(CHEST)
    for path in CHILD_CHESTS:
        child = cdo(path)
        child.set_editor_property("bFloatsOnWater", True)
        save(path)
    # tous les coffres : 50 doublons et 1 plume de phoenix (demande de Paul)
    for path in (CHEST,) + CHILD_CHESTS:
        chest = cdo(path)
        chest.set_editor_property("DoubloonValue", 50)
        chest.set_editor_property("PhoenixFeatherValue", 1)
        save(path)
    # coffre lâché par chaque NME
    for data in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path("/Game/Placeable/Character/Foe", recursive=True):
        foe_class = assets.load_blueprint_class(str(data.package_name))
        loot = unreal.get_default_object(foe_class).get_editor_property("DeathLootClass") if foe_class else None
        if loot:
            log(f"{data.asset_name} lâche {loot.get_name()}")
    for path in (CHEST,) + CHILD_CHESTS:
        log(f"{path.rsplit('/', 1)[1]} : flotte = {cdo(path).get_editor_property('bFloatsOnWater')}, doublons {cdo(path).get_editor_property('DoubloonValue')}, plumes {cdo(path).get_editor_property('PhoenixFeatherValue')}")


def elites():
    for path in ELITES:
        if not assets.does_asset_exist(path):
            log(f"absent : {path}")
            continue
        foe = cdo(path)
        foe.set_editor_property("bBossArmor", True)
        foe.set_editor_property("bBossShield", False)
        save(path)
        foe = cdo(path)
        log(f"{path.rsplit('/', 1)[1]} : élite {foe.get_editor_property('bIsElite')}, Boss armor {foe.get_editor_property('bBossArmor')}, bouclier {foe.get_editor_property('bBossShield')}")
    for path in BOSSES:
        foe = cdo(path)
        log(f"{path.rsplit('/', 1)[1]} : Boss armor {foe.get_editor_property('bBossArmor')}, bouclier {foe.get_editor_property('bBossShield')}")


def ships():
    pirate = cdo(PIRATE)
    for name in DECK_SHIPS:
        path = f"{SHIP_DIR}/{name}"
        ship = cdo(path)
        for prop in SPEED_PROPS:
            ship.set_editor_property(prop, pirate.get_editor_property(prop) * SHIP_SPEED)
        save(path)
        ship = cdo(path)
        log(f"{name} : " + ", ".join(f"{p} {ship.get_editor_property(p):.0f}" for p in SPEED_PROPS))
    native = unreal.get_default_object(unreal.YSLongboat)
    boat = cdo(LONGBOAT)
    for prop in SPEED_PROPS:
        boat.set_editor_property(prop, native.get_editor_property(prop) * SHIP_SPEED)
    save(LONGBOAT)
    boat = cdo(LONGBOAT)
    log("BP_Longboat : " + ", ".join(f"{p} {boat.get_editor_property(p):.0f}" for p in SPEED_PROPS))
    log("BP_PirateShip (référence, inchangé) : " + ", ".join(f"{p} {pirate.get_editor_property(p):.0f}" for p in SPEED_PROPS))


try:
    chests()
    elites()
    ships()
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_tweaks_2026_11_08.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
