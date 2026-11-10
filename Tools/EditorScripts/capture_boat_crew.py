"""
Captures de vérification (lecture seule, rien n'est sauvegardé) : les 5 membres d'équipage et le sloop / brigantin / galion avec leurs échelles.
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/capture_boat_crew.py"
Images : Saved/Screenshots/BoatCrew/*.png
"""
import os
import unreal

OUT = os.path.join(unreal.Paths.project_saved_dir(), "Screenshots", "BoatCrew")
CREW = ["Charpentier", "Medic", "Navigateur", "Cuisinier", "Flibustier"]
SHIPS = ["BP_GameBoat_Sloop", "BP_GameBoat_Brigantin", "BP_GameBoat_Galion"]
report = []

actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def spawn(cls, loc, rot=unreal.Rotator()):
    return actors.spawn_actor_from_class(cls, loc, rot)


def capture(name, loc, rot, fov=60.0, size=(1280, 720)):
    rt = unreal.RenderingLibrary.create_render_target2d(unreal.EditorLevelLibrary.get_editor_world(), size[0], size[1], unreal.TextureRenderTargetFormat.RTF_RGBA8)
    cap = spawn(unreal.SceneCapture2D, loc, rot)
    comp = cap.get_editor_property("capture_component2d")
    comp.set_editor_property("texture_target", rt)
    comp.set_editor_property("fov_angle", fov)
    comp.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    comp.capture_scene()
    unreal.RenderingLibrary.export_render_target(unreal.EditorLevelLibrary.get_editor_world(), rt, OUT, name)
    report.append(f"{name} écrit")


try:
    os.makedirs(OUT, exist_ok=True)
    level.new_level("/Temp/BoatCrewCapture")
    spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(roll=0, pitch=-50, yaw=30))
    spawn(unreal.SkyLight, unreal.Vector(0, 0, 500))
    floor = spawn(unreal.StaticMeshActor, unreal.Vector(0, 0, -5))
    floor.static_mesh_component.set_static_mesh(unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Plane"))
    floor.set_actor_scale3d(unreal.Vector(400, 400, 1))
    for i, name in enumerate(CREW):
        cls = unreal.EditorAssetLibrary.load_blueprint_class(f"/Game/Placeable/InteractiveObjects/BoatCrew/BP_BoatCrew_{name}")
        spawn(cls, unreal.Vector(0, (i - 2) * 180.0, 0), unreal.Rotator(yaw=180))
    # équipage vu de face (depuis -X, il regarde vers -X)
    capture("crew_front", unreal.Vector(-650, 0, 220), unreal.Rotator(pitch=-12, yaw=0), 70.0)
    capture("crew_top", unreal.Vector(-900, -300, 900), unreal.Rotator(pitch=-45, yaw=20), 60.0)
    for j, ship in enumerate(SHIPS):
        cls = unreal.EditorAssetLibrary.load_blueprint_class(f"/Game/Placeable/Ship/{ship}")
        y = 6000.0 + j * 6000.0
        spawn(cls, unreal.Vector(0, y, 0))
        # vu du côté bâbord (-Y), de haut
        capture(f"{ship}_port", unreal.Vector(-500, y - 4200, 2200), unreal.Rotator(pitch=-28, yaw=90), 70.0)
        capture(f"{ship}_ladder", unreal.Vector(-1400 * (1 if j else 0.3), y - 1500 * (0.5 + 0.25 * j), 300), unreal.Rotator(pitch=-5, yaw=70), 60.0)
    report.append("OK")
except Exception as e:
    import traceback
    report.append(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "capture_boat_crew.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
