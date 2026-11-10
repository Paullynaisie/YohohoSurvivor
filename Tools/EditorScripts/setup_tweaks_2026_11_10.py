"""
Retours de Paul du 2026-11-10 (relançable : valeurs absolues) :
  - poids des bateaux (Ship|Collision Weight) : Galion 4 (BP_PirateShip, BP_GameBoat_Galion, BP_NME_Galion, BP_NME_Galion_Nemesis),
    Brigantin 3, Sloop 2, Jolly Ship et chaloupes 1
  - caméra de la barre +20 % (dézoom) sauf la chaloupe de départ : sloop 4320, Brigantin 5400, Galion 6480, BP_Longboat 3600 x 1.2
  - Item Scale : coffres et postes de combat x1.5, cartes au trésor et boussole x2
  - bulle sur TOUS les objets ramassables (révélée à moins de 10 m, cachée à plus de 100 m ou sous l'eau : défauts C++) ; icônes
    provisoires prises dans /Game/Art/Textures/Icones quand l'objet n'en a pas (à remplacer par Paul)
  - nuits des îles à objectifs (Objective Flow) : 120 s avant l'overtime

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="<chemin absolu>/Tools/EditorScripts/setup_tweaks_2026_11_10.py"
Rapport : Saved/Logs/setup_tweaks_2026_11_10.txt
"""
import os
import unreal

SHIP_DIR = "/Game/Placeable/Ship"
OBJ_DIR = "/Game/Placeable/InteractiveObjects"
ICONS = "/Game/Art/Textures/Icones"
WEIGHTS = {
    "BP_PirateShip": 4.0, "BP_GameBoat_Galion": 4.0, "BP_NME_Galion": 4.0, "BP_NME_Galion_Nemesis": 4.0,
    "BP_GameBoat_Brigantin": 3.0, "BP_GameBoat_Sloop": 2.0,
    "BP_NME_JollyShip": 1.0, "BP_Longboat": 1.0, "BP_GameBoat_Longboat": 1.0,
}
SLOOP_HELM = 3600.0 * 1.2
HELM = {"BP_GameBoat_Sloop": SLOOP_HELM, "BP_GameBoat_Brigantin": SLOOP_HELM * 1.25, "BP_GameBoat_Galion": SLOOP_HELM * 1.5,
        "BP_PirateShip": 5400.0 * 1.2}
LONGBOAT_HELM_BEFORE = 3600.0
# (objet, échelle, icône provisoire si aucune)
ITEMS = [
    (f"{OBJ_DIR}/BP_InteractiveObject_Chest", 1.5, "t_i_ChestIcon"),
    (f"{OBJ_DIR}/BP_InteractiveObject_TreasureChest", 1.5, "t_i_ChestIcon"),
    (f"{OBJ_DIR}/BP_InteractiveObject_BossChest", 1.5, "t_i_ChestIcon"),
    (f"{OBJ_DIR}/CombatPosts/BP_CombatPostItem_Cannon", 1.5, "t_i_ShootSystemIcon"),
    (f"{OBJ_DIR}/CombatPosts/BP_CombatPostItem_Gatling", 1.5, "t_i_ShootSystemIcon"),
    (f"{OBJ_DIR}/BP_InteractiveObject_TreasureMap", 2.0, "t_i_RoadmapIcon"),
    (f"{OBJ_DIR}/BP_InteractiveObject_SuperTreasureMap", 2.0, "t_i_RoadmapIcon"),
    (f"{OBJ_DIR}/BP_InteractiveObject_Compass", 2.0, "t_i_LuckIcon"),
    (f"{OBJ_DIR}/BP_InteractiveObject_BoatCore", None, "t_i_BulkUpIcon"),
    (f"{OBJ_DIR}/BP_InteractiveObject_Carpenter", None, "t_i_CDRicon"),
    (f"{OBJ_DIR}/BP_InteractiveObject_Medic", None, "t_i_RegenIcon"),
]
NIGHT = 120.0

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_tweaks_2026_11_10] {msg}")
    report.append(str(msg))


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


try:
    for name, weight in WEIGHTS.items():
        path = f"{SHIP_DIR}/{name}"
        if not assets.does_asset_exist(path):
            log(f"ABSENT : {path}")
            continue
        cdo(path).set_editor_property("Weight", weight)
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"{name} : poids {cdo(path).get_editor_property('Weight')}")

    for name, arm in HELM.items():
        path = f"{SHIP_DIR}/{name}"
        template = cdo(path).get_editor_property("HelmStation").get_editor_property("child_actor_template")
        before = template.get_editor_property("CameraArmLength")
        template.set_editor_property("CameraArmLength", arm)
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"{name} : caméra de la barre {before:.0f} -> {arm:.0f}")
    longboat = f"{SHIP_DIR}/BP_Longboat"
    template = cdo(longboat).get_editor_property("HelmStation").get_editor_property("child_actor_template")
    before = template.get_editor_property("CameraArmLength")
    if abs(before - LONGBOAT_HELM_BEFORE) < 1.0:
        template.set_editor_property("CameraArmLength", before * 1.2)
        assets.save_asset(longboat, only_if_is_dirty=False)
    log(f"BP_Longboat : caméra de la barre {before:.0f} -> {template.get_editor_property('CameraArmLength'):.0f}")

    for path, scale, icon in ITEMS:
        if not assets.does_asset_exist(path):
            log(f"ABSENT : {path}")
            continue
        default = cdo(path)
        if scale:
            default.set_editor_property("ItemScale", scale)
        default.set_editor_property("bShowIndicator", True)
        current = default.get_editor_property("IndicatorIcon")
        if not current or (current.get_name() == "t_i_ChestIcon" and icon != "t_i_ChestIcon"):
            default.set_editor_property("IndicatorIcon", assets.load_asset(f"{ICONS}/{icon}"))
        assets.save_asset(path, only_if_is_dirty=False)
        default = cdo(path)
        log(f"{path.rsplit('/', 1)[1]} : échelle {default.get_editor_property('ItemScale')}, bulle {default.get_editor_property('bShowIndicator')}, "
            f"icône {default.get_editor_property('IndicatorIcon').get_name() if default.get_editor_property('IndicatorIcon') else '-'}")

    for path in assets.list_assets("/Game/Core/Islands", recursive=False):
        data = assets.load_asset(path.split(".")[0])
        if isinstance(data, unreal.YSIslandData) and data.get_editor_property("objective_flow"):
            before = data.get_editor_property("night_duration")
            data.set_editor_property("night_duration", NIGHT)
            assets.save_loaded_asset(data, only_if_is_dirty=False)
            log(f"{data.get_name()} : nuit {before:.0f} -> {NIGHT:.0f} s")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_tweaks_2026_11_10.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
