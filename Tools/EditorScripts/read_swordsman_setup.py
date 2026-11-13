# Lecture seule : armes, sets et pools du Swordsman (préparation de « The Gun was better »).
import os
import unreal

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
report = []


def log(msg):
    report.append(str(msg))


def find(name):
    for a in registry.get_assets_by_path("/Game", recursive=True):
        if str(a.asset_name) == name:
            return str(a.package_name)
    return None


def components(path):
    bp = assets.load_asset(path)
    out = []
    for h in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        d = lib.get_data(h)
        obj = lib.get_object(d)
        out.append((str(lib.get_variable_name(d)), obj.get_class().get_name() if obj else None, lib.is_inherited_component(d)))
    return bp, out


try:
    for name in ("BP_BattleSword", "BP_Epee", "BP_Pistolet", "BP_BattlePetoir", "BP_AutoPetoir"):
        path = find(name)
        log(f"== {name} : {path}")
        if not path:
            continue
        bp, comps = components(path)
        log(f"   parent : {unreal.BlueprintEditorLibrary.get_parent_class(bp).get_name() if hasattr(unreal.BlueprintEditorLibrary, 'get_parent_class') else '?'}")
        for c in comps:
            log(f"   {c}")
        cdo = unreal.get_default_object(assets.load_blueprint_class(path))
        log(f"   display_name {cdo.get_editor_property('display_name')}")
    for name in ("DA_WeaponSet_AkimboSabre", "DA_WeaponSet_SabrePistolet", "DA_WeaponSet_AkimboBattlePetoir"):
        path = find(name)
        log(f"== {name} : {path}")
        if path:
            s = assets.load_asset(path)
            log(f"   {s.get_editor_property('display_name')} droite {s.get_editor_property('right_hand_weapon')} gauche {s.get_editor_property('left_hand_weapon')}")
    for name in ("DA_Up_Weapon_Bloodlust", "DA_Up_Weapon_Ricochet", "DA_Up_MoveAction_Roll"):
        path = find(name)
        log(f"== {name} : {path}")
        if path:
            u = assets.load_asset(path)
            for p in ("required_weapon_classes", "unlock_shards", "bonus_shards", "unlock_weight", "bonus_weight", "super_weight", "max_bonus_stacks", "bonus_gold_value", "twin_upgrade", "icon", "super_name", "super_description"):
                try:
                    log(f"   {p} = {u.get_editor_property(p)}")
                except Exception as e:
                    log(f"   {p} : {e}")
    for name in ("PlayerCharacter_mouss", "PlayerCharacter_JohnnyPlaytest", "PlayerCharacter_HiredMouss"):
        path = find(name)
        log(f"== {name} : {path}")
        if path:
            cdo = unreal.get_default_object(assets.load_blueprint_class(path))
            pool = cdo.get_editor_property("upgrade_pool")
            for i, u in enumerate(pool):
                log(f"   {i} {u.get_name() if u else None}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "read_swordsman_setup.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
