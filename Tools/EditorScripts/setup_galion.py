"""
Bateau x2 et NME Galion (2026-10-30) :
  - BP_PirateShip : largeur et longueur doublées, hauteurs gardées (pont à +210, marches franchissables, planche d'embarquement) ; mâts, canons
    décoratifs, barre, nid d'pie et beaupré gardent leur taille (seulement replacés) ; points de coque x2 ; postes replacés + 2 canons
    (CannonLeft2, CannonRight2) ; réglages liés à la taille : zone du core 900, caméra de la barre 5400, chaloupe lancée à 800 derrière la barre
  - GameMap / TestMap : le bateau du port (tag HarborSea) reculé de la largeur gagnée, sa planche reste contre le ponton
  - FoeCharacter_Galion_Captain : copie de FoeCharacter_Skull_Elite
  - /Game/Placeable/Ship/BP_NME_Galion : copie de BP_PirateShip reparentée sur AYSFoeGalion, matériaux rouges, équipage
  - /Game/Placeable/Ship/Stations/BP_NME_ShipCannon (enfant de BP_ShipCannon, réglages à part : 3 AMMO, 50 dégâts) + /Game/Placeable/AMMO/NME_CanonAMMO
    (enfant de CanonAMMO, rouge) : canons du Galion
  - BP_TestGameMode : Galion Class (pavé numérique 8 de TestMap, debug YSSpawnGalion ; pas de vagues)
Relançable : le bateau n'est doublé qu'une fois (coque plus longue que 3000 = déjà fait), rien de ce qui existe n'est recréé.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_galion.py"
Rapport : Saved/Logs/setup_galion.txt
"""
import math
import os
import unreal

SHIP_BP = "/Game/Placeable/Ship/BP_PirateShip"
GALION_BP = "/Game/Placeable/Ship/BP_NME_Galion"
FOE_DIR = "/Game/Placeable/Character/Foe"
ELITE = f"{FOE_DIR}/FoeCharacter_Skull_Elite"
CAPTAIN = f"{FOE_DIR}/FoeCharacter_Galion_Captain"
SKULL = f"{FOE_DIR}/FoeCharacter_Skull"
SKULL_GUN = f"{FOE_DIR}/FoeCharacter_Skull_Gun"
SKULL_BOMB = f"{FOE_DIR}/FoeCharacter_Skull_Bomb"
CANNON_BP = "/Game/Placeable/Ship/Stations/BP_ShipCannon"
NME_CANNON_BP = "/Game/Placeable/Ship/Stations/BP_NME_ShipCannon"
CANON_AMMO = "/Game/Placeable/AMMO/CanonAMMO"
NME_CANON_AMMO = "/Game/Placeable/AMMO/NME_CanonAMMO"
CORE_BP = "/Game/Placeable/Ship/Stations/BP_ShipCore"
LAUNCHER_BP = "/Game/Placeable/Ship/Stations/BP_LongboatLauncher"
TEST_GAME_MODE = "/Game/Core/GameModes/BP_TestGameMode"
MAPS = ["/Game/Maps/GameMap", "/Game/Maps/TestMap"]
MAT = "/Game/Art/Materials/PH_materials"

XY = 2.0
WIDTH_GAIN = 500.0  # demi-largeur gagnée (flanc de 500 à 1000)
LENGTH_GAIN = 1700.0  # proue gagnée (bout de 1700 à 3400)

# seulement replacés (taille gardée)
KEEP_SIZE = ("MainMast", "ForeMast", "MizzenMast", "CrowsNest", "Helm", "HelmWheel", "Lantern", "FlagSkull")
RED_MATERIALS = {
    "m_PH_monochrome_Brown": f"{MAT}/m_PH_monochrome_Red",
    "m_PH_monochrome_Sand": f"{MAT}/m_PH_monochrome_LightRed",
    "m_PH_monochrome_white": f"{MAT}/m_PH_monochrome_LightRed",
}

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
actors_sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
report = []


def log(msg):
    unreal.log(f"[setup_galion] {msg}")
    report.append(msg)


def v(x, y, z):
    return unreal.Vector(x, y, z)


def rot(yaw=0.0, pitch=0.0, roll=0.0):
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


def components(bp):
    out = {}
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if obj:
            out[obj.get_name().replace("_GEN_VARIABLE", "")] = obj
    return out


def double_shape(name, comp):
    """Largeur et longueur x2, hauteur gardée"""
    location = comp.get_editor_property("relative_location")
    scale = comp.get_editor_property("relative_scale3d")
    if name.startswith("Cannon") or name == "Bowsprit" or name == "Gangplank":
        # canons décoratifs, beaupré, planche : au bord de la coque, taille gardée (planche : plus large)
        if name.startswith("Cannon"):
            location = v(location.x * XY, location.y + math.copysign(WIDTH_GAIN, location.y), location.z)
        elif name == "Bowsprit":
            location = v(location.x + LENGTH_GAIN, location.y, location.z)
        else:
            location = v(location.x, location.y - WIDTH_GAIN, location.z)
            scale = v(scale.x * XY, scale.y, scale.z)
    elif name in KEEP_SIZE:
        location = v(location.x * XY, location.y * XY, location.z)
    elif "_Yard" in name or "_Sail" in name:
        # vergues et voiles : plus longues (axe Z local = le long de Y), pas plus épaisses
        location = v(location.x * XY, location.y * XY, location.z)
        scale = v(scale.x, scale.y, scale.z * XY)
    else:
        location = v(location.x * XY, location.y * XY, location.z)
        scale = v(scale.x * XY, scale.y * XY, scale.z)
    comp.set_editor_property("relative_location", location)
    comp.set_editor_property("relative_scale3d", scale)


def station_placements(comps):
    """Postes replacés sur les formes du bateau doublé"""
    mast = comps["MainMast"].get_editor_property("relative_location")
    door = comps["CastleDoor"].get_editor_property("relative_location")
    castle = comps["Castle"]
    castle_front = castle.get_editor_property("relative_location").x + castle.get_editor_property("relative_scale3d").x * 50.0
    bow = comps["HullBow"].get_editor_property("relative_location")
    bow_radius = comps["HullBow"].get_editor_property("relative_scale3d").x * 50.0
    wheel = comps["HelmWheel"].get_editor_property("relative_location")
    side_y = 933.0  # même écart au bastingage que les canons placés par Paul
    log(f"bateau : avant du gaillard {castle_front:.0f}, proue {bow.x + bow_radius:.0f}, roue {wheel.x:.0f}")
    return {
        "HelmStation": (wheel, 0.0),
        "CoreStation": (v((castle_front + mast.x) / 2.0, 0.0, 210.0), 0.0),
        "CrowsNestStation": (v(mast.x, mast.y, 210.0), 0.0),
        "LongboatStation": (v(castle_front + 45.0, door.y, 210.0), 180.0),
        "CannonFront": (v(bow.x + bow_radius - 104.0, 0.0, 210.0), 0.0),
        "CannonLeft": (v(900.0, -side_y, 210.0), -90.0),
        "CannonRight": (v(900.0, side_y, 210.0), 90.0),
        "CannonLeft2": (v(-700.0, -side_y, 210.0), -90.0),
        "CannonRight2": (v(-700.0, side_y, 210.0), 90.0),
        "BoardingAccess": (v(0.0, -1000.0, 0.0), -90.0),
    }


def sync_template(comp):
    """Gabarit de l'acteur enfant = réglages du Blueprint du poste (nouveaux réglages : Takes Damage...)"""
    cls = comp.get_editor_property("child_actor_class")
    template = comp.get_editor_property("child_actor_template")
    if cls and template:
        return unreal.YSEditorScriptLibrary.copy_editable_properties(unreal.get_default_object(cls), template)
    return 0


try:
    # --- réglages des postes liés à la taille
    cannon_class = assets.load_blueprint_class(CANNON_BP)
    core_cdo = unreal.get_default_object(assets.load_blueprint_class(CORE_BP))
    core_cdo.set_editor_property("AcceptRange", 900.0)
    assets.save_asset(CORE_BP, only_if_is_dirty=False)
    launcher_cdo = unreal.get_default_object(assets.load_blueprint_class(LAUNCHER_BP))
    launcher_cdo.set_editor_property("SpawnDistanceBehindHelm", 800.0)
    assets.save_asset(LAUNCHER_BP, only_if_is_dirty=False)

    # --- BP_PirateShip x2
    ship_bp = assets.load_asset(SHIP_BP)
    comps = components(ship_bp)
    ship_class = assets.load_blueprint_class(SHIP_BP)
    ship_cdo = unreal.get_default_object(ship_class)
    hull = comps["Hull"]
    if hull.get_editor_property("relative_scale3d").x < 30.0:
        for name, comp in comps.items():
            if isinstance(comp, unreal.StaticMeshComponent):
                double_shape(name, comp)
        log("BP_PirateShip : formes x2 en largeur et en longueur")
    else:
        log("BP_PirateShip : déjà doublé")
    probes = ship_cdo.get_editor_property("HullProbePoints")
    if max(p.x for p in probes) < 3000.0:
        ship_cdo.set_editor_property("HullProbePoints", [unreal.Vector2D(p.x * XY, p.y * XY) for p in probes])
    log(f"points de coque : {[(p.x, p.y) for p in ship_cdo.get_editor_property('HullProbePoints')]}")
    placements = station_placements(comps)
    for name, (location, yaw) in placements.items():
        comp = ship_cdo.get_editor_property(name)
        comp.set_editor_property("relative_location", location)
        comp.set_editor_property("relative_rotation", rot(yaw=yaw))
        if name in ("CannonLeft2", "CannonRight2"):
            comp.set_child_actor_class(cannon_class)
        copied = sync_template(comp)
        log(f"BP_PirateShip.{name} en ({location.x:.0f}, {location.y:.0f}, {location.z:.0f}) yaw {yaw:.0f} ({copied} réglages)")
    helm_template = ship_cdo.get_editor_property("HelmStation").get_editor_property("child_actor_template")
    helm_template.set_editor_property("CameraArmLength", 5400.0)
    unreal.BlueprintEditorLibrary.compile_blueprint(ship_bp)
    assets.save_asset(SHIP_BP, only_if_is_dirty=False)

    # --- bateau du port : reculé de la largeur gagnée
    for map_path in MAPS:
        levels.load_level(map_path)
        moved = False
        for ship in actors_sub.get_all_level_actors():
            tags = [str(t) for t in ship.tags] if isinstance(ship, unreal.YSPirateShip) else []
            if "HarborSea" in tags and "ShipX2" not in tags:
                local_y = ship.get_actor_right_vector()
                ship.set_actor_location(ship.get_actor_location() + local_y * WIDTH_GAIN, False, False)
                ship.set_editor_property("tags", list(ship.tags) + [unreal.Name("ShipX2")])
                moved = True
                log(f"{map_path} : {ship.get_actor_label()} reculé en {ship.get_actor_location()}")
        for ship in actors_sub.get_all_level_actors():
            if isinstance(ship, unreal.YSPirateShip):
                log(f"{map_path} : {ship.get_actor_label()} en {ship.get_actor_location()} yaw {ship.get_actor_rotation().yaw:.0f}")
        if moved:
            levels.save_current_level()

    # --- capitaine : copie de l'élite
    if not assets.does_asset_exist(CAPTAIN):
        assets.duplicate_asset(ELITE, CAPTAIN)
        captain_cdo = unreal.get_default_object(assets.load_blueprint_class(CAPTAIN))
        captain_cdo.set_editor_property("FoeName", unreal.Text("Foe Captain"))
        assets.save_asset(CAPTAIN, only_if_is_dirty=False)
        log(f"créé : {CAPTAIN}")
    else:
        log(f"existe déjà : {CAPTAIN}")

    # --- Galion : copie rouge du bateau
    if not assets.does_asset_exist(GALION_BP):
        assets.duplicate_asset(SHIP_BP, GALION_BP)
        galion = assets.load_asset(GALION_BP)
        unreal.BlueprintEditorLibrary.reparent_blueprint(galion, unreal.YSFoeGalion)
        unreal.BlueprintEditorLibrary.compile_blueprint(galion)
        recolored = 0
        for name, comp in components(galion).items():
            if isinstance(comp, unreal.StaticMeshComponent):
                material = comp.get_material(0)
                target = RED_MATERIALS.get(material.get_name()) if material else None
                if target:
                    comp.set_material(0, assets.load_asset(target))
                    recolored += 1
        unreal.BlueprintEditorLibrary.compile_blueprint(galion)
        galion_cdo = unreal.get_default_object(assets.load_blueprint_class(GALION_BP))
        galion_cdo.set_editor_property("CaptainClass", assets.load_blueprint_class(CAPTAIN))
        skull = assets.load_blueprint_class(SKULL)
        gun = assets.load_blueprint_class(SKULL_GUN)
        bomb = assets.load_blueprint_class(SKULL_BOMB)
        groups = []
        for classes, count in (([skull], 6), ([gun], 3), ([skull, bomb, gun], 6)):
            group = unreal.YSGalionCrewGroup()
            group.set_editor_property("Classes", classes)
            group.set_editor_property("Count", count)
            groups.append(group)
        galion_cdo.set_editor_property("CrewGroups", groups)
        # pas de zone Brooker / Medic sur un bateau de NME
        core_template = galion_cdo.get_editor_property("CoreStation").get_editor_property("child_actor_template")
        core_template.set_editor_property("bShowZone", False)
        unreal.BlueprintEditorLibrary.compile_blueprint(galion)
        assets.save_asset(GALION_BP, only_if_is_dirty=False)
        log(f"créé : {GALION_BP} ({recolored} formes en rouge)")
    else:
        log(f"existe déjà : {GALION_BP}")

    # --- canons du Galion : Blueprint et AMMO à part
    if not assets.does_asset_exist(NME_CANON_AMMO):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", assets.load_blueprint_class(CANON_AMMO))
        unreal.AssetToolsHelpers.get_asset_tools().create_asset("NME_CanonAMMO", "/Game/Placeable/AMMO", unreal.Blueprint, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(assets.load_asset(NME_CANON_AMMO))
        ammo_cdo = unreal.get_default_object(assets.load_blueprint_class(NME_CANON_AMMO))
        ammo_cdo.set_editor_property("Color", unreal.LinearColor(1.0, 0.02, 0.02, 1.0))
        log(f"créé : {NME_CANON_AMMO}")
    # rouge franc (un matériau émissif vire au blanc à cette taille)
    ammo_mesh = unreal.get_default_object(assets.load_blueprint_class(NME_CANON_AMMO)).get_editor_property("mesh")
    ammo_mesh.set_material(0, assets.load_asset(f"{MAT}/m_PH_monochrome_Red"))
    assets.save_asset(NME_CANON_AMMO, only_if_is_dirty=False)
    if not assets.does_asset_exist(NME_CANNON_BP):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", cannon_class)
        unreal.AssetToolsHelpers.get_asset_tools().create_asset("BP_NME_ShipCannon", "/Game/Placeable/Ship/Stations", unreal.Blueprint, factory)
        nme_cannon_cdo = unreal.get_default_object(assets.load_blueprint_class(NME_CANNON_BP))
        nme_cannon_cdo.set_editor_property("ProjectileClass", assets.load_blueprint_class(NME_CANON_AMMO))
        nme_cannon_cdo.set_editor_property("AmmoPerShot", 3)
        nme_cannon_cdo.set_editor_property("Damage", 50.0)
        assets.save_asset(NME_CANNON_BP, only_if_is_dirty=False)
        log(f"créé : {NME_CANNON_BP}")
    nme_cannon_class = assets.load_blueprint_class(NME_CANNON_BP)
    galion = assets.load_asset(GALION_BP)
    galion_cdo = unreal.get_default_object(assets.load_blueprint_class(GALION_BP))
    for name in ("CannonFront", "CannonLeft", "CannonRight", "CannonLeft2", "CannonRight2"):
        comp = galion_cdo.get_editor_property(name)
        if comp.get_editor_property("child_actor_class") != nme_cannon_class:
            comp.set_child_actor_class(nme_cannon_class)
    # (après la compilation, sinon les gabarits gardent les valeurs de l'ancienne classe et les réglages du Blueprint ne s'appliquent pas)
    unreal.BlueprintEditorLibrary.compile_blueprint(galion)
    galion_cdo = unreal.get_default_object(assets.load_blueprint_class(GALION_BP))
    for name in ("CannonFront", "CannonLeft", "CannonRight", "CannonLeft2", "CannonRight2"):
        comp = galion_cdo.get_editor_property(name)
        sync_template(comp)
        template = comp.get_editor_property("child_actor_template")
        log(f"BP_NME_Galion.{name} : {template.get_editor_property('AmmoPerShot')} AMMO x {template.get_editor_property('Damage'):.0f}")
    assets.save_asset(GALION_BP, only_if_is_dirty=False)
    log("BP_NME_Galion : canons -> BP_NME_ShipCannon")

    # --- GameMode
    game_mode_cdo = unreal.get_default_object(assets.load_blueprint_class(TEST_GAME_MODE))
    game_mode_cdo.set_editor_property("GalionClass", assets.load_blueprint_class(GALION_BP))
    assets.save_asset(TEST_GAME_MODE, only_if_is_dirty=False)
    log("BP_TestGameMode : Galion Class réglée")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_galion.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
