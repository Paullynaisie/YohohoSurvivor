"""
Système de critique des attaques auto (/Game/Core/Upgrades) :
  - DA_Upgrade_AtkCritique (UYSCriticalAttackUpgrade, unique, Silver) : Auto swing -> la 5e atk est toujours critique ;
    PewPew -> la 3e balle sur un même NME est toujours critique
  - DA_Upgrade_AtkSpeed (UYSHandCooldownReductionUpgrade, Auto atk seule) : séparée de la CD reduction (même valeurs),
    + dégâts critiques +10 / 20 / 30 % (Bronze / Silver / Gold) par bonus ; requiert Atk critique ; jumelle de Shoot system
  - DA_Upgrade_CDReduction : Ability seule (plus d'Atk speed)
  - DA_Upgrade_ShootSystem : + dégâts critiques +10 / 20 / 30 % ; requiert Atk critique ; jumelle d'Atk speed
  - pools de PlayerCharacter_mouss et PlayerCharacter_JohnnyPlaytest : Atk critique et Atk speed ajoutées à la fin
Les réglages déjà faits par Paul ne sont pas réécrits (assets créés une fois ; liens et critiques écrits s'ils sont vides).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_critical_upgrades.py"
Rapport : Saved/Logs/setup_critical_upgrades.txt
"""
import os
import unreal

UPGRADES = "/Game/Core/Upgrades"
CRIT = f"{UPGRADES}/DA_Upgrade_AtkCritique"
ATK_SPEED = f"{UPGRADES}/DA_Upgrade_AtkSpeed"
CD_REDUCTION = f"{UPGRADES}/DA_Upgrade_CDReduction"
SHOOT_SYSTEM = f"{UPGRADES}/DA_Upgrade_ShootSystem"
CHARACTERS = ["/Game/Placeable/Character/PlayerCharacter_mouss", "/Game/Placeable/Character/PlayerCharacter_JohnnyPlaytest"]
CRITICAL_VALUES = (0.1, 0.2, 0.3)
COPIED = ["offer_weight", "can_be_bronze", "can_be_silver", "can_be_gold", "stat_label", "max_stacks", "bronze_value", "silver_value", "gold_value",
          "ability_label", "auto_atk_label", "bonus_name", "min_cooldown_multiplier", "auto_atk_bonus_name"]

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
report = []


def log(msg):
    unreal.log(f"[setup_critical_upgrades] {msg}")
    report.append(msg)


def create(path, cls):
    if assets.does_asset_exist(path):
        return assets.load_asset(path), False
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    name = path.rsplit("/", 1)[1]
    asset = tools.create_asset(name, UPGRADES, cls, factory)
    log(f"créé : {path}")
    return asset, True


def set_critical(upgrade):
    if upgrade.get_editor_property("critical_damage_bronze") == 0 and upgrade.get_editor_property("critical_damage_silver") == 0 and upgrade.get_editor_property("critical_damage_gold") == 0:
        for prop, value in zip(("critical_damage_bronze", "critical_damage_silver", "critical_damage_gold"), CRITICAL_VALUES):
            upgrade.set_editor_property(prop, value)


try:
    crit, _ = create(CRIT, unreal.YSCriticalAttackUpgrade)
    assets.save_asset(CRIT, only_if_is_dirty=False)

    cd = assets.load_asset(CD_REDUCTION)
    shoot = assets.load_asset(SHOOT_SYSTEM)

    atk_speed, created = create(ATK_SPEED, unreal.YSHandCooldownReductionUpgrade)
    if created:
        for prop in COPIED:
            atk_speed.set_editor_property(prop, cd.get_editor_property(prop))
        atk_speed.set_editor_property("display_name", "Atk speed")
        atk_speed.set_editor_property("ability_variant", False)
        atk_speed.set_editor_property("auto_atk_variant", True)

    # CD reduction : Ability seule
    cd.set_editor_property("auto_atk_variant", False)
    assets.save_asset(CD_REDUCTION, only_if_is_dirty=False)

    # Atk speed et Shoot system : après Atk critique, jumelles, bonus critiques
    for upgrade, twin in ((atk_speed, shoot), (shoot, atk_speed)):
        if upgrade.get_editor_property("required_upgrade") is None:
            upgrade.set_editor_property("required_upgrade", crit)
        if upgrade.get_editor_property("twin_upgrade") is None:
            upgrade.set_editor_property("twin_upgrade", twin)
        set_critical(upgrade)
    assets.save_asset(ATK_SPEED, only_if_is_dirty=False)
    assets.save_asset(SHOOT_SYSTEM, only_if_is_dirty=False)

    # pools des personnages : à la fin (les index existants ne bougent pas)
    for path in CHARACTERS:
        cdo = unreal.get_default_object(assets.load_blueprint_class(path))
        pool = list(cdo.get_editor_property("upgrade_pool"))
        added = [u.get_name() for u in (crit, atk_speed) if u not in pool]
        for upgrade in (crit, atk_speed):
            if upgrade not in pool:
                pool.append(upgrade)
        if added:
            cdo.set_editor_property("upgrade_pool", pool)
            assets.save_asset(path, only_if_is_dirty=False)
            log(f"{path.rsplit('/', 1)[1]} : ajouté au pool {added}")

    # relecture
    for path in (CRIT, ATK_SPEED, CD_REDUCTION, SHOOT_SYSTEM, f"{UPGRADES}/DA_Upgrade_Ricochet", f"{UPGRADES}/DA_Upgrade_Bloodlust"):
        u = assets.load_asset(path)
        req = u.get_editor_property("required_upgrade")
        twin = u.get_editor_property("twin_upgrade")
        line = f"relu : {u.get_name()} requise {req.get_name() if req else None}, jumelle {twin.get_name() if twin else None}"
        if isinstance(u, unreal.YSStatUpgrade):
            line += f", critique {u.get_editor_property('critical_damage_bronze')} / {u.get_editor_property('critical_damage_silver')} / {u.get_editor_property('critical_damage_gold')}"
        if isinstance(u, unreal.YSHandStatUpgrade):
            line += f", variantes Ability {u.get_editor_property('ability_variant')} Auto atk {u.get_editor_property('auto_atk_variant')}, valeurs {u.get_editor_property('bronze_value')} / {u.get_editor_property('silver_value')} / {u.get_editor_property('gold_value')}"
        log(line)
    for path in CHARACTERS:
        pool = unreal.get_default_object(assets.load_blueprint_class(path)).get_editor_property("upgrade_pool")
        log(f"relu : pool {path.rsplit('/', 1)[1]} : {[(i, u.get_name() if u else None) for i, u in enumerate(pool)]}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_critical_upgrades.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
