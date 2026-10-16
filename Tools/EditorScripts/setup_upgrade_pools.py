"""
Upgrade Pool des personnages joueurs : remet les upgrades par défaut de la classe C++
(Right hand Atk bonus, Left hand Atk bonus) si le Blueprint a gardé une ancienne liste
(ex. les placeholders « Dégâts +10 », supprimés).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_upgrade_pools.py"
Rapport : Saved/Logs/setup_upgrade_pools.txt
"""
import os
import unreal

CHARACTERS = [
    "/Game/Placeable/Character/PlayerCharacter_mouss",
    "/Game/Placeable/Character/PlayerCharacter_mouss2sabre",
    "/Game/Placeable/Character/PlayerCharacter_mouss2pistol",
]

assets = unreal.EditorAssetLibrary
report = []


def log(msg):
    unreal.log(f"[setup_upgrade_pools] {msg}")
    report.append(msg)


def describe(pool):
    return [f"{u.get_class().get_name()}:{u.get_editor_property('display_name')}:{u.get_editor_property('hand')}" if u and isinstance(u, unreal.YSHandAtkBonusUpgrade)
            else (u.get_class().get_name() if u else None) for u in pool]


def is_default(pool):
    return (len(pool) == 2 and all(u and isinstance(u, unreal.YSHandAtkBonusUpgrade) for u in pool)
            and pool[0].get_editor_property("hand") == unreal.YSHand.RIGHT
            and pool[1].get_editor_property("hand") == unreal.YSHand.LEFT)


try:
    native = unreal.get_default_object(unreal.YSPlayerCharacter)
    log(f"C++ : {describe(native.get_editor_property('upgrade_pool'))}")

    for path in CHARACTERS:
        cdo = unreal.get_default_object(assets.load_blueprint_class(path))
        pool = cdo.get_editor_property("upgrade_pool")
        log(f"{path} : {describe(pool)}")
        if is_default(pool):
            continue

        new_pool = []
        for hand, name in ((unreal.YSHand.RIGHT, "Right hand Atk bonus"), (unreal.YSHand.LEFT, "Left hand Atk bonus")):
            upgrade = unreal.new_object(unreal.YSHandAtkBonusUpgrade, outer=cdo)
            upgrade.set_editor_property("hand", hand)
            upgrade.set_editor_property("display_name", name)
            new_pool.append(upgrade)
        cdo.set_editor_property("upgrade_pool", new_pool)
        assets.save_asset(path, only_if_is_dirty=False)

        check = unreal.get_default_object(assets.load_blueprint_class(path)).get_editor_property("upgrade_pool")
        log(f"  -> remis : {describe(check)}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_upgrade_pools.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
