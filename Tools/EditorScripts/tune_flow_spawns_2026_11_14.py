"""
Spawns du FlowMod allégés (demande de Paul du 2026-11-14), sur les 4 îles du flow (DA_I_StartIsland_Map1, DA_I_Tropical_Map1,
DA_I_Desert_Map1, DA_I_PortTown_Map1) :
  - spawn constant : 2 Skull toutes les 3 s (au lieu de 2 s) ;
  - jour, vague à 10 s : 15 Skull (au lieu de 20) ;
  - nuit, vagues en cercle à 0 s et à 30 s (x2) : 15 Skull (au lieu de 20) ;
  - overtime : 15 Skull_Nemesis toutes les 30 s (au lieu de 20).
Carte au trésor d'objectif (BP_InteractiveObject_ObjectiveTreasureMap) : embuscade 15 Skull + 5 Skull_Gun + 2 Skull_Bomb (au lieu de 20 / 7 / 3).
Relançable : seules les anciennes valeurs sont changées (le reste est rapporté, pas touché).

Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/tune_flow_spawns_2026_11_14.py"
Ou depuis l'éditeur ouvert : Output Log, menu Cmd -> Python, py "<projet>/Tools/EditorScripts/tune_flow_spawns_2026_11_14.py"
Rapport : Saved/Logs/tune_flow_spawns_2026_11_14.txt
"""
import os
import unreal

ISLANDS = ["DA_I_StartIsland_Map1", "DA_I_Tropical_Map1", "DA_I_Desert_Map1", "DA_I_PortTown_Map1"]
ISLAND_DIR = "/Game/Core/Islands/"
TREASURE_MAP = "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_ObjectiveTreasureMap"
AMBUSH = {"FoeCharacter_Skull": (20, 15), "FoeCharacter_Skull_Gun": (7, 5), "FoeCharacter_Skull_Bomb": (3, 2)}
report = []


def log(msg):
    unreal.log(f"[tune_flow_spawns] {msg}")
    report.append(msg)


def cname(c):
    n = c.get_name() if c else "None"
    return n[:-2] if n.endswith("_C") else n


def shape(e):
    return str(e.get_editor_property("shape")).upper()


def retune_events(da, prop, match, old, new, label):
    """match(event) choisit les vagues ; leurs entrées de NME à old passent à new"""
    events = list(da.get_editor_property(prop))
    changed = False
    for i, e in enumerate(events):
        if not match(e):
            continue
        foes = list(e.get_editor_property("foes"))
        for k, f in enumerate(foes):
            count = f.get_editor_property("count")
            who = cname(f.get_editor_property("foe_class"))
            if count == old:
                f.set_editor_property("count", new)
                foes[k] = f
                changed = True
                log(f"  {label} [{i}] t={e.get_editor_property('time'):g}s : {who} {old} -> {new}")
            else:
                log(f"  {label} [{i}] : {who} à {count} (pas {old}) -> pas touché")
        e.set_editor_property("foes", foes)
        events[i] = e
    if changed:
        da.set_editor_property(prop, events)
    return changed


for name in ISLANDS:
    path = ISLAND_DIR + name
    da = unreal.EditorAssetLibrary.load_asset(path)
    if not da:
        log(f"{path} : INTROUVABLE")
        continue
    log(path)
    dirty = False
    cs = da.get_editor_property("constant_spawn")
    interval = cs.get_editor_property("interval")
    if abs(interval - 2.0) < 0.01:
        cs.set_editor_property("interval", 3.0)
        da.set_editor_property("constant_spawn", cs)
        dirty = True
        log(f"  spawn constant : {cs.get_editor_property('count')} NME toutes les 2 s -> 3 s")
    else:
        log(f"  spawn constant : intervalle {interval:g} s (pas 2) -> pas touché")
    dirty |= retune_events(da, "day_events", lambda e: abs(e.get_editor_property("time") - 10.0) < 0.01 and "OFF_SCREEN" in shape(e), 20, 15, "jour")
    dirty |= retune_events(da, "night_events", lambda e: "CIRCLE" in shape(e), 20, 15, "nuit")
    dirty |= retune_events(da, "overtime_events", lambda e: "CIRCLE" in shape(e), 20, 15, "overtime")
    if dirty:
        log(f"  sauvé {unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)}")

cls = unreal.EditorAssetLibrary.load_blueprint_class(TREASURE_MAP)
cdo = unreal.get_default_object(cls)
groups = list(cdo.get_editor_property("ambush_groups"))
log(TREASURE_MAP)
changed = False
for i, g in enumerate(groups):
    classes = [cname(c) for c in g.get_editor_property("classes")]
    count = g.get_editor_property("count")
    key = classes[0] if len(classes) == 1 else None
    if key in AMBUSH and count == AMBUSH[key][0]:
        g.set_editor_property("count", AMBUSH[key][1])
        groups[i] = g
        changed = True
        log(f"  embuscade {key} : {count} -> {AMBUSH[key][1]}")
    else:
        log(f"  embuscade {classes} à {count} -> pas touché")
if changed:
    cdo.set_editor_property("ambush_groups", groups)
    log(f"  sauvé {unreal.EditorAssetLibrary.save_asset(TREASURE_MAP, only_if_is_dirty=False)}")

path = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "tune_flow_spawns_2026_11_14.txt")
with open(path, "w", encoding="utf-8") as file:
    file.write("\n".join(report))
# lancé par -ExecutePythonScript : on ferme ; lancé depuis l'éditeur ouvert (Output Log, py "...") : on le laisse ouvert
if "executepythonscript" in unreal.SystemLibrary.get_command_line().lower():
    unreal.SystemLibrary.quit_editor()
