"""
Ajoute un bandana au joueur :
  - composant « Bandana » (UYSDanglingChainComponent) enfant de « Head Feature » dans PlayerCharacter_mouss,
    placé à l'arrière de la sphère (nuque)

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_bandana.py"
Rapport : Saved/Logs/setup_bandana.txt
"""
import os
import unreal

PLAYER_BP = "/Game/Placeable/Character/PlayerCharacter_mouss"
PARENT_NAME, CHAIN_NAME = "Head Feature", "Bandana"
# arrière de la sphère (mesh de 100 unités, rayon 50 en local), un peu sous l'équateur
CHAIN_LOCATION = unreal.Vector(-50.0, 0.0, -10.0)

assets = unreal.EditorAssetLibrary
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_bandana] {msg}")
    report.append(msg)


def find_handle(subsystem, blueprint, name):
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(blueprint):
        if str(lib.get_variable_name(lib.get_data(handle))) == name:
            return handle
    return None


try:
    bp = assets.load_asset(PLAYER_BP)
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)

    if find_handle(sds, bp, CHAIN_NAME):
        log(f"{CHAIN_NAME} existe déjà, rien ajouté")
    else:
        parent = find_handle(sds, bp, PARENT_NAME)
        if parent is None:
            raise RuntimeError(f"composant « {PARENT_NAME} » introuvable")

        params = unreal.AddNewSubobjectParams(parent_handle=parent, new_class=unreal.YSDanglingChainComponent, blueprint_context=bp)
        handle, fail_reason = sds.add_new_subobject(params)
        if not lib.is_handle_valid(handle):
            raise RuntimeError(f"ajout impossible : {fail_reason}")

        sds.rename_subobject(handle, unreal.Text(CHAIN_NAME))
        chain = lib.get_object(lib.get_data(handle))
        chain.set_editor_property("relative_location", CHAIN_LOCATION)
        log(f"{CHAIN_NAME} ajouté sous {PARENT_NAME} à {CHAIN_LOCATION}")

    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(PLAYER_BP, only_if_is_dirty=False)
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_bandana.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
