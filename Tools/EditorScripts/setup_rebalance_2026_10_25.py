import unreal

# Rebalancing du 2026-10-25 (relançable : valeurs absolues)
#  - Skull Bomb : 1 NME sur 15 dans les vagues (Spawn Weight calculé d'après les autres NME de la règle de vagues)
#  - BloodRage ++ : armure par stack divisée par 2 (Silver 5 -> 2.5, Gold 10 -> 5)
#  - caméra des joueurs reculée de 20 % (bras 1400 -> 1680)

BOMB_SHARE = 1.0 / 15.0
BLOODRAGE_SILVER = 2.5
BLOODRAGE_GOLD = 5.0
ARM_LENGTH = 1680.0

PLAYERS = ["PlayerCharacter_mouss", "PlayerCharacter_Cannoneer", "PlayerCharacter_Quartermaster", "PlayerCharacter_Captain", "PlayerCharacter_JohnnyPlaytest"]

lines = []

def log(text):
    unreal.log(text)
    lines.append(text)

def bp_class(path):
    return unreal.EditorAssetLibrary.load_blueprint_class(path)

# --- Skull Bomb : 1 sur 15 ---
bomb_path = "/Game/Placeable/Character/Foe/FoeCharacter_Skull_Bomb"
bomb_class = bp_class(bomb_path)
game_mode = unreal.get_default_object(bp_class("/Game/Core/GameModes/BP_TestGameMode"))
others = 0.0
for rule in game_mode.get_editor_property("SpawnRules"):
    if rule.get_class().get_name() == "YSTimedWaveSpawnRule":
        for cls in rule.get_editor_property("EnemyClasses"):
            if cls and cls != bomb_class:
                others += unreal.get_default_object(cls).get_editor_property("SpawnWeight")
weight = others * BOMB_SHARE / (1.0 - BOMB_SHARE)
bomb_cdo = unreal.get_default_object(bomb_class)
log("Skull Bomb SpawnWeight %.4f -> %.4f (autres NME : %.2f)" % (bomb_cdo.get_editor_property("SpawnWeight"), weight, others))
bomb_cdo.set_editor_property("SpawnWeight", weight)
log("save %s : %s" % (bomb_path, unreal.EditorAssetLibrary.save_asset(bomb_path, only_if_is_dirty=False)))

# --- BloodRage ++ ---
br_path = "/Game/Core/Upgrades/DA_Upgrade_BloodRage"
br = unreal.load_asset(br_path)
log("BloodRage Silver %s -> %s, Gold %s -> %s" % (br.get_editor_property("BonusSilverValue"), BLOODRAGE_SILVER, br.get_editor_property("BonusGoldValue"), BLOODRAGE_GOLD))
br.set_editor_property("BonusSilverValue", BLOODRAGE_SILVER)
br.set_editor_property("BonusGoldValue", BLOODRAGE_GOLD)
log("save %s : %s" % (br_path, unreal.EditorAssetLibrary.save_asset(br_path, only_if_is_dirty=False)))

# --- caméra ---
for name in PLAYERS:
    path = "/Game/Placeable/Character/Player/" + name
    cls = bp_class(path)
    if not cls:
        log("introuvable : " + path)
        continue
    boom = unreal.get_default_object(cls).get_editor_property("CameraBoom")
    log("%s bras %s -> %s" % (name, boom.get_editor_property("TargetArmLength"), ARM_LENGTH))
    boom.set_editor_property("TargetArmLength", ARM_LENGTH)
    log("save %s : %s" % (path, unreal.EditorAssetLibrary.save_asset(path, only_if_is_dirty=False)))

with open(unreal.Paths.project_saved_dir() + "Logs/setup_rebalance_2026_10_25.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(lines))
unreal.SystemLibrary.quit_editor()
