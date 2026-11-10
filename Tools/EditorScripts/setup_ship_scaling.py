"""
Scaling des bateaux et postes de combat (demande de Paul, 2026-11-10) :
  - barres de vie des bateaux (Ship Health Bar Scale) 2x plus petites
  - passif du Captain : « Equipage » ; Maître canonnier : +10 % de dégâts aux postes par niveau (Combat Post Damage Per Level)
  - PV des bateaux des joueurs (Ship Max Health) : sloop 5000, brigantin 10000, galion 15000 ; postes de combat = PV du bateau / 5 (C++)
  - postes : Canon (1 obus 250 + explosion 750 sur 7.5 m), Gatling (10 x 125 en 1 s), Mortier (obus en cloche 4 s, 650 sur 15 m, 35 m max),
    Grappeshot (éventail de 5 x 350) ; recharge 4 s (x2 pour un minion / NME)
  - objets : BP_CombatPostItem_Mortar, BP_CombatPostItem_GrapeShot (enfants de BP_CombatPostItem_Cannon), ajoutés aux Combat Post Item Classes
  - Galions de NME : réceptacles BP_NME_CombatPostReceptacle (poste tiré au hasard parmi les 4)
Relançable : les Blueprints qui existent ne sont pas recréés ; les valeurs sont remises.

Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/setup_ship_scaling.py"
Rapport : Saved/Logs/setup_ship_scaling.txt
"""
import os
import unreal

SHIP_DIR = "/Game/Placeable/Ship"
STATIONS = f"{SHIP_DIR}/Stations"
ITEMS = "/Game/Placeable/InteractiveObjects/CombatPosts"
AMMO = "/Game/Placeable/AMMO"
CANNON = f"{STATIONS}/BP_ShipCannon"
GATLING = f"{STATIONS}/BP_ShipGatling"
MORTAR = f"{STATIONS}/BP_ShipMortar"
GRAPESHOT = f"{STATIONS}/BP_ShipGrapeShot"
RECEPTACLE = f"{STATIONS}/BP_CombatPostReceptacle"
NME_RECEPTACLE = f"{STATIONS}/BP_NME_CombatPostReceptacle"
ITEM_CANNON = f"{ITEMS}/BP_CombatPostItem_Cannon"
ITEM_GATLING = f"{ITEMS}/BP_CombatPostItem_Gatling"
ITEM_MORTAR = f"{ITEMS}/BP_CombatPostItem_Mortar"
ITEM_GRAPESHOT = f"{ITEMS}/BP_CombatPostItem_GrapeShot"
BLAST = f"{AMMO}/BP_ExplosiveShotBlast"
BOMB = f"{AMMO}/BP_CannoneerBomb"
SHELL = f"{AMMO}/BP_MortarShell"
SHIP_HEALTH = {"BP_GameBoat_Sloop": 5000.0, "BP_GameBoat_Brigantin": 10000.0, "BP_GameBoat_Galion": 15000.0}
NME_GALIONS = ("BP_NME_Galion", "BP_NME_Galion_Nemesis")
CANNON_SLOTS = ("CannonFront", "CannonLeft", "CannonRight", "CannonLeft2", "CannonRight2")
GAME_MODES = ("/Game/Core/GameModes/BP_FlowMod", "/Game/Core/GameModes/BP_TestGameMode", "/Game/Core/GameModes/BP_TestMapGameMode")
PASSIVES = "/Game/Core/Passives"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
copy_props = unreal.YSEditorScriptLibrary.copy_editable_properties
report = []


def log(msg):
    unreal.log(f"[setup_ship_scaling] {msg}")
    report.append(str(msg))


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def bp_class(path):
    return assets.load_blueprint_class(path)


def compile_save(path):
    unreal.BlueprintEditorLibrary.compile_blueprint(assets.load_asset(path))
    assets.save_asset(path, only_if_is_dirty=False)


def make_bp(path, parent_class):
    if assets.does_asset_exist(path):
        return False
    directory, name = path.rsplit("/", 1)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    bp = tools.create_asset(name, directory, unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    return True


def set_props(path, values):
    default = cdo(path)
    for key, value in values.items():
        default.set_editor_property(key, value)
    compile_save(path)
    default = cdo(path)
    shown = {k: (v.get_name() if hasattr(v, "get_name") else v) for k, v in ((k, default.get_editor_property(k)) for k in values)}
    log(f"{path.rsplit('/', 1)[1]} : {shown}")


try:
    # barres de vie des bateaux 2x plus petites (défaut C++ 1.5 ; seules les valeurs encore à l'ancien défaut 3 sont divisées)
    for path in assets.list_assets(SHIP_DIR, recursive=False):
        path = path.split(".")[0]
        try:
            default = cdo(path)
        except Exception:
            continue
        if not isinstance(default, unreal.YSPirateShip):
            continue
        scale = default.get_editor_property("ShipHealthBarScale")
        if scale > 2.0:
            default.set_editor_property("ShipHealthBarScale", scale / 2.0)
            compile_save(path)
        log(f"{path.rsplit('/', 1)[1]} : barre de vie x{cdo(path).get_editor_property('ShipHealthBarScale')}")

    # passifs
    for path in assets.list_assets(PASSIVES, recursive=False):
        data = assets.load_asset(path.split(".")[0])
        if not isinstance(data, unreal.YSPassive):
            continue
        if data.get_name() == "DA_Passive_ToiTaGueule":
            data.set_editor_property("DisplayName", unreal.Text("Equipage"))
            data.set_editor_property("Description", unreal.Text("Son équipage peut utiliser les postes de combat des bateaux"))
        if data.get_name() == "DA_Passive_MaitreCanonnier":
            data.set_editor_property("CombatPostDamagePerLevel", 0.1)
        assets.save_loaded_asset(data, only_if_is_dirty=False)
        log(f"{data.get_name()} : « {data.get_editor_property('DisplayName')} », postes par niveau {data.get_editor_property('CombatPostDamagePerLevel')}")

    # PV des bateaux des joueurs
    for name, health in SHIP_HEALTH.items():
        set_props(f"{SHIP_DIR}/{name}", {"ShipMaxHealth": health, "CombatPostHealthDivisor": 5.0})

    # obus du mortier : bombe du Cannoneer, explosion de 15 m
    if make_bp(SHELL, bp_class(BOMB)):
        log("créé : BP_MortarShell (enfant de BP_CannoneerBomb)")
    set_props(SHELL, {"ExplosionRadius": 1500.0})

    # postes
    blast = bp_class(BLAST)
    set_props(CANNON, {"AmmoPerShot": 1, "Spread": 0.0, "Damage": 250.0, "ShotsPerBurst": 1, "ExplosionClass": blast, "ExplosionDamage": 750.0,
                       "ExplosionRadius": 750.0, "Cooldown": 4.0, "NonPlayerCooldownMultiplier": 2.0, "DamagePerPlayerLevel": 0.05})
    set_props(GATLING, {"AmmoPerShot": 1, "Spread": 0.0, "Damage": 125.0, "ShotsPerBurst": 10, "BurstInterval": 0.1, "ExplosionClass": None,
                        "Cooldown": 4.0, "NonPlayerCooldownMultiplier": 2.0})
    if make_bp(GRAPESHOT, bp_class(CANNON)):
        log("créé : BP_ShipGrapeShot (enfant de BP_ShipCannon)")
    if make_bp(MORTAR, unreal.YSShipMortar):
        copy_props(cdo(CANNON), cdo(MORTAR))
        log("créé : BP_ShipMortar (AYSShipMortar, réglages copiés de BP_ShipCannon)")
    for item, parent, name in ((ITEM_MORTAR, ITEM_CANNON, "Mortier"), (ITEM_GRAPESHOT, ITEM_CANNON, "Grappeshot")):
        if make_bp(item, bp_class(parent)):
            log(f"créé : {item.rsplit('/', 1)[1]} (enfant de {parent.rsplit('/', 1)[1]})")
    set_props(GRAPESHOT, {"AmmoPerShot": 5, "Spread": 20.0, "Damage": 350.0, "ShotsPerBurst": 1, "ExplosionClass": None, "Cooldown": 4.0,
                          "NonPlayerCooldownMultiplier": 2.0, "DisplayName": unreal.Text("Grappeshot"), "CombatPostItemClass": bp_class(ITEM_GRAPESHOT)})
    set_props(MORTAR, {"ShellClass": bp_class(SHELL), "Damage": 650.0, "FlightDuration": 4.0, "MaxRange": 3500.0, "MinRange": 500.0, "AmmoPerShot": 1,
                       "ShotsPerBurst": 1, "ExplosionClass": None, "Cooldown": 4.0, "NonPlayerCooldownMultiplier": 2.0, "AutoFireRange": 3500.0,
                       "DisplayName": unreal.Text("Mortier"), "CombatPostItemClass": bp_class(ITEM_MORTAR)})
    # fût du mortier relevé vers le ciel (visuel, à retoucher)
    barrel = cdo(MORTAR).get_editor_property("CannonBarrel")
    rot = barrel.get_editor_property("relative_rotation")
    log(f"BP_ShipMortar : fût rotation (R {rot.roll:.0f}, P {rot.pitch:.0f}, Y {rot.yaw:.0f}), à retoucher dans le Blueprint")
    set_props(ITEM_MORTAR, {"PostClass": bp_class(MORTAR), "DisplayName": unreal.Text("Poste de combat : Mortier")})
    set_props(ITEM_GRAPESHOT, {"PostClass": bp_class(GRAPESHOT), "DisplayName": unreal.Text("Poste de combat : Grappeshot")})
    log(f"objets : canon « {cdo(ITEM_CANNON).get_editor_property('DisplayName')} », Gatling « {cdo(ITEM_GATLING).get_editor_property('DisplayName')} »")

    items = [bp_class(p) for p in (ITEM_CANNON, ITEM_GATLING, ITEM_MORTAR, ITEM_GRAPESHOT)]
    for gm in GAME_MODES:
        set_props(gm, {"CombatPostItemClasses": items})

    # Galions de NME : postes tirés au hasard
    if make_bp(NME_RECEPTACLE, bp_class(RECEPTACLE)):
        log("créé : BP_NME_CombatPostReceptacle (enfant de BP_CombatPostReceptacle)")
    set_props(NME_RECEPTACLE, {"RandomPostClasses": [bp_class(p) for p in (CANNON, GATLING, MORTAR, GRAPESHOT)]})
    for name in NME_GALIONS:
        path = f"{SHIP_DIR}/{name}"
        ship = cdo(path)
        for slot in CANNON_SLOTS:
            comp = ship.get_editor_property(slot)
            if comp and comp.get_editor_property("child_actor_class"):
                comp.set_child_actor_class(bp_class(NME_RECEPTACLE))
        compile_save(path)
        ship = cdo(path)
        log(f"{name} : {[(s, ship.get_editor_property(s).get_editor_property('child_actor_class').get_name()) for s in CANNON_SLOTS if ship.get_editor_property(s).get_editor_property('child_actor_class')]}")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_ship_scaling.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
