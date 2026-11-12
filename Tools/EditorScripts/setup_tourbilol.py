"""
Boulet et Chaîne (Quartermaster, demande de Paul du 2026-11-12) :
  - BP_MorningStar : l'Ability Bumping Slash (composant « BumpingSlash ») est remplacée par « Tourbilol » (UYSTourbilolAbility,
    « TOURBILOL ! ») : réglages communs d'Auto swing copiés du Bumping Slash (HitTags, traînée...), puis 50 dégâts, recharge 10,
    pas d'icône ; Golden Explosion Class = BP_CannoneerBomb (comme le Rocket jump).
  - DA_Up_Weapon_TourbillonInfinie (UYSInfiniteTourbillonUpgrade, « Tourbillon infinie ») : réglages de DA_Up_Weapon_Yolo
    (poids, shards, arme requise BP_MorningStar), puis textes / valeurs de l'upgrade ; ajoutée au pool du Quartermaster.
Relançable : ne recrée rien qui existe (retouches de Paul gardées).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_tourbilol.py"
Rapport : Saved/Logs/setup_tourbilol.txt
"""
import os
import unreal

MORNING_STAR = "/Game/Placeable/Weapons/Player/Wp_Melee/BP_MorningStar"
BOMB = "/Game/Placeable/AMMO/BP_CannoneerBomb"
QUARTERMASTER = "/Game/Placeable/Character/Player/PlayerCharacter_Quartermaster"
UP_DIR = "/Game/Core/Upgrades/Up_Character/Up_Weapons"
YOLO = f"{UP_DIR}/DA_Up_Weapon_Yolo"
UP_NAME = "DA_Up_Weapon_TourbillonInfinie"
UP_PATH = f"{UP_DIR}/{UP_NAME}"
OLD_COMPONENT, NEW_COMPONENT = "BumpingSlash", "Tourbilol"

assets = unreal.EditorAssetLibrary
lib = unreal.SubobjectDataBlueprintFunctionLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
report = []


def log(msg):
    unreal.log(f"[setup_tourbilol] {msg}")
    report.append(msg)


def find_handle(subsystem, blueprint, name):
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        if str(lib.get_variable_name(lib.get_data(handle))) == name:
            return handle
    return None


try:
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)

    # --- BP_MorningStar : Tourbilol à la place du Bumping Slash
    bp = assets.load_asset(MORNING_STAR)
    old = find_handle(sds, bp, OLD_COMPONENT)
    if find_handle(sds, bp, NEW_COMPONENT):
        log(f"BP_MorningStar : {NEW_COMPONENT} déjà là (pas touché)")
    else:
        handles = sds.k2_gather_subobject_data_for_blueprint(bp)
        params = unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=unreal.YSTourbilolAbility, blueprint_context=bp)
        handle, fail_reason = sds.add_new_subobject(params)
        if not lib.is_handle_valid(handle):
            raise RuntimeError(f"ajout de {NEW_COMPONENT} impossible : {fail_reason}")
        sds.rename_subobject(handle, unreal.Text(NEW_COMPONENT))
        tourbilol = lib.get_object(lib.get_data(handle))
        if old:
            count = unreal.YSEditorScriptLibrary.copy_editable_properties(lib.get_object(lib.get_data(old)), tourbilol)
            log(f"BP_MorningStar : {count} réglages copiés du Bumping Slash")
        for prop, value in {
            "display_name": unreal.Text("TOURBILOL !"),
            "damage": 50.0,
            "cooldown": 10.0,
            "icon": None,
            "reset_auto_attack_on_use": False,
            "golden_explosion_class": assets.load_blueprint_class(BOMB),
        }.items():
            tourbilol.set_editor_property(prop, value)
        log(f"BP_MorningStar : {NEW_COMPONENT} (YSTourbilolAbility) ajouté")

    old = find_handle(sds, bp, OLD_COMPONENT)
    if old:
        root = sds.k2_gather_subobject_data_for_blueprint(bp)[0]
        removed = sds.delete_subobject(root, old, bp)
        log(f"BP_MorningStar : {OLD_COMPONENT} supprimé ({removed})")
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(MORNING_STAR, only_if_is_dirty=False)

    # --- upgrade d'arme Tourbillon infinie
    if assets.does_asset_exist(UP_PATH):
        upgrade = assets.load_asset(UP_PATH)
        log(f"{UP_NAME} existe déjà (pas touché)")
    else:
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.YSInfiniteTourbillonUpgrade)
        upgrade = tools.create_asset(UP_NAME, UP_DIR, unreal.YSInfiniteTourbillonUpgrade, factory)
        count = unreal.YSEditorScriptLibrary.copy_editable_properties(assets.load_asset(YOLO), upgrade)
        log(f"{UP_NAME} créé, {count} réglages copiés de DA_Up_Weapon_Yolo")
        defaults = unreal.get_default_object(unreal.YSInfiniteTourbillonUpgrade)
        for prop in ("display_name", "unlock_name", "unlock_description", "bonus_name", "bonus_description", "bonus_stat_label",
                     "bonus_silver_value", "bonus_gold_value", "max_bonus_stacks", "super_name", "super_description", "required_action_class"):
            upgrade.set_editor_property(prop, defaults.get_editor_property(prop))
        upgrade.set_editor_property("required_slot", unreal.YSActionSlot.ABILITY)
        assets.save_asset(UP_PATH, only_if_is_dirty=False)

    qm = unreal.get_default_object(assets.load_blueprint_class(QUARTERMASTER))
    pool = list(qm.get_editor_property("upgrade_pool"))
    if upgrade not in pool:
        pool.append(upgrade)
        qm.set_editor_property("upgrade_pool", pool)
        assets.save_asset(QUARTERMASTER, only_if_is_dirty=False)
        log(f"{UP_NAME} ajoutée au pool du Quartermaster (index {len(pool) - 1})")

    # --- relecture
    bp = assets.load_asset(MORNING_STAR)
    names = [str(lib.get_variable_name(lib.get_data(h))) for h in sds.k2_gather_subobject_data_for_blueprint(bp)]
    log("BP_MorningStar : composants " + ", ".join(n for n in names if n and n != "None"))
    handle = find_handle(sds, bp, NEW_COMPONENT)
    if handle:
        t = lib.get_object(lib.get_data(handle))
        log("Tourbilol : " + ", ".join(f"{p}={t.get_editor_property(p)}" for p in
            ("display_name", "damage", "cooldown", "base_turns", "turn_duration", "hit_tags", "golden_explosion_class", "slash_trail")))
    up = assets.load_asset(UP_PATH)
    log("Upgrade : " + ", ".join(f"{p}={up.get_editor_property(p)}" for p in
        ("display_name", "required_action_class", "required_slot", "required_weapon_classes", "bonus_gold_value", "max_bonus_stacks", "shard_system", "unlock_weight")))
    pool = unreal.get_default_object(assets.load_blueprint_class(QUARTERMASTER)).get_editor_property("upgrade_pool")
    log("pool du Quartermaster : " + ", ".join(f"{i} {u.get_name()}" for i, u in enumerate(pool) if u))
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_tourbilol.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
