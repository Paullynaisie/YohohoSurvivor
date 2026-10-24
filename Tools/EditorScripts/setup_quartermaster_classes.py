"""
Upgrades de classe du Quartermaster (set de départ DA_WeaponSet_GreatAxe) + taille par set d'arme :
  - taille : Quartermaster remis à Character Scale 1 ; Character Scale Multiplier des sets : GreatAxe x1.25, Anchor Bearer x1.5,
    Boulet et Chaîne x1.25
  - « Anchor Bearer » (DA_WeaponSet_AnchorBearer) : /Game/Placeable/Weapons/Player/BP_GreatAnchor = copie de BP_GreatAxe (même Auto swing)
    avec une tête d'ancre en primitives (bras + pattes sous « Coloration », jas, organeau) ; Bumping Slash remplacé par « MeteorJump »
    (réglages du Bumping Slash, saut réglé comme le lancer du BombThrower de BP_Sniper : durée, sommet, portée, pas de recul)
  - « Boulet et Chaîne » (DA_WeaponSet_BouletEtChaine) : BP_MorningStar = copie de BP_GreatAxe (même Auto swing, même Bumping Slash),
    tête retirée : poignée courte (Mesh) ; « BallChain » (chaîne de la main principale, boulet lourd au bout : « Ball » + pointes,
    sous « Coloration », et la « Hitbox » de l'arme placée sur le boulet), « HandChain » (entre les deux mains : vers l'Off Hand Socket),
    « TailChain » (pend derrière l'autre main)
  - BloodRage : Great Anchor et MorningStar ajoutées à ses armes ; Bloodlust : exclues
  - /Game/Core/Upgrades/DA_ClassUpgrade_Quartermaster2AnchorBearer et DA_ClassUpgrade_Quartermaster2BouletEtChaine, à la fin du pool
Ne recrée rien qui existe déjà (réglages de Paul conservés).

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_quartermaster_classes.py"
Rapport : Saved/Logs/setup_quartermaster_classes.txt
"""
import math
import os
import unreal

WEAPON_DIR = "/Game/Placeable/Weapons/Player"
AXE = f"{WEAPON_DIR}/BP_GreatAxe"
SNIPER = f"{WEAPON_DIR}/BP_Sniper"
ANCHOR_NAME, STAR_NAME = "BP_GreatAnchor", "BP_MorningStar"
ANCHOR, STAR = f"{WEAPON_DIR}/{ANCHOR_NAME}", f"{WEAPON_DIR}/{STAR_NAME}"
SETS_DIR = "/Game/Core/WeaponSets"
AXE_SET = f"{SETS_DIR}/DA_WeaponSet_GreatAxe"
UPGRADES_DIR = "/Game/Core/Upgrades"
BLOODRAGE = f"{UPGRADES_DIR}/DA_Upgrade_BloodRage"
BLOODLUST = f"{UPGRADES_DIR}/DA_Upgrade_Bloodlust"
QM = "/Game/Placeable/Character/Player/PlayerCharacter_Quartermaster"
MAT = "/Game/Art/Materials/PH_materials"
SPHERE = "/Engine/BasicShapes/Sphere"
CYLINDER = "/Engine/BasicShapes/Cylinder"
CONE = "/Engine/BasicShapes/Cone"
WHITE = "m_PH_emissive_turboWhite"
GREY = "m_PH_monochrome_Grey"
# MorningStar : chaîne du boulet (segments x longueur), rayon du boulet
BALL_SEGMENTS, BALL_SEGMENT_LENGTH, BALL_RADIUS = 6, 20.0, 28.0

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
ys = unreal.YSEditorScriptLibrary
report = []


def log(msg):
    unreal.log(f"[setup_quartermaster_classes] {msg}")
    report.append(msg)


def vec(x, y, z):
    return unreal.Vector(x, y, z)


def rot(pitch=0.0, yaw=0.0, roll=0.0):
    return unreal.Rotator(roll=roll, pitch=pitch, yaw=yaw)


def handles(bp):
    return subsystem.k2_gather_subobject_data_for_blueprint(bp)


def name_of(handle):
    return str(lib.get_variable_name(lib.get_data(handle)))


def find_handle(bp, name):
    return next((h for h in handles(bp) if name_of(h) == name), None)


def templates(bp):
    result = []
    for h in handles(bp):
        obj = lib.get_object(lib.get_data(h))
        if obj and obj not in result:
            result.append(obj)
    return result


def action_of(bp, cls):
    return next((t for t in templates(bp) if type(t) == cls), None)


def add_component(bp, cls, name, parent=None):
    parent = parent or handles(bp)[0]
    handle, fail = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=parent, new_class=cls, blueprint_context=bp))
    if not fail.is_empty():
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    subsystem.rename_subobject(handle, unreal.Text(name))
    return handle, lib.get_object(lib.get_data(handle))


def shape(bp, name, mesh, material, location, rotation, scale, parent=None):
    handle, comp = add_component(bp, unreal.StaticMeshComponent, name, parent)
    comp.set_editor_property("static_mesh", assets.load_asset(mesh))
    comp.set_material(0, assets.load_asset(f"{MAT}/{material}"))
    comp.set_editor_property("relative_location", location)
    comp.set_editor_property("relative_rotation", rotation)
    comp.set_editor_property("relative_scale3d", scale)
    comp.set_collision_profile_name("NoCollision")
    return handle, comp


def coloration_of(bp):
    """Composant « Coloration » de l'arme (celui de la GreatAxe copiée, sinon créé)"""
    return find_handle(bp, "Coloration") or add_component(bp, unreal.SceneComponent, "Coloration")[0]


def tilt_pitch(dx, dz):
    """Pitch qui oriente l'axe Z d'une forme (cylindre, cône) vers (dx, 0, dz)"""
    return -math.degrees(math.atan2(dx, dz))


def remove_head(bp):
    """Retire les formes de la GreatAxe (tout sauf le manche natif Mesh)"""
    root = handles(bp)[0]
    removed = []
    while True:
        target = next((h for h in handles(bp) if isinstance(lib.get_object(lib.get_data(h)), unreal.StaticMeshComponent)
                       and name_of(h) != "Mesh" and name_of(h) not in removed), None)
        if target is None:
            break
        removed.append(name_of(target))
        if subsystem.delete_subobject(root, target, bp) == 0:
            log(f"ATTENTION : {name_of(target)} non supprimé (hérité ?)")
    log(f"{bp.get_name()} : formes de la GreatAxe retirées ({', '.join(removed)})")


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def save(path):
    assets.save_asset(path, only_if_is_dirty=False)


def create_data_asset(name, folder, cls):
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", cls)
    return tools.create_asset(name, folder, cls, factory)


def duplicate_weapon(name, display, setup, setup_defaults):
    path = f"{WEAPON_DIR}/{name}"
    if assets.does_asset_exist(path):
        log(f"existe déjà : {path}")
        return
    tools.duplicate_asset(name, WEAPON_DIR, assets.load_asset(AXE))
    bp = assets.load_asset(path)
    setup(bp)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    weapon = cdo(path)
    weapon.set_editor_property("display_name", display)
    # composants natifs (manche Mesh, Off Hand Socket) : réglés sur le CDO après compilation
    setup_defaults(weapon)
    save(path)
    log(f"créé : {path} (copie de BP_GreatAxe, « {display} »)")


def setup_anchor(bp):
    remove_head(bp)
    # verge = manche (Mesh, gris : anchor_defaults) ; organeau (anneau) en bas, jas (barre transversale) ; diamant + bras + pattes en haut (Coloration)
    shape(bp, "Ring", CYLINDER, GREY, vec(0.0, 0.0, -58.0), rot(roll=90.0), vec(0.26, 0.26, 0.04))
    shape(bp, "Stock", CYLINDER, GREY, vec(0.0, 0.0, -28.0), rot(roll=90.0), vec(0.06, 0.06, 0.7))
    coloration = coloration_of(bp)
    shape(bp, "Crown", SPHERE, WHITE, vec(0.0, 0.0, 135.0), rot(), vec(0.16, 0.16, 0.16), coloration)
    for side, sign in (("Left", -1.0), ("Right", 1.0)):
        dx, dz = 40.0 * sign, -40.0
        shape(bp, f"Arm{side}", CYLINDER, WHITE, vec(dx * 0.5, 0.0, 135.0 + dz * 0.5), rot(pitch=tilt_pitch(dx, dz)), vec(0.08, 0.08, 0.57), coloration)
        shape(bp, f"Fluke{side}", CONE, WHITE, vec(dx, 0.0, 135.0 + dz + 8.0), rot(pitch=tilt_pitch(0.5 * sign, 0.87)), vec(0.22, 0.12, 0.28), coloration)
    hitbox = next(t for t in templates(bp) if isinstance(t, unreal.BoxComponent))
    hitbox.set_editor_property("relative_location", vec(0.0, 0.0, 110.0))
    hitbox.set_editor_property("box_extent", vec(70.0, 50.0, 60.0))

    # Bumping Slash -> MeteorJump (mêmes réglages, saut réglé comme le lancer du Sniper)
    bump = action_of(bp, unreal.YSBumpingSlashAbility)
    _, jump = add_component(bp, unreal.YSMeteorJumpAbility, "MeteorJump")
    ys.copy_editable_properties(bump, jump)
    jump.set_editor_property("display_name", "MeteorJump")
    thrower = action_of(assets.load_asset(SNIPER), unreal.YSBombThrowerAbility)
    for src, dst in (("flight_duration", "jump_duration"), ("apex_height", "jump_apex_height"), ("max_throw_distance", "max_jump_distance"), ("retreat_step", "retreat_step")):
        jump.set_editor_property(dst, thrower.get_editor_property(src))
    old = find_handle(bp, "BumpingSlash")
    if old is None:
        raise RuntimeError("composant BumpingSlash introuvable dans la copie de la GreatAxe")
    subsystem.delete_subobject(handles(bp)[0], old, bp)


def anchor_defaults(weapon):
    weapon.get_editor_property("mesh").set_material(0, assets.load_asset(f"{MAT}/{GREY}"))


def morning_star_defaults(weapon):
    # poignée courte = Mesh (natif) dans la main principale ; l'autre main tient la chaîne plus bas
    grip = weapon.get_editor_property("mesh")
    grip.set_editor_property("relative_location", vec(0.0, 0.0, 0.0))
    grip.set_editor_property("relative_scale3d", vec(0.08, 0.08, 0.3))
    weapon.get_editor_property("off_hand_socket").set_editor_property("relative_location", vec(0.0, 0.0, -40.0))


def setup_morning_star(bp):
    remove_head(bp)
    socket_handle = find_handle(bp, "OffHandSocket")

    # boulet (Coloration) + 6 pointes ; posé à sa place de repos (sous la main), la chaîne le place en jeu
    coloration = coloration_of(bp)
    rest_z = 15.0 - BALL_SEGMENTS * BALL_SEGMENT_LENGTH
    ball_scale = BALL_RADIUS / 50.0
    ball_handle, _ = shape(bp, "Ball", SPHERE, WHITE, vec(0.0, 0.0, rest_z), rot(), vec(ball_scale, ball_scale, ball_scale), coloration)
    spike = vec(0.12 / ball_scale, 0.12 / ball_scale, 0.18 / ball_scale)
    distance = 50.0 + 9.0 / ball_scale
    for name, location, rotation in (("SpikeFront", vec(distance, 0, 0), rot(pitch=-90.0)), ("SpikeBack", vec(-distance, 0, 0), rot(pitch=90.0)),
                                     ("SpikeRight", vec(0, distance, 0), rot(roll=-90.0)), ("SpikeLeft", vec(0, -distance, 0), rot(roll=90.0)),
                                     ("SpikeTop", vec(0, 0, distance), rot()), ("SpikeBottom", vec(0, 0, -distance), rot(pitch=180.0))):
        shape(bp, name, CONE, WHITE, location, rotation, spike, ball_handle)
    hitbox = next(t for t in templates(bp) if isinstance(t, unreal.BoxComponent))
    hitbox.set_editor_property("relative_location", vec(0.0, 0.0, rest_z))
    hitbox.set_editor_property("box_extent", vec(BALL_RADIUS + 10.0, BALL_RADIUS + 10.0, BALL_RADIUS + 10.0))

    def chain(name, location, parent, segments, length, radius, trail, damping, iterations, end_mass=1.0, end_anchor="", attached=(), ground_radius=0.0):
        _, c = add_component(bp, unreal.YSDanglingChainComponent, name, parent)
        c.set_editor_property("relative_location", location)
        c.set_editor_property("segment_count", segments)
        c.set_editor_property("segment_length", length)
        c.set_editor_property("segment_radius", radius)
        c.set_editor_property("segment_material", assets.load_asset(f"{MAT}/{GREY}"))
        c.set_editor_property("trail_acceleration", trail)
        c.set_editor_property("damping", damping)
        c.set_editor_property("solver_iterations", iterations)
        c.set_editor_property("end_mass", end_mass)
        c.set_editor_property("end_anchor_component_name", end_anchor)
        c.set_editor_property("end_attached_component_names", list(attached))
        c.set_editor_property("end_ground_radius", ground_radius)

    # chaîne du boulet (main principale), chaîne tenue entre les deux mains, bout qui pend derrière l'autre main
    chain("BallChain", vec(0.0, 0.0, 15.0), None, BALL_SEGMENTS, BALL_SEGMENT_LENGTH, 2.5, 0.0, 0.01, 8, 1.5, "", ("Ball", "Hitbox"), BALL_RADIUS)
    chain("HandChain", vec(0.0, 0.0, -15.0), None, 4, 10.0, 2.0, 0.0, 0.08, 6, 1.0, "OffHandSocket")
    chain("TailChain", vec(0.0, 0.0, 0.0), socket_handle, 3, 12.0, 2.0, 200.0, 0.08, 4)


def create_weapon_set(name, display, right, scale):
    path = f"{SETS_DIR}/{name}"
    if assets.does_asset_exist(path):
        log(f"existe déjà : {path}")
        return assets.load_asset(path)
    weapon_set = create_data_asset(name, SETS_DIR, unreal.YSWeaponSet)
    # PV / pushback du set de départ du Quartermaster
    ys.copy_editable_properties(assets.load_asset(AXE_SET), weapon_set)
    weapon_set.set_editor_property("display_name", display)
    weapon_set.set_editor_property("right_hand_weapon", assets.load_blueprint_class(right))
    weapon_set.set_editor_property("left_hand_weapon", None)
    weapon_set.set_editor_property("character_scale_multiplier", scale)
    save(path)
    log(f"créé : {path} ({display}, taille x{scale})")
    return weapon_set


try:
    # 1) taille : par set d'arme (le Quartermaster revient à 1)
    qm = cdo(QM)
    if abs(qm.get_editor_property("character_scale") - 1.0) > 0.001:
        log(f"Quartermaster : Character Scale {qm.get_editor_property('character_scale')} -> 1")
        qm.set_editor_property("character_scale", 1.0)
        save(QM)
    axe_set = assets.load_asset(AXE_SET)
    if abs(axe_set.get_editor_property("character_scale_multiplier") - 1.0) < 0.001:
        axe_set.set_editor_property("character_scale_multiplier", 1.25)
        save(AXE_SET)
        log("DA_WeaponSet_GreatAxe : taille x1.25")

    # 2) armes
    duplicate_weapon(ANCHOR_NAME, "Great Anchor", setup_anchor, anchor_defaults)
    duplicate_weapon(STAR_NAME, "MorningStar", setup_morning_star, morning_star_defaults)

    # 3) sets
    anchor_set = create_weapon_set("DA_WeaponSet_AnchorBearer", "Anchor Bearer", ANCHOR, 1.5)
    star_set = create_weapon_set("DA_WeaponSet_BouletEtChaine", "Boulet et Chaîne", STAR, 1.25)

    # 4) BloodRage / Bloodlust : les nouvelles armes du Quartermaster sont comme la GreatAxe
    new_weapons = [assets.load_blueprint_class(ANCHOR), assets.load_blueprint_class(STAR)]
    for path, prop in ((BLOODRAGE, "required_weapon_classes"), (BLOODLUST, "excluded_weapon_classes")):
        upgrade = assets.load_asset(path)
        classes = list(upgrade.get_editor_property(prop))
        added = [w for w in new_weapons if w not in classes]
        if added:
            upgrade.set_editor_property(prop, classes + added)
            save(path)
        log(f"{upgrade.get_name()}.{prop} : " + ", ".join(c.get_name() for c in upgrade.get_editor_property(prop) if c))

    # 5) upgrades de classe, à la fin du pool du Quartermaster
    class_upgrades = [
        ("DA_ClassUpgrade_Quartermaster2AnchorBearer", "Anchor Bearer", anchor_set,
         "Great Anchor : MeteorJump, un saut en cloche\nqui bumpe les NME touchés\nEncore plus grand"),
        ("DA_ClassUpgrade_Quartermaster2BouletEtChaine", "Boulet et Chaîne", star_set,
         "MorningStar : un boulet au bout d'une chaîne,\nla hitbox suit le boulet"),
    ]
    qm = cdo(QM)
    pool = list(qm.get_editor_property("upgrade_pool"))
    for name, display, target, description in class_upgrades:
        path = f"{UPGRADES_DIR}/{name}"
        if not assets.does_asset_exist(path):
            upgrade = create_data_asset(name, UPGRADES_DIR, unreal.YSClassUpgrade)
            upgrade.set_editor_property("display_name", display)
            upgrade.set_editor_property("description", description)
            upgrade.set_editor_property("source_weapon_set", assets.load_asset(AXE_SET))
            upgrade.set_editor_property("target_weapon_set", target)
            save(path)
            log(f"créé : {path} (GreatAxe -> {display})")
        upgrade = assets.load_asset(path)
        if upgrade not in pool:
            pool.append(upgrade)
            log(f"{name} ajoutée au pool du Quartermaster (index {len(pool) - 1})")
    qm.set_editor_property("upgrade_pool", pool)
    save(QM)

    # relecture
    for path in (ANCHOR, STAR):
        for t in templates(assets.load_asset(path)):
            if isinstance(t, unreal.YSAbilityComponent):
                extra = ""
                if isinstance(t, unreal.YSMeteorJumpAbility):
                    extra = (f", saut {t.get_editor_property('jump_duration')} s, sommet {t.get_editor_property('jump_apex_height')}, "
                             f"portée {t.get_editor_property('max_jump_distance')}, bump {t.get_editor_property('bump_distance')}")
                log(f"relu {path.split('/')[-1]}.{t.get_name()} : {t.get_class().get_name()} « {t.get_editor_property('display_name')} », dégâts {t.get_editor_property('damage')}, recharge {t.get_editor_property('cooldown')}{extra}")
            if isinstance(t, unreal.YSDanglingChainComponent):
                log(f"relu {path.split('/')[-1]}.{t.get_name()} : {t.get_editor_property('segment_count')} x {t.get_editor_property('segment_length')}, "
                    f"bout {t.get_editor_property('end_anchor_component_name')} / {list(t.get_editor_property('end_attached_component_names'))}, masse {t.get_editor_property('end_mass')}")
    for path in (AXE_SET, f"{SETS_DIR}/DA_WeaponSet_AnchorBearer", f"{SETS_DIR}/DA_WeaponSet_BouletEtChaine"):
        s = assets.load_asset(path)
        log(f"relu {s.get_name()} : taille x{s.get_editor_property('character_scale_multiplier')}, PV {s.get_editor_property('max_health')}")
    log(f"relu Quartermaster : Character Scale {cdo(QM).get_editor_property('character_scale')}")
    log("relu pool du Quartermaster : " + ", ".join(f"{i} {u.get_name()}" for i, u in enumerate(cdo(QM).get_editor_property("upgrade_pool")) if u))
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_quartermaster_classes.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
