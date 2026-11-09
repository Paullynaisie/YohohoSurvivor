"""
Flow en 3 parties, partie 1 (2026-11-09), relançable (ne retouche pas une île déjà créée) :
  - /Game/Core/Islands/DA_I_StartIsland_Map1 = copie de DA_I_StartIsland :
      * génération Tropical identique, rayon 65 m
      * 2 pontons parallèles sur la plage (Beach Piers) : écart = largeur de la coque de BP_GameBoat_Sloop + marge, longueur = sa longueur + marge
      * un seul objectif principal : le CoreBoat, le plus au nord possible dans l'espace accessible (North Most) ; plus d'objectif secondaire
      * Objective Flow : jour 120 s, nuit 90 s, évacuation 30 s ; vagues d'overtime de la nuit déplacées dans Overtime Events (comptées depuis
        l'overtime) ; spawn constant 2 NME / 2 s (Skull, Skull Nemesis en overtime)
  - BP_FlowMod : Start Island = DA_I_StartIsland_Map1, plus d'îles suivantes (Next Islands Enabled décoché)
  - /Game/Core/UI/WBP_FlowHUD (enfant de YSFlowHUDWidget, réglages Layout) -> BP_YSPlayerController (Flow HUD Class)

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_flow_part1.py"
Rapport : Saved/Logs/setup_flow_part1.txt
"""
import os
import unreal

ISLAND_DIR = "/Game/Core/Islands"
SOURCE_DA = f"{ISLAND_DIR}/DA_I_StartIsland"
MAP1_DA = f"{ISLAND_DIR}/DA_I_StartIsland_Map1"
FLOW_GM = "/Game/Core/GameModes/BP_FlowMod"
PC_BP = "/Game/Core/Player/BP_YSPlayerController"
UI_DIR = "/Game/Core/UI"
FLOW_WIDGET = f"{UI_DIR}/WBP_FlowHUD"
SLOOP_BP = "/Game/Placeable/Ship/BP_GameBoat_Sloop"

RADIUS = 6500.0
DAY_DURATION = 120.0
NIGHT_DURATION = 90.0
EVACUATION_DURATION = 30.0
PIER_SIDE_MARGIN = 80.0      # de chaque côté de la coque du sloop
PIER_LENGTH_MARGIN = 400.0
PIER_WIDTH = 300.0

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
report = []


def log(msg):
    report.append(str(msg))


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def find_class(name, root="/Game/Placeable"):
    for path in assets.list_assets(root, recursive=True):
        package = path.split(".")[0]
        if package.rsplit("/", 1)[1] == name:
            return assets.load_blueprint_class(package)
    raise RuntimeError(f"introuvable : {name}")


def event_text(ev):
    foes = [(f.get_editor_property("foe_class").get_name() if f.get_editor_property("foe_class") else None, f.get_editor_property("count")) for f in ev.get_editor_property("foes")]
    return (f"t={ev.get_editor_property('time')} rep={ev.get_editor_property('repeat_count')}/{ev.get_editor_property('repeat_interval')} "
            f"{ev.get_editor_property('shape')} {foes} overtime={ev.get_editor_property('overtime_level')}")


try:
    # 1) île de départ de la Map 1
    if not assets.does_asset_exist(MAP1_DA):
        data = assets.duplicate_asset(SOURCE_DA, MAP1_DA)
        rules = data.get_editor_property("generation")
        rules.set_editor_property("radius", RADIUS)

        # pontons : largeur et longueur de la coque du sloop (Hull Probe Points)
        hull = cdo(SLOOP_BP).get_editor_property("HullProbePoints")
        half_width = max(abs(p.y) for p in hull)
        length = max(p.x for p in hull) - min(p.x for p in hull)
        rules.set_editor_property("beach_piers", True)
        rules.set_editor_property("beach_pier_gap", 2.0 * (half_width + PIER_SIDE_MARGIN))
        rules.set_editor_property("beach_pier_length", length + PIER_LENGTH_MARGIN)
        rules.set_editor_property("beach_pier_width", PIER_WIDTH)
        data.set_editor_property("generation", rules)
        log(f"sloop : coque {length:.0f} x {2 * half_width:.0f} cm -> pontons écartés de {2.0 * (half_width + PIER_SIDE_MARGIN):.0f}, longs de {length + PIER_LENGTH_MARGIN:.0f}")

        # objectif unique : le CoreBoat, au nord
        core = None
        for objective in data.get_editor_property("main_objectives"):
            item = objective.get_editor_property("item_class")
            if item and "BoatCore" in item.get_name():
                core = objective
        if core is None:
            raise RuntimeError("pas d'objectif CoreBoat dans DA_I_StartIsland")
        core.set_editor_property("display_name", "Trouve et ramène un CoreBoat")
        core.set_editor_property("north_most", True)
        core.set_editor_property("spawn_day", 1)
        data.set_editor_property("main_objectives", [core])
        data.set_editor_property("secondary_objectives", [])

        # flow : durées, vagues d'overtime à part, spawn constant
        data.set_editor_property("objective_flow", True)
        data.set_editor_property("day_duration", DAY_DURATION)
        data.set_editor_property("night_duration", NIGHT_DURATION)
        data.set_editor_property("evacuation_duration", EVACUATION_DURATION)
        night = []
        overtime = []
        for ev in data.get_editor_property("night_events"):
            if ev.get_editor_property("overtime_level"):
                ev.set_editor_property("time", max(0.0, ev.get_editor_property("time") - NIGHT_DURATION))
                overtime.append(ev)
            else:
                night.append(ev)
        data.set_editor_property("night_events", night)
        data.set_editor_property("overtime_events", overtime)

        constant = data.get_editor_property("constant_spawn")
        constant.set_editor_property("enabled", True)
        constant.set_editor_property("interval", 2.0)
        constant.set_editor_property("count", 2)
        constant.set_editor_property("foes", [find_class("FoeCharacter_Skull")])
        constant.set_editor_property("overtime_foes", [find_class("FoeCharacter_Skull_Nemesis")])
        data.set_editor_property("constant_spawn", constant)

        assets.save_loaded_asset(data, only_if_is_dirty=False)
        log(f"créé : {MAP1_DA}")
    else:
        log(f"existe déjà (pas retouché) : {MAP1_DA}")

    data = assets.load_asset(MAP1_DA)
    rules = data.get_editor_property("generation")
    log(f"relu : rayon {rules.get_editor_property('radius')}, pontons {rules.get_editor_property('beach_piers')} écart {rules.get_editor_property('beach_pier_gap')} "
        f"longueur {rules.get_editor_property('beach_pier_length')}, flow {data.get_editor_property('objective_flow')}, jour {data.get_editor_property('day_duration')} "
        f"nuit {data.get_editor_property('night_duration')} évacuation {data.get_editor_property('evacuation_duration')}")
    for o in data.get_editor_property("main_objectives"):
        log(f"relu : objectif principal « {o.get_editor_property('display_name')} » {o.get_editor_property('item_class').get_name()} nord {o.get_editor_property('north_most')}")
    log(f"relu : {len(data.get_editor_property('secondary_objectives'))} objectif(s) secondaire(s)")
    for kind in ["day_events", "night_events", "overtime_events"]:
        for ev in data.get_editor_property(kind):
            log(f"relu : {kind} {event_text(ev)}")
    constant = data.get_editor_property("constant_spawn")
    log(f"relu : spawn constant {constant.get_editor_property('enabled')} {constant.get_editor_property('count')} / {constant.get_editor_property('interval')} s, "
        f"{[c.get_name() for c in constant.get_editor_property('foes')]} / overtime {[c.get_name() for c in constant.get_editor_property('overtime_foes')]}")

    # 2) gamemode : île de la Map 1, plus d'îles suivantes
    gm = cdo(FLOW_GM)
    gm.set_editor_property("start_island", data)
    gm.set_editor_property("next_islands_enabled", False)
    assets.save_asset(FLOW_GM, only_if_is_dirty=False)
    gm = cdo(FLOW_GM)
    log(f"relu : BP_FlowMod île {gm.get_editor_property('start_island').get_name()}, îles suivantes {gm.get_editor_property('next_islands_enabled')}")

    # 3) HUD du flow
    if not assets.does_asset_exist(FLOW_WIDGET):
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", unreal.YSFlowHUDWidget)
        tools.create_asset("WBP_FlowHUD", UI_DIR, unreal.WidgetBlueprint, factory)
        assets.save_asset(FLOW_WIDGET, only_if_is_dirty=False)
        log(f"créé : {FLOW_WIDGET}")
    cdo(PC_BP).set_editor_property("flow_hud_class", assets.load_blueprint_class(FLOW_WIDGET))
    assets.save_asset(PC_BP, only_if_is_dirty=False)
    log(f"relu : BP_YSPlayerController Flow HUD Class {cdo(PC_BP).get_editor_property('flow_hud_class').get_name()}")
    log("OK")
except Exception as error:
    import traceback
    log(f"ERREUR : {error}\n{traceback.format_exc()}")

with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_flow_part1.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
