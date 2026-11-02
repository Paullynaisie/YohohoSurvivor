"""
Stamina (2026-11-02), relançable :
  - composant MoveActionAbility (« Roll ») de chaque personnage joueur : réglages de roulade copiés du Roll and Bomb de BP_Bomb, icône t_i_RollNBomb
  - /Game/Core/UI/WBP_StaminaBar (enfant de YSStaminaBarWidget, réglages Layout) -> BP_YSPlayerController
  - WBP_BloodlustStacks poussé à droite de l'icône de la MoveAction (s'il était encore à sa place d'origine)
  - shards DA_Up_Shard_StaminaMax (Multiplicative, 1 / 0.065 : x2.95 à 30 / 30) et DA_Up_Shard_StaminaRegen (Multiplicative, 1 / 0.135 : x5.05)
  - upgrade DA_Up_Stats_StaminaUpgrade (StaminaMax, StaminaRegen, StaminaMax, StaminaRegen, Random ; bloquée par StaminaMax) dans la ligne AllChara
  - passifs : Larbin (description, classe redirigée vers YSPassiveStaminaPassive) ; Quartermaster -> DA_Passive_ToughGuyCarry (DA_Passive_ToughGuy déconnecté, pas supprimé)
Ne recrée rien qui existe.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_stamina.py"
Rapport : Saved/Logs/setup_stamina.txt
"""
import os
import unreal

CHARACTER_DIR = "/Game/Placeable/Character/Player"
CHARACTERS = ["PlayerCharacter_mouss", "PlayerCharacter_JohnnyPlaytest", "PlayerCharacter_Cannoneer", "PlayerCharacter_Harquebusier",
              "PlayerCharacter_Quartermaster", "PlayerCharacter_Captain"]
BOMB_BP = "/Game/Placeable/Weapons/Player/Wp_Misc/BP_Bomb"
ROLL_ICON = "/Game/Art/Textures/Icones/t_i_RollNBomb"
ROLL_PROPERTIES = ["roll_duration", "roll_speed_multiplier", "force_move_without_input", "roll_speed_decay_exponent", "roll_through_foes",
                   "extra_invulnerability", "roll_anim_turns", "roll_anim_duration", "roll_anim_ease_exponent", "roll_anim_tuck_height",
                   "aim_duration", "aim_move_speed_multiplier"]
UI_DIR = "/Game/Core/UI"
STAMINA_WIDGET = f"{UI_DIR}/WBP_StaminaBar"
BLOODLUST_WIDGET = f"{UI_DIR}/WBP_BloodlustStacks"
PC_BP = "/Game/Core/Player/BP_YSPlayerController"
SHARDS_DIR = "/Game/Core/Upgrades/Up_Shards"
STATS_DIR = "/Game/Core/Upgrades/Up_Character"
TABLE_PATH = "/Game/Core/Upgrades/DT_Chara_Upgrades"
PASSIVE_DIR = "/Game/Core/Passives"
LARBIN_DESCRIPTION = "Consomme 2x moins de stamina quand il porte un objet ou qu'il nage"
CARRY_DESCRIPTION = "Peut porter un objet et attaquer en même temps : son autre main porte l'objet sur son flanc"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
report = []


def log(msg):
    unreal.log(f"[setup_stamina] {msg}")
    report.append(msg)


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def data_asset(directory, name, cls):
    path = f"{directory}/{name}"
    if assets.does_asset_exist(path):
        return assets.load_asset(path), False
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    asset = tools.create_asset(name, directory, cls, factory)
    log(f"créé : {path}")
    return asset, True


def find_roll_and_bomb():
    # composants ajoutés dans le Blueprint : modèles du SimpleConstructionScript, pas le CDO
    subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
    bp = assets.load_asset(BOMB_BP)
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        lib = unreal.SubobjectDataBlueprintFunctionLibrary
        obj = lib.get_object(lib.get_data(handle))
        if isinstance(obj, unreal.YSRollAndBombAbility):
            return obj
    raise RuntimeError(f"pas de Roll and Bomb dans {BOMB_BP}")


try:
    # 1) MoveAction de chaque personnage
    source = find_roll_and_bomb()
    icon = assets.load_asset(ROLL_ICON)
    for character in CHARACTERS:
        path = f"{CHARACTER_DIR}/{character}"
        roll = cdo(path).get_editor_property("move_action_ability")
        for prop in ROLL_PROPERTIES:
            roll.set_editor_property(prop, source.get_editor_property(prop))
        roll.set_editor_property("icon", icon)
        assets.save_asset(path, only_if_is_dirty=False)
    for character in CHARACTERS:
        roll = cdo(f"{CHARACTER_DIR}/{character}").get_editor_property("move_action_ability")
        log(f"relu : {character} Roll {roll.get_editor_property('roll_duration')} s, x{roll.get_editor_property('roll_speed_multiplier')}, "
            f"coût {roll.get_editor_property('stamina_cost')}, icône {roll.get_editor_property('icon').get_name()}")

    # 2) UI
    if not assets.does_asset_exist(STAMINA_WIDGET):
        factory = unreal.WidgetBlueprintFactory()
        factory.set_editor_property("parent_class", unreal.YSStaminaBarWidget)
        tools.create_asset("WBP_StaminaBar", UI_DIR, unreal.WidgetBlueprint, factory)
        assets.save_asset(STAMINA_WIDGET, only_if_is_dirty=False)
        log(f"créé : {STAMINA_WIDGET}")
    cdo(PC_BP).set_editor_property("stamina_bar_class", assets.load_blueprint_class(STAMINA_WIDGET))
    assets.save_asset(PC_BP, only_if_is_dirty=False)

    bloodlust = cdo(BLOODLUST_WIDGET)
    offset = bloodlust.get_editor_property("screen_offset")
    if offset.x < 176.0:
        bloodlust.set_editor_property("screen_offset", unreal.Vector2D(176.0, offset.y))
        assets.save_asset(BLOODLUST_WIDGET, only_if_is_dirty=False)
    log(f"relu : WBP_BloodlustStacks X = {cdo(BLOODLUST_WIDGET).get_editor_property('screen_offset').x}")

    # 3) shards
    move_speed = assets.load_asset(f"{SHARDS_DIR}/DA_Up_Shard_MoveSpeed")
    shards = {}
    for name, label, description, stat, index in (
        ("DA_Up_Shard_StaminaMax", "Stamina max", "Augmente la stamina maximum", unreal.YSShardStat.STAMINA_MAX, 0.065),
        ("DA_Up_Shard_StaminaRegen", "Stamina regen", "Augmente la regen de stamina", unreal.YSShardStat.STAMINA_REGEN, 0.135),
    ):
        shard, created = data_asset(SHARDS_DIR, name, unreal.YSUpShard)
        if created:
            shard.set_editor_property("display_name", label)
            shard.set_editor_property("description", description)
            shard.set_editor_property("icon", move_speed.get_editor_property("icon"))
            shard.set_editor_property("stat", stat)
            shard.set_editor_property("equation", unreal.YSShardEquation.MULTIPLICATIVE)
            shard.set_editor_property("base", 1.0)
            shard.set_editor_property("index", index)
            shard.set_editor_property("max_level", move_speed.get_editor_property("max_level"))
            assets.save_loaded_asset(shard, only_if_is_dirty=False)
        shards[name] = shard
        log(f"relu : {name} base {shard.get_editor_property('base')}, index {shard.get_editor_property('index')}, max {shard.get_editor_property('max_level')}")

    # 4) upgrade
    stamina_max = shards["DA_Up_Shard_StaminaMax"]
    stamina_regen = shards["DA_Up_Shard_StaminaRegen"]
    random_shard = assets.load_asset(f"{SHARDS_DIR}/DA_Up_Shard_Random")
    stats, created = data_asset(STATS_DIR, "DA_Up_Stats_StaminaUpgrade", unreal.YSUpStats)
    if created:
        stats.set_editor_property("display_name", "Stamina Upgrade")
        stats.set_editor_property("description", "Plus de stamina, et elle remonte plus vite")
        stats.set_editor_property("icon", assets.load_asset(f"{STATS_DIR}/DA_Up_Stats_FastnFurious").get_editor_property("icon"))
        stats.set_editor_property("shards", [stamina_max, stamina_regen, stamina_max, stamina_regen, random_shard])
        stats.set_editor_property("blocking_shard", stamina_max)
        assets.save_loaded_asset(stats, only_if_is_dirty=False)
    log(f"relu : {stats.get_name()} {[s.get_name() for s in stats.get_editor_property('shards')]}")

    table = assets.load_asset(TABLE_PATH)
    row = list(unreal.YSEditorScriptLibrary.get_chara_upgrades_row(table, "AllChara"))
    if stats not in row:
        row.append(stats)
        unreal.YSEditorScriptLibrary.set_chara_upgrades_row(table, "AllChara", row)
        assets.save_loaded_asset(table, only_if_is_dirty=False)
    log(f"AllChara : {len(row)} upgrades, dernière {row[-1].get_name()}")

    # 5) passifs
    larbin = assets.load_asset(f"{PASSIVE_DIR}/DA_Passive_Larbin")
    larbin.set_editor_property("description", LARBIN_DESCRIPTION)
    assets.save_loaded_asset(larbin, only_if_is_dirty=False)
    carry, created = data_asset(PASSIVE_DIR, "DA_Passive_ToughGuyCarry", unreal.YSCarryAttackPassive)
    if created:
        carry.set_editor_property("display_name", assets.load_asset(f"{PASSIVE_DIR}/DA_Passive_ToughGuy").get_editor_property("display_name"))
        carry.set_editor_property("description", CARRY_DESCRIPTION)
        assets.save_loaded_asset(carry, only_if_is_dirty=False)
    quartermaster = f"{CHARACTER_DIR}/PlayerCharacter_Quartermaster"
    cdo(quartermaster).set_editor_property("passive", carry)
    assets.save_asset(quartermaster, only_if_is_dirty=False)
    for character in CHARACTERS:
        passive = cdo(f"{CHARACTER_DIR}/{character}").get_editor_property("passive")
        log(f"relu : {character} passif {passive.get_name()} ({passive.get_class().get_name()}) : {passive.get_editor_property('description')}")
    log("OK")
except Exception as error:
    import traceback
    log(f"ERREUR : {error}\n{traceback.format_exc()}")

with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_stamina.txt"), "w", encoding="utf-8") as f:
    f.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
