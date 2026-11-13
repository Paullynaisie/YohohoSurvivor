# Upgrades d'arme du Swordsman « Blade Throw » et « BladeMaster » (2026-11-13, demande de Paul) :
# - BP_LesserSpinningBlade (enfant de AYSLesserSpinningBlade, réglages copiés de BP_SpinningSword, rotation x3, poignée en -35/0/0, level 1) ;
# - Ability de BP_BattleSword et BP_GunSword remplacée par UYSSwordsmanAbility (réglages copiés : Dashing Slash / Mark for Delete gardés) ;
# - DA_Up_Weapon_BladeThrow et DA_Up_Weapon_BladeMaster (2 Golden chacune), en fin de pool de Mouss, JohnnyPlaytest et HiredMouss.
# Relançable : ne recrée rien qui existe ; les réglages listés ici sont réappliqués.
import os
import unreal

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
ys = unreal.YSEditorScriptLibrary
T = unreal.Text

PLAYER_WEAPONS = "/Game/Placeable/Weapons/Player"
SPINNING = f"{PLAYER_WEAPONS}/BP_SpinningSword"
LESSER = f"{PLAYER_WEAPONS}/BP_LesserSpinningBlade"
SWORDS = [f"{PLAYER_WEAPONS}/Wp_Melee/BP_BattleSword", f"{PLAYER_WEAPONS}/Wp_Melee/BP_GunSword"]
UP_WEAPONS = "/Game/Core/Upgrades/Up_Character/Up_Weapons"
BLOODLUST = f"{UP_WEAPONS}/DA_Up_Weapon_Bloodlust"
CHARACTERS = ["/Game/Placeable/Character/Player/PlayerCharacter_mouss", "/Game/Placeable/Character/Player/PlayerCharacter_JohnnyPlaytest",
              "/Game/Placeable/Character/Player/PlayerCharacter_HiredMouss"]
UPGRADES = {
    "BladeThrow": (unreal.YSBladeThrowUpgrade, [
        ("Critical Mass", "Chaque critique de ce personnage donne une stack\nde Critical Mass au NME (5 max) ; à sa mort,\nSpinning Sword du level de ses stacks"),
        ("Swordsrang", "Lames 2x moins rapides ; au point de croisement,\nelles reviennent vers le joueur"),
    ]),
    "BladeMaster": (unreal.YSBladeMasterUpgrade, [
        ("Bladeguedon", "Blade Mastery avec une Self Spinning Sword level 5 :\nelle disparaît et 5 Lesser Spinning Blades\npartent en parallèle devant le joueur"),
        ("Bladékinésis", "Les Self Spinning Swords suivent le curseur\nà la vitesse du joueur"),
    ]),
}

report = []


def log(msg):
    report.append(str(msg))
    unreal.log(f"[setup_blade_upgrades] {msg}")


def component(bp, name):
    for h in sds.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(h)
        if str(lib.get_variable_name(data)) == name:
            return h, lib.get_object(data)
    return None, None


def golden(name, desc):
    variant = unreal.YSGoldenMoveAction()
    variant.set_editor_property("name", T(name))
    variant.set_editor_property("description", T(desc))
    return variant


try:
    # --- Lesser Spinning Blade ---
    if assets.does_asset_exist(LESSER):
        log(f"{LESSER} : existe déjà")
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.YSLesserSpinningBlade)
        bp = tools.create_asset("BP_LesserSpinningBlade", PLAYER_WEAPONS, unreal.Blueprint, factory)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        lesser_cdo = unreal.get_default_object(bp.generated_class())
        spinning_cdo = unreal.get_default_object(assets.load_blueprint_class(SPINNING))
        count = ys.copy_editable_properties(spinning_cdo, lesser_cdo)
        lesser_cdo.set_editor_property("rotation_speed", spinning_cdo.get_editor_property("rotation_speed") * 3.0)
        lesser_cdo.set_editor_property("sword_location", unreal.Vector(-35.0, 0.0, 0.0))
        lesser_cdo.set_editor_property("max_level", 1)
        assets.save_asset(LESSER, only_if_is_dirty=False)
        log(f"{LESSER} : créé ({count} réglages copiés de BP_SpinningSword, rotation x3, poignée -35/0/0)")

    # --- Ability des épées du Swordsman ---
    for path in SWORDS:
        bp = assets.load_asset(path)
        handle, ability = component(bp, "DashingSlash")
        if ability is None:
            log(f"{path} : pas de composant DashingSlash")
            continue
        if isinstance(ability, unreal.YSSwordsmanAbility):
            log(f"{path} : Swordsman Ability déjà en place")
            continue
        root = sds.k2_gather_subobject_data_for_blueprint(bp)[0]
        new_handle, fail = sds.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=root, new_class=unreal.YSSwordsmanAbility, blueprint_context=bp))
        if not fail.is_empty():
            raise RuntimeError(f"{path} : ajout impossible : {fail}")
        new_ability = lib.get_object(lib.get_data(new_handle))
        count = ys.copy_editable_properties(ability, new_ability)
        old_class = ability.get_class().get_name()
        sds.delete_subobjects(root, [handle], bp)
        sds.rename_subobject(new_handle, T("DashingSlash"))
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"{path} : {old_class} remplacé par Swordsman Ability ({count} réglages copiés)")

    # --- Upgrades ---
    bloodlust = assets.load_asset(BLOODLUST)
    sword_classes = [assets.load_blueprint_class(p) for p in SWORDS]
    created = []
    for key, (cls, goldens) in UPGRADES.items():
        path = f"{UP_WEAPONS}/DA_Up_Weapon_{key}"
        if assets.does_asset_exist(path):
            upgrade = assets.load_asset(path)
            log(f"{path} : existe déjà")
        else:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property("data_asset_class", cls)
            upgrade = tools.create_asset(f"DA_Up_Weapon_{key}", UP_WEAPONS, cls, factory)
            for prop in ("unlock_shards", "bonus_shards", "unlock_weight", "bonus_weight", "super_weight"):
                upgrade.set_editor_property(prop, bloodlust.get_editor_property(prop))
            log(f"{path} : créé (shards et poids du Bloodlust)")
        upgrade.set_editor_property("required_weapon_classes", sword_classes)
        upgrade.set_editor_property("golden_variants", [golden(n, d) for n, d in goldens])
        assets.save_asset(path, only_if_is_dirty=False)
        created.append(upgrade)

    # --- Pools ---
    for path in CHARACTERS:
        bp = assets.load_asset(path)
        cdo = unreal.get_default_object(bp.generated_class())
        pool = list(cdo.get_editor_property("upgrade_pool"))
        added = []
        for upgrade in created:
            if upgrade not in pool:
                pool.append(upgrade)
                added.append(f"{len(pool) - 1}:{upgrade.get_name()}")
        if added:
            cdo.set_editor_property("upgrade_pool", pool)
            unreal.BlueprintEditorLibrary.compile_blueprint(bp)
            assets.save_asset(path, only_if_is_dirty=False)
        log(f"{path} : {added if added else 'déjà à jour'} ; index {[pool.index(u) for u in created]}")

    # --- Relecture ---
    for path in SWORDS:
        _, ability = component(assets.load_asset(path), "DashingSlash")
        log(f"relu {path}.DashingSlash : {ability.get_class().get_name()}, dégâts {ability.get_editor_property('damage')}, recharge {ability.get_editor_property('cooldown')}, "
            f"Blade Throw {ability.get_editor_property('blade_throw_cooldown')} s, Blade Mastery {ability.get_editor_property('blade_mastery_cooldown')} s")
    lesser_cdo = unreal.get_default_object(assets.load_blueprint_class(LESSER))
    log(f"relu Lesser : rotation {lesser_cdo.get_editor_property('rotation_speed')}, poignée {lesser_cdo.get_editor_property('sword_location')}, level max {lesser_cdo.get_editor_property('max_level')}")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_blade_upgrades.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
