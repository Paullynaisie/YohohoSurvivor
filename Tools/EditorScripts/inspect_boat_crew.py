"""
Lecture seule (2026-11-10) : valeurs utiles pour l'équipage du bateau, les échelles de corde, le brigantin, les zones de dépôt.
  UnrealEditor-Cmd.exe YohohoSurvivor.uproject -run=pythonscript -script="<projet>/Tools/EditorScripts/inspect_boat_crew.py" -unattended
Rapport : Saved/Logs/inspect_boat_crew.txt
"""
import os
import unreal

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    report.append(str(msg))


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def fmt(vec):
    return f"({vec.x:.1f}, {vec.y:.1f}, {vec.z:.1f})"


def rot(r):
    return f"(R {r.roll:.1f}, P {r.pitch:.1f}, Y {r.yaw:.1f})"


def dump_components(path, filter_fn=None):
    bp = assets.load_asset(path)
    log(f"== {path}")
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(handle)
        obj = lib.get_object(data)
        if not obj:
            continue
        name = obj.get_name().replace("_GEN_VARIABLE", "")
        if filter_fn and not filter_fn(name, obj):
            continue
        line = f"  {name} [{obj.get_class().get_name()}]"
        if isinstance(obj, unreal.SceneComponent):
            parent = obj.get_attach_parent()
            line += f" parent={parent.get_name().replace('_GEN_VARIABLE', '') if parent else '-'}"
            line += f" loc={fmt(obj.get_editor_property('relative_location'))} rot={rot(obj.get_editor_property('relative_rotation'))} scale={fmt(obj.get_editor_property('relative_scale3d'))}"
        if isinstance(obj, unreal.StaticMeshComponent):
            mesh = obj.get_editor_property("static_mesh")
            mat = obj.get_material(0)
            line += f" mesh={mesh.get_path_name() if mesh else '-'} mat={mat.get_path_name() if mat else '-'} coll={obj.get_collision_profile_name()}"
        if isinstance(obj, unreal.ChildActorComponent):
            cls = obj.get_editor_property("child_actor_class")
            line += f" child={cls.get_name() if cls else '-'}"
        if isinstance(obj, unreal.DecalComponent):
            line += f" decal_size={fmt(obj.get_editor_property('decal_size'))}"
        log(line)


def prop(obj, name):
    try:
        return obj.get_editor_property(name)
    except Exception as e:
        return f"<{e!r}>"


try:
    ships = {
        "Sloop": "/Game/Placeable/Ship/BP_GameBoat_Sloop",
        "Brigantin": "/Game/Placeable/Ship/BP_GameBoat_Brigantin",
        "Galion": "/Game/Placeable/Ship/BP_GameBoat_Galion",
        "Pirate": "/Game/Placeable/Ship/BP_PirateShip",
        "Longboat": "/Game/Placeable/Ship/BP_Longboat",
        "NMEGalion": "/Game/Placeable/Ship/BP_NME_Galion",
    }
    for label, path in ships.items():
        ship = cdo(path)
        hull = prop(ship, "HullProbePoints")
        log(f"#### {label} hull={[(round(p.x), round(p.y)) for p in hull] if isinstance(hull, list) or hasattr(hull, '__iter__') else hull}")
        for comp_name in ("HelmStation", "CoreStation", "LongboatStation", "CrowsNestStation", "BoardingAccess", "BoardingAccessStarboard",
                          "CannonFront", "CannonFront2", "CannonLeft", "CannonRight", "CannonLeft2", "CannonRight2"):
            comp = prop(ship, comp_name)
            if not isinstance(comp, unreal.ChildActorComponent):
                log(f"  {comp_name}: {comp}")
                continue
            cls = comp.get_editor_property("child_actor_class")
            tpl = comp.get_editor_property("child_actor_template")
            extra = ""
            if tpl:
                for p in ("CameraArmLength", "AcceptRange", "AccessReach", "AccessHalfWidth", "BoardPoint", "SpawnDistanceBehindHelm", "Cooldown"):
                    try:
                        val = tpl.get_editor_property(p)
                        extra += f" {p}={fmt(val) if isinstance(val, unreal.Vector) else val}"
                    except Exception:
                        pass
            log(f"  {comp_name}: {cls.get_name() if cls else '-'} loc={fmt(comp.get_editor_property('relative_location'))} rot={rot(comp.get_editor_property('relative_rotation'))}{extra}")
        log(f"  Weight={prop(ship, 'Weight')} MaxForwardSpeed={prop(ship, 'MaxForwardSpeed')}")
    for path in ("/Game/Placeable/Ship/BP_GameBoat_Sloop", "/Game/Placeable/Ship/BP_GameBoat_Brigantin", "/Game/Placeable/Ship/BP_PirateShip"):
        dump_components(path)

    dump_components("/Game/Placeable/Character/Player/PlayerCharacter_JohnnyPlaytest")
    jp = cdo("/Game/Placeable/Character/Player/PlayerCharacter_JohnnyPlaytest")
    for p in ("HandLocation", "HandRotation", "CustomizationColorComponentName", "SwimSpeedMultiplier", "CharacterScale"):
        log(f"  JohnnyPlaytest.{p} = {prop(jp, p)}")
    for comp_name in ("BodyMesh", "BodyPivot", "FaceDecal", "LeftHand", "RightHand", "LeftHandMesh", "RightHandMesh", "CapsuleComponent"):
        comp = prop(jp, comp_name)
        if isinstance(comp, unreal.SceneComponent):
            mesh = comp.get_editor_property("static_mesh") if isinstance(comp, unreal.StaticMeshComponent) else None
            mat = comp.get_material(0) if isinstance(comp, unreal.PrimitiveComponent) else None
            log(f"  JP.{comp_name} loc={fmt(comp.get_editor_property('relative_location'))} rot={rot(comp.get_editor_property('relative_rotation'))} scale={fmt(comp.get_editor_property('relative_scale3d'))} mesh={mesh.get_path_name() if mesh else '-'} mat={mat.get_path_name() if mat else '-'}")
            if isinstance(comp, unreal.CapsuleComponent):
                log(f"    capsule r={comp.get_editor_property('capsule_radius')} hh={comp.get_editor_property('capsule_half_height')}")
            if isinstance(comp, unreal.DecalComponent):
                log(f"    decal size={fmt(comp.get_editor_property('decal_size'))} mat={comp.get_editor_property('decal_material')} tex={prop(comp, 'FaceTexture')}")
        else:
            log(f"  JP.{comp_name}: {comp}")

    for path in ("/Game/Placeable/Character/Player/PlayerCharacter_mouss", "/Game/Placeable/Character/Player/PlayerCharacter_Cannoneer",
                 "/Game/Placeable/Character/Player/PlayerCharacter_Harquebusier", "/Game/Placeable/Character/Player/PlayerCharacter_Quartermaster",
                 "/Game/Placeable/Character/Player/PlayerCharacter_Captain", "/Game/Placeable/Character/Summon/CaptainMinion"):
        try:
            log(f"{path.rsplit('/', 1)[1]}.SwimSpeedMultiplier = {prop(cdo(path), 'SwimSpeedMultiplier')}")
        except Exception as e:
            log(f"{path} : {e!r}")

    for path in ("/Game/Placeable/InteractiveObjects/BP_InteractiveObject_Carpenter", "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_Medic",
                 "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_Compass"):
        if not assets.does_asset_exist(path):
            log(f"absent : {path}")
            continue
        obj = cdo(path)
        log(f"#### {path}")
        for p in ("DisplayName", "ObjectiveName", "CarryLocation", "CarryRotation", "CarrierSpeedMultiplier", "FoeAttractionMultiplier", "ItemScale", "bShowIndicator",
                  "bRevealIndicatorNearby", "IndicatorIcon", "IndicatorColor", "IndicatorHeight", "bFloatsOnWater", "bRisesToSurface", "DeliveredNPCClass", "InteractPrompt"):
            val = prop(obj, p)
            log(f"  {p} = {fmt(val) if isinstance(val, unreal.Vector) else (rot(val) if isinstance(val, unreal.Rotator) else val)}")
        dump_components(path)

    dump_components("/Game/Placeable/Weapons/Player/Wp_Guns/BP_Sniper")
    dump_components("/Game/Placeable/Weapons/BP_Arme")

    flow = cdo("/Game/Core/GameModes/BP_FlowMod")
    log(f"FlowMod CrewItemClasses = {[c.get_name() if c else None for c in prop(flow, 'CrewItemClasses')]}")
    log(f"FlowMod CompassItemClass = {prop(flow, 'CompassItemClass')}")
    for asset in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path("/Game/Core/Islands", recursive=True):
        data = asset.get_asset()
        if isinstance(data, unreal.YSIslandData):
            log(f"{asset.asset_name}: BoatZoneRadius={prop(data, 'BoatZoneRadius')} StartBoatClass={prop(data, 'StartBoatClass')}")
    for asset in unreal.AssetRegistryHelpers.get_asset_registry().get_assets_by_path("/Game/Art/Textures", recursive=True):
        if str(asset.asset_name).startswith("t_i_"):
            log(f"icon {asset.package_name}")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "inspect_boat_crew.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
