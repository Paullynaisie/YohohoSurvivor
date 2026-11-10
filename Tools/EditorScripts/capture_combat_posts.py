"""
Captures de vérification (lecture seule, rien n'est sauvegardé) : chaque poste de combat du bateau à côté de son objet transportable.
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/capture_combat_posts.py"
Images : Saved/Screenshots/CombatPosts/*.png
"""
import os
import unreal

OUT = os.path.join(unreal.Paths.project_saved_dir(), "Screenshots", "CombatPosts")
POSTS = (("Cannon", "BP_ShipCannon"), ("Gatling", "BP_ShipGatling"), ("Mortar", "BP_ShipMortar"), ("GrapeShot", "BP_ShipGrapeShot"))
report = []
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
level = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)


def spawn(cls, loc, rot=unreal.Rotator()):
    return actors.spawn_actor_from_class(cls, loc, rot)


def capture(name, loc, rot, fov=50.0):
    world = unreal.EditorLevelLibrary.get_editor_world()
    rt = unreal.RenderingLibrary.create_render_target2d(world, 1280, 720, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    cap = spawn(unreal.SceneCapture2D, loc, rot)
    comp = cap.get_editor_property("capture_component2d")
    comp.set_editor_property("texture_target", rt)
    comp.set_editor_property("fov_angle", fov)
    comp.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    comp.capture_scene()
    unreal.RenderingLibrary.export_render_target(world, rt, OUT, name)
    report.append(f"{name} écrit")


try:
    os.makedirs(OUT, exist_ok=True)
    level.new_level("/Temp/CombatPostCapture")
    spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(roll=0, pitch=-50, yaw=30))
    spawn(unreal.SkyLight, unreal.Vector(0, 0, 500))
    floor = spawn(unreal.StaticMeshActor, unreal.Vector(0, 0, -5))
    floor.static_mesh_component.set_static_mesh(unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Plane"))
    floor.set_actor_scale3d(unreal.Vector(400, 400, 1))
    for i, (item, station) in enumerate(POSTS):
        y = i * 2000.0
        obj = spawn(unreal.EditorAssetLibrary.load_blueprint_class(f"/Game/Placeable/InteractiveObjects/CombatPosts/BP_CombatPostItem_{item}"), unreal.Vector(0, y - 150, 0))
        obj.set_actor_scale3d(unreal.Vector(1.5, 1.5, 1.5))  # Item Scale appliqué en jeu
        spawn(unreal.EditorAssetLibrary.load_blueprint_class(f"/Game/Placeable/Ship/Stations/{station}"), unreal.Vector(0, y + 150, 0))
        capture(f"{item}_side", unreal.Vector(0, y - 50, 120) + unreal.Vector(-60, -650, 150), unreal.Rotator(pitch=-15, yaw=80))
        capture(f"{item}_top", unreal.Vector(-450, y, 450), unreal.Rotator(pitch=-40, yaw=0))
    report.append("OK")
except Exception as e:
    import traceback
    report.append(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "capture_combat_posts.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
