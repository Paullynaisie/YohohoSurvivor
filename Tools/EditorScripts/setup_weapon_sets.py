"""
Sets d'arme et personnage de test :
  - /Game/Core/WeaponSets : DA_WeaponSet_SabrePistolet (Mouss), DA_WeaponSet_AkimboBattlePetoir (Mouss 2pistol),
    DA_WeaponSet_AkimboSabre (Mouss 2sabre) ; assignés aux personnages (3C > Weapons > Weapon Set)
  - /Game/Placeable/Character/PlayerCharacter_JohnnyPlaytest : copie de PlayerCharacter_mouss (« JohnnyPlaytest »),
    dernier choix de l'écran de sélection, peut prendre les mêmes upgrades de classe que Mouss
Ne recrée pas ce qui existe déjà (les réglages de Paul sont conservés).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_weapon_sets.py"
Rapport : Saved/Logs/setup_weapon_sets.txt
"""
import os
import unreal

SETS_DIR = "/Game/Core/WeaponSets"
WEAPONS_DIR = "/Game/Placeable/Weapons"
CHAR_DIR = "/Game/Placeable/Character"
GAME_MODE_BP = "/Game/Core/GameModes/BP_TestGameMode"
UPGRADES_DIR = "/Game/Core/Upgrades"
JOHNNY = "PlayerCharacter_JohnnyPlaytest"

# nom de l'asset : (nom affiché, arme main droite, arme main gauche, personnages)
SETS = {
    "DA_WeaponSet_SabrePistolet": ("Sabre et Pistolet", "BP_Epee", "BP_Pistolet", ["PlayerCharacter_mouss"]),
    "DA_WeaponSet_AkimboBattlePetoir": ("Akimbo Battle Petoir", "BP_BattlePetoir", "BP_BattlePetoir", ["PlayerCharacter_mouss2pistol"]),
    "DA_WeaponSet_AkimboSabre": ("Akimbo Sabre", "BP_Epee", "BP_Epee", ["PlayerCharacter_mouss2sabre"]),
}

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
report = []


def log(msg):
    unreal.log(f"[setup_weapon_sets] {msg}")
    report.append(msg)


def weapon_class(name):
    return assets.load_blueprint_class(f"{WEAPONS_DIR}/{name}")


def character_class(name):
    return assets.load_blueprint_class(f"{CHAR_DIR}/{name}")


try:
    # sets d'arme
    for asset_name, (display, right, left, characters) in SETS.items():
        path = f"{SETS_DIR}/{asset_name}"
        if assets.does_asset_exist(path):
            log(f"existe déjà : {path}")
            weapon_set = assets.load_asset(path)
        else:
            factory = unreal.DataAssetFactory()
            factory.set_editor_property("data_asset_class", unreal.YSWeaponSet)
            weapon_set = tools.create_asset(asset_name, SETS_DIR, unreal.YSWeaponSet, factory)
            weapon_set.set_editor_property("display_name", display)
            weapon_set.set_editor_property("right_hand_weapon", weapon_class(right))
            weapon_set.set_editor_property("left_hand_weapon", weapon_class(left))
            assets.save_asset(path, only_if_is_dirty=False)
            log(f"créé : {path} ({right} / {left})")
        for character in characters:
            cdo = unreal.get_default_object(character_class(character))
            # vide, ou hérité du set d'un autre personnage de la liste (les persos Mouss 2… sont des enfants de Mouss)
            current = cdo.get_editor_property("weapon_set")
            if current is None or (current != weapon_set and current.get_name() in SETS and character not in SETS[current.get_name()][3]):
                cdo.set_editor_property("weapon_set", weapon_set)
                assets.save_asset(f"{CHAR_DIR}/{character}", only_if_is_dirty=False)
                log(f"{character} : set {asset_name}")

    # JohnnyPlaytest : copie de Mouss (après l'assignation du set)
    johnny_path = f"{CHAR_DIR}/{JOHNNY}"
    if assets.does_asset_exist(johnny_path):
        log(f"existe déjà : {johnny_path}")
    else:
        tools.duplicate_asset(JOHNNY, CHAR_DIR, assets.load_asset(f"{CHAR_DIR}/PlayerCharacter_mouss"))
        unreal.BlueprintEditorLibrary.compile_blueprint(assets.load_asset(johnny_path))
        johnny_cdo = unreal.get_default_object(character_class(JOHNNY))
        johnny_cdo.set_editor_property("character_name", "JohnnyPlaytest")
        assets.save_asset(johnny_path, only_if_is_dirty=False)
        log(f"créé : {johnny_path} (copie de PlayerCharacter_mouss)")
    johnny_class = character_class(JOHNNY)

    # écran de sélection : JohnnyPlaytest toujours en dernier
    gm = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP))
    selectable = [c for c in gm.get_editor_property("selectable_characters") if c != johnny_class]
    selectable.append(johnny_class)
    gm.set_editor_property("selectable_characters", selectable)
    assets.save_asset(GAME_MODE_BP, only_if_is_dirty=False)

    # upgrades de classe de Mouss : aussi pour JohnnyPlaytest
    for name in ["DA_ClassUpgrade_Mouss2Sabre", "DA_ClassUpgrade_Mouss2Pistol"]:
        upgrade = assets.load_asset(f"{UPGRADES_DIR}/{name}")
        extra = list(upgrade.get_editor_property("extra_source_classes"))
        if johnny_class not in extra:
            extra.append(johnny_class)
            upgrade.set_editor_property("extra_source_classes", extra)
            assets.save_asset(f"{UPGRADES_DIR}/{name}", only_if_is_dirty=False)
            log(f"{name} : ouvert à {JOHNNY}")

    # relecture
    for character in ["PlayerCharacter_mouss", "PlayerCharacter_mouss2sabre", "PlayerCharacter_mouss2pistol", JOHNNY]:
        cdo = unreal.get_default_object(character_class(character))
        weapon_set = cdo.get_editor_property("weapon_set")
        log(f"relu : {character} « {cdo.get_editor_property('character_name')} » set "
            f"{weapon_set.get_name() if weapon_set else None}, pool {len(cdo.get_editor_property('upgrade_pool'))}")
    check_gm = unreal.get_default_object(assets.load_blueprint_class(GAME_MODE_BP))
    log("relu : sélection " + ", ".join(c.get_name() for c in check_gm.get_editor_property("selectable_characters")))
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_weapon_sets.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
