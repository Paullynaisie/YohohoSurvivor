"""
Passifs des personnages (Data Assets dans /Game/Core/Passives, 3C > Passive des personnages) :
  - DA_Passive_Larbin (Mouss, et JohnnyPlaytest, sa copie de test) : pas de ralentissement en portant un objet
  - DA_Passive_Marksman (Cannoneer) : critiques à 200 % au lieu de 150 %
  - DA_Passive_ToughGuy (Quartermaster) : ignore 20 % des dégâts subis (%Armor, avant l'armure)
  - DA_Passive_ToiTaGueule (Captain) : sans effet pour l'instant (votes, plus tard)
Ne recrée pas un passif qui existe (textes et valeurs de Paul conservés) ; ne remplace pas un passif déjà mis sur un personnage.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_passives.py"
Rapport : Saved/Logs/setup_passives.txt
"""
import os
import unreal

PASSIVE_DIR = "/Game/Core/Passives"
CHARACTER_DIR = "/Game/Placeable/Character/Player"

# nom de l'asset, classe, nom affiché, description
PASSIVES = [
    ("DA_Passive_Larbin", unreal.YSCarrySpeedPassive, "Larbin", "Ne subit pas de perte de déplacement quand il porte un objet"),
    ("DA_Passive_Marksman", unreal.YSCriticalDamagePassive, "Marksman", "Les critiques font 200 % de dégâts au lieu de 150 %"),
    ("DA_Passive_ToughGuy", unreal.YSArmorPercentPassive, "Tough Guy", "Ignore 20 % des dégâts subis"),
    ("DA_Passive_ToiTaGueule", unreal.YSPassive, "Toi ta gueule", "En cas de vote, le Captain décide qui n'a pas le droit de voter"),
]
CHARACTERS = {
    "PlayerCharacter_mouss": "DA_Passive_Larbin",
    "PlayerCharacter_JohnnyPlaytest": "DA_Passive_Larbin",
    "PlayerCharacter_Cannoneer": "DA_Passive_Marksman",
    "PlayerCharacter_Quartermaster": "DA_Passive_ToughGuy",
    "PlayerCharacter_Captain": "DA_Passive_ToiTaGueule",
}

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
report = []


def log(msg):
    unreal.log(f"[setup_passives] {msg}")
    report.append(msg)


try:
    for name, cls, display, description in PASSIVES:
        path = f"{PASSIVE_DIR}/{name}"
        if assets.does_asset_exist(path):
            log(f"existe déjà : {path}")
            continue
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", cls)
        asset = tools.create_asset(name, PASSIVE_DIR, cls, factory)
        asset.set_editor_property("display_name", display)
        asset.set_editor_property("description", description)
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"créé : {path} ({cls.__name__})")

    for character, passive_name in CHARACTERS.items():
        path = f"{CHARACTER_DIR}/{character}"
        if not assets.does_asset_exist(path):
            log(f"absent : {path}")
            continue
        cdo = unreal.get_default_object(assets.load_blueprint_class(path))
        current = cdo.get_editor_property("passive")
        if current:
            log(f"{character} : garde {current.get_name()}")
            continue
        cdo.set_editor_property("passive", assets.load_asset(f"{PASSIVE_DIR}/{passive_name}"))
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"{character} : passif {passive_name}")

    for character in CHARACTERS:
        passive = unreal.get_default_object(assets.load_blueprint_class(f"{CHARACTER_DIR}/{character}")).get_editor_property("passive")
        log(f"relu : {character} -> {passive.get_editor_property('display_name') if passive else 'aucun'}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_passives.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
