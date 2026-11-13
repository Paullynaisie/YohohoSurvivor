"""Lecture seule : composants des BP de NME (/Game/Placeable/Character/Foe) qui ont une collision active
(type d'objet, réponse au canal Pawn) : cherche ce qui bloque un joueur intangible (capsule qui ignore Pawn et Foe).
UnrealEditor-Cmd.exe <uproject> -run=pythonscript -script="Tools/EditorScripts/check_foe_collisions.py" -unattended
Rapport : Saved/Logs/check_foe_collisions.txt
"""
import os
import unreal

lines = []


def log(text):
    lines.append(text)
    unreal.log(text)


registry = unreal.AssetRegistryHelpers.get_asset_registry()
assets = registry.get_assets_by_path("/Game/Placeable/Character/Foe", recursive=True)
for data in assets:
    if str(data.asset_class_path.asset_name) != "Blueprint":
        continue
    path = str(data.package_name)
    bp = unreal.load_asset(path)
    cls = unreal.EditorAssetLibrary.load_blueprint_class(path)
    if not cls:
        continue
    cdo = unreal.get_default_object(cls)
    comps = []
    try:
        comps = unreal.SubobjectDataSubsystem
    except Exception:
        pass
    # composants C++ (CDO) + composants ajoutés dans le BP (SubobjectDataSubsystem)
    found = []
    sub = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    handles = sub.k2_gather_subobject_data_for_blueprint(bp)
    for h in handles:
        d = unreal.SubobjectDataBlueprintFunctionLibrary.get_data(h)
        obj = unreal.SubobjectDataBlueprintFunctionLibrary.get_object(d)
        if not isinstance(obj, unreal.PrimitiveComponent):
            continue
        enabled = obj.get_collision_enabled()
        if enabled == unreal.CollisionEnabled.NO_COLLISION:
            continue
        found.append(f"    {obj.get_name()} ({obj.get_class().get_name()}) : {enabled}, objet {obj.get_collision_object_type()}, "
                     f"Pawn {obj.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)}, profil {obj.get_collision_profile_name()}")
    log(f"{path} :")
    for f in found:
        log(f)

out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "check_foe_collisions.txt")
with open(out, "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
