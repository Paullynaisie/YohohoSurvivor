"""
Quatrième personnage jouable : le Captain (UNIQUE : un seul par partie ; taille standard).
  - /Game/Placeable/AMMO/CaptainAMMO (enfant de la balle de l'Auto shot de BP_AutoPetoir, dorée) : Marks Target = le dernier NME
    touché devient le NME marqué du Captain (marque au sol sous lui)
  - /Game/Placeable/Weapons/Player/BP_CaptainGun : copie de BP_AutoPetoir (« Captain Gun ») ; Auto shot 1 balle par rafale, CaptainAMMO
  - /Game/Placeable/Weapons/Player/BP_CaptainHook (« Crochet du capitaine », arme d'invocation) : poignée + garde, crochet sous
    « Coloration » ; « SummonCrew » = Summon the crew ! (Ability : invoque 2 CaptainMinion, 2 max), pas d'attaque auto
  - Minions : /Game/Placeable/Weapons/Player/BP_MinionAutoGun (copie de BP_AutoPetoir : 10 dégâts, recharge 2 s) et BP_MinionSword
    (copie de BP_Epee : 20 dégâts, Auto Atk CD 2 s) ; sets DA_WeaponSet_MinionAutoGun / DA_WeaponSet_MinionSword (main droite, 50 PV) ;
    /Game/Placeable/Character/Summon/CaptainMinion : copie de Mouss reparentée sur AYSCaptainMinion, sans Head Feature
  - /Game/Core/Upgrades/DA_Upgrade_SummonSystem (UYSSummonSystemUpgrade : +1 minion max par bonus)
  - /Game/Core/WeaponSets/DA_WeaponSet_Captain (droite Captain Gun, gauche Crochet du capitaine ; PV / pushback du set de Mouss)
  - /Game/Placeable/Character/Player/PlayerCharacter_Captain : copie de Mouss (« Captain », Unique Character, set Captain,
    pool de Mouss + Summon system) ; Head Feature : bandana retiré, TRICORNE (calotte, fond, 3 bords relevés en triangle,
    3 pointes) enfant de Head Feature (couleur de la personnalisation) + galon doré sur les bords (enfant de BodyPivot : reste doré)
  - sélection (BP_TestGameMode / BP_TestMapGameMode) : ..., Captain, JohnnyPlaytest toujours en dernier
Ne recrée rien qui existe déjà (réglages de Paul conservés).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_captain.py"
Rapport : Saved/Logs/setup_captain.txt
"""
import math
import os
import unreal

AMMO_DIR = "/Game/Placeable/AMMO"
CAPTAIN_AMMO_NAME = "CaptainAMMO"
CAPTAIN_AMMO = f"{AMMO_DIR}/{CAPTAIN_AMMO_NAME}"
WEAPON_DIR = "/Game/Placeable/Weapons/Player"
BASE_WEAPON = "/Game/Placeable/Weapons/BP_Arme"
AUTO_PETOIR = f"{WEAPON_DIR}/BP_AutoPetoir"
SWORD = f"{WEAPON_DIR}/BP_Epee"
CAPTAIN_GUN_NAME, MINION_GUN_NAME, MINION_SWORD_NAME, HOOK_NAME = "BP_CaptainGun", "BP_MinionAutoGun", "BP_MinionSword", "BP_CaptainHook"
CAPTAIN_GUN, MINION_GUN, MINION_SWORD, HOOK = (f"{WEAPON_DIR}/{n}" for n in (CAPTAIN_GUN_NAME, MINION_GUN_NAME, MINION_SWORD_NAME, HOOK_NAME))
SETS_DIR = "/Game/Core/WeaponSets"
MOUSS_SET = f"{SETS_DIR}/DA_WeaponSet_SabrePistolet"
UPGRADES_DIR = "/Game/Core/Upgrades"
SUMMON_UPGRADE_NAME = "DA_Upgrade_SummonSystem"
SUMMON_UPGRADE = f"{UPGRADES_DIR}/{SUMMON_UPGRADE_NAME}"
CHAR_DIR = "/Game/Placeable/Character/Player"
SUMMON_DIR = "/Game/Placeable/Character/Summon"
MOUSS = f"{CHAR_DIR}/PlayerCharacter_mouss"
JOHNNY = f"{CHAR_DIR}/PlayerCharacter_JohnnyPlaytest"
CAPTAIN_NAME = "PlayerCharacter_Captain"
CAPTAIN = f"{CHAR_DIR}/{CAPTAIN_NAME}"
MINION_NAME = "CaptainMinion"
MINION = f"{SUMMON_DIR}/{MINION_NAME}"
GAME_MODES = ["/Game/Core/GameModes/BP_TestGameMode", "/Game/Core/GameModes/BP_TestMapGameMode"]
MAT = "/Game/Art/Materials/PH_materials"
SPHERE = "/Engine/BasicShapes/Sphere"
CYLINDER = "/Engine/BasicShapes/Cylinder"
CUBE = "/Engine/BasicShapes/Cube"
CONE = "/Engine/BasicShapes/Cone"
HEAD = "Head Feature"
# tricorne (espace du corps) : pointe à l'avant ; R = distance des pointes au centre
HAT_X, HAT_BASE_Z, HAT_R = -2.0, 74.0, 52.0
HOOK_SCALE = 2.5

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
ys = unreal.YSEditorScriptLibrary
report = []


def log(msg):
    unreal.log(f"[setup_captain] {msg}")
    report.append(msg)


def vec(x, y, z):
    return unreal.Vector(x, y, z)


def rot(pitch=0.0, yaw=0.0, roll=0.0):
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


def handles(bp):
    return subsystem.k2_gather_subobject_data_for_blueprint(bp)


def find_handle(bp, name):
    for h in handles(bp):
        if str(lib.get_variable_name(lib.get_data(h))) == name:
            return h
    return None


def templates(bp):
    result = []
    for h in handles(bp):
        obj = lib.get_object(lib.get_data(h))
        if obj and obj not in result:
            result.append(obj)
    return result


def add_component(bp, cls, name, parent=None):
    parent = parent or handles(bp)[0]
    handle, fail = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent, new_class=cls, blueprint_context=bp))
    if not fail.is_empty():
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    subsystem.rename_subobject(handle, unreal.Text(name))
    return handle, lib.get_object(lib.get_data(handle))


def shape(bp, name, mesh, material, location, rotation, scale, parent=None, decals=True):
    _, comp = add_component(bp, unreal.StaticMeshComponent, name, parent)
    comp.set_editor_property("static_mesh", assets.load_asset(mesh))
    comp.set_material(0, material if isinstance(material, unreal.MaterialInterface) else assets.load_asset(f"{MAT}/{material}"))
    comp.set_editor_property("relative_location", location)
    comp.set_editor_property("relative_rotation", rotation)
    comp.set_editor_property("relative_scale3d", scale)
    comp.set_collision_profile_name("NoCollision")
    comp.set_editor_property("receives_decals", decals)
    return comp


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def create_bp(name, folder, parent_class):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    return tools.create_asset(name, folder, unreal.Blueprint, factory)


def delete_components(bp, prefixes):
    root = handles(bp)[0]
    removed = 0
    while True:
        old = next((h for h in handles(bp) if str(lib.get_variable_name(lib.get_data(h))).startswith(prefixes)), None)
        if old is None:
            break
        subsystem.delete_subobject(root, old, bp)
        removed += 1
    return removed


def action_of(bp, cls):
    return next((t for t in templates(bp) if type(t) == cls), None)


def create_weapon_set(name, display, right, left, max_health=None):
    path = f"{SETS_DIR}/{name}"
    if assets.does_asset_exist(path):
        log(f"existe déjà : {path}")
        return assets.load_asset(path)
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", unreal.YSWeaponSet)
    weapon_set = tools.create_asset(name, SETS_DIR, unreal.YSWeaponSet, factory)
    ys.copy_editable_properties(assets.load_asset(MOUSS_SET), weapon_set)
    weapon_set.set_editor_property("display_name", display)
    weapon_set.set_editor_property("right_hand_weapon", assets.load_blueprint_class(right) if right else None)
    weapon_set.set_editor_property("left_hand_weapon", assets.load_blueprint_class(left) if left else None)
    if max_health is not None:
        weapon_set.set_editor_property("override_max_health", True)
        weapon_set.set_editor_property("max_health", max_health)
    assets.save_asset(path, only_if_is_dirty=False)
    log(f"créé : {path} ({display})")
    return weapon_set


def duplicate_weapon(name, source, display, setup):
    path = f"{WEAPON_DIR}/{name}"
    if assets.does_asset_exist(path):
        log(f"existe déjà : {path}")
        return
    tools.duplicate_asset(name, WEAPON_DIR, assets.load_asset(source))
    bp = assets.load_asset(path)
    setup(bp)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    cdo(path).set_editor_property("display_name", display)
    assets.save_asset(path, only_if_is_dirty=False)
    log(f"créé : {path} (copie de {source.split('/')[-1]})")


class HeadSpace:
    """Coordonnées de l'espace du corps (BodyPivot) converties dans celui de Head Feature (pitch + échelle)"""
    def __init__(self, comp):
        self.loc = comp.get_editor_property("relative_location")
        self.pitch = comp.get_editor_property("relative_rotation").pitch
        self.scale = comp.get_editor_property("relative_scale3d")

    def location(self, x, y, z):
        dx, dy, dz = x - self.loc.x, y - self.loc.y, z - self.loc.z
        p = math.radians(self.pitch)
        lx = dx * math.cos(p) + dz * math.sin(p)
        lz = -dx * math.sin(p) + dz * math.cos(p)
        return vec(lx / self.scale.x, dy / self.scale.y, lz / self.scale.z)

    def scale3d(self, x, y, z):
        return vec(x / self.scale.x, y / self.scale.y, z / self.scale.z)


def setup_tricorne(bp):
    head_handle = find_handle(bp, HEAD)
    body_handle = find_handle(bp, "BodyPivot")
    if head_handle is None or body_handle is None:
        raise RuntimeError("Head Feature / BodyPivot introuvable")
    head = lib.get_object(lib.get_data(head_handle))
    color = head.get_material(0)
    log(f"bandana de Mouss retiré ({delete_components(bp, ('Bandana', 'Sphere'))} composants)")
    space = HeadSpace(head)

    def hat(name, mesh, x, y, z, pitch, yaw, roll, sx, sy, sz):
        # rotation donnée dans l'espace du corps : le pitch de Head Feature est compensé ; échelle convertie
        shape(bp, name, mesh, color, space.location(x, y, z), rot(pitch=pitch - space.pitch, yaw=yaw, roll=roll),
              space.scale3d(sx, sy, sz), head_handle, decals=False)

    # fond du chapeau (sous les bords) et calotte
    hat("HatBase", CYLINDER, HAT_X, 0.0, HAT_BASE_Z, 0.0, 0.0, 0.0, 0.66, 0.66, 0.04)
    hat("HatCrown", CYLINDER, HAT_X, 0.0, HAT_BASE_Z + 12.0, 0.0, 0.0, 0.0, 0.5, 0.5, 0.24)
    hat("HatCrownTop", SPHERE, HAT_X, 0.0, HAT_BASE_Z + 24.0, 0.0, 0.0, 0.0, 0.5, 0.5, 0.18)
    # 3 bords relevés (côtés du triangle : arrière, avant gauche, avant droit), inclinés vers l'extérieur ; galon doré au sommet
    side_distance, side_length = HAT_R * 0.5, HAT_R * math.sqrt(3.0)
    for i, angle in enumerate((60.0, 180.0, 300.0)):
        a = math.radians(angle)
        cx, cy = HAT_X + side_distance * math.cos(a), side_distance * math.sin(a)
        hat(f"HatBrim{i + 1}", CUBE, cx, cy, HAT_BASE_Z + 9.0, 0.0, angle - 90.0, -25.0, side_length / 100.0, 0.05, 0.2)
        ox, oy = cx + 4.5 * math.cos(a), 4.5 * math.sin(a)
        shape(bp, f"HatTrim{i + 1}", CUBE, "m_PH_monochrome_Yellow", vec(ox, oy, HAT_BASE_Z + 18.0), rot(yaw=angle - 90.0, roll=-25.0),
              vec(side_length / 100.0 * 0.98, 0.06, 0.03), body_handle, decals=False)
    # 3 pointes du tricorne (avant, arrière gauche, arrière droite)
    for i, angle in enumerate((0.0, 120.0, 240.0)):
        a = math.radians(angle)
        hat(f"HatPoint{i + 1}", CONE, HAT_X + (HAT_R - 4.0) * math.cos(a), (HAT_R - 4.0) * math.sin(a), HAT_BASE_Z + 6.0, -70.0, angle, 0.0, 0.14, 0.14, 0.16)
    log("tricorne : HatBase, HatCrown, HatCrownTop, HatBrim1..3, HatPoint1..3 (couleur de personnalisation) + HatTrim1..3 dorés (BodyPivot)")


def setup_hook(bp):
    s = HOOK_SCALE
    # poignée = Mesh (natif, réglé plus bas) ; garde ; crochet sous « Coloration »
    shape(bp, "Guard", CYLINDER, "m_PH_monochrome_Grey", vec(0.0, 0.0, 7.0 * s), rot(), vec(0.08 * s, 0.08 * s, 0.012 * s))
    coloration, _ = add_component(bp, unreal.SceneComponent, "Coloration")
    white = "m_PH_emissive_turboWhite"
    shape(bp, "HookShaft", CYLINDER, white, vec(0.0, 0.0, 16.0 * s), rot(), vec(0.016 * s, 0.016 * s, 0.17 * s), coloration)
    # arc du crochet : 4 segments sur un demi-cercle de rayon 8 (centre (8, 0, 25)), puis la pointe vers le bas
    cx, cz, r = 8.0, 25.0, 8.0
    for i in range(4):
        a0, a1 = math.radians(180.0 - 45.0 * i), math.radians(180.0 - 45.0 * (i + 1))
        x0, z0 = cx + r * math.cos(a0), cz + r * math.sin(a0)
        x1, z1 = cx + r * math.cos(a1), cz + r * math.sin(a1)
        tilt = math.degrees(math.atan2(x1 - x0, z1 - z0))   # angle depuis +Z vers +X
        shape(bp, f"HookArc{i + 1}", CYLINDER, white, vec((x0 + x1) * 0.5 * s, 0.0, (z0 + z1) * 0.5 * s), rot(pitch=-tilt),
              vec(0.016 * s, 0.016 * s, 0.075 * s), coloration)
    shape(bp, "HookTip", CONE, white, vec(16.0 * s, 0.0, 21.0 * s), rot(pitch=180.0), vec(0.03 * s, 0.03 * s, 0.07 * s), coloration)


try:
    # 1) Captain AMMO : balle de l'Auto shot de l'AutoPetoir, dorée, qui marque
    auto_petoir_bp = assets.load_asset(AUTO_PETOIR)
    auto_shot_src = action_of(auto_petoir_bp, unreal.YSAutoShotAbility)
    if not assets.does_asset_exist(CAPTAIN_AMMO):
        parent_ammo = auto_shot_src.get_editor_property("projectile_class")
        create_bp(CAPTAIN_AMMO_NAME, AMMO_DIR, parent_ammo)
        ammo = cdo(CAPTAIN_AMMO)
        ammo.set_editor_property("marks_target", True)
        ammo.get_editor_property("mesh").set_material(0, assets.load_asset(f"{MAT}/m_PH_emissive_turboYellow"))
        assets.save_asset(CAPTAIN_AMMO, only_if_is_dirty=False)
        log(f"créé : {CAPTAIN_AMMO} (enfant de {parent_ammo.get_name()}, Marks Target)")
    else:
        log(f"existe déjà : {CAPTAIN_AMMO}")

    # 2) armes copiées : Captain Gun, minion gun, minion sword
    def captain_gun(bp):
        shot = action_of(bp, unreal.YSAutoShotAbility)
        shot.set_editor_property("bullets_per_burst", 1)
        shot.set_editor_property("projectile_class", assets.load_blueprint_class(CAPTAIN_AMMO))

    def minion_gun(bp):
        shot = action_of(bp, unreal.YSAutoShotAbility)
        shot.set_editor_property("cooldown", 2.0)
        shot.set_editor_property("damage", 10.0)

    def minion_sword(bp):
        swing = action_of(bp, unreal.YSAutoSwingAbility)
        swing.set_editor_property("auto_atk_cooldown", 2.0)
        swing.set_editor_property("damage", 20.0)

    duplicate_weapon(CAPTAIN_GUN_NAME, AUTO_PETOIR, "Captain Gun", captain_gun)
    duplicate_weapon(MINION_GUN_NAME, AUTO_PETOIR, "Minion AutoGun", minion_gun)
    duplicate_weapon(MINION_SWORD_NAME, SWORD, "Minion Sword", minion_sword)

    # 3) sets des minions (50 PV)
    minion_gun_set = create_weapon_set("DA_WeaponSet_MinionAutoGun", "Minion AutoGun", MINION_GUN, None, 50.0)
    minion_sword_set = create_weapon_set("DA_WeaponSet_MinionSword", "Minion Sword", MINION_SWORD, None, 50.0)

    # 4) minion : copie de Mouss reparentée, sans Head Feature, le corps prend la couleur du Captain
    if not assets.does_asset_exist(MINION):
        tools.duplicate_asset(MINION_NAME, SUMMON_DIR, assets.load_asset(MOUSS))
        bp = assets.load_asset(MINION)
        unreal.BlueprintEditorLibrary.reparent_blueprint(bp, unreal.YSCaptainMinion)
        log(f"minion : Head Feature retiré ({delete_components(bp, ('Bandana', 'Sphere', 'Head'))} composants)")
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        minion = cdo(MINION)
        minion.set_editor_property("character_name", "Captain Minion")
        minion.set_editor_property("customization_color_component_name", "BodyPivot")
        minion.set_editor_property("weapon_set", minion_sword_set)
        minion.set_editor_property("upgrade_pool", [])
        assets.save_asset(MINION, only_if_is_dirty=False)
        log(f"créé : {MINION} (AYSCaptainMinion : {isinstance(minion, unreal.YSCaptainMinion)})")
    else:
        log(f"existe déjà : {MINION}")

    # 5) Crochet du capitaine : arme d'invocation, Summon the crew !
    if not assets.does_asset_exist(HOOK):
        bp = create_bp(HOOK_NAME, WEAPON_DIR, assets.load_blueprint_class(BASE_WEAPON))
        setup_hook(bp)
        _, summon = add_component(bp, unreal.YSSummonCrewAbility, "SummonCrew")
        summon.set_editor_property("minion_class", assets.load_blueprint_class(MINION))
        summon.set_editor_property("minion_weapon_sets", [minion_gun_set, minion_sword_set])
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        weapon = cdo(HOOK)
        weapon.set_editor_property("display_name", "Crochet du capitaine")
        grip = weapon.get_editor_property("mesh")
        grip.set_editor_property("static_mesh", assets.load_asset(CYLINDER))
        grip.set_material(0, assets.load_asset(f"{MAT}/m_PH_monochrome_Brown"))
        grip.set_editor_property("relative_location", vec(0.0, 0.0, 0.0))
        grip.set_editor_property("relative_scale3d", vec(0.04 * HOOK_SCALE, 0.04 * HOOK_SCALE, 0.14 * HOOK_SCALE))
        grip.set_collision_profile_name("NoCollision")
        weapon.get_editor_property("muzzle").set_editor_property("relative_location", vec(16.0 * HOOK_SCALE, 0.0, 21.0 * HOOK_SCALE))
        assets.save_asset(HOOK, only_if_is_dirty=False)
        log(f"créé : {HOOK} (Summon the crew : 2 minions, 2 max)")
    else:
        log(f"existe déjà : {HOOK}")

    # 6) upgrade Summon system
    if not assets.does_asset_exist(SUMMON_UPGRADE):
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.YSSummonSystemUpgrade)
        upgrade = tools.create_asset(SUMMON_UPGRADE_NAME, UPGRADES_DIR, unreal.YSSummonSystemUpgrade, factory)
        upgrade.set_editor_property("offer_weight", 10.0)
        assets.save_asset(SUMMON_UPGRADE, only_if_is_dirty=False)
        log(f"créé : {SUMMON_UPGRADE}")
    else:
        log(f"existe déjà : {SUMMON_UPGRADE}")

    # 7) set du Captain
    captain_set = create_weapon_set("DA_WeaponSet_Captain", "Captain", CAPTAIN_GUN, HOOK)

    # 8) personnage : copie de Mouss, tricorne, unique
    if not assets.does_asset_exist(CAPTAIN):
        tools.duplicate_asset(CAPTAIN_NAME, CHAR_DIR, assets.load_asset(MOUSS))
        bp = assets.load_asset(CAPTAIN)
        setup_tricorne(bp)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        character = cdo(CAPTAIN)
        character.set_editor_property("character_name", "Captain")
        character.set_editor_property("weapon_set", captain_set)
        character.set_editor_property("unique_character", True)
        pool = list(character.get_editor_property("upgrade_pool")) + [assets.load_asset(SUMMON_UPGRADE)]
        character.set_editor_property("upgrade_pool", pool)
        assets.save_asset(CAPTAIN, only_if_is_dirty=False)
        log(f"créé : {CAPTAIN} (copie de Mouss, unique, set Captain, Summon system en index {len(pool) - 1})")
    else:
        log(f"existe déjà : {CAPTAIN}")

    # 9) sélection : Captain avant JohnnyPlaytest (toujours dernier)
    captain_class = assets.load_blueprint_class(CAPTAIN)
    johnny_class = assets.load_blueprint_class(JOHNNY)
    for path in GAME_MODES:
        gm = cdo(path)
        current = list(gm.get_editor_property("selectable_characters"))
        if captain_class in current:
            log(f"{path} : Captain déjà dans la sélection")
            continue
        selectable = [c for c in current if c != johnny_class] + [captain_class]
        if johnny_class in current:
            selectable.append(johnny_class)
        gm.set_editor_property("selectable_characters", selectable)
        assets.save_asset(path, only_if_is_dirty=False)

    # relecture
    for path in GAME_MODES:
        log(f"relu {path} : " + ", ".join(c.get_name() for c in cdo(path).get_editor_property("selectable_characters") if c))
    for path in (CAPTAIN_GUN, MINION_GUN, MINION_SWORD, HOOK):
        for t in templates(assets.load_asset(path)):
            if isinstance(t, unreal.YSAbilityComponent):
                extra = ""
                if isinstance(t, unreal.YSCoupDPetoirAbility):
                    extra = f", {t.get_editor_property('bullets_per_burst')} balle(s), AMMO {t.get_editor_property('projectile_class').get_name()}"
                if type(t) == unreal.YSAutoSwingAbility:
                    extra = f", Auto Atk CD {t.get_editor_property('auto_atk_cooldown')}"
                if isinstance(t, unreal.YSSummonCrewAbility):
                    log(f"relu {path.split('/')[-1]}.{t.get_name()} : {t.get_editor_property('minions_per_summon')} minions / {t.get_editor_property('base_max_minions')} max, "
                        f"recharge {t.get_editor_property('cooldown')}, {len(t.get_editor_property('minion_weapon_sets'))} sets")
                    continue
                log(f"relu {path.split('/')[-1]}.{t.get_name()} : {t.get_class().get_name()} dégâts {t.get_editor_property('damage')}, recharge {t.get_editor_property('cooldown')}{extra}")
    character = cdo(CAPTAIN)
    log(f"relu Captain : set {character.get_editor_property('weapon_set').get_name()}, unique {character.get_editor_property('unique_character')}, "
        f"{len(character.get_editor_property('upgrade_pool'))} upgrades")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_captain.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
