"""
Captures de vérification (lecture seule, rien n'est sauvegardé) : tête du kraken entourée de 8 tentacules, à côté d'un Galion et d'un sloop.
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/capture_kraken.py"
Images : Saved/Screenshots/Kraken/*.png
"""
import math
import os
import unreal

OUT = os.path.join(unreal.Paths.project_saved_dir(), "Screenshots", "Kraken")
assets = unreal.EditorAssetLibrary
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report = []


def spawn(cls, loc, rot=unreal.Rotator()):
    return actors.spawn_actor_from_class(cls, loc, rot)


def capture(name, loc, rot, fov=60.0, source=unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR):
    world = unreal.EditorLevelLibrary.get_editor_world()
    rt = unreal.RenderingLibrary.create_render_target2d(world, 1280, 720, unreal.TextureRenderTargetFormat.RTF_RGBA8)
    cap = spawn(unreal.SceneCapture2D, loc, rot)
    comp = cap.get_editor_property("capture_component2d")
    comp.set_editor_property("texture_target", rt)
    comp.set_editor_property("fov_angle", fov)
    comp.set_editor_property("capture_source", source)
    comp.capture_scene()
    unreal.RenderingLibrary.export_render_target(world, rt, OUT, name + ".png")
    report.append(f"{name} écrit")


try:
    os.makedirs(OUT, exist_ok=True)
    unreal.get_editor_subsystem(unreal.LevelEditorSubsystem).new_level("/Temp/KrakenCapture")
    spawn(unreal.DirectionalLight, unreal.Vector(0, 0, 1000), unreal.Rotator(roll=0, pitch=-40, yaw=30))
    spawn(unreal.SkyLight, unreal.Vector(0, 0, 500))
    floor = spawn(unreal.StaticMeshActor, unreal.Vector(0, 0, -30))
    floor.static_mesh_component.set_static_mesh(assets.load_asset("/Engine/BasicShapes/Plane"))
    floor.set_actor_scale3d(unreal.Vector(2000, 2000, 1))
    floor.static_mesh_component.set_material(0, assets.load_asset("/Game/Art/Materials/PH_materials/m_PH_monochrome_Blue"))
    spawn(assets.load_blueprint_class("/Game/Art/Props/Kraken/BP_Prop_KrakenHead"), unreal.Vector(0, 0, 0), unreal.Rotator(yaw=180))
    tentacle = assets.load_blueprint_class("/Game/Art/Props/Kraken/BP_Prop_KrakenTentacle")
    for i in range(8):
        a = 22.5 + 45.0 * i
        t = spawn(tentacle, unreal.Vector(math.cos(math.radians(a)) * 2400, math.sin(math.radians(a)) * 2400, 0), unreal.Rotator(yaw=a))
        s = 1.0 if i % 2 == 0 else 0.85
        t.set_actor_scale3d(unreal.Vector(s, s, s))
    spawn(assets.load_blueprint_class("/Game/Placeable/Ship/BP_GameBoat_Galion"), unreal.Vector(-3000, -9000, -30), unreal.Rotator(yaw=90))
    spawn(assets.load_blueprint_class("/Game/Placeable/Ship/BP_GameBoat_Sloop"), unreal.Vector(-3000, 6500, -30), unreal.Rotator(yaw=90))
    capture("kraken_front", unreal.Vector(-11000, 0, 3500), unreal.Rotator(pitch=-12, yaw=0), 70.0)
    capture("kraken_top", unreal.Vector(-9000, -2000, 9000), unreal.Rotator(pitch=-45, yaw=10), 70.0)
    capture("kraken_ships", unreal.Vector(-16000, -1000, 6000), unreal.Rotator(pitch=-18, yaw=5), 75.0)
    capture("kraken_close", unreal.Vector(-5500, -2500, 1500), unreal.Rotator(pitch=-2, yaw=22), 60.0)
    capture("kraken_face", unreal.Vector(-5000, 0, 1300), unreal.Rotator(pitch=2, yaw=0), 60.0)
    report.append("OK")
except Exception as e:
    import traceback
    report.append(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "capture_kraken.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
