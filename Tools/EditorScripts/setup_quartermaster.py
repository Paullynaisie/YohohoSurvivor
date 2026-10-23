"""
Troisième personnage jouable : le Quartermaster (UNIQUE : un seul par partie ; 20 % plus grand).
  - /Game/Placeable/Weapons/Player/BP_GreatAxe (enfant de BP_Arme, À DEUX MAINS : OffHandSocket au milieu du manche) :
      manche long, tête d'acier, lame (sous le composant « Coloration », m_PH_emissive_turboWhite par défaut), pique arrière ;
      « Hitbox » (UBoxComponent, sans collision) autour de la tête ;
      « AutoSwing » = Auto swing en Auto atk : TOUS les réglages de l'Auto swing de BP_Epee (dégâts, poses, timings,
        second swing, estoc critique, détection, akimbo...) ;
      « BumpingSlash » = Bumping Slash en Ability : tous les réglages du Dashing Slash de BP_BattleSword, sans invulnérabilité,
        bump à 800 cm (cloche 300 cm, 1 s, 1 tour)
  - /Game/Core/WeaponSets/DA_WeaponSet_GreatAxe (main droite ; PV / pushback copiés du set de Mouss)
  - /Game/Placeable/Character/Player/PlayerCharacter_Quartermaster : copie de PlayerCharacter_mouss (« Quartermaster »,
    Unique Character, Character Scale 1.2, set GreatAxe) ; Head Feature : bandana de Mouss retiré, bandeau (cylindre)
    autour de la tête avec un nœud sur le côté (petite sphère + 2 cordes qui pendent), tout enfant de Head Feature
    (couleur de la personnalisation)
  - sélection (BP_TestGameMode / BP_TestMapGameMode) : ..., Quartermaster, JohnnyPlaytest toujours en dernier
Ne recrée rien qui existe déjà (réglages de Paul conservés).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_quartermaster.py"
Rapport : Saved/Logs/setup_quartermaster.txt
"""
import math
import os
import unreal

WEAPON_DIR = "/Game/Placeable/Weapons/Player"
BASE_WEAPON = "/Game/Placeable/Weapons/BP_Arme"
SWORD = f"{WEAPON_DIR}/BP_Epee"
BATTLE_SWORD = f"{WEAPON_DIR}/BP_BattleSword"
AXE_NAME = "BP_GreatAxe"
AXE = f"{WEAPON_DIR}/{AXE_NAME}"
SETS_DIR = "/Game/Core/WeaponSets"
MOUSS_SET = f"{SETS_DIR}/DA_WeaponSet_SabrePistolet"
SET_NAME = "DA_WeaponSet_GreatAxe"
SET_PATH = f"{SETS_DIR}/{SET_NAME}"
CHAR_DIR = "/Game/Placeable/Character/Player"
MOUSS = f"{CHAR_DIR}/PlayerCharacter_mouss"
JOHNNY = f"{CHAR_DIR}/PlayerCharacter_JohnnyPlaytest"
QM_NAME = "PlayerCharacter_Quartermaster"
QM = f"{CHAR_DIR}/{QM_NAME}"
GAME_MODES = ["/Game/Core/GameModes/BP_TestGameMode", "/Game/Core/GameModes/BP_TestMapGameMode"]
MAT = "/Game/Art/Materials/PH_materials"
SPHERE = "/Engine/BasicShapes/Sphere"
CYLINDER = "/Engine/BasicShapes/Cylinder"
CUBE = "/Engine/BasicShapes/Cube"
CONE = "/Engine/BasicShapes/Cone"
HEAD = "Head Feature"
QM_SCALE = 1.2
# bandeau (espace du corps) : centre, rayon, hauteur ; nœud sur le côté gauche, vers l'arrière
BAND_CENTER = (-1.0, 0.0, 40.0)
BAND_RADIUS, BAND_HEIGHT = 46.0, 12.0
KNOT_ANGLE = 230.0   # degrés depuis l'avant (+X) vers la droite (+Y) : 230 = arrière gauche

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
ys = unreal.YSEditorScriptLibrary
report = []


def log(msg):
    unreal.log(f"[setup_quartermaster] {msg}")
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


def setup_head(bp):
    """Head Feature du Quartermaster : bandeau + nœud (sphère et 2 cordes) sur le côté"""
    head_handle = find_handle(bp, HEAD)
    if head_handle is None:
        raise RuntimeError(f"composant « {HEAD} » introuvable")
    head = lib.get_object(lib.get_data(head_handle))
    color = head.get_material(0)
    root = handles(bp)[0]

    # réglages des cordes du bandana de Mouss (repris pour les cordes du nœud)
    mouss_rope = next((t for t in templates(assets.load_asset(MOUSS)) if isinstance(t, unreal.YSDanglingChainComponent)), None)

    removed = 0
    while True:
        old = next((h for h in handles(bp) if str(lib.get_variable_name(lib.get_data(h))).startswith(("Bandana", "Sphere"))), None)
        if old is None:
            break
        subsystem.delete_subobject(root, old, bp)
        removed += 1
    log(f"bandana de Mouss retiré ({removed} composants)")

    space = HeadSpace(head)
    cx, cy, cz = BAND_CENTER
    # bandeau : cylindre plat autour de la tête, incliné comme elle
    shape(bp, "Headband", CYLINDER, color, space.location(cx, cy, cz), rot(), space.scale3d(BAND_RADIUS / 50.0, BAND_RADIUS / 50.0, BAND_HEIGHT / 100.0),
          head_handle, decals=False)

    # nœud sur le côté : petite sphère sur le bandeau + 2 cordes qui pendent (et flottent vers l'arrière)
    a = math.radians(KNOT_ANGLE)
    kx, ky = cx + (BAND_RADIUS + 3.0) * math.cos(a), cy + (BAND_RADIUS + 3.0) * math.sin(a)
    shape(bp, "HeadbandKnot", SPHERE, color, space.location(kx, ky, cz), rot(), space.scale3d(0.16, 0.16, 0.16), head_handle, decals=False)
    for i, (dx, dz) in enumerate(((-3.0, 3.0), (3.0, -3.0))):
        _, rope = add_component(bp, unreal.YSDanglingChainComponent, f"HeadbandRope{i + 1}", head_handle)
        if mouss_rope:
            ys.copy_editable_properties(mouss_rope, rope)
        rope.set_editor_property("relative_location", space.location(kx + dx, ky, cz + dz))
        rope.set_editor_property("segment_material", color)
    log(f"bandeau (rayon {BAND_RADIUS}, hauteur {BAND_HEIGHT}) + nœud HeadbandKnot + cordes HeadbandRope1/2 (réglages du bandana de Mouss)")


try:
    # 1) GreatAxe : hache à deux mains, Auto swing de l'épée + Bumping Slash
    if not assets.does_asset_exist(AXE):
        bp = create_bp(AXE_NAME, WEAPON_DIR, assets.load_blueprint_class(BASE_WEAPON))
        # manche = Mesh (natif, réglé plus bas) ; pommeau, tête d'acier, pique arrière et pointe
        shape(bp, "Pommel", SPHERE, "m_PH_monochrome_Grey", vec(0.0, 0.0, -47.0), rot(), vec(0.12, 0.12, 0.12))
        shape(bp, "AxeHead", CUBE, "m_PH_monochrome_Grey", vec(0.0, 0.0, 118.0), rot(), vec(0.22, 0.1, 0.32))
        shape(bp, "BackSpike", CONE, "m_PH_monochrome_Grey", vec(-20.0, 0.0, 118.0), rot(pitch=90.0), vec(0.15, 0.15, 0.25))
        shape(bp, "TopSpike", CONE, "m_PH_monochrome_Grey", vec(0.0, 0.0, 145.0), rot(), vec(0.08, 0.08, 0.18))
        # lame : sous « Coloration » (couleur de main / de classe en jeu), turbo blanc par défaut
        coloration_handle, _ = add_component(bp, unreal.SceneComponent, "Coloration")
        shape(bp, "Blade", CUBE, "m_PH_emissive_turboWhite", vec(25.0, 0.0, 118.0), rot(), vec(0.3, 0.04, 0.44), coloration_handle)
        shape(bp, "BladeEdge", CYLINDER, "m_PH_emissive_turboWhite", vec(38.0, 0.0, 118.0), rot(roll=90.0), vec(0.6, 0.6, 0.04), coloration_handle)

        _, hitbox = add_component(bp, unreal.BoxComponent, "Hitbox")
        hitbox.set_editor_property("relative_location", vec(30.0, 0.0, 100.0))
        hitbox.set_editor_property("box_extent", vec(60.0, 60.0, 90.0))
        hitbox.set_collision_profile_name("NoCollision")

        sword_swing = next(t for t in templates(assets.load_asset(SWORD)) if type(t) == unreal.YSAutoSwingAbility)
        dash_src = next(t for t in templates(assets.load_asset(BATTLE_SWORD)) if isinstance(t, unreal.YSDashingSlashAbility))

        _, swing = add_component(bp, unreal.YSAutoSwingAbility, "AutoSwing")
        copied = ys.copy_editable_properties(sword_swing, swing)
        log(f"AutoSwing : {copied} réglages copiés de l'Auto swing de BP_Epee")

        _, bump = add_component(bp, unreal.YSBumpingSlashAbility, "BumpingSlash")
        copied = ys.copy_editable_properties(dash_src, bump)
        bump.set_editor_property("dash_invulnerable", False)
        bump.set_editor_property("bump_distance", 800.0)
        bump.set_editor_property("bump_apex_height", 300.0)
        bump.set_editor_property("bump_duration", 1.0)
        bump.set_editor_property("bump_spin_turns", 1.0)
        log(f"BumpingSlash : {copied} réglages copiés du Dashing Slash de BP_BattleSword, sans invulnérabilité, bump 800 cm")
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)

        weapon = cdo(AXE)
        weapon.set_editor_property("display_name", "GreatAxe")
        weapon.set_editor_property("two_handed", True)
        sword = cdo(SWORD)
        for prop in ("override_hand_location", "hand_location", "override_hand_rotation", "hand_rotation"):
            weapon.set_editor_property(prop, sword.get_editor_property(prop))
        shaft = weapon.get_editor_property("mesh")
        shaft.set_editor_property("static_mesh", assets.load_asset(CYLINDER))
        shaft.set_material(0, assets.load_asset(f"{MAT}/m_PH_monochrome_Brown"))
        shaft.set_editor_property("relative_location", vec(0.0, 0.0, 45.0))
        shaft.set_editor_property("relative_scale3d", vec(0.07, 0.07, 1.8))
        shaft.set_collision_profile_name("NoCollision")
        weapon.get_editor_property("muzzle").set_editor_property("relative_location", vec(45.0, 0.0, 118.0))
        weapon.get_editor_property("off_hand_socket").set_editor_property("relative_location", vec(0.0, 0.0, 50.0))
        assets.save_asset(AXE, only_if_is_dirty=False)
        log(f"créé : {AXE} (deux mains ; AutoSwing + BumpingSlash)")
    else:
        log(f"existe déjà : {AXE}")

    # 2) set d'arme (PV / pushback du set de Mouss)
    if not assets.does_asset_exist(SET_PATH):
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.YSWeaponSet)
        weapon_set = tools.create_asset(SET_NAME, SETS_DIR, unreal.YSWeaponSet, factory)
        ys.copy_editable_properties(assets.load_asset(MOUSS_SET), weapon_set)
        weapon_set.set_editor_property("display_name", "GreatAxe")
        weapon_set.set_editor_property("right_hand_weapon", assets.load_blueprint_class(AXE))
        weapon_set.set_editor_property("left_hand_weapon", None)
        assets.save_asset(SET_PATH, only_if_is_dirty=False)
        log(f"créé : {SET_PATH}")
    else:
        log(f"existe déjà : {SET_PATH}")

    # 3) personnage : copie de Mouss, bandeau, unique, plus grand
    if not assets.does_asset_exist(QM):
        tools.duplicate_asset(QM_NAME, CHAR_DIR, assets.load_asset(MOUSS))
        bp = assets.load_asset(QM)
        setup_head(bp)
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        character = cdo(QM)
        character.set_editor_property("character_name", "Quartermaster")
        character.set_editor_property("weapon_set", assets.load_asset(SET_PATH))
        character.set_editor_property("unique_character", True)
        character.set_editor_property("character_scale", QM_SCALE)
        assets.save_asset(QM, only_if_is_dirty=False)
        log(f"créé : {QM} (copie de Mouss, unique, taille x{QM_SCALE}, set GreatAxe)")
    else:
        log(f"existe déjà : {QM}")

    # 4) sélection : Quartermaster avant JohnnyPlaytest (toujours dernier)
    qm_class = assets.load_blueprint_class(QM)
    johnny_class = assets.load_blueprint_class(JOHNNY)
    for path in GAME_MODES:
        gm = cdo(path)
        current = list(gm.get_editor_property("selectable_characters"))
        if qm_class in current:
            log(f"{path} : Quartermaster déjà dans la sélection")
            continue
        selectable = [c for c in current if c != johnny_class] + [qm_class]
        if johnny_class in current:
            selectable.append(johnny_class)
        gm.set_editor_property("selectable_characters", selectable)
        assets.save_asset(path, only_if_is_dirty=False)

    # relecture
    for path in GAME_MODES:
        log(f"relu {path} : " + ", ".join(c.get_name() for c in cdo(path).get_editor_property("selectable_characters") if c))
    for t in templates(assets.load_asset(AXE)):
        if isinstance(t, unreal.YSAbilityComponent):
            log(f"relu {t.get_name()} : {t.get_class().get_name()} dégâts {t.get_editor_property('damage')}, recharge {t.get_editor_property('cooldown')}")
    character = cdo(QM)
    log(f"relu Quartermaster : nom {character.get_editor_property('character_name')}, set {character.get_editor_property('weapon_set').get_name()}, "
        f"unique {character.get_editor_property('unique_character')}, taille {character.get_editor_property('character_scale')}, "
        f"{len(character.get_editor_property('upgrade_pool'))} upgrades")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_quartermaster.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
