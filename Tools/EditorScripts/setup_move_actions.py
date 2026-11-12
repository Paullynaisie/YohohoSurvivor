"""
MoveActions par personnage (demande de Paul du 2026-11-12) : ajoute au Blueprint du personnage un composant MoveAction qui
remplace le Roll de base (AYSPlayerCharacter::ActiveMoveAction) :
  - PlayerCharacter_mouss         -> « Run »             (UYSRunAbility : 20 puis 5 / s, vitesse x1.3)
  - PlayerCharacter_Quartermaster -> « ShoulderBump »    (UYSShoulderBumpAbility : réglages de roulade copiés de son Roll, 75,
                                                           ni intangible ni invulnérable, bump perpendiculaire de 400)
  - PlayerCharacter_Captain       -> « MoraleBoost »     (UYSMoraleBoostAbility : 65, x1.35 pendant 5 s à 15 m)
Cannoneer et Harquebusier gardent le Roll. HiredMouss (enfant de Mouss) hérite du Run sans jamais l'utiliser.
Relançable : ne touche pas un composant déjà présent (retouches de Paul gardées).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_move_actions.py"
Rapport : Saved/Logs/setup_move_actions.txt
"""
import os
import unreal

FOLDER = "/Game/Placeable/Character/Player"
SETUPS = [
    # (Blueprint, nom du composant, classe, copier les réglages du Roll de base, réglages imposés)
    ("PlayerCharacter_mouss", "Run", unreal.YSRunAbility, False, {}),
    ("PlayerCharacter_Quartermaster", "ShoulderBump", unreal.YSShoulderBumpAbility, True, {
        "display_name": unreal.Text("Shoulder bump"),
        "stamina_cost": 75.0,
        "roll_through_foes": False,
        "extra_invulnerability": 0.0,
        "aim_duration": 0.0,
        "icon": None,
    }),
    ("PlayerCharacter_Captain", "MoraleBoost", unreal.YSMoraleBoostAbility, False, {}),
]

assets = unreal.EditorAssetLibrary
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_move_actions] {msg}")
    report.append(msg)


def find_handle(subsystem, blueprint, name):
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        if str(lib.get_variable_name(lib.get_data(handle))) == name:
            return handle
    return None


try:
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    for bp_name, comp_name, comp_class, copy_roll, overrides in SETUPS:
        path = f"{FOLDER}/{bp_name}"
        bp = assets.load_asset(path)
        if bp is None:
            log(f"ERREUR : {path} introuvable")
            continue
        if find_handle(sds, bp, comp_name):
            log(f"{bp_name} : composant {comp_name} déjà là (pas touché)")
            continue

        handles = sds.k2_gather_subobject_data_for_blueprint(bp)
        params = unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=comp_class, blueprint_context=bp)
        handle, fail_reason = sds.add_new_subobject(params)
        if not lib.is_handle_valid(handle):
            log(f"ERREUR : {bp_name} : ajout de {comp_name} impossible : {fail_reason}")
            continue
        sds.rename_subobject(handle, unreal.Text(comp_name))
        template = lib.get_object(lib.get_data(handle))

        if copy_roll:
            roll = unreal.get_default_object(bp.generated_class()).get_editor_property("MoveActionAbility")
            count = unreal.YSEditorScriptLibrary.copy_editable_properties(roll, template)
            log(f"{bp_name} : {count} réglages copiés du Roll de base")
        for prop, value in overrides.items():
            template.set_editor_property(prop, value)

        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"{bp_name} : composant {comp_name} ({comp_class.__name__}) ajouté")

    # vérification : valeurs lues sur le Blueprint recompilé
    for bp_name, comp_name, comp_class, _, _ in SETUPS:
        bp = assets.load_asset(f"{FOLDER}/{bp_name}")
        handle = find_handle(sds, bp, comp_name)
        if not handle:
            log(f"VERIF {bp_name} : {comp_name} absent")
            continue
        obj = lib.get_object(lib.get_data(handle))
        values = [f"{p}={obj.get_editor_property(p)}" for p in ("display_name", "stamina_cost", "cooldown")]
        if comp_class == unreal.YSShoulderBumpAbility:
            values += [f"{p}={obj.get_editor_property(p)}" for p in ("roll_duration", "roll_speed_multiplier", "roll_through_foes", "bump_distance")]
        log(f"VERIF {bp_name}.{comp_name} : " + ", ".join(values))
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_move_actions.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
