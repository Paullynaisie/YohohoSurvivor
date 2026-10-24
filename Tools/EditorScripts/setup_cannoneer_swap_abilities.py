"""
Cannoneer (demande de Paul, 2026-10-24) : Abilities du Shotgun et du Sniper inversées et modifiées
  - BP_Shotgun : « BombThrower » (réglages copiés de celui du Sniper : lancer, animation, bombe en main) avec Bomb Count 3 et
    Bomb Spread Angle 30 (deux bombes de plus en arc de cercle) ; « RollAndBomb » retiré
  - BP_Sniper : « RollBombShot » = Roll, bomb and shot (réglages copiés de la Roll and Bomb du Shotgun : roulade, bombe, mine ;
    puis visée 0.25 s immobile et tir d'une SniperAMMO, dégâts de la balle = ceux de la bombe) ; « BombThrower » retiré
  - BP_CannoneerMine : Mine Trigger Delay 0.5 s (déclenchement -> explosion)
Relançable : ne refait pas un échange déjà fait.

Lancement (éditeur fermé) :
  UnrealEditor-Cmd.exe YohohoSurvivor.uproject -run=pythonscript -script="Tools/EditorScripts/setup_cannoneer_swap_abilities.py" -unattended -nullrhi
Rapport : Saved/Logs/setup_cannoneer_swap_abilities.txt
"""
import os
import unreal

WEAPONS = "/Game/Placeable/Weapons/Player"
SHOTGUN = f"{WEAPONS}/BP_Shotgun"
SNIPER = f"{WEAPONS}/BP_Sniper"
SNIPER_AMMO = "/Game/Placeable/AMMO/SniperAMMO"
MINE = "/Game/Placeable/AMMO/BP_CannoneerMine"
SHOTGUN_BOMB_COUNT = 3
SHOTGUN_BOMB_SPREAD = 30.0
SNIPER_AIM_DURATION = 0.25
MINE_TRIGGER_DELAY = 0.5

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
ys = unreal.YSEditorScriptLibrary
report = []


def log(msg):
    unreal.log(f"[setup_cannoneer_swap_abilities] {msg}")
    report.append(msg)


def find_component(bp, cls, exact=False):
    for h in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(h))
        if (type(obj) is cls) if exact else isinstance(obj, cls):
            return obj
    return None


def add_component(bp, cls, name):
    root = subsystem.k2_gather_subobject_data_for_blueprint(bp)[0]
    handle, fail = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=root, new_class=cls, blueprint_context=bp))
    if not fail.is_empty():
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    subsystem.rename_subobject(handle, unreal.Text(name))
    return lib.get_object(lib.get_data(handle))


def remove_components(bp, cls, exact=False):
    removed = 0
    while True:
        handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
        old = next((h for h in handles if ((type(lib.get_object(lib.get_data(h))) is cls) if exact else isinstance(lib.get_object(lib.get_data(h)), cls))), None)
        if old is None:
            return removed
        subsystem.delete_subobject(handles[0], old, bp)
        removed += 1


def save(bp, path):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(path, only_if_is_dirty=False)


try:
    shotgun = assets.load_asset(SHOTGUN)
    sniper = assets.load_asset(SNIPER)

    # 1) Shotgun : BombThrower (copie de celui du Sniper) avec 3 bombes en arc
    thrower = find_component(shotgun, unreal.YSBombThrowerAbility)
    if thrower is None:
        source = find_component(sniper, unreal.YSBombThrowerAbility)
        if source is None:
            raise RuntimeError("pas de BombThrower sur le Sniper à copier")
        thrower = add_component(shotgun, unreal.YSBombThrowerAbility, "BombThrower")
        ys.copy_editable_properties(source, thrower)
        log("Shotgun : BombThrower ajouté (réglages du Sniper)")
    thrower.set_editor_property("bomb_count", SHOTGUN_BOMB_COUNT)
    thrower.set_editor_property("bomb_spread_angle", SHOTGUN_BOMB_SPREAD)

    # 2) Sniper : Roll, bomb and shot (copie de la Roll and Bomb du Shotgun)
    rbs = find_component(sniper, unreal.YSRollBombShotAbility)
    if rbs is None:
        source = find_component(shotgun, unreal.YSRollAndBombAbility, exact=True)
        if source is None:
            raise RuntimeError("pas de RollAndBomb sur le Shotgun à copier")
        rbs = add_component(sniper, unreal.YSRollBombShotAbility, "RollBombShot")
        ys.copy_editable_properties(source, rbs)
        rbs.set_editor_property("display_name", unreal.Text("Roll, bomb and shot"))
        rbs.set_editor_property("aim_duration", SNIPER_AIM_DURATION)
        rbs.set_editor_property("aim_move_speed_multiplier", 0.0)
        rbs.set_editor_property("projectile_class", assets.load_blueprint_class(SNIPER_AMMO))
        rbs.set_editor_property("damage", rbs.get_editor_property("bomb_damage"))
        log("Sniper : RollBombShot ajouté (réglages de la Roll and Bomb du Shotgun, visée, SniperAMMO)")

    # 3) anciennes Abilities retirées
    log(f"Shotgun : RollAndBomb retiré ({remove_components(shotgun, unreal.YSRollAndBombAbility, exact=True)})")
    log(f"Sniper : BombThrower retiré ({remove_components(sniper, unreal.YSBombThrowerAbility)})")
    save(shotgun, SHOTGUN)
    save(sniper, SNIPER)

    # 4) Mine : déclenchement -> explosion en 0.5 s
    mine = unreal.get_default_object(assets.load_blueprint_class(MINE))
    mine.set_editor_property("mine_trigger_delay", MINE_TRIGGER_DELAY)
    assets.save_asset(MINE, only_if_is_dirty=False)

    # relecture
    for path in (SHOTGUN, SNIPER):
        bp = assets.load_asset(path)
        for h in subsystem.k2_gather_subobject_data_for_blueprint(bp):
            obj = lib.get_object(lib.get_data(h))
            if isinstance(obj, unreal.YSAbilityComponent):
                line = f"relu {path.split('/')[-1]}.{lib.get_variable_name(lib.get_data(h))} : {type(obj).__name__} « {obj.get_editor_property('display_name')} », recharge {obj.get_editor_property('cooldown')}"
                if isinstance(obj, unreal.YSBombThrowerAbility):
                    line += f", bombes {obj.get_editor_property('bomb_count')} à {obj.get_editor_property('bomb_spread_angle')}°, dégâts {obj.get_editor_property('bomb_damage')}"
                if isinstance(obj, unreal.YSRollBombShotAbility):
                    line += (f", bombe {obj.get_editor_property('bomb_damage')}, balle {obj.get_editor_property('damage')} "
                             f"({obj.get_editor_property('projectile_class').get_name() if obj.get_editor_property('projectile_class') else None}), "
                             f"roulade {obj.get_editor_property('roll_duration')} s, visée {obj.get_editor_property('aim_duration')} s, mine {obj.get_editor_property('mine_class')}")
                log(line)
    log(f"relu BP_CannoneerMine : Mine Trigger Delay {unreal.get_default_object(assets.load_blueprint_class(MINE)).get_editor_property('mine_trigger_delay')}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_cannoneer_swap_abilities.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
