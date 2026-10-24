"""
Réglages demandés par Paul (2026-10-24) :
  - JohnnyPlaytest : personnage de test (bEditorOnlyCharacter) = proposé dans l'éditeur, retiré des builds packagées
  - Summon the crew (composant « SummonCrew » de BP_CaptainHook) : recharge 20 s (défaut C++) ; « GoMinions » du Cursed Hook gardé à 10 s (les autres réglages nouveaux : invocation jusqu'au
    maximum, minion passif toutes les 5 s, gardent les valeurs par défaut du C++, aussi sur « GoMinions » du Cursed Hook)
  - relecture : Bump Elites des Bumping Slash / Meteor Jump (GreatAxe, MorningStar, GreatAnchor), réglages d'invocation des crochets
Relançable.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_captain_summon_tweaks.py"
Rapport : Saved/Logs/setup_captain_summon_tweaks.txt
"""
import os
import unreal

JOHNNY = "/Game/Placeable/Character/Player/PlayerCharacter_JohnnyPlaytest"
WEAPONS = "/Game/Placeable/Weapons/Player"
HOOK = f"{WEAPONS}/BP_CaptainHook"
CURSED_HOOK = f"{WEAPONS}/BP_CursedHook"
BUMP_WEAPONS = [f"{WEAPONS}/BP_GreatAxe", f"{WEAPONS}/BP_MorningStar", f"{WEAPONS}/BP_GreatAnchor"]
SUMMON_COOLDOWN = 20.0
GO_MINIONS_COOLDOWN = 10.0

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_captain_summon_tweaks] {msg}")
    report.append(msg)


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def action_components(path, cls):
    """(nom, template) des composants de ce type déclarés dans le Blueprint, + archétype vu par le CDO (composant hérité)"""
    bp = assets.load_asset(path)
    result = []
    for h in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(h)
        obj = lib.get_object(data)
        if isinstance(obj, cls) and all(o is not obj for _, o in result):
            name = str(lib.get_variable_name(data))
            result.append((name, obj))
            try:
                archetype = cdo(path).get_editor_property(name)
                if isinstance(archetype, cls) and archetype is not obj:
                    result.append((name, archetype))
            except Exception:
                pass
    return bp, result


try:
    # 1) JohnnyPlaytest : éditeur seulement
    johnny = cdo(JOHNNY)
    if not johnny.get_editor_property("editor_only_character"):
        johnny.set_editor_property("editor_only_character", True)
        assets.save_asset(JOHNNY, only_if_is_dirty=False)
    log(f"JohnnyPlaytest éditeur seulement : {cdo(JOHNNY).get_editor_property('editor_only_character')}")

    # 2) Summon the crew : recharge 20 s
    bp, summons = action_components(HOOK, unreal.YSSummonCrewAbility)
    changed = False
    for name, action in summons:
        if type(action) is unreal.YSSummonCrewAbility and abs(action.get_editor_property("cooldown") - SUMMON_COOLDOWN) > 0.001:
            action.set_editor_property("cooldown", SUMMON_COOLDOWN)
            changed = True
    if changed:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assets.save_asset(HOOK, only_if_is_dirty=False)

    # « Allez-y mes minions ! » (sous-classe) : garde la recharge des Abilities d'avant playtest (10 s), pas celle de Summon the crew
    bp, go_minions = action_components(CURSED_HOOK, unreal.YSGoMinionsAbility)
    changed = False
    for name, action in go_minions:
        if abs(action.get_editor_property("cooldown") - GO_MINIONS_COOLDOWN) > 0.001:
            action.set_editor_property("cooldown", GO_MINIONS_COOLDOWN)
            changed = True
    if changed:
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assets.save_asset(CURSED_HOOK, only_if_is_dirty=False)

    # relecture
    for path in (HOOK, CURSED_HOOK):
        _, actions = action_components(path, unreal.YSSummonCrewAbility)
        for name, action in actions:
            log(f"relu {path.split('/')[-1]}.{name} ({type(action).__name__}) : recharge {action.get_editor_property('cooldown')}, "
                f"jusqu'au max {action.get_editor_property('summon_up_to_max')}, passif {action.get_editor_property('passive_spawn_interval')} s, "
                f"max {action.get_editor_property('base_max_minions')}")
    for path in BUMP_WEAPONS:
        _, actions = action_components(path, unreal.YSBumpingSlashAbility)
        for name, action in actions:
            log(f"relu {path.split('/')[-1]}.{name} ({type(action).__name__}) : bump des élites {action.get_editor_property('bump_elites')}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_captain_summon_tweaks.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
