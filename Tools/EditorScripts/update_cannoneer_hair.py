"""
Retouches de la coiffure du Cannoneer (demande de Paul) :
  - tresses plaquées 2x plus larges : bosses Braid*_* (échelle Y x2) et cordes BraidRope1..4 (rayon des segments x2)
  - BraidRope5 (corde placée par Paul devant le visage) : pend droit vers le bas (Trail Acceleration 0)
Relançable : les largeurs sont fixées (pas multipliées à chaque lancement).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/update_cannoneer_hair.py"
Rapport : Saved/Logs/update_cannoneer_hair.txt
"""
import os
import unreal

CANNONEER = "/Game/Placeable/Character/Player/PlayerCharacter_Cannoneer"
BRAID_WIDTH = 0.30          # échelle Y des bosses (0.15 au départ)
BACK_ROPE_RADIUS = 8.0      # rayon des cordes qui pendent derrière (4 au départ)
FRONT_ROPE = "BraidRope5"

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[update_cannoneer_hair] {msg}")
    report.append(msg)


try:
    bp = assets.load_asset(CANNONEER)
    comps = {}
    for h in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(h))
        if obj:
            comps[str(lib.get_variable_name(lib.get_data(h)))] = obj

    bumps = 0
    for name, comp in comps.items():
        if name.startswith("Braid") and "_" in name and isinstance(comp, unreal.StaticMeshComponent):
            scale = comp.get_editor_property("relative_scale3d")
            comp.set_editor_property("relative_scale3d", unreal.Vector(scale.x, BRAID_WIDTH, scale.z))
            bumps += 1
    log(f"{bumps} bosses de tresses : largeur (échelle Y) {BRAID_WIDTH}")

    for i in range(1, 5):
        rope = comps.get(f"BraidRope{i}")
        if rope:
            rope.set_editor_property("segment_radius", BACK_ROPE_RADIUS)
    log(f"cordes BraidRope1..4 : rayon {BACK_ROPE_RADIUS}")

    front = comps.get(FRONT_ROPE)
    if front:
        front.set_editor_property("trail_acceleration", 0.0)
        log(f"{FRONT_ROPE} : Trail Acceleration 0 (pend droit vers le bas)")
    else:
        log(f"{FRONT_ROPE} introuvable")

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(CANNONEER, only_if_is_dirty=False)
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "update_cannoneer_hair.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
