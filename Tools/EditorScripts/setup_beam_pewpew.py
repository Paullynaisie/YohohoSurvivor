# Upgrades d'arme du Gunslinger, suite (2026-11-13, demande de Paul) :
# - BeamShot : /Game/Placeable/AMMO/BeamAMMO (enfant de la classe C++ AYSBeamShot : rayon de 8 m, 65 cm d'épaisseur, 0.1 s),
#   upgrade DA_Up_Weapon_BeamShot (++ = +10 % de dégâts multi-cibles minimum, Golden Solar Shot / Overheat) ;
#   jauge de chaleur WBP_OverheatGauge (même dossier que WBP_BloodlustStacks) choisie par BP_YSPlayerController ;
# - Pew Pew Pew : composant « PewPewPew » (UYSPewPewPewAbility, en sommeil) ajouté à BP_BattlePetoir et BP_SwordWasBetterPetoir,
#   réglages de tir copiés de leur Rolling shot (balle PetoirAMMO), upgrade DA_Up_Weapon_PewPewPew (++ = +1 cycle gratuit,
#   Golden Bullet Eater / Death Blossom).
# Les deux upgrades : shards, poids et icône du Ricochet, ajoutées en fin de pool de Mouss, JohnnyPlaytest et HiredMouss.
# Relançable : ne recrée rien qui existe ; les réglages listés ici sont réappliqués.
import os
import unreal

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
registry = unreal.AssetRegistryHelpers.get_asset_registry()
sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
ys = unreal.YSEditorScriptLibrary
T = unreal.Text

AMMO = "/Game/Placeable/AMMO"
BEAM_AMMO = f"{AMMO}/BeamAMMO"
PETOIR_AMMO = f"{AMMO}/PetoirAMMO"
GUNS = "/Game/Placeable/Weapons/Player/Wp_Guns"
BATTLE_PETOIR = f"{GUNS}/BP_BattlePetoir"
SWB_PETOIR = f"{GUNS}/BP_SwordWasBetterPetoir"
UP_WEAPONS = "/Game/Core/Upgrades/Up_Character/Up_Weapons"
RICOCHET = f"{UP_WEAPONS}/DA_Up_Weapon_Ricochet"
BEAM_UPGRADE = f"{UP_WEAPONS}/DA_Up_Weapon_BeamShot"
PEW_UPGRADE = f"{UP_WEAPONS}/DA_Up_Weapon_PewPewPew"
PC_BP = "/Game/Core/Player/BP_YSPlayerController"
CHARACTERS = ["/Game/Placeable/Character/Player/PlayerCharacter_mouss", "/Game/Placeable/Character/Player/PlayerCharacter_JohnnyPlaytest",
              "/Game/Placeable/Character/Player/PlayerCharacter_HiredMouss"]
# réglages de tir du Rolling shot repris par le Pew pew pew (même pistolet)
SHOT_PROPS = ("damage", "assist_aim_distance", "recoil_pitch", "recoil_kick_back", "recoil_kick_duration", "recoil_recover_duration",
              "recoil_kick_ease_exponent", "recoil_recover_ease_exponent", "play_reload_anim", "reload_spin_duration", "reload_spin_turns",
              "reload_spin_axis", "reload_spin_ease_exponent", "icon", "icon_tint", "weapon_type")

report = []


def log(msg):
    report.append(str(msg))
    unreal.log(f"[setup_beam_pewpew] {msg}")


def handles(bp):
    return sds.k2_gather_subobject_data_for_blueprint(bp)


def component(bp, name):
    for h in handles(bp):
        data = lib.get_data(h)
        if str(lib.get_variable_name(data)) == name:
            return h, lib.get_object(data)
    return None, None


def add_component(bp, cls, name):
    handle, fail = sds.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=handles(bp)[0], new_class=cls, blueprint_context=bp))
    if not fail.is_empty():
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    sds.rename_subobject(handle, T(name))
    return lib.get_object(lib.get_data(handle))


def golden(name, desc):
    variant = unreal.YSGoldenMoveAction()
    variant.set_editor_property("name", T(name))
    variant.set_editor_property("description", T(desc))
    return variant


def save_bp(bp, path):
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(path, only_if_is_dirty=False)


def find_asset_path(name):
    """Chemin d'un asset du projet par son nom (rangements de Paul)."""
    for data in registry.get_assets_by_path("/Game", recursive=True):
        if str(data.asset_name) == name:
            return str(data.package_name)
    return None


def weapon_upgrade(path, cls, name, ricochet):
    if assets.does_asset_exist(path):
        log(f"{path} : existe déjà")
        return assets.load_asset(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    upgrade = tools.create_asset(name, UP_WEAPONS, cls, factory)
    for prop in ("unlock_shards", "bonus_shards", "unlock_weight", "bonus_weight", "super_weight", "icon"):
        upgrade.set_editor_property(prop, ricochet.get_editor_property(prop))
    log(f"{path} : créé (shards, poids et icône du Ricochet)")
    return upgrade


try:
    # --- BeamAMMO : enfant de la classe C++ (cylindre, couleur du joueur) ---
    if assets.does_asset_exist(BEAM_AMMO):
        log(f"{BEAM_AMMO} : existe déjà")
        beam = assets.load_asset(BEAM_AMMO)
    else:
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.YSBeamShot)
        beam = tools.create_asset("BeamAMMO", AMMO, unreal.Blueprint, factory)
        log(f"{BEAM_AMMO} : créé (enfant de YSBeamShot)")
    beam_cdo = unreal.get_default_object(beam.generated_class())
    beam_cdo.set_editor_property("beam_length", 800.0)
    beam_cdo.set_editor_property("beam_width", 65.0)
    beam_cdo.set_editor_property("life_time", 0.1)
    beam_cdo.set_editor_property("use_player_color", True)
    save_bp(beam, BEAM_AMMO)

    # --- Pew pew pew : composant en sommeil sur les deux Petoir du Gunslinger ---
    petoir_ammo = assets.load_blueprint_class(PETOIR_AMMO)
    for path in (BATTLE_PETOIR, SWB_PETOIR):
        bp = assets.load_asset(path)
        rolling = ys.get_inherited_component_override(bp, "RollingShot")
        if rolling is None:
            _, rolling = component(bp, "RollingShot")
        _, pew = component(bp, "PewPewPew")
        if pew is None:
            pew = add_component(bp, unreal.YSPewPewPewAbility, "PewPewPew")
            log(f"{path} : Pew pew pew ajouté")
        for prop in SHOT_PROPS:
            pew.set_editor_property(prop, rolling.get_editor_property(prop))
        pew.set_editor_property("projectile_class", petoir_ammo)
        pew.set_editor_property("display_name", T("Pew pew pew"))
        pew.set_editor_property("cooldown", 20.0)
        pew.set_editor_property("free_cycles", 3)
        pew.set_editor_property("first_shot_interval", 0.35)
        pew.set_editor_property("interval_reduction_per_shot", 0.02)
        pew.set_editor_property("min_shot_interval", 0.05)
        pew.set_editor_property("extra_cycle_stamina_cost", 10.0)
        pew.set_editor_property("circle_bullet_on_impact", 1)
        pew.set_editor_property("cardinal_assist_angle", 45.0)
        pew.set_editor_property("cycle_yaw_offset", 15.0)
        pew.set_editor_property("min_move_speed_ratio", 1.0)
        pew.set_editor_property("center_camera", True)
        save_bp(bp, path)

    ricochet = assets.load_asset(RICOCHET)
    required = [assets.load_blueprint_class(BATTLE_PETOIR), assets.load_blueprint_class(SWB_PETOIR)]

    # --- Upgrade BeamShot ---
    upgrade = weapon_upgrade(BEAM_UPGRADE, unreal.YSBeamShotUpgrade, "DA_Up_Weapon_BeamShot", ricochet)
    upgrade.set_editor_property("required_weapon_classes", required)
    upgrade.set_editor_property("unlock_description", T("L'Auto atk et l'Ability tirent un rayon de 8 m\nqui touche tous les NME (shoot system Explosif)"))
    upgrade.set_editor_property("bonus_silver_value", 0.1)
    upgrade.set_editor_property("bonus_gold_value", 0.1)
    upgrade.set_editor_property("max_bonus_stacks", 5)
    upgrade.set_editor_property("heat_per_shot", 0.05)
    upgrade.set_editor_property("heat_cooldown_delay", 1.0)
    upgrade.set_editor_property("heat_decay_per_second", 0.2)
    upgrade.set_editor_property("heat_critical_threshold", 0.5)
    upgrade.set_editor_property("overheat_move_speed_multiplier", 0.35)
    upgrade.set_editor_property("golden_variants", [
        golden("Solar Shot", "Le tir de l'autre pistolet reste une balle :\nà l'impact, CircleBullet du level du nombre de NME\ntouchés par le rayon (max 5)"),
        golden("Overheat", "Chaque tir chauffe de 5 % (+1 % de vitesse d'atk par %) ;\nde 50 à 99 % : tirs critiques ; 100 % = surchauffe :\nstamina à 0, 35 % de vitesse, plus de tir jusqu'au refroidissement"),
    ])
    assets.save_asset(BEAM_UPGRADE, only_if_is_dirty=False)
    beam_upgrade = upgrade

    # --- Upgrade Pew Pew Pew ---
    upgrade = weapon_upgrade(PEW_UPGRADE, unreal.YSPewPewPewUpgrade, "DA_Up_Weapon_PewPewPew", ricochet)
    upgrade.set_editor_property("required_weapon_classes", required)
    upgrade.set_editor_property("bonus_silver_value", 1.0)
    upgrade.set_editor_property("bonus_gold_value", 1.0)
    upgrade.set_editor_property("max_bonus_stacks", 5)
    upgrade.set_editor_property("bullet_eater_health", 5.0)
    upgrade.set_editor_property("bullet_eater_stamina", 5.0)
    upgrade.set_editor_property("bullet_eater_radius", 150.0)
    upgrade.set_editor_property("golden_variants", [
        golden("Bullet Eater", "Les balles des CircleBullet peuvent te toucher :\n+5 PV et +5 stamina par balle"),
        golden("Death Blossom", "Plus de cycle payé en stamina,\nmais le level des CircleBullet = numéro du cycle"),
    ])
    assets.save_asset(PEW_UPGRADE, only_if_is_dirty=False)
    pew_upgrade = upgrade

    # --- Pools ---
    for path in CHARACTERS:
        if not assets.does_asset_exist(path):
            log(f"{path} : introuvable")
            continue
        bp = assets.load_asset(path)
        cdo = unreal.get_default_object(bp.generated_class())
        pool = list(cdo.get_editor_property("upgrade_pool"))
        changed = False
        for up in (beam_upgrade, pew_upgrade):
            if up in pool:
                log(f"{path} : {up.get_name()} déjà dans le pool (index {pool.index(up)})")
                continue
            pool.append(up)
            changed = True
            log(f"{path} : {up.get_name()} ajoutée au pool, index {len(pool) - 1}")
        if changed:
            cdo.set_editor_property("upgrade_pool", pool)
            save_bp(bp, path)

    # --- Jauge de chaleur : même dossier que WBP_BloodlustStacks ---
    bloodlust_widget = find_asset_path("WBP_BloodlustStacks")
    ui_dir = bloodlust_widget.rsplit("/", 1)[0] if bloodlust_widget else "/Game/Core/UI"
    widget_path = find_asset_path("WBP_OverheatGauge") or f"{ui_dir}/WBP_OverheatGauge"
    if assets.does_asset_exist(widget_path):
        log(f"{widget_path} : existe déjà")
    else:
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", unreal.YSOverheatWidget)
        tools.create_asset("WBP_OverheatGauge", ui_dir, unreal.WidgetBlueprint, factory)
        log(f"{widget_path} : créé")
    widget_bp = assets.load_asset(widget_path)
    unreal.BlueprintEditorLibrary.compile_blueprint(widget_bp)
    assets.save_asset(widget_path, only_if_is_dirty=False)
    pc_path = find_asset_path("BP_YSPlayerController") or PC_BP
    pc_cdo = unreal.get_default_object(assets.load_blueprint_class(pc_path))
    pc_cdo.set_editor_property("overheat_widget_class", assets.load_blueprint_class(widget_path))
    assets.save_asset(pc_path, only_if_is_dirty=False)

    # --- Relecture ---
    cdo = unreal.get_default_object(assets.load_blueprint_class(BEAM_AMMO))
    log(f"relu BeamAMMO : longueur {cdo.get_editor_property('beam_length')}, épaisseur {cdo.get_editor_property('beam_width')}, "
        f"durée {cdo.get_editor_property('life_time')}, couleur du joueur {cdo.get_editor_property('use_player_color')}, mesh {cdo.get_editor_property('mesh').get_editor_property('static_mesh').get_name()}")
    for path in (BATTLE_PETOIR, SWB_PETOIR):
        _, pew = component(assets.load_asset(path), "PewPewPew")
        log(f"relu {path.rsplit('/', 1)[1]}.PewPewPew : dégâts {pew.get_editor_property('damage')}, recharge {pew.get_editor_property('cooldown')}, "
            f"{pew.get_editor_property('free_cycles')} cycles, balle {pew.get_editor_property('projectile_class').get_name()}")
    for path in (BEAM_UPGRADE, PEW_UPGRADE):
        up = assets.load_asset(path)
        log(f"relu {up.get_name()} : ++ {up.get_editor_property('bonus_gold_value')} x{up.get_editor_property('max_bonus_stacks')}, "
            f"Golden {[str(g.get_editor_property('name')) for g in up.get_editor_property('golden_variants')]}, "
            f"armes {[c.get_name() for c in up.get_editor_property('required_weapon_classes')]}")
    log(f"relu {pc_path} -> {unreal.get_default_object(assets.load_blueprint_class(pc_path)).get_editor_property('overheat_widget_class').get_name()}")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_beam_pewpew.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
