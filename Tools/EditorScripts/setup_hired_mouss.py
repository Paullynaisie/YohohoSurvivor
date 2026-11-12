"""
Hired crew : crée /Game/Placeable/Character/Player/PlayerCharacter_HiredMouss = Blueprint ENFANT de PlayerCharacter_mouss
(suit toutes ses retouches : set, upgrades, passif, visuel) avec le composant « HiredCrew » (UYSHiredCrewComponent : IA, couleur,
réanimation au bateau). Ne recrée rien qui existe (relançable : ajoute le composant s'il manque).
AYSGameMode::HiredMoussClass pointe sur ce Blueprint par défaut (C++).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_hired_mouss.py"
Rapport : Saved/Logs/setup_hired_mouss.txt
"""
import os
import unreal

MOUSS_BP = "/Game/Placeable/Character/Player/PlayerCharacter_mouss"
FOLDER, NAME = "/Game/Placeable/Character/Player", "PlayerCharacter_HiredMouss"
HIRED_BP = f"{FOLDER}/{NAME}"
COMPONENT_NAME = "HiredCrew"

assets = unreal.EditorAssetLibrary
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_hired_mouss] {msg}")
    report.append(msg)


def find_handle(subsystem, blueprint, name):
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        if str(lib.get_variable_name(lib.get_data(handle))) == name:
            return handle
    return None


try:
    mouss = assets.load_asset(MOUSS_BP)
    if mouss is None:
        raise RuntimeError(f"{MOUSS_BP} introuvable")

    if assets.does_asset_exist(HIRED_BP):
        bp = assets.load_asset(HIRED_BP)
        log(f"{HIRED_BP} existe déjà")
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", mouss.generated_class())
        bp = unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME, FOLDER, unreal.Blueprint, factory)
        if bp is None:
            raise RuntimeError("création du Blueprint impossible")
        log(f"{HIRED_BP} créé (enfant de {MOUSS_BP})")

    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    if find_handle(sds, bp, COMPONENT_NAME):
        log(f"composant {COMPONENT_NAME} déjà là")
    else:
        handles = sds.k2_gather_subobject_data_for_blueprint(bp)
        params = unreal.AddNewSubobjectParams(parent_handle=handles[0], new_class=unreal.YSHiredCrewComponent, blueprint_context=bp)
        handle, fail_reason = sds.add_new_subobject(params)
        if not lib.is_handle_valid(handle):
            raise RuntimeError(f"ajout du composant impossible : {fail_reason}")
        sds.rename_subobject(handle, unreal.Text(COMPONENT_NAME))
        log(f"composant {COMPONENT_NAME} (YSHiredCrewComponent) ajouté")

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(HIRED_BP, only_if_is_dirty=False)

    # vérification : le CDO trouve bien le composant, parent = Mouss
    cdo = unreal.get_default_object(bp.generated_class())
    log(f"parent : {bp.generated_class().get_super_class().get_name() if hasattr(bp.generated_class(), 'get_super_class') else '?'}")
    log(f"nom du personnage : {cdo.get_editor_property('character_name')}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_hired_mouss.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
