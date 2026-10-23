"""
Second personnage jouable : le Cannoneer (base = Mouss ; seuls la coiffure « Head Feature » et le set d'arme changent).
  - /Game/Placeable/AMMO/BP_CannoneerBomb (AYSPlayerBomb) : bombe posée par la Roll and Bomb, visuel copié de BP_NME_Bomb
    (sphère noire, mèche, bout rouge), mèche 1 s, explosion rayon 300 sur les NME
  - /Game/Placeable/AMMO/ShotgunAMMO (enfant de PetoirAMMO) : petite AMMO jaune, courte portée
  - /Game/Placeable/Weapons/Player/BP_Shotgun (enfant de BP_Arme, À DEUX MAINS : OffHandSocket sous le fût) :
      « AutoShot » = Auto shot en shoot system Shotgun (5 AMMO à +/- 20°, 10 dégâts chacune, visée assistée du pétoir)
      « RollAndBomb » = Roll and Bomb (Ability : bombe aux pieds + roulade du Rolling shot de BP_Pistolet, recharge 5 s)
  - /Game/Core/WeaponSets/DA_WeaponSet_Shotgun (main droite : BP_Shotgun)
  - /Game/Placeable/Character/Player/PlayerCharacter_Cannoneer : copie de PlayerCharacter_mouss (« Cannoneer », set Shotgun) ;
    Head Feature : bandana retiré, tresses plaquées (4 rangées de nattes sur le crâne + 4 cordes qui pendent derrière),
    mèche devant le visage, 2 cylindres de lunettes ; tout est enfant de Head Feature (prend la couleur de la personnalisation)
  - sélection (BP_TestGameMode / BP_TestMapGameMode) : Mouss, Cannoneer, ..., JohnnyPlaytest toujours en dernier
Ne recrée rien qui existe déjà (réglages de Paul conservés).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_cannoneer.py"
Rapport : Saved/Logs/setup_cannoneer.txt
"""
import math
import os
import unreal

AMMO_DIR = "/Game/Placeable/AMMO"
BOMB_NAME = "BP_CannoneerBomb"
BOMB = f"{AMMO_DIR}/{BOMB_NAME}"
PETOIR_AMMO = f"{AMMO_DIR}/PetoirAMMO"
SHOTGUN_AMMO_NAME = "ShotgunAMMO"
SHOTGUN_AMMO = f"{AMMO_DIR}/{SHOTGUN_AMMO_NAME}"
NME_BOMB = "/Game/Placeable/Weapons/Foe/BP_NME_Bomb"
WEAPON_DIR = "/Game/Placeable/Weapons/Player"
BASE_WEAPON = "/Game/Placeable/Weapons/BP_Arme"
PISTOL = f"{WEAPON_DIR}/BP_Pistolet"
BATTLE_PETOIR = f"{WEAPON_DIR}/BP_BattlePetoir"
SHOTGUN_NAME = "BP_Shotgun"
SHOTGUN = f"{WEAPON_DIR}/{SHOTGUN_NAME}"
SETS_DIR = "/Game/Core/WeaponSets"
SET_NAME = "DA_WeaponSet_Shotgun"
SET_PATH = f"{SETS_DIR}/{SET_NAME}"
CHAR_DIR = "/Game/Placeable/Character/Player"
MOUSS = f"{CHAR_DIR}/PlayerCharacter_mouss"
JOHNNY = f"{CHAR_DIR}/PlayerCharacter_JohnnyPlaytest"
CANNONEER_NAME = "PlayerCharacter_Cannoneer"
CANNONEER = f"{CHAR_DIR}/{CANNONEER_NAME}"
GAME_MODES = ["/Game/Core/GameModes/BP_TestGameMode", "/Game/Core/GameModes/BP_TestMapGameMode"]
MAT = "/Game/Art/Materials/PH_materials"
SPHERE = "/Engine/BasicShapes/Sphere"
CYLINDER = "/Engine/BasicShapes/Cylinder"
CUBE = "/Engine/BasicShapes/Cube"
HEAD = "Head Feature"
# la bombe des NME est centrée 20 cm au-dessus de la main (rayon 25) : posée au sol, tout monte de 5 cm
BOMB_LIFT = 5.0
# lunettes sur le front (espace du corps) : ouverture vers l avant, un peu vers le haut
GOGGLE_X, GOGGLE_Z, GOGGLE_PITCH = 47.0, 50.0, -75.0
GOGGLE_SCALE = (0.2, 0.2, 0.1)

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_cannoneer] {msg}")
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


def copy_props(src, dst, names):
    for n in names:
        dst.set_editor_property(n, src.get_editor_property(n))


def create_bp(name, folder, parent_class):
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    return tools.create_asset(name, folder, unreal.Blueprint, factory)


# --- Coiffure : coordonnées données dans l'espace du corps (BodyPivot), converties dans celui de Head Feature ---
class HeadSpace:
    def __init__(self, comp):
        self.loc = comp.get_editor_property("relative_location")
        self.pitch = comp.get_editor_property("relative_rotation").pitch
        self.scale = comp.get_editor_property("relative_scale3d")

    def location(self, x, y, z):
        """Point du corps -> point local de Head Feature (rotation pitch puis échelle inversées)"""
        dx, dy, dz = x - self.loc.x, y - self.loc.y, z - self.loc.z
        p = math.radians(self.pitch)
        lx = dx * math.cos(p) + dz * math.sin(p)
        lz = -dx * math.sin(p) + dz * math.cos(p)
        return vec(lx / self.scale.x, dy / self.scale.y, lz / self.scale.z)

    def scale3d(self, x, y, z):
        return vec(x / self.scale.x, y / self.scale.y, z / self.scale.z)


def setup_hair(bp):
    """Head Feature du Cannoneer : tresses plaquées, mèche, lunettes (tout enfant de Head Feature)"""
    head_handle = find_handle(bp, HEAD)
    if head_handle is None:
        raise RuntimeError(f"composant « {HEAD} » introuvable")
    head = lib.get_object(lib.get_data(head_handle))
    color = head.get_material(0)
    root = handles(bp)[0]

    # bandana de Mouss : retiré
    removed = 0
    while True:
        old = next((h for h in handles(bp) if str(lib.get_variable_name(lib.get_data(h))).startswith(("Bandana", "Sphere"))), None)
        if old is None:
            break
        subsystem.delete_subobject(root, old, bp)
        removed += 1
    log(f"bandana de Mouss retiré ({removed} composants)")

    # tresses plaquées : 4 rangées de nattes de l'avant vers l'arrière du crâne (repère local de la sphère, rayon 50),
    # chaque natte = une bosse allongée dans le sens de la rangée, à moitié enfoncée (aplatie par l'échelle de Head Feature)
    rows = [-30.0, -10.0, 10.0, 30.0]
    angles = [40.0, 63.0, 86.0, 109.0, 132.0, 155.0]   # depuis l'avant (+X) en passant par le dessus (+Z)
    for r, y in enumerate(rows):
        radius = math.sqrt(50.0 ** 2 - y ** 2)
        for i, a in enumerate(angles):
            phi = math.radians(a)
            shape(bp, f"Braid{r + 1}_{i + 1}", SPHERE, color, vec(radius * math.cos(phi), y, radius * math.sin(phi)),
                  rot(pitch=90.0 - a, yaw=180.0), vec(0.24, 0.15, 0.13), head_handle)
        # bout de la natte qui pend derrière la tête (corde)
        phi = math.radians(172.0)
        _, chain = add_component(bp, unreal.YSDanglingChainComponent, f"BraidRope{r + 1}", head_handle)
        chain.set_editor_property("relative_location", vec(radius * math.cos(phi), y, radius * math.sin(phi)))
        chain.set_editor_property("segment_count", 5)
        chain.set_editor_property("segment_length", 9.0)
        chain.set_editor_property("segment_radius", 4.0)
        chain.set_editor_property("segment_mesh", assets.load_asset(CYLINDER))
        chain.set_editor_property("segment_material", color)
    log("tresses plaquées : 4 rangées de 6 nattes + 4 cordes (BraidRope1..4)")

    # mèche devant le visage et lunettes : placées dans l'espace du corps (le visage est à x 42, z 10)
    space = HeadSpace(head)
    shape(bp, "FaceLock", SPHERE, color, space.location(48.0, -9.0, 24.0), rot(pitch=-space.pitch),
          space.scale3d(0.06, 0.14, 0.42), head_handle, decals=False)
    for side, y in (("Left", -12.0), ("Right", 12.0)):
        # cylindres sur le front, ouverture vers l'avant et un peu vers le haut
        shape(bp, f"Goggle{side}", CYLINDER, color, space.location(GOGGLE_X, y, GOGGLE_Z), rot(pitch=GOGGLE_PITCH - space.pitch),
              space.scale3d(*GOGGLE_SCALE), head_handle, decals=False)
    log("mèche FaceLock et lunettes GoggleLeft / GoggleRight ajoutées")


try:
    # 1) bombe du Cannoneer : visuel copié de la bombe des NME
    nme_bomb_bp = assets.load_asset(NME_BOMB)
    nme_bomb = cdo(NME_BOMB)
    bomb_action = next((t for t in templates(nme_bomb_bp) if isinstance(t, unreal.YSFoeBombAction)), None)
    if not assets.does_asset_exist(BOMB):
        bp = create_bp(BOMB_NAME, AMMO_DIR, unreal.YSPlayerBomb)
        for t in templates(nme_bomb_bp):
            if isinstance(t, unreal.StaticMeshComponent) and t.get_name().replace("_GEN_VARIABLE", "") not in ("Mesh",) and t != nme_bomb.get_editor_property("mesh"):
                loc = t.get_editor_property("relative_location")
                shape(bp, t.get_name().replace("_GEN_VARIABLE", ""), t.get_editor_property("static_mesh").get_path_name().split(".")[0], t.get_material(0),
                      vec(loc.x, loc.y, loc.z + BOMB_LIFT), t.get_editor_property("relative_rotation"), t.get_editor_property("relative_scale3d"))
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        bomb = cdo(BOMB)
        src_mesh = nme_bomb.get_editor_property("mesh")
        mesh = bomb.get_editor_property("mesh")
        mesh.set_editor_property("static_mesh", src_mesh.get_editor_property("static_mesh"))
        mesh.set_material(0, src_mesh.get_material(0))
        src_loc = src_mesh.get_editor_property("relative_location")
        mesh.set_editor_property("relative_location", vec(src_loc.x, src_loc.y, src_loc.z + BOMB_LIFT))
        mesh.set_editor_property("relative_scale3d", src_mesh.get_editor_property("relative_scale3d"))
        if bomb_action:
            bomb.set_editor_property("fuse_duration", bomb_action.get_editor_property("bomb_fuse_duration"))
            bomb.set_editor_property("bomb_color", bomb_action.get_editor_property("bomb_color"))
            bomb.set_editor_property("fuse_color", bomb_action.get_editor_property("fuse_color"))
            bomb.set_editor_property("fuse_glow", bomb_action.get_editor_property("fuse_glow"))
            bomb.set_editor_property("bomb_material", bomb_action.get_editor_property("bomb_material"))
            bomb.set_editor_property("explosion_material", bomb_action.get_editor_property("explosion_material"))
            bomb.set_editor_property("zone_material", bomb_action.get_editor_property("preview_material"))
            bomb.set_editor_property("explosion_grow_duration", bomb_action.get_editor_property("explosion_grow_duration"))
            bomb.set_editor_property("explosion_fade_duration", bomb_action.get_editor_property("explosion_fade_duration"))
        assets.save_asset(BOMB, only_if_is_dirty=False)
        log(f"créé : {BOMB} (visuel de BP_NME_Bomb, mèche {bomb.get_editor_property('fuse_duration')} s, rayon {bomb.get_editor_property('explosion_radius')})")
    else:
        log(f"existe déjà : {BOMB}")

    # 2) Shotgun AMMO : petite, courte portée
    if not assets.does_asset_exist(SHOTGUN_AMMO):
        create_bp(SHOTGUN_AMMO_NAME, AMMO_DIR, assets.load_blueprint_class(PETOIR_AMMO))
        ammo = cdo(SHOTGUN_AMMO)
        ammo.set_editor_property("life_time", 0.45)
        ammo_mesh = ammo.get_editor_property("mesh")
        ammo_mesh.set_editor_property("relative_scale3d", vec(0.3, 0.3, 0.3))
        ammo_mesh.set_material(0, assets.load_asset(f"{MAT}/m_PH_emissive_turboYellow"))
        assets.save_asset(SHOTGUN_AMMO, only_if_is_dirty=False)
        log(f"créé : {SHOTGUN_AMMO} (durée de vie 0.45 s, mesh x0.3 jaune)")
    else:
        log(f"existe déjà : {SHOTGUN_AMMO}")

    # 3) Shotgun : arme à deux mains, Auto shot en Shotgun + Roll and Bomb
    if not assets.does_asset_exist(SHOTGUN):
        bp = create_bp(SHOTGUN_NAME, WEAPON_DIR, assets.load_blueprint_class(BASE_WEAPON))
        # crosse = Mesh (natif) ; boîtier, double canon, fût (sous lequel se place l'autre main)
        shape(bp, "Receiver", CUBE, "m_PH_monochrome_Grey", vec(8.0, 0.0, 4.0), rot(), vec(0.2, 0.1, 0.12))
        shape(bp, "BarrelLeft", CYLINDER, "m_PH_monochrome_Grey", vec(40.0, -3.2, 7.0), rot(pitch=-90.0), vec(0.06, 0.06, 0.56))
        shape(bp, "BarrelRight", CYLINDER, "m_PH_monochrome_Grey", vec(40.0, 3.2, 7.0), rot(pitch=-90.0), vec(0.06, 0.06, 0.56))
        shape(bp, "ForeEnd", CUBE, "m_PH_monochrome_Brown", vec(32.0, 0.0, 2.5), rot(), vec(0.2, 0.1, 0.06))
        shape(bp, "BarrelBand", CYLINDER, "m_PH_monochrome_Grey", vec(60.0, 0.0, 7.0), rot(pitch=-90.0), vec(0.15, 0.15, 0.03))

        pistol = assets.load_asset(PISTOL)
        rolling = next(t for t in templates(pistol) if isinstance(t, unreal.YSRollingShotAbility))
        battle = assets.load_asset(BATTLE_PETOIR)
        auto_src = next(t for t in templates(battle) if isinstance(t, unreal.YSAutoShotAbility))

        _, auto = add_component(bp, unreal.YSAutoShotAbility, "AutoShot")
        auto.set_editor_property("display_name", "Shotgun")
        auto.set_editor_property("shoot_system", unreal.YSShootSystem.SHOTGUN)
        auto.set_editor_property("shotgun_ammo_per_shot", 5)
        auto.set_editor_property("shotgun_spread", 20.0)
        auto.set_editor_property("damage", 10.0)
        auto.set_editor_property("projectile_class", assets.load_blueprint_class(SHOTGUN_AMMO))
        copy_props(auto_src, auto, ["cooldown", "assist_aim_distance", "assist_aim_angle", "recoil_kick_duration", "recoil_recover_duration", "icon_tint"])
        auto.set_editor_property("recoil_pitch", 30.0)
        auto.set_editor_property("recoil_kick_back", 25.0)
        auto.set_editor_property("play_reload_anim", False)

        _, roll = add_component(bp, unreal.YSRollAndBombAbility, "RollAndBomb")
        copy_props(rolling, roll, ["roll_duration", "roll_speed_multiplier", "force_move_without_input", "roll_speed_decay_exponent",
                                   "roll_through_foes", "extra_invulnerability", "roll_anim_turns", "roll_anim_duration",
                                   "roll_anim_ease_exponent", "roll_anim_tuck_height", "icon_tint"])
        roll.set_editor_property("cooldown", 5.0)
        roll.set_editor_property("bomb_class", assets.load_blueprint_class(BOMB))
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)

        weapon = cdo(SHOTGUN)
        weapon.set_editor_property("display_name", "Shotgun")
        weapon.set_editor_property("two_handed", True)
        weapon.set_editor_property("override_hand_location", True)
        weapon.set_editor_property("hand_location", vec(30.0, 28.0, -5.0))
        weapon.set_editor_property("override_hand_rotation", True)
        weapon.set_editor_property("hand_rotation", rot())
        stock = weapon.get_editor_property("mesh")
        stock.set_editor_property("static_mesh", assets.load_asset(CUBE))
        stock.set_material(0, assets.load_asset(f"{MAT}/m_PH_monochrome_Brown"))
        stock.set_editor_property("relative_location", vec(-12.0, 0.0, 0.0))
        stock.set_editor_property("relative_rotation", rot(pitch=-12.0))
        stock.set_editor_property("relative_scale3d", vec(0.3, 0.09, 0.13))
        stock.set_collision_profile_name("NoCollision")
        weapon.get_editor_property("muzzle").set_editor_property("relative_location", vec(69.0, 0.0, 7.0))
        weapon.get_editor_property("off_hand_socket").set_editor_property("relative_location", vec(32.0, 0.0, -2.0))
        assets.save_asset(SHOTGUN, only_if_is_dirty=False)
        log(f"créé : {SHOTGUN} (deux mains ; AutoShot Shotgun 5 x 10 dégâts +/- 20°, recharge {auto.get_editor_property('cooldown')} s ; "
            f"RollAndBomb roulade {roll.get_editor_property('roll_duration')} s x{roll.get_editor_property('roll_speed_multiplier')}, recharge 5 s)")
    else:
        log(f"existe déjà : {SHOTGUN}")

    # 4) set d'arme
    if not assets.does_asset_exist(SET_PATH):
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.YSWeaponSet)
        weapon_set = tools.create_asset(SET_NAME, SETS_DIR, unreal.YSWeaponSet, factory)
        weapon_set.set_editor_property("display_name", "Shotgun")
        weapon_set.set_editor_property("right_hand_weapon", assets.load_blueprint_class(SHOTGUN))
        assets.save_asset(SET_PATH, only_if_is_dirty=False)
        log(f"créé : {SET_PATH}")
    else:
        log(f"existe déjà : {SET_PATH}")

    # 5) personnage : copie de Mouss, coiffure et set d'arme propres
    if not assets.does_asset_exist(CANNONEER):
        tools.duplicate_asset(CANNONEER_NAME, CHAR_DIR, assets.load_asset(MOUSS))
        bp = assets.load_asset(CANNONEER)
        setup_hair(bp)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        character = cdo(CANNONEER)
        character.set_editor_property("character_name", "Cannoneer")
        character.set_editor_property("weapon_set", assets.load_asset(SET_PATH))
        assets.save_asset(CANNONEER, only_if_is_dirty=False)
        log(f"créé : {CANNONEER} (copie de Mouss, set Shotgun)")
    else:
        log(f"existe déjà : {CANNONEER}")

    # 6) sélection : Cannoneer avant JohnnyPlaytest (toujours dernier)
    cannoneer_class = assets.load_blueprint_class(CANNONEER)
    johnny_class = assets.load_blueprint_class(JOHNNY)
    for path in GAME_MODES:
        gm = cdo(path)
        selectable = list(gm.get_editor_property("selectable_characters"))
        if cannoneer_class in selectable:
            log(f"{path} : Cannoneer déjà dans la sélection")
            continue
        selectable = [c for c in selectable if c != johnny_class] + [cannoneer_class]
        if johnny_class in gm.get_editor_property("selectable_characters"):
            selectable.append(johnny_class)
        gm.set_editor_property("selectable_characters", selectable)
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"{path} : sélection " + ", ".join(c.get_name() for c in selectable if c))

    # relecture
    for path in GAME_MODES:
        log(f"relu {path} : " + ", ".join(c.get_name() for c in cdo(path).get_editor_property("selectable_characters") if c))
    shotgun_bp = assets.load_asset(SHOTGUN)
    for t in templates(shotgun_bp):
        if isinstance(t, unreal.YSAbilityComponent):
            log(f"relu {t.get_name()} : {t.get_class().get_name()} recharge {t.get_editor_property('cooldown')}")
    character = cdo(CANNONEER)
    log(f"relu Cannoneer : nom {character.get_editor_property('character_name')}, set {character.get_editor_property('weapon_set').get_name()}, "
        f"{len(character.get_editor_property('upgrade_pool'))} upgrades")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_cannoneer.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
