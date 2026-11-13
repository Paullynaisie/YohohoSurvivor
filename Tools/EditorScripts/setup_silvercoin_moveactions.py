"""
SilverCoin upgrades et MoveActions en upgrades (demande de Paul du 2026-11-13) :
  - /Game/Core/Upgrades/Up_Character/Up_MoveActions/DA_Up_MoveAction_{Roll, Run, ShoulderBump, MoraleBoost, Camouflage}
    (UYSMoveActionUpgrade : Déblocage, ++ x5, variantes Golden dans l'ordre des effets C++) ; ajoutés à la fin de l'UpgradePool
    de chaque personnage joueur (pas les minions) ;
  - MoveActions : composants C++ du personnage (MoveActionAbility = Roll, RunMoveAction, ShoulderBumpMoveAction,
    MoraleBoostMoveAction, CamouflageMoveAction) : réglages du Shoulder bump du Quartermaster et du Booster de moral du Captain
    copiés sur tous les personnages, valeurs de Paul imposées (Run 20 / 10 par s / x1.25, Booster 3 s, Camouflage 60 / 10 par s),
    bombe du Little Gift sur le Roll ; les anciens composants MoveAction ajoutés dans les Blueprints (Run, ShoulderBump,
    MoraleBoost) sont supprimés ;
  - /Game/Art/Materials/M_YSCamouflage : matériau translucide (paramètres Color, Opacity) du Camouflage ;
  - GameModes : cycle des SilverCoin upgrades (MoveAction, classe, arme, Golden MoveAction, Golden d'arme), 2 propositions.
Ne recrée pas un asset qui existe déjà (textes et réglages de Paul gardés) ; relançable.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="<chemin absolu>/Tools/EditorScripts/setup_silvercoin_moveactions.py"
Rapport : Saved/Logs/setup_silvercoin_moveactions.txt
"""
import os
import unreal

UPGRADES_DIR = "/Game/Core/Upgrades"
MOVE_DIR = f"{UPGRADES_DIR}/Up_Character/Up_MoveActions"
RANDOM_SHARD = f"{UPGRADES_DIR}/Up_Shards/DA_Up_Shard_Random"
CHARACTERS_DIR = "/Game/Placeable/Character/Player"
# parents d'abord (HiredMouss et JohnnyPlaytest après Mouss)
CHARACTERS = ["PlayerCharacter_mouss", "PlayerCharacter_Cannoneer", "PlayerCharacter_Harquebusier", "PlayerCharacter_Quartermaster",
              "PlayerCharacter_Captain", "PlayerCharacter_JohnnyPlaytest", "PlayerCharacter_HiredMouss"]
GAME_MODES = ["/Game/Core/GameModes/BP_TestGameMode", "/Game/Core/GameModes/BP_TestMapGameMode", "/Game/Core/GameModes/BP_FlowMod"]
CAMOUFLAGE_MATERIAL = "/Game/Art/Materials/M_YSCamouflage"
BOMB_CLASS = "/Game/Placeable/AMMO/BP_CannoneerBomb"
ROLL_ICON = "/Game/Art/Textures/Icones/t_i_RollNBomb"

T = unreal.Text

# clé : (classe, nom, description, nom du ++, label du ++, description du ++, valeur du ++, [(nom Golden, description)])
MOVE_ACTIONS = {
    "Roll": (unreal.YSRollAbility, "Roll",
             "MoveAction (Espace) : roulade invulnérable qui traverse les NME (75 de stamina)",
             "Roll ++", "Coût en stamina", "-5 de stamina par roulade", 5.0, [
                 ("Sleight of hands", "Chaque roulade rend la recharge de l'Ability"),
                 ("Stunts artist", "Chaque roulade rend critique ta prochaine attaque auto"),
                 ("Little Gift", "La roulade devient « Bomb and Roll » : une bombe à tes pieds\n(même coût en stamina, pas de recharge)"),
                 ("BunnyHop", "Une roulade sur 3 est gratuite en stamina"),
             ]),
    "Run": (unreal.YSRunAbility, "Run",
            "MoveAction (Espace, à maintenir) : +25 % de vitesse\n(20 de stamina pour 1 s, puis 10 par seconde)\nAttaquer ou lancer une Ability arrête la course",
            "Run ++", "Vitesse de course (x)", "+5 % de vitesse de course", 0.05, [
                ("Free running", "Pendant la course, tu traverses les NME et les joueurs"),
                ("Fenwick", "Tu peux courir en portant un objet ;\npendant la course, porter ne consomme plus de stamina"),
                ("Run and Hit", "Arrêter la course en attaquant ajoute la stamina dépensée\naux dégâts de base de l'attaque"),
                ("Impulsion", "La première seconde de course devient un petit dash\nintangible et invulnérable (40 de stamina au départ)"),
            ]),
    "ShoulderBump": (unreal.YSShoulderBumpAbility, "Shoulder bump",
                     "MoveAction (Espace) : dash qui bumpe les NME sur le côté (75 de stamina)",
                     "Shoulder bump ++", "Distance du dash (%)", "+20 % de distance de dash", 0.2, [
                         ("Buldozer", "Chaque NME touché relance le dash"),
                         ("Compacteur", "Les NME bumpés atterrissent 2 m devant toi"),
                         ("Brutass", "Le dash inflige les dégâts de ton attaque auto"),
                         ("Diamond shoulder", "Invincible pendant le dash"),
                     ]),
    "MoraleBoost": (unreal.YSMoraleBoostAbility, "Booster de moral",
                    "MoveAction (Espace) : tu lèves les mains 0.5 s ; toi et les alliés à 15 m\ngagnez +35 % de vitesse pendant 3 s (65 de stamina, pas cumulable)",
                    "Booster de moral ++", "Durée du boost (s)", "+1 s de boost", 1.0, [
                        ("Motivé pour cogner", "Le boost donne aussi +35 % de dégâts"),
                        ("Motivé pour tuer", "La prochaine attaque auto des boostés est critique"),
                        ("Motivé pour survivre", "Les boostés regagnent 5 % de leurs PV max par seconde"),
                        ("Motivé pour bombarder", "Les recharges des Abilities des boostés vont 2x plus vite"),
                        ("Motivé pour courir", "Un boosté qui inflige des dégâts relance son boost"),
                    ]),
    "Camouflage": (unreal.YSCamouflageAbility, "Camouflage",
                   "MoveAction (Espace, à maintenir) : invisible aux NME\n(60 de stamina pour 1 s, puis 10 par seconde)\nAttaquer ou lancer une Ability y met fin",
                   "Camouflage ++", "Coût d'activation", "-10 de stamina à l'activation", 10.0, [
                       ("Like a Shadow", "Intangible et invincible pendant le camouflage,\nmais consommation x2 (activation et maintien)"),
                       ("Sucker Punch", "Y mettre fin en attaquant bumpe sur place\nles NME touchés par cette attaque"),
                       ("Thief", "Camouflage possible en portant un objet ;\nporter ne consomme plus de stamina pendant le camouflage"),
                       ("Shadow strike", "Y mettre fin en attaquant frappe comme à 20 / 30\nen Shoot system (distance) ou Melee Range (mêlée)"),
                   ]),
}

# composant C++ du personnage par classe de MoveAction
COMPONENTS = {
    "Roll": "MoveActionAbility",
    "Run": "RunMoveAction",
    "ShoulderBump": "ShoulderBumpMoveAction",
    "MoraleBoost": "MoraleBoostMoveAction",
    "Camouflage": "CamouflageMoveAction",
}

# valeurs de Paul imposées sur chaque personnage
OVERRIDES = {
    "Run": {"stamina_cost": 20.0, "stamina_per_second": 10.0, "min_duration": 1.0, "run_speed_multiplier": 1.25, "hold_to_sustain": True},
    "Camouflage": {"stamina_cost": 60.0, "stamina_per_second": 10.0, "min_duration": 1.0, "hold_to_sustain": True},
    "MoraleBoost": {"duration": 3.0, "raise_hands_duration": 0.5},
}

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_silvercoin_moveactions] {msg}")
    report.append(msg)


def make_camouflage_material():
    if assets.does_asset_exist(CAMOUFLAGE_MATERIAL):
        log(f"{CAMOUFLAGE_MATERIAL} : existe déjà (pas touché)")
        return
    folder, name = CAMOUFLAGE_MATERIAL.rsplit("/", 1)
    material = tools.create_asset(name, folder, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    mel = unreal.MaterialEditingLibrary
    color = mel.create_material_expression(material, unreal.MaterialExpressionVectorParameter, -500, 0)
    color.set_editor_property("parameter_name", "Color")
    color.set_editor_property("default_value", unreal.LinearColor(1.0, 1.0, 1.0, 1.0))
    opacity = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -500, 250)
    opacity.set_editor_property("parameter_name", "Opacity")
    opacity.set_editor_property("default_value", 0.2)
    mel.connect_material_property(color, "", unreal.MaterialProperty.MP_BASE_COLOR)
    mel.connect_material_property(opacity, "", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(material)
    assets.save_asset(CAMOUFLAGE_MATERIAL, only_if_is_dirty=False)
    log(f"{CAMOUFLAGE_MATERIAL} : créé (translucide, Color / Opacity)")


def make_upgrades():
    random_shard = assets.load_asset(RANDOM_SHARD)
    roll_icon = assets.load_asset(ROLL_ICON) if assets.does_asset_exist(ROLL_ICON) else None
    upgrades = {}
    for key, (cls, name, desc, bonus_name, bonus_label, bonus_desc, bonus_value, goldens) in MOVE_ACTIONS.items():
        path = f"{MOVE_DIR}/DA_Up_MoveAction_{key}"
        if assets.does_asset_exist(path):
            upgrades[key] = assets.load_asset(path)
            log(f"{path} : existe déjà (pas touché)")
            continue
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.YSMoveActionUpgrade)
        asset = tools.create_asset(f"DA_Up_MoveAction_{key}", MOVE_DIR, unreal.YSMoveActionUpgrade, factory)
        asset.set_editor_property("display_name", T(name))
        asset.set_editor_property("move_action_class", cls)
        asset.set_editor_property("unlock_name", T(name))
        asset.set_editor_property("unlock_description", T(desc))
        asset.set_editor_property("bonus_name", T(bonus_name))
        asset.set_editor_property("bonus_stat_label", T(bonus_label))
        asset.set_editor_property("bonus_description", T(bonus_desc))
        asset.set_editor_property("bonus_silver_value", bonus_value)
        asset.set_editor_property("bonus_gold_value", bonus_value)
        asset.set_editor_property("max_bonus_stacks", 5)
        asset.set_editor_property("super_name", T(f"Golden {name}"))
        asset.set_editor_property("shard_system", True)
        asset.set_editor_property("unlock_shards", [random_shard, random_shard])
        asset.set_editor_property("bonus_shards", [random_shard, random_shard])
        for weight in ("unlock_weight", "bonus_weight", "super_weight"):
            asset.set_editor_property(weight, 1.0)
        if key == "Roll" and roll_icon:
            asset.set_editor_property("icon", roll_icon)
        variants = []
        for golden_name, golden_desc in goldens:
            variant = unreal.YSGoldenMoveAction()
            variant.set_editor_property("name", T(golden_name))
            variant.set_editor_property("description", T(golden_desc))
            variants.append(variant)
        asset.set_editor_property("golden_variants", variants)
        assets.save_asset(path, only_if_is_dirty=False)
        upgrades[key] = asset
        log(f"{path} : créé ({len(variants)} Golden)")
    return upgrades


def find_bp_move_components(sds, bp):
    """Composants MoveAction ajoutés dans le Blueprint (pas ceux du C++ ni hérités) : [(handle, nom, objet)]"""
    found = []
    move_classes = tuple(cls for cls, *_ in MOVE_ACTIONS.values())
    for handle in sds.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(handle)
        if lib.is_native_component(data) or lib.is_inherited_component(data):
            continue
        obj = lib.get_object(data)
        if obj and isinstance(obj, move_classes):
            found.append((handle, str(lib.get_variable_name(data)), obj))
    return found


try:
    make_camouflage_material()
    upgrades = make_upgrades()
    sds = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    bomb_class = assets.load_blueprint_class(BOMB_CLASS)

    # réglages des anciens composants des Blueprints (Shoulder bump du Quartermaster, Booster du Captain) : sources pour tous
    sources = {}
    for name in CHARACTERS:
        bp = assets.load_asset(f"{CHARACTERS_DIR}/{name}")
        for _, comp_name, obj in find_bp_move_components(sds, bp):
            for key, (cls, *_rest) in MOVE_ACTIONS.items():
                if type(obj) == cls and key in ("ShoulderBump", "MoraleBoost") and key not in sources:
                    sources[key] = obj
                    log(f"source des réglages {key} : {name}.{comp_name}")

    for name in CHARACTERS:
        path = f"{CHARACTERS_DIR}/{name}"
        bp = assets.load_asset(path)
        cdo = unreal.get_default_object(bp.generated_class())
        changes = []

        # réglages des MoveActions C++ du personnage
        for key, comp_prop in COMPONENTS.items():
            comp = cdo.get_editor_property(comp_prop)
            if key in sources:
                count = unreal.YSEditorScriptLibrary.copy_editable_properties(sources[key], comp)
                changes.append(f"{comp_prop} : {count} réglages copiés")
            for prop, value in OVERRIDES.get(key, {}).items():
                comp.set_editor_property(prop, value)
            if key == "Roll" and bomb_class and not comp.get_editor_property("bomb_class"):
                comp.set_editor_property("bomb_class", bomb_class)
                comp.set_editor_property("bomb_damage", 50.0)
                changes.append("Roll : bombe du Little Gift")

        # pool : les 5 upgrades de MoveAction à la fin
        pool = list(cdo.get_editor_property("upgrade_pool"))
        added = []
        for key, upgrade in upgrades.items():
            if upgrade not in pool:
                pool.append(upgrade)
                added.append(f"{len(pool) - 1}:{key}")
        if added:
            cdo.set_editor_property("upgrade_pool", pool)
            changes.append(f"pool + {added}")

        # anciens composants MoveAction du Blueprint supprimés
        old = find_bp_move_components(sds, bp)
        if old:
            handles = sds.k2_gather_subobject_data_for_blueprint(bp)
            deleted = sds.delete_subobjects(handles[0], [h for h, _, _ in old], bp)
            changes.append(f"supprimés : {[n for _, n, _ in old]} ({deleted})")

        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"{name} : {changes if changes else 'rien à changer'}")

    # GameModes : cycle des SilverCoin upgrades, 2 propositions
    kinds = unreal.YSUpgradeSelectionKind
    cycle = [kinds.MOVE_ACTION, kinds.CLASS, kinds.WEAPON, kinds.GOLDEN_MOVE_ACTION, kinds.GOLDEN_WEAPON]
    for path in GAME_MODES:
        if not assets.does_asset_exist(path):
            log(f"{path} : introuvable")
            continue
        bp = assets.load_asset(path)
        cdo = unreal.get_default_object(bp.generated_class())
        if not isinstance(cdo, unreal.YSGameMode):
            log(f"{path} : pas un YSGameMode")
            continue
        cdo.set_editor_property("doubloon_reward_cycle", cycle)
        cdo.set_editor_property("doubloon_reward_after_cycle", kinds.MYTHIC)
        cdo.set_editor_property("silver_coin_choice_count", 2)
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"{path} : cycle {[str(k) for k in cdo.get_editor_property('doubloon_reward_cycle')]}, {cdo.get_editor_property('silver_coin_choice_count')} propositions")

    # vérification
    for name in CHARACTERS:
        bp = assets.load_asset(f"{CHARACTERS_DIR}/{name}")
        cdo = unreal.get_default_object(bp.generated_class())
        pool = cdo.get_editor_property("upgrade_pool")
        indexes = [f"{i}:{u.get_name()}" for i, u in enumerate(pool) if u and isinstance(u, unreal.YSMoveActionUpgrade)]
        sb = cdo.get_editor_property("ShoulderBumpMoveAction")
        mb = cdo.get_editor_property("MoraleBoostMoveAction")
        run = cdo.get_editor_property("RunMoveAction")
        roll = cdo.get_editor_property("MoveActionAbility")
        left = [n for _, n, _ in find_bp_move_components(sds, bp)]
        log(f"VERIF {name} : pool {len(pool)} {indexes} ; Roll {roll.get_editor_property('stamina_cost')} bombe {roll.get_editor_property('bomb_class')} ; "
            f"Run {run.get_editor_property('stamina_cost')} / {run.get_editor_property('stamina_per_second')} x{run.get_editor_property('run_speed_multiplier')} ; "
            f"SB {sb.get_editor_property('stamina_cost')} roll {sb.get_editor_property('roll_duration')} x{sb.get_editor_property('roll_speed_multiplier')} bump {sb.get_editor_property('bump_distance')} ; "
            f"Booster {mb.get_editor_property('stamina_cost')} {mb.get_editor_property('duration')} s x{mb.get_editor_property('speed_multiplier')} ; restants BP {left}")
    log("OK")
except Exception as error:
    import traceback
    log(f"ERREUR : {error}\n{traceback.format_exc()}")

with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_silvercoin_moveactions.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
