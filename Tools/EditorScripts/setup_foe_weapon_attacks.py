"""
Attaques à distance des NME définies par leur ARME (et plus par le personnage) :
  - /Game/Placeable/Weapons/Foe/BP_NME_Bomb (« NME bomb », enfant de BP_Arme, non répliquée) : bombe en primitives dans la main
    (sphère noire, mèche, bout rouge émissif ; Muzzle au centre = départ du lancer) + composant « BombThrow »
    (UYSFoeBombAction : portée, recharge, lancer en cloche, mèche, prévisualisation au sol, explosion)
  - /Game/Core/WeaponSets/DA_WeaponSet_NME_Bomb (main droite) -> FoeCharacter_Skull_Bomb (50 % gaucher)
  - BP_NME_Pistol : composant « Shot » (UYSFoeShotAction : portée, recharge, visée + ligne au sol, balle NME_GunAMMO)
Réglages repris de Skull Bomb / Skull Gun (anciennes propriétés Foe|Ranged du personnage, sauvegardées avant leur suppression
dans Saved/foe_ranged_values.json ; sans ce fichier : valeurs par défaut du C++). Ne recrée pas ce qui existe.

Lancement (éditeur fermé) :
  UnrealEditor.exe YohohoSurvivor.uproject -ExecutePythonScript="Tools/EditorScripts/setup_foe_weapon_attacks.py"
Rapport : Saved/Logs/setup_foe_weapon_attacks.txt
"""
import json
import os
import unreal

FOE_WEAPONS = "/Game/Placeable/Weapons/Foe"
BASE_WEAPON = "/Game/Placeable/Weapons/BP_Arme"
NME_BOMB_NAME = "BP_NME_Bomb"
NME_BOMB = f"{FOE_WEAPONS}/{NME_BOMB_NAME}"
NME_PISTOL = f"{FOE_WEAPONS}/BP_NME_Pistol"
PLAYER_PISTOL = "/Game/Placeable/Weapons/BP_Pistolet"
RECOIL_PROPS = ["recoil_pitch", "recoil_kick_back", "recoil_kick_duration", "recoil_recover_duration"]
SETS_DIR = "/Game/Core/WeaponSets"
BOMB_SET_NAME = "DA_WeaponSet_NME_Bomb"
BOMB_SET = f"{SETS_DIR}/{BOMB_SET_NAME}"
SKULL_BOMB = "/Game/Placeable/Character/FoeCharacter_Skull_Bomb"
MAT = "/Game/Art/Materials/PH_materials"
SPHERE = "/Engine/BasicShapes/Sphere"
CYLINDER = "/Engine/BasicShapes/Cylinder"
LEFT_HANDED_CHANCE = 0.5
VALUES = os.path.join(unreal.Paths.project_saved_dir(), "foe_ranged_values.json")

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_foe_weapon_attacks] {msg}")
    report.append(msg)


def components(bp):
    return [lib.get_object(lib.get_data(h)) for h in subsystem.k2_gather_subobject_data_for_blueprint(bp) if lib.get_object(lib.get_data(h))]


def add_component(bp, cls, name):
    root = subsystem.k2_gather_subobject_data_for_blueprint(bp)[0]
    handle, fail = subsystem.add_new_subobject(unreal.AddNewSubobjectParams(parent_handle=root, new_class=cls, blueprint_context=bp))
    if not fail.is_empty():
        raise RuntimeError(f"ajout de {name} impossible : {fail}")
    subsystem.rename_subobject(handle, unreal.Text(name))
    return lib.get_object(lib.get_data(handle))


def value(raw):
    """Valeur du JSON -> valeur Unreal (couleurs, assets, classes)"""
    if isinstance(raw, dict):
        if "color" in raw:
            return unreal.LinearColor(*raw["color"])
        if "path" in raw:
            path = raw["path"]
            if path.startswith("/Script/"):
                return unreal.load_class(None, path)
            if path.endswith("_C"):
                return unreal.load_class(None, path)
            return unreal.load_asset(path)
    return raw


def copy(saved, component, mapping):
    for old, new in mapping.items():
        if old in saved and saved[old] not in (None, ""):
            component.set_editor_property(new, value(saved[old]))


try:
    saved = json.load(open(VALUES)) if os.path.exists(VALUES) else {}
    log(f"réglages sauvegardés : {'trouvés' if saved else 'ABSENTS (valeurs par défaut)'}")
    bomb_saved = saved.get("FoeCharacter_Skull_Bomb", {})
    gun_saved = saved.get("FoeCharacter_Skull_Gun", {})

    # 1) NME bomb : arme + action de lancer
    if not assets.does_asset_exist(NME_BOMB):
        factory = unreal.BlueprintFactory()
        factory.set_editor_property("parent_class", assets.load_blueprint_class(BASE_WEAPON))
        bp = tools.create_asset(NME_BOMB_NAME, FOE_WEAPONS, unreal.Blueprint, factory)
        fuse = add_component(bp, unreal.StaticMeshComponent, "Fuse")
        fuse.set_editor_property("static_mesh", assets.load_asset(CYLINDER))
        fuse.set_material(0, assets.load_asset(f"{MAT}/m_PH_monochrome_Brown"))
        fuse.set_editor_property("relative_location", unreal.Vector(0, 0, 20))
        fuse.set_editor_property("relative_scale3d", unreal.Vector(0.05, 0.05, 0.15))
        fuse.set_collision_profile_name("NoCollision")
        tip = add_component(bp, unreal.StaticMeshComponent, "FuseTip")
        tip.set_editor_property("static_mesh", assets.load_asset(SPHERE))
        tip.set_material(0, assets.load_asset(f"{MAT}/m_PH_emissive_turboRed"))
        tip.set_editor_property("relative_location", unreal.Vector(0, 0, 28))
        tip.set_editor_property("relative_scale3d", unreal.Vector(0.06, 0.06, 0.06))
        tip.set_collision_profile_name("NoCollision")
        action = add_component(bp, unreal.YSFoeBombAction, "BombThrow")
        copy(bomb_saved, action, {
            "attack_range": "attack_range", "throw_interval": "attack_interval", "base_bomb_damage": "base_damage",
            "bomb_damage_per_level": "damage_per_level", "bomb_apex_height": "bomb_apex_height", "bomb_flight_duration": "bomb_flight_duration",
            "bomb_fuse_duration": "bomb_fuse_duration", "bomb_color": "bomb_color", "fuse_color": "fuse_color", "fuse_glow": "fuse_glow",
            "bomb_material": "bomb_material", "explosion_radius": "explosion_radius", "explosion_grow_duration": "explosion_grow_duration",
            "explosion_fade_duration": "explosion_fade_duration", "explosion_color": "explosion_color", "landing_zone_color": "preview_color",
            "landing_zone_fill_opacity": "preview_fill_opacity", "landing_zone_edge_opacity": "preview_edge_opacity",
            "landing_zone_material": "preview_material", "explosion_material": "explosion_material", "bomb_class": "bomb_class"})
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        weapon = unreal.get_default_object(assets.load_blueprint_class(NME_BOMB))
        weapon.set_editor_property("replicates", False)
        weapon.set_editor_property("display_name", "NME bomb")
        mesh = weapon.get_editor_property("mesh")
        mesh.set_editor_property("static_mesh", assets.load_asset(SPHERE))
        mesh.set_material(0, assets.load_asset(f"{MAT}/m_PH_monochrome_Black"))
        mesh.set_editor_property("relative_scale3d", unreal.Vector(0.35, 0.35, 0.35))
        mesh.set_collision_profile_name("NoCollision")
        assets.save_asset(NME_BOMB, only_if_is_dirty=False)
        log(f"créé : {NME_BOMB}")
    else:
        log(f"existe déjà : {NME_BOMB}")

    # 2) set d'arme NME bomb -> Skull Bomb
    if not assets.does_asset_exist(BOMB_SET):
        factory = unreal.DataAssetFactory()
        factory.set_editor_property("data_asset_class", unreal.YSWeaponSet)
        weapon_set = tools.create_asset(BOMB_SET_NAME, SETS_DIR, unreal.YSWeaponSet, factory)
        weapon_set.set_editor_property("display_name", "NME bomb")
        weapon_set.set_editor_property("right_hand_weapon", assets.load_blueprint_class(NME_BOMB))
        assets.save_asset(BOMB_SET, only_if_is_dirty=False)
        log(f"créé : {BOMB_SET}")
    bomb_foe = unreal.get_default_object(assets.load_blueprint_class(SKULL_BOMB))
    if bomb_foe.get_editor_property("weapon_set") is None:
        bomb_foe.set_editor_property("weapon_set", assets.load_asset(BOMB_SET))
        bomb_foe.set_editor_property("left_handed_chance", LEFT_HANDED_CHANCE)
        assets.save_asset(SKULL_BOMB, only_if_is_dirty=False)
        log("FoeCharacter_Skull_Bomb : set NME bomb, 50 % gaucher")

    # 3) NME pistol : action de tir
    pistol = assets.load_asset(NME_PISTOL)
    if not any(isinstance(c, unreal.YSFoeShotAction) for c in components(pistol)):
        action = add_component(pistol, unreal.YSFoeShotAction, "Shot")
        copy(gun_saved, action, {
            "attack_range": "attack_range", "throw_interval": "attack_interval", "base_shot_damage": "base_damage",
            "shot_damage_per_level": "damage_per_level", "shot_aim_duration": "aim_duration", "shot_projectile_class": "projectile_class",
            "aim_line_length": "aim_line_length", "aim_line_width": "aim_line_width", "aim_line_opacity": "aim_line_opacity",
            "aim_line_color": "preview_color", "landing_zone_material": "preview_material"})
        unreal.BlueprintEditorLibrary.compile_blueprint(pistol)
        assets.save_asset(NME_PISTOL, only_if_is_dirty=False)
        log("BP_NME_Pistol : action de tir ajoutée")
    else:
        log("BP_NME_Pistol : action de tir déjà là")

    # 4) recul du NME pistol = celui du pistolet du joueur (Recoil de son Rolling shot), tant qu'il est aux valeurs par défaut
    player_shot = next(c for c in components(assets.load_asset(PLAYER_PISTOL)) if isinstance(c, unreal.YSCoupDPetoirAbility))
    pistol = assets.load_asset(NME_PISTOL)
    shot = next(c for c in components(pistol) if isinstance(c, unreal.YSFoeShotAction))
    defaults = unreal.get_default_object(unreal.YSFoeShotAction)
    if all(shot.get_editor_property(p) == defaults.get_editor_property(p) for p in RECOIL_PROPS):
        for prop in RECOIL_PROPS:
            shot.set_editor_property(prop, player_shot.get_editor_property(prop))
        unreal.BlueprintEditorLibrary.compile_blueprint(pistol)
        assets.save_asset(NME_PISTOL, only_if_is_dirty=False)
        log(f"BP_NME_Pistol : recul copié de {player_shot.get_name()} : " + ", ".join(f"{p} {player_shot.get_editor_property(p)}" for p in RECOIL_PROPS))
    else:
        log("BP_NME_Pistol : recul déjà réglé, inchangé")

    # relecture
    for path in (NME_BOMB, NME_PISTOL):
        for c in components(assets.load_asset(path)):
            if isinstance(c, unreal.YSFoeRangedAction):
                props = ["attack_range", "attack_interval", "base_damage", "damage_per_level", "preview_material"]
                props += ["explosion_radius", "bomb_class", "bomb_material"] if isinstance(c, unreal.YSFoeBombAction) else ["aim_duration", "projectile_class", "aim_line_length"]
                log(f"relu : {path.split('/')[-1]}.{c.get_name()} " + ", ".join(f"{p} {c.get_editor_property(p).get_name() if isinstance(c.get_editor_property(p), unreal.Object) else c.get_editor_property(p)}" for p in props))
    bomb_foe = unreal.get_default_object(assets.load_blueprint_class(SKULL_BOMB))
    log(f"relu : Skull Bomb set {bomb_foe.get_editor_property('weapon_set').get_name()} gaucher {bomb_foe.get_editor_property('left_handed_chance')}")
    log("OK")
except Exception as e:
    log(f"ERREUR : {e!r}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_foe_weapon_attacks.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
