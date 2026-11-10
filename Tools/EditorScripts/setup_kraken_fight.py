"""
Boss fight final contre le Kraken (partie 3 du flow, demande de Paul du 2026-11-10) :
  - /Game/Placeable/Character/Foe/Kraken : FoeCharacter_KrakenHead (AYSKrakenHead, 50 000 PV), FoeCharacter_KrakenTentacle_Range
    (AYSKrakenTentacleRange, 8 000), FoeCharacter_KrakenTentacle_Melee (AYSKrakenTentacleMelee, 8 000, NME qui en sortent = FoeCharacter_Skull),
    FoeCharacter_KrakenHead_Final (AYSKrakenHeadFinal, 65 000) ; visuels = props BP_Prop_KrakenHead / BP_Prop_KrakenTentacle ;
    barre de vie des tentacules = celle des NME (widget de FoeCharacter_Skull)
  - BP_KrakenArena (AYSKrakenArena) : les 4 NME, pointillés = ceux de la carte au trésor x3
  - BP_FlowMod : Kraken Arena Class
  - WBP_FlowHUD : « Trouvez la créature » (toutes les boussoles), « Affrontez le Kraken » (partie 3)
  - FlowMap : le kraken de prévisualisation (acteurs tagués KrakenPreview) est retiré : l'arène pose le vrai au lancement
Relançable : les Blueprints qui existent ne sont pas recréés ; les références sont remises.

Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/setup_kraken_fight.py"
Rapport : Saved/Logs/setup_kraken_fight.txt
"""
import os
import unreal

KRAKEN_DIR = "/Game/Placeable/Character/Foe/Kraken"
HEAD = f"{KRAKEN_DIR}/FoeCharacter_KrakenHead"
RANGE = f"{KRAKEN_DIR}/FoeCharacter_KrakenTentacle_Range"
MELEE = f"{KRAKEN_DIR}/FoeCharacter_KrakenTentacle_Melee"
FINAL = f"{KRAKEN_DIR}/FoeCharacter_KrakenHead_Final"
ARENA = f"{KRAKEN_DIR}/BP_KrakenArena"
PROP_HEAD = "/Game/Art/Props/Kraken/BP_Prop_KrakenHead"
PROP_TENTACLE = "/Game/Art/Props/Kraken/BP_Prop_KrakenTentacle"
SKULL = "/Game/Placeable/Character/Foe/Base/FoeCharacter_Skull"
TREASURE_MAP = "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_TreasureMap"
FLOW_MOD = "/Game/Core/GameModes/BP_FlowMod"
FLOW_HUD = "/Game/Core/UI/WBP_FlowHUD"
FLOW_MAP = "/Game/Maps/FlowMap"
PREVIEW_TAG = "KrakenPreview"
FINAL_VISUAL_SCALE = 1.15  # tête finale un peu plus grosse (choix de Claude)

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
actors = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
levels = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
report = []


def log(msg):
    unreal.log(f"[setup_kraken_fight] {msg}")
    report.append(str(msg))


def cdo(path):
    return unreal.get_default_object(assets.load_blueprint_class(path))


def compile_save(path):
    unreal.BlueprintEditorLibrary.compile_blueprint(assets.load_asset(path))
    assets.save_asset(path, only_if_is_dirty=False)


def make_bp(path, parent_class):
    if assets.does_asset_exist(path):
        return False
    directory, name = path.rsplit("/", 1)
    factory = unreal.BlueprintFactory()
    factory.set_editor_property("parent_class", parent_class)
    bp = tools.create_asset(name, directory, unreal.Blueprint, factory)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    return True


try:
    skull_bar = cdo(SKULL).get_editor_property("HealthBar")
    for path, parent, prop in ((HEAD, unreal.YSKrakenHead, PROP_HEAD), (RANGE, unreal.YSKrakenTentacleRange, PROP_TENTACLE),
                               (MELEE, unreal.YSKrakenTentacleMelee, PROP_TENTACLE), (FINAL, unreal.YSKrakenHeadFinal, PROP_HEAD)):
        created = make_bp(path, parent)
        default = cdo(path)
        default.get_editor_property("Visual").set_editor_property("child_actor_class", assets.load_blueprint_class(prop))
        bar = default.get_editor_property("HealthBar")
        for key in ("widget_class", "draw_size", "space"):
            bar.set_editor_property(key, skull_bar.get_editor_property(key))
        if path == MELEE:
            default.set_editor_property("GrabSpawnClass", assets.load_blueprint_class(SKULL))
        if path == FINAL and created:
            default.get_editor_property("VisualPivot").set_editor_property("relative_scale3d", unreal.Vector(FINAL_VISUAL_SCALE, FINAL_VISUAL_SCALE, FINAL_VISUAL_SCALE))
        compile_save(path)
        default = cdo(path)
        log(f"{path.rsplit('/', 1)[1]} : {'créé' if created else 'existe déjà'}, visuel {default.get_editor_property('Visual').get_editor_property('child_actor_class').get_name()}, "
            f"{default.get_editor_property('BaseHealth'):.0f} PV, barre {default.get_editor_property('HealthBar').get_editor_property('widget_class')}")

    created = make_bp(ARENA, unreal.YSKrakenArena)
    arena = cdo(ARENA)
    arena.set_editor_property("HeadClass", assets.load_blueprint_class(HEAD))
    arena.set_editor_property("TentacleRangeClass", assets.load_blueprint_class(RANGE))
    arena.set_editor_property("TentacleMeleeClass", assets.load_blueprint_class(MELEE))
    arena.set_editor_property("HeadFinalClass", assets.load_blueprint_class(FINAL))
    treasure = cdo(TREASURE_MAP)
    for key in ("DashSpacing", "DashLength", "DashWidth"):
        arena.set_editor_property(key, treasure.get_editor_property(key) * 3.0)
    for key in ("DashMesh", "DashMaterial", "DashColor", "DashColorParameterName"):
        arena.set_editor_property(key, treasure.get_editor_property(key))
    compile_save(ARENA)
    arena = cdo(ARENA)
    log(f"BP_KrakenArena : {'créé' if created else 'existe déjà'}, rayon {arena.get_editor_property('Radius')}, pointillés {arena.get_editor_property('DashSpacing')} / "
        f"{arena.get_editor_property('DashLength')} / {arena.get_editor_property('DashWidth')}, {arena.get_editor_property('DashMaterial').get_name() if arena.get_editor_property('DashMaterial') else '-'}")

    flow = cdo(FLOW_MOD)
    flow.set_editor_property("KrakenArenaClass", assets.load_blueprint_class(ARENA))
    compile_save(FLOW_MOD)
    log(f"BP_FlowMod : Kraken Arena Class {cdo(FLOW_MOD).get_editor_property('KrakenArenaClass').get_name()}, distance {cdo(FLOW_MOD).get_editor_property('KrakenArenaDistance')}")

    hud = cdo(FLOW_HUD)
    hud.set_editor_property("CompassDoneText", unreal.Text("Trouvez la créature"))
    hud.set_editor_property("KrakenObjectiveText", unreal.Text("Affrontez le Kraken"))
    compile_save(FLOW_HUD)
    log(f"WBP_FlowHUD : « {cdo(FLOW_HUD).get_editor_property('CompassDoneText')} » / « {cdo(FLOW_HUD).get_editor_property('KrakenObjectiveText')} »")

    if not levels.load_level(FLOW_MAP):
        raise RuntimeError("FlowMap introuvable")
    removed = 0
    for actor in actors.get_all_level_actors():
        if actor.actor_has_tag(PREVIEW_TAG):
            actors.destroy_actor(actor)
            removed += 1
    if removed and not levels.save_current_level():
        raise RuntimeError("sauvegarde de la FlowMap impossible")
    log(f"FlowMap : {removed} acteur(s) du kraken de prévisualisation retiré(s)")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    with open(os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_kraken_fight.txt"), "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
