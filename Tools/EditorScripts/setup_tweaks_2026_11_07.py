"""
Réglages du 2026-11-07 (demandes de Paul), relançable (ne touche une valeur que si elle a encore l'ancienne valeur) :
  - Boss Gun : Roll360shot 2x moins loin (Roll Distance 1200 -> 600) sur BP_NME_BossPistol.
  - Elite Gun / Boss Gun : portée d'amorce de l'attaque (Attack Range du NME shot) x1.5 sur BP_NME_ElitePistol et BP_NME_BossPistol
    (seulement s'ils ont encore la portée héritée de BP_NME_Pistol).
  - Personnages joueurs : Interaction Distance 200 -> 300 ; nage : Swim Stamina Cost et Drown Health Cost à 1.
  - BP_FlowMod : Next Islands Gap 50000 -> 25000.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_tweaks_2026_11_07.py"
Rapport : Saved/Logs/setup_tweaks_2026_11_07.txt
"""
import os
import unreal

WEAPON_DIR = "/Game/Placeable/Weapons/Foe"
PISTOL_BP = f"{WEAPON_DIR}/BP_NME_Pistol"
ELITE_PISTOL_BP = f"{WEAPON_DIR}/BP_NME_ElitePistol"
BOSS_PISTOL_BP = f"{WEAPON_DIR}/BP_NME_BossPistol"
FLOW_MOD_BP = "/Game/Core/GameModes/BP_FlowMod"
PLAYER_DIRS = ["/Game/Placeable/Character/Player", "/Game/Placeable/Character/Summon"]

RANGE_MULTIPLIER = 1.5
OLD_ROLL_DISTANCE, NEW_ROLL_DISTANCE = 1200.0, 600.0
OLD_INTERACTION, NEW_INTERACTION = 200.0, 300.0
SWIM_COST = 1.0
OLD_GAP, NEW_GAP = 50000.0, 25000.0

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_tweaks_2026_11_07] {msg}")
    report.append(msg)


def save(path):
    assets.save_asset(path, only_if_is_dirty=False)


def component_template(bp, component_class):
    """Composant du Blueprint (hérité compris : le gabarit propre à ce Blueprint)."""
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(handle)
        obj = lib.get_object_for_blueprint(data, bp)
        if obj and isinstance(obj, component_class):
            return obj
    return None


def close(a, b):
    return abs(a - b) < 0.01


def tweak_pistols():
    base_bp = assets.load_asset(PISTOL_BP)
    base_shot = component_template(base_bp, unreal.YSFoeShotAction)
    base_range = base_shot.get_editor_property("AttackRange")
    log(f"BP_NME_Pistol : Attack Range {base_range:.0f} (Skull Gun, inchangé)")
    for path in (ELITE_PISTOL_BP, BOSS_PISTOL_BP):
        bp = assets.load_asset(path)
        shot = component_template(bp, unreal.YSFoeShotAction)
        current = shot.get_editor_property("AttackRange")
        target = base_range * RANGE_MULTIPLIER
        if close(current, base_range):
            shot.set_editor_property("AttackRange", target)
            log(f"{path} : Attack Range {current:.0f} -> {target:.0f}")
        else:
            log(f"{path} : Attack Range {current:.0f} (déjà différent de la portée héritée, pas touché)")
        if path == BOSS_PISTOL_BP:
            roll = component_template(bp, unreal.YSFoeRoll360ShotAction)
            distance = roll.get_editor_property("RollDistance")
            if close(distance, OLD_ROLL_DISTANCE):
                roll.set_editor_property("RollDistance", NEW_ROLL_DISTANCE)
                log(f"{path} : Roll360shot Roll Distance {distance:.0f} -> {NEW_ROLL_DISTANCE:.0f}")
            else:
                log(f"{path} : Roll360shot Roll Distance {distance:.0f} (pas touché)")
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        save(path)


def tweak_players():
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    for folder in PLAYER_DIRS:
        for data in registry.get_assets_by_path(folder, recursive=True):
            if str(data.asset_class_path.asset_name) != "Blueprint":
                continue
            path = str(data.package_name)
            cls = assets.load_blueprint_class(path)
            cdo = unreal.get_default_object(cls) if cls else None
            if not isinstance(cdo, unreal.YSPlayerCharacter):
                continue
            changes = []
            interaction = cdo.get_editor_property("InteractionDistance")
            if close(interaction, OLD_INTERACTION):
                cdo.set_editor_property("InteractionDistance", NEW_INTERACTION)
                changes.append(f"Interaction Distance {interaction:.0f} -> {NEW_INTERACTION:.0f}")
            for name in ("SwimStaminaCost", "DrownHealthCost"):
                value = cdo.get_editor_property(name)
                if not close(value, SWIM_COST):
                    cdo.set_editor_property(name, SWIM_COST)
                    changes.append(f"{name} {value:g} -> {SWIM_COST:g}")
            if changes:
                save(path)
            log(f"{path} : {', '.join(changes) if changes else 'rien à changer'}")


def tweak_flow_mod():
    cdo = unreal.get_default_object(assets.load_blueprint_class(FLOW_MOD_BP))
    gap = cdo.get_editor_property("NextIslandsGap")
    if close(gap, OLD_GAP):
        cdo.set_editor_property("NextIslandsGap", NEW_GAP)
        save(FLOW_MOD_BP)
        log(f"BP_FlowMod : Next Islands Gap {gap:.0f} -> {NEW_GAP:.0f}")
    else:
        log(f"BP_FlowMod : Next Islands Gap {gap:.0f} (pas touché)")


try:
    tweak_pistols()
    tweak_players()
    tweak_flow_mod()
    log("Terminé")
except Exception as error:  # rapport même en cas d'erreur
    log(f"ERREUR : {error}")
    raise
finally:
    path = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_tweaks_2026_11_07.txt")
    with open(path, "w", encoding="utf-8") as file:
        file.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
