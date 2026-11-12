"""
Cartes au trésor d'objectif, Super carte au trésor, world boss Davy Jones (2026-11-12). Relançable : ce qui existe déjà n'est pas recréé
(les formes ne sont ajoutées / retirées qu'à la création), les réglages listés ci-dessous sont réappliqués.

  - BP_InteractiveObject_ObjectiveTreasureMap : copie de la carte au trésor reparentée sur YSObjectiveTreasureMap, croix en vert émissif
    (m_PH_emissive_turboGreen), pointillés verts, 2 Basic Chest en récompense, embuscade 20 Skull / 7 Skull Gun / 3 Skull Bomb
  - BP_InteractiveObject_SuperTreasureMap : reparentée sur YSSuperTreasureMap (pointillés dorés vers Davy Jones)
  - Weapons/Foe/BP_NME_DavyJonesSword (copie de BP_NME_EliteGreatSword : même swing que le Boss Skull) et BP_NME_DavyJonesHook (copie du
    crochet du Captain sans son Ability, non répliqué comme les armes de NME) ; set DA_WeaponSet_NME_DavyJones (épée droite, crochet gauche)
  - Character/Foe/Boss/FoeCharacter_DavyJones : copie du Boss Skull, 5000 PV de base, sans Boss Big Slash, Shield Action = tentacule à
    distance du Kraken, Head Feature copiée de celle du Captain, pas de coffre à sa mort (butin de l'arène)
  - Ship/BP_DavyJonesGalion : copie de BP_NME_Galion sans capitaine ni équipage, grand mât (vergues, voiles, nid de pie, drapeau) remplacé par
    une plateforme (DavyPlatform) sur un pilier
  - Character/Foe/DavyJones/BP_DavyJonesArena : équipages des 3 phases, butin (3 Elite Chest, 2 Boss Chest, + 2 membres d'équipage)
  - BP_FlowMod : Objective Treasure Map Class, Davy Jones Arena Class

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_davy_jones.py"
Rapport : Saved/Logs/setup_davy_jones.txt
"""
import os
import unreal

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
registry = unreal.AssetRegistryHelpers.get_asset_registry()
bp_lib = unreal.BlueprintEditorLibrary
report = []

OBJECTS_DIR = "/Game/Placeable/InteractiveObjects"
TREASURE_MAP = f"{OBJECTS_DIR}/BP_InteractiveObject_TreasureMap"
OBJECTIVE_MAP = f"{OBJECTS_DIR}/BP_InteractiveObject_ObjectiveTreasureMap"
SUPER_MAP = f"{OBJECTS_DIR}/BP_InteractiveObject_SuperTreasureMap"
FOE_WEAPONS = "/Game/Placeable/Weapons/Foe"
DAVY_SWORD = f"{FOE_WEAPONS}/BP_NME_DavyJonesSword"
DAVY_HOOK = f"{FOE_WEAPONS}/BP_NME_DavyJonesHook"
WEAPON_SETS = "/Game/Core/WeaponSets"
DAVY_SET = f"{WEAPON_SETS}/DA_WeaponSet_NME_DavyJones"
BOSS_DIR = "/Game/Placeable/Character/Foe/Boss"
DAVY = f"{BOSS_DIR}/FoeCharacter_DavyJones"
SHIP_DIR = "/Game/Placeable/Ship"
DAVY_GALION = f"{SHIP_DIR}/BP_DavyJonesGalion"
ARENA_DIR = "/Game/Placeable/Character/Foe/DavyJones"
ARENA = f"{ARENA_DIR}/BP_DavyJonesArena"
FLOW_GM = "/Game/Core/GameModes/BP_FlowMod"
MAT = "/Game/Art/Materials/PH_materials"
CYLINDER = "/Engine/BasicShapes/Cylinder.Cylinder"

GREEN = unreal.LinearColor(0.1, 1.0, 0.2, 1.0)
# grand mât du Galion : formes retirées (la plateforme le remplace)
MAIN_MAST_PARTS = ["MainMast", "MainMast_Yard0", "MainMast_Yard1", "MainMast_Sail0", "MainMast_Sail1", "MainMast_SailOpen0", "MainMast_SailOpen1",
                   "CrowsNest", "CrowsNestStation", "Flag", "FlagSkull"]


def log(msg):
    unreal.log(f"[setup_davy_jones] {msg}")
    report.append(str(msg))


def find_path(name):
    for data in registry.get_assets_by_path("/Game", recursive=True):
        if str(data.asset_name) == name:
            return str(data.package_name)
    return None


def find_class(name):
    path = find_path(name)
    if not path:
        raise RuntimeError(f"{name} introuvable")
    return assets.load_blueprint_class(path)


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def parent_of(bp):
    """Classe parente d'un Blueprint (classe générée -> sa super classe)."""
    generated = bp.generated_class()
    return unreal.SystemLibrary.get_super_class(generated) if hasattr(unreal.SystemLibrary, "get_super_class") else generated.get_super_class()


def duplicate(src, dst):
    """Copie src en dst si dst n'existe pas ; renvoie (blueprint, créé)."""
    created = not assets.does_asset_exist(dst)
    if created:
        if not assets.duplicate_asset(src, dst):
            raise RuntimeError(f"copie {src} -> {dst} impossible")
        log(f"créé : {dst} (copie de {src})")
    else:
        log(f"existe déjà : {dst}")
    return assets.load_asset(dst), created


def handles_by_name(bp):
    result = {}
    for h in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        data = lib.get_data(h)
        result[str(lib.get_variable_name(data))] = h
    return result


def root_handle(bp):
    handles = subsystem.k2_gather_subobject_data_for_blueprint(bp)
    return handles[0]


def remove_components(bp, names):
    by_name = handles_by_name(bp)
    context = root_handle(bp)
    removed = []
    for name in names:
        h = by_name.get(name)
        if h is None:
            continue
        try:
            count = subsystem.delete_subobject(context, h, bp)
        except Exception as exc:  # noqa
            log(f"  {name} : suppression impossible ({exc})")
            continue
        if count:
            removed.append(name)
        else:
            log(f"  {name} : pas supprimé (hérité ?)")
    log(f"{bp.get_name()} : composants retirés {removed}")


def add_component(bp, parent_name, name, cls):
    by_name = handles_by_name(bp)
    parent = by_name.get(parent_name)
    if parent is None:
        raise RuntimeError(f"{bp.get_name()} : parent {parent_name} introuvable ({list(by_name)})")
    params = unreal.AddNewSubobjectParams(parent_handle=parent, new_class=cls, blueprint_context=bp)
    handle, fail = subsystem.add_new_subobject(params)
    if not lib.is_handle_valid(handle):
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    subsystem.rename_subobject(handle, unreal.Text(name))
    return lib.get_object(lib.get_data(handle))


def component_template(bp, name):
    h = handles_by_name(bp).get(name)
    return lib.get_object(lib.get_data(h)) if h is not None else None


def copy_mesh_component(src, dst):
    dst.set_static_mesh(src.get_editor_property("static_mesh"))
    for index in range(src.get_num_materials()):
        dst.set_material(index, src.get_material(index))
    dst.set_collision_profile_name("NoCollision")
    dst.set_editor_property("cast_shadow", src.get_editor_property("cast_shadow"))
    for prop in ["relative_location", "relative_rotation", "relative_scale3d"]:
        dst.set_editor_property(prop, src.get_editor_property(prop))


try:
    green_emissive = assets.load_asset(f"{MAT}/m_PH_emissive_turboGreen")
    chest = find_class("BP_InteractiveObject_Chest")
    elite_chest = find_class("BP_InteractiveObject_TreasureChest")
    boss_chest = find_class("BP_InteractiveObject_BossChest")
    skull = find_class("FoeCharacter_Skull")
    skull_gun = find_class("FoeCharacter_Skull_Gun")
    skull_bomb = find_class("FoeCharacter_Skull_Bomb")
    skull_elite = find_class("FoeCharacter_Skull_Elite")
    gun_elite = find_class("FoeCharacter_Skull_Gun_Elite")
    tentacle_range = find_class("FoeCharacter_KrakenTentacle_Range")

    def group(classes, count):
        return unreal.YSGalionCrewGroup(classes=classes, count=count)

    # --- 1) carte au trésor d'objectif
    map_bp, created = duplicate(TREASURE_MAP, OBJECTIVE_MAP)
    if not unreal.MathLibrary.class_is_child_of(map_bp.generated_class(), unreal.YSObjectiveTreasureMap):
        bp_lib.reparent_blueprint(map_bp, unreal.YSObjectiveTreasureMap)
        log("ObjectiveTreasureMap reparentée sur YSObjectiveTreasureMap")
    crosses = []
    for name in handles_by_name(map_bp):
        if "cross" in name.lower():
            component_template(map_bp, name).set_material(0, green_emissive)
            crosses.append(name)
    log(f"ObjectiveTreasureMap : croix en vert émissif {crosses}")
    bp_lib.compile_blueprint(map_bp)
    objective_map = cdo(OBJECTIVE_MAP)
    objective_map.set_editor_property("display_name", unreal.Text("Carte au trésor d'objectif"))
    objective_map.set_editor_property("dash_color", GREEN)
    objective_map.set_editor_property("guide_indicator_color", GREEN)
    objective_map.set_editor_property("reward_classes", [chest, chest])
    objective_map.set_editor_property("reward_random_combat_post", False)
    objective_map.set_editor_property("ambush_groups", [group([skull], 20), group([skull_gun], 7), group([skull_bomb], 3)])
    assets.save_asset(OBJECTIVE_MAP, only_if_is_dirty=False)

    # --- 2) super carte au trésor : pointillés dorés vers Davy Jones
    super_bp = assets.load_asset(SUPER_MAP)
    if not unreal.MathLibrary.class_is_child_of(super_bp.generated_class(), unreal.YSSuperTreasureMap):
        bp_lib.reparent_blueprint(super_bp, unreal.YSSuperTreasureMap)
        log("SuperTreasureMap reparentée sur YSSuperTreasureMap")
    bp_lib.compile_blueprint(super_bp)
    assets.save_asset(SUPER_MAP, only_if_is_dirty=False)

    # --- 3) armes de Davy Jones et leur set
    duplicate(f"{FOE_WEAPONS}/BP_NME_EliteGreatSword", DAVY_SWORD)
    assets.save_asset(DAVY_SWORD, only_if_is_dirty=False)
    hook_bp, created = duplicate(find_path("BP_CaptainHook"), DAVY_HOOK)
    if created:
        # Ability du Captain retirée (invocation des minions)
        remove_components(hook_bp, [n for n in handles_by_name(hook_bp) if n not in ("None",) and "summon" in n.lower()])
        bp_lib.compile_blueprint(hook_bp)
    nme_replicates = cdo(f"{FOE_WEAPONS}/BP_NME_Sword").get_editor_property("replicates")
    cdo(DAVY_HOOK).set_editor_property("replicates", nme_replicates)
    log(f"crochet de Davy Jones : répliqué = {nme_replicates} (comme BP_NME_Sword)")
    assets.save_asset(DAVY_HOOK, only_if_is_dirty=False)

    set_asset, _ = duplicate(f"{WEAPON_SETS}/DA_WeaponSet_NME_EliteGreatSword", DAVY_SET)
    set_asset.set_editor_property("display_name", unreal.Text("Davy Jones"))
    set_asset.set_editor_property("right_hand_weapon", assets.load_blueprint_class(DAVY_SWORD))
    set_asset.set_editor_property("left_hand_weapon", assets.load_blueprint_class(DAVY_HOOK))
    assets.save_asset(DAVY_SET, only_if_is_dirty=False)

    # --- 4) Davy Jones : copie du Boss Skull
    davy_bp, created = duplicate(f"{BOSS_DIR}/FoeCharacter_Skull_Boss", DAVY)
    if created:
        # Head Feature du Captain (et ses enfants), même hiérarchie et mêmes positions (le corps du NME est celui du joueur)
        captain_bp = assets.load_asset(find_path("PlayerCharacter_Captain"))
        captain = handles_by_name(captain_bp)
        children = {}
        for name, h in captain.items():
            data = lib.get_data(h)
            parent_data = lib.get_data(lib.get_parent_handle(data)) if hasattr(lib, "get_parent_handle") else None
            parent_name = str(lib.get_variable_name(parent_data)) if parent_data is not None else ""
            children.setdefault(parent_name, []).append(name)
        copied = []

        def copy_tree(name, parent_name):
            src = lib.get_object(lib.get_data(captain[name]))
            if not isinstance(src, unreal.StaticMeshComponent):
                return
            dst = add_component(davy_bp, parent_name, name, unreal.StaticMeshComponent)
            copy_mesh_component(src, dst)
            copied.append(name)
            for child in children.get(name, []):
                copy_tree(child, name)

        davy_root = str(lib.get_variable_name(lib.get_data(root_handle(davy_bp))))
        davy_handles = handles_by_name(davy_bp)
        attach_name = "CapsuleComponent" if "CapsuleComponent" in davy_handles else davy_root
        copy_tree("Head Feature", attach_name)
        # plume (sous BodyPivot chez le Captain, au niveau du chapeau)
        for name in children.get("BodyPivot", []):
            obj = lib.get_object(lib.get_data(captain[name]))
            if name not in copied and isinstance(obj, unreal.StaticMeshComponent) and name not in ("BodyMesh", "RightHandMesh", "LeftHandMesh"):
                copy_tree(name, attach_name)
        log(f"Davy Jones : Head Feature du Captain copiée {copied} (sous {attach_name})")
        bp_lib.compile_blueprint(davy_bp)
    davy = cdo(DAVY)
    davy.set_editor_property("foe_name", unreal.Text("Davy Jones"))
    davy.set_editor_property("weapon_set", assets.load_asset(DAVY_SET))
    # 5000 PV de base (niveau 0, 1 joueur) : 125 x Health Multiplier 40, même progression par niveau que le Boss Skull
    health_multiplier = davy.get_editor_property("health_multiplier")
    base = 5000.0 / max(health_multiplier, 0.001)
    davy.set_editor_property("base_health", base)
    davy.set_editor_property("health_per_level", base)
    davy.set_editor_property("boss_big_slash", False)
    davy.set_editor_property("shield_summon_class", tentacle_range)
    davy.set_editor_property("death_loot_class", None)
    davy.set_editor_property("numpad_spawn_key", -1)
    davy.set_editor_property("left_handed_chance", 0.0)
    assets.save_asset(DAVY, only_if_is_dirty=False)
    log(f"Davy Jones : PV de base {base:.1f} x {health_multiplier} = {base * health_multiplier:.0f}")

    # --- 5) Galion de Davy Jones
    galion_bp, created = duplicate(f"{SHIP_DIR}/BP_NME_Galion", DAVY_GALION)
    if created:
        mast = component_template(galion_bp, "MainMast")
        mast_location = mast.get_editor_property("relative_location") if mast else unreal.Vector(200, 0, 1060)
        deck = component_template(galion_bp, "Deck")
        remove_components(galion_bp, MAIN_MAST_PARTS)
        deck_name = "Deck" if deck else str(lib.get_variable_name(lib.get_data(root_handle(galion_bp))))
        parent_name = "Root" if "Root" in handles_by_name(galion_bp) else deck_name
        platform_z = 1000.0
        for name, location, scale, material in [
            ("DavyPlatformPillar", unreal.Vector(mast_location.x, 0, (210.0 + platform_z) * 0.5), unreal.Vector(1.0, 1.0, (platform_z - 210.0) / 100.0), "m_PH_monochrome_LightBlack"),
            ("DavyPlatform", unreal.Vector(mast_location.x, 0, platform_z), unreal.Vector(5.0, 5.0, 0.3), "m_PH_monochrome_Black"),
        ]:
            shape = add_component(galion_bp, parent_name, name, unreal.StaticMeshComponent)
            shape.set_static_mesh(assets.load_asset(CYLINDER))
            shape.set_material(0, assets.load_asset(f"{MAT}/{material}"))
            shape.set_editor_property("relative_location", location)
            shape.set_editor_property("relative_scale3d", scale)
            if deck:
                shape.set_collision_profile_name(deck.get_collision_profile_name())
                shape.set_editor_property("mobility", deck.get_editor_property("mobility"))
            else:
                shape.set_collision_profile_name("BlockAll")
        log(f"Galion de Davy Jones : plateforme à Z {platform_z} (X {mast_location.x})")
        bp_lib.compile_blueprint(galion_bp)
    galion = cdo(DAVY_GALION)
    galion.set_editor_property("ship_name", unreal.Text("Galion de Davy Jones"))
    galion.set_editor_property("captain_class", None)
    galion.set_editor_property("captain_classes", [])
    galion.set_editor_property("extra_captain_classes", [])
    galion.set_editor_property("crew_groups", [])
    galion.set_editor_property("boarding_crew_groups", [])
    galion.set_editor_property("platform_component_name", "DavyPlatform")
    # nid de pie (composant C++ du bateau, pas supprimable) : sans poste, comme sur la chaloupe
    nest = galion.get_editor_property("CrowsNestStation")
    if nest:
        nest.set_editor_property("child_actor_class", None)
        log("Galion de Davy Jones : CrowsNestStation sans poste")
    # pas de poste de combat (2026-11-12, demande de Paul) : canons, réceptacles et ShootPosts sans acteur
    galion_bp = assets.load_asset(DAVY_GALION)
    emptied = []
    for name in sorted(handles_by_name(galion_bp)):
        if not (name.startswith("Cannon") or name.startswith("ShootPost")):
            continue
        template = None
        try:
            template = galion.get_editor_property(name)
        except Exception:  # noqa
            template = component_template(galion_bp, name)
        if isinstance(template, unreal.ChildActorComponent):
            template.set_editor_property("child_actor_class", None)
            emptied.append(name)
    log(f"Galion de Davy Jones : postes de combat retirés {emptied}")
    assets.save_asset(DAVY_GALION, only_if_is_dirty=False)

    # --- 6) arène
    if not assets.does_asset_exist(ARENA):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", unreal.YSDavyJonesArena)
        tools.create_asset("BP_DavyJonesArena", ARENA_DIR, unreal.Blueprint, factory)
        log(f"créé : {ARENA}")
    bp_lib.compile_blueprint(assets.load_asset(ARENA))
    arena = cdo(ARENA)
    arena.set_editor_property("davy_jones_class", assets.load_blueprint_class(DAVY))
    arena.set_editor_property("galion_class", assets.load_blueprint_class(DAVY_GALION))
    arena.set_editor_property("crew_waves", [
        unreal.YSDavyJonesCrewWave(groups=[group([skull], 25), group([skull_gun], 5), group([skull_bomb], 3)]),
        unreal.YSDavyJonesCrewWave(groups=[group([skull], 35), group([skull_gun], 10), group([skull_bomb], 5)]),
        unreal.YSDavyJonesCrewWave(groups=[group([skull_elite, gun_elite], 2), group([skull_gun], 20)]),
    ])
    arena.set_editor_property("loot_classes", [elite_chest, elite_chest, elite_chest, boss_chest, boss_chest])
    arena.set_editor_property("random_crew_loot_count", 2)
    assets.save_asset(ARENA, only_if_is_dirty=False)

    # --- 7) FlowMod
    flow = cdo(FLOW_GM)
    flow.set_editor_property("objective_treasure_map_class", assets.load_blueprint_class(OBJECTIVE_MAP))
    flow.set_editor_property("davy_jones_arena_class", assets.load_blueprint_class(ARENA))
    assets.save_asset(FLOW_GM, only_if_is_dirty=False)

    # --- relecture
    check = cdo(DAVY)
    log(f"relu Davy Jones : set {check.get_editor_property('weapon_set').get_name()}, Big Slash {check.get_editor_property('boss_big_slash')}, "
        f"Shield Action {check.get_editor_property('shield_summon_class').get_name()}, PV de base {check.get_editor_property('base_health')}, "
        f"bouclier {check.get_editor_property('boss_shield')}")
    check_set = assets.load_asset(DAVY_SET)
    log(f"relu set : droite {check_set.get_editor_property('right_hand_weapon').get_name()}, gauche {check_set.get_editor_property('left_hand_weapon').get_name()}")
    log(f"relu Galion : composants {sorted(handles_by_name(assets.load_asset(DAVY_GALION)))}")
    log(f"relu crochet : composants {sorted(handles_by_name(assets.load_asset(DAVY_HOOK)))}")
    log(f"relu Davy Jones : composants {sorted(handles_by_name(assets.load_asset(DAVY)))}")
    check_arena = cdo(ARENA)
    log(f"relu arène : {len(check_arena.get_editor_property('crew_waves'))} équipages, butin {[c.get_name() for c in check_arena.get_editor_property('loot_classes')]}")
    check_flow = cdo(FLOW_GM)
    log(f"relu FlowMod : carte d'objectif {check_flow.get_editor_property('objective_treasure_map_class').get_name()}, arène {check_flow.get_editor_property('davy_jones_arena_class').get_name()}")
    check_map = cdo(OBJECTIVE_MAP)
    log(f"relu carte d'objectif : YSObjectiveTreasureMap {unreal.MathLibrary.class_is_child_of(assets.load_blueprint_class(OBJECTIVE_MAP), unreal.YSObjectiveTreasureMap)}, pointillés {check_map.get_editor_property('dash_color')}, "
        f"récompenses {[c.get_name() for c in check_map.get_editor_property('reward_classes')]}, embuscade {[g.get_editor_property('count') for g in check_map.get_editor_property('ambush_groups')]}")
    log(f"relu super carte : YSSuperTreasureMap {unreal.MathLibrary.class_is_child_of(assets.load_blueprint_class(SUPER_MAP), unreal.YSSuperTreasureMap)}")
    log("OK")
except Exception as exc:  # noqa
    import traceback
    log(f"ERREUR : {exc}\n{traceback.format_exc()}")

out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_davy_jones.txt")
with open(out, "w", encoding="utf-8") as f:
    f.write("\n".join(report))
unreal.SystemLibrary.quit_editor()
