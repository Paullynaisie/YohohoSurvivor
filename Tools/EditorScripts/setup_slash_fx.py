"""
TEST de VFX de slash (demande de Paul, 2026-11-10) : traînée de l'Auto swing du sabre de Mouss.
  - /Game/Art/Materials/VFX/M_SlashFX : translucide, non éclairé, deux faces ; Emissive = couleur des sommets x Glow (paramètre), Opacity = alpha des sommets
  - BP_Epee (sabre de Mouss ; BP_BattleSword en hérite) : composant Auto swing -> Slash Trail coché, Slash Material = M_SlashFX,
    réglages du VFX remis aux défauts C++, Show Hitbox décoché (la boîte rouge de debug cachait le VFX ; à recocher dans Auto swing|Debug si besoin)
Relançable : le matériau n'est pas recréé s'il existe.

Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/setup_slash_fx.py"
Rapport : Saved/Logs/setup_slash_fx.txt
"""
import os
import unreal

MATERIAL_DIR = "/Game/Art/Materials/VFX"
MATERIAL = f"{MATERIAL_DIR}/M_SlashFX"
OLD_ADDITIVE = f"{MATERIAL_DIR}/M_SlashTrail"  # 1er essai additif (couleur délavée sur le sable), supprimé
SWORD_BP = "/Game/Placeable/Weapons/Player/Wp_Melee/BP_Epee"
BATTLE_BP = "/Game/Placeable/Weapons/Player/Wp_Melee/BP_BattleSword"

assets = unreal.EditorAssetLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mel = unreal.MaterialEditingLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
report = []


def log(msg):
    unreal.log(f"[setup_slash_fx] {msg}")
    report.append(str(msg))


def make_material():
    if assets.does_asset_exist(MATERIAL):
        log(f"existe déjà : {MATERIAL}")
        return assets.load_asset(MATERIAL)
    material = tools.create_asset("M_SlashFX", MATERIAL_DIR, unreal.Material, unreal.MaterialFactoryNew())
    material.set_editor_property("blend_mode", unreal.BlendMode.BLEND_TRANSLUCENT)
    material.set_editor_property("shading_model", unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property("two_sided", True)
    vertex = mel.create_material_expression(material, unreal.MaterialExpressionVertexColor, -700, 0)
    glow = mel.create_material_expression(material, unreal.MaterialExpressionScalarParameter, -700, 250)
    glow.set_editor_property("parameter_name", "Glow")
    glow.set_editor_property("default_value", 1.5)
    by_glow = mel.create_material_expression(material, unreal.MaterialExpressionMultiply, -350, 0)
    mel.connect_material_expressions(vertex, "", by_glow, "A")
    mel.connect_material_expressions(glow, "", by_glow, "B")
    mel.connect_material_property(by_glow, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    mel.connect_material_property(vertex, "A", unreal.MaterialProperty.MP_OPACITY)
    mel.recompile_material(material)
    assets.save_asset(MATERIAL, only_if_is_dirty=False)
    log(f"créé : {MATERIAL}")
    return material
def swing_components(bp):
    out = []
    names = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if obj:
            names.append(f"{obj.get_name()} ({obj.get_class().get_name()})")
        if isinstance(obj, unreal.YSAutoSwingAbility):
            out.append(obj)
    if not out:
        log(f"composants vus : {names}")
    return out


try:
    material = make_material()
    bp = assets.load_asset(SWORD_BP)
    comps = swing_components(bp)
    if not comps:
        raise RuntimeError(f"aucun Auto swing dans {SWORD_BP}")
    for comp in comps:
        comp.set_editor_property("bSlashTrail", True)
        comp.set_editor_property("SlashMaterial", material)
        comp.set_editor_property("bShowHitbox", False)
        for key, value in (("SlashLengthRatio", 0.6), ("SlashOuterAlpha", 0.85), ("SlashWhiteMix", 0.1), ("SlashGlow", 1.5)):
            comp.set_editor_property(key, value)
        log(f"BP_Epee.{comp.get_name()} : Slash Trail {comp.get_editor_property('bSlashTrail')}, matériau {comp.get_editor_property('SlashMaterial').get_name()}, "
            f"Show Hitbox {comp.get_editor_property('bShowHitbox')}, durée {comp.get_editor_property('SlashLifetime')} s")
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assets.save_asset(SWORD_BP, only_if_is_dirty=False)
    if assets.does_asset_exist(BATTLE_BP):
        unreal.BlueprintEditorLibrary.compile_blueprint(assets.load_asset(BATTLE_BP))
        assets.save_asset(BATTLE_BP, only_if_is_dirty=False)
        log("BP_BattleSword recompilé (hérite du réglage)")
    if assets.does_asset_exist(OLD_ADDITIVE):
        log(f"suppression de {OLD_ADDITIVE} : {assets.delete_asset(OLD_ADDITIVE)}")
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_slash_fx.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
