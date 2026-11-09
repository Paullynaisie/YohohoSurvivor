"""
Réglages du 2026-11-09 (demandes de Paul). À NE PAS RELANCER pour les multiplications (pontons, caméra, AMMO) : chaque valeur n'est
modifiée que si elle a encore sa valeur d'avant (sinon le rapport le signale et n'y touche pas).
  - DA_I_StartIsland_Map1 : pontons de la plage 1.5x moins longs (2720 -> 1813) ; le sloop garde sa place dans l'eau (Berth Distance From Shore
    = moitié de sa coque + 200, il dépasse du bout des pontons)
  - caméra des personnages joueurs dézoomée de 15 % (CameraBoom 1680 -> 1932, 1600 -> 1840) ; BP_GameBoatSeat (caméra pendant la traversée) idem
    REMPLACÉ ensuite à la demande de Paul : tous les personnages à 1840 (défaut C++ aussi), BP_GameBoatSeat remis à 1680
  - /Game/Placeable/AMMO/ExplosiveAMMO : durée de vie x2
  - BP_FlowMod : 2 relances max par sélection d'upgrade (Max Upgrade Rerolls)
  - personnages joueurs : perte de dégâts multi-cibles 5 % par NME (min 50 %)

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="<chemin absolu>/Tools/EditorScripts/setup_tweaks_2026_11_09.py"
Rapport : Saved/Logs/setup_tweaks_2026_11_09.txt
"""
import os
import unreal

MAP1_DA = "/Game/Core/Islands/DA_I_StartIsland_Map1"
SLOOP_BP = "/Game/Placeable/Ship/BP_GameBoat_Sloop"
SEAT_BP = "/Game/Placeable/Ship/Stations/BP_GameBoatSeat"
AMMO_BP = "/Game/Placeable/AMMO/ExplosiveAMMO"
FLOW_GM = "/Game/Core/GameModes/BP_FlowMod"
CHARACTER_ROOT = "/Game/Placeable/Character"

OLD_PIER_LENGTH = 2720.0
OLD_ARM = 1680.0
NEW_ARM = 1932.0
# Mouss, JohnnyPlaytest, Quartermaster, Captain étaient réglés à 1600
ARMS = {1680.0: 1932.0, 1600.0: 1840.0}

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    report.append(str(msg))


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


try:
    # 1) pontons
    data = assets.load_asset(MAP1_DA)
    rules = data.get_editor_property("generation")
    length = rules.get_editor_property("beach_pier_length")
    if abs(length - OLD_PIER_LENGTH) < 1.0:
        hull = cdo(SLOOP_BP).get_editor_property("HullProbePoints")
        hull_length = max(p.x for p in hull) - min(p.x for p in hull)
        rules.set_editor_property("beach_pier_length", round(OLD_PIER_LENGTH / 1.5))
        rules.set_editor_property("berth_distance_from_shore", hull_length * 0.5 + 200.0)
        data.set_editor_property("generation", rules)
        assets.save_loaded_asset(data, only_if_is_dirty=False)
    else:
        log(f"pontons : longueur {length} déjà changée, pas touchée")
    rules = assets.load_asset(MAP1_DA).get_editor_property("generation")
    log(f"relu : pontons {rules.get_editor_property('beach_pier_length')} cm, sloop à {rules.get_editor_property('berth_distance_from_shore')} cm du bord de l'eau")

    # 2) caméra : tous les personnages joueurs (Blueprints enfants d'AYSPlayerCharacter, minions compris : même caméra, sans effet)
    for path in assets.list_assets(CHARACTER_ROOT, recursive=True):
        package = path.split(".")[0]
        asset_data = assets.find_asset_data(package)
        if asset_data.asset_class_path.asset_name != "Blueprint":
            continue
        bp_class = assets.load_blueprint_class(package)
        if not bp_class or not unreal.MathLibrary.class_is_child_of(bp_class, unreal.YSPlayerCharacter):
            continue
        character = unreal.get_default_object(bp_class)
        boom = character.get_editor_property("CameraBoom")
        arm = boom.get_editor_property("target_arm_length")
        for old, new in ARMS.items():
            if abs(arm - old) < 1.0:
                boom.set_editor_property("target_arm_length", new)
        character.set_editor_property("multi_hit_damage_falloff", 0.05)
        character.set_editor_property("multi_hit_min_damage_multiplier", 0.5)
        assets.save_asset(package, only_if_is_dirty=False)
        character = cdo(package)
        log(f"relu : {package.rsplit('/', 1)[1]} caméra {arm} -> {character.get_editor_property('CameraBoom').get_editor_property('target_arm_length')}, "
            f"multi-cibles {character.get_editor_property('multi_hit_damage_falloff')} / min {character.get_editor_property('multi_hit_min_damage_multiplier')}")

    seat = cdo(SEAT_BP)
    if abs(seat.get_editor_property("camera_arm_length") - OLD_ARM) < 1.0:
        seat.set_editor_property("camera_arm_length", NEW_ARM)
        assets.save_asset(SEAT_BP, only_if_is_dirty=False)
    log(f"relu : BP_GameBoatSeat caméra {cdo(SEAT_BP).get_editor_property('camera_arm_length')}")

    # 3) AMMO explosive
    ammo = cdo(AMMO_BP)
    life = ammo.get_editor_property("life_time")
    ammo.set_editor_property("life_time", life * 2.0)
    assets.save_asset(AMMO_BP, only_if_is_dirty=False)
    log(f"relu : ExplosiveAMMO durée de vie {life} -> {cdo(AMMO_BP).get_editor_property('life_time')} s")

    # 4) relances
    gm = cdo(FLOW_GM)
    gm.set_editor_property("max_upgrade_rerolls", 2)
    assets.save_asset(FLOW_GM, only_if_is_dirty=False)
    gm = cdo(FLOW_GM)
    log(f"relu : BP_FlowMod relance {gm.get_editor_property('allow_upgrade_reroll')}, max {gm.get_editor_property('max_upgrade_rerolls')}")
    log("OK")
except Exception as error:
    import traceback
    log(f"ERREUR : {error}\n{traceback.format_exc()}")

with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_tweaks_2026_11_09.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
