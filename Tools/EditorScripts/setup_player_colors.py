"""
Couleur émissive des joueurs (demande de Paul, 2026-11-10), relançable :
  - crée m_PH_emissive_turboPink (copie de turboPurple) et m_PH_emissive_turboCyan (copie de turboBlue) s'ils n'existent pas,
    en remplaçant leur couleur principale ;
  - DT_CustomizationColors : Emissive Material / Emissive Color de chaque ligne (Rouge et Noir : blanc) ;
  - traînée de slash (réglages du sabre de Mouss, BP_Epee) cochée sur toutes les actions de coupe des armes des joueurs et des minions
    (Auto swing, Dashing / Bumping Slash, Pull, Meteor Jump ; pas le Rocket jump, sans coupe ; jamais les armes de NME).
Lancement (éditeur fermé, chemin ABSOLU du script) :
  UnrealEditor.exe YohohoSurvivor.uproject /Game/Maps/MainMenu -ExecutePythonScript="<projet>/Tools/EditorScripts/setup_player_colors.py"
Rapport : Saved/Logs/setup_player_colors.txt
"""
import os
import unreal

PH = "/Game/Art/Materials/PH_materials"
TABLE = "/Game/Core/Customization/DT_CustomizationColors"
SWORD_BP = "/Game/Placeable/Weapons/Player/Wp_Melee/BP_Epee"
WEAPON_ROOT = "/Game/Placeable/Weapons/Player"
EXCLUDED_CLASSES = ("YSRocketJumpAbility",)
SLASH_PROPS = ("SlashLifetime", "SlashFadeExponent", "SlashSubdivisions", "SlashLengthRatio", "SlashInnerAlpha", "SlashOuterAlpha",
               "bSlashUsePlayerColor", "SlashColor", "SlashWhiteMix", "SlashMaterial", "SlashGlow")

# nouveaux émissifs : (nom, source, couleur principale de la source, nouvelle couleur)
NEW_EMISSIVES = (
    ("m_PH_emissive_turboPink", "m_PH_emissive_turboPurple", (1.0, 0.0, 1.0), (1.0, 0.45, 0.75)),
    ("m_PH_emissive_turboCyan", "m_PH_emissive_turboBlue", (0.0, 0.0, 1.0), (0.0, 1.0, 1.0)),
)
# ligne de la Data Table -> (émissif, couleur émissive des VFX)
ROWS = {
    "Rouge": ("m_PH_emissive_turboWhite", (1.0, 1.0, 1.0)),
    "Bleu": ("m_PH_emissive_turboBlue", (0.0, 0.0, 1.0)),
    "Vert": ("m_PH_emissive_turboGreen", (0.0, 1.0, 0.0)),
    "Violet": ("m_PH_emissive_turboPurple", (1.0, 0.0, 1.0)),
    "Rose": ("m_PH_emissive_turboPink", (1.0, 0.45, 0.75)),
    "Jaune": ("m_PH_emissive_turboYellow", (1.0, 1.0, 0.0)),
    "Cyan": ("m_PH_emissive_turboCyan", (0.0, 1.0, 1.0)),
    "Noir": ("m_PH_emissive_turboWhite", (1.0, 1.0, 1.0)),
}

assets = unreal.EditorAssetLibrary
subsystem = unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
lib = unreal.SubobjectDataBlueprintFunctionLibrary
ys = unreal.YSEditorScriptLibrary
report = []


def log(msg):
    unreal.log(f"[setup_player_colors] {msg}")
    report.append(str(msg))


def color(rgb):
    return unreal.LinearColor(rgb[0], rgb[1], rgb[2], 1.0)


def make_emissives():
    for name, source, old, new in NEW_EMISSIVES:
        path = f"{PH}/{name}"
        if assets.does_asset_exist(path):
            log(f"existe déjà : {name}")
            continue
        material = assets.duplicate_asset(f"{PH}/{source}", path)
        count = ys.replace_material_color_constants(material, color(old), color(new), 0.01)
        assets.save_asset(path, only_if_is_dirty=False)
        log(f"créé : {name} (copie de {source}, {count} nœud(s) {old} -> {new}) ; couleurs {[(round(c.r, 2), round(c.g, 2), round(c.b, 2)) for c in ys.get_material_color_constants(material)]}")


def fill_table():
    table = assets.load_asset(TABLE)
    names = [str(n) for n in unreal.DataTableFunctionLibrary.get_data_table_row_names(table)]
    for row in names:
        if row not in ROWS:
            log(f"ATTENTION : ligne {row} sans émissif prévu (laissée telle quelle)")
            continue
        material_name, rgb = ROWS[row]
        material = assets.load_asset(f"{PH}/{material_name}")
        ok = ys.set_customization_color_emissive(table, row, material, color(rgb))
        log(f"{row} : {material_name}, couleur {rgb} -> {ok}")
    assets.save_asset(TABLE, only_if_is_dirty=False)


def swing_components(bp):
    out = []
    for handle in subsystem.k2_gather_subobject_data_for_blueprint(bp):
        obj = lib.get_object(lib.get_data(handle))
        if isinstance(obj, unreal.YSAutoSwingAbility) and obj.get_class().get_name() not in EXCLUDED_CLASSES:
            out.append(obj)
    return out


def setup_slash():
    reference = swing_components(assets.load_asset(SWORD_BP))[0]
    values = {key: reference.get_editor_property(key) for key in SLASH_PROPS}
    log(f"réglages copiés de BP_Epee.{reference.get_name()} : {values}")
    for path in assets.list_assets(WEAPON_ROOT, recursive=True):
        if str(assets.find_asset_data(path).asset_class_path.asset_name) != "Blueprint":
            continue
        bp = assets.load_asset(path)
        comps = swing_components(bp)
        if not comps:
            continue
        for comp in comps:
            comp.set_editor_property("bSlashTrail", True)
            for key, value in values.items():
                comp.set_editor_property(key, value)
            log(f"{bp.get_name()}.{comp.get_name()} ({comp.get_class().get_name()}) : Slash Trail coché")
        unreal.BlueprintEditorLibrary.compile_blueprint(bp)
        assets.save_asset(path, only_if_is_dirty=False)


try:
    make_emissives()
    fill_table()
    setup_slash()
    log("OK")
except Exception as e:
    import traceback
    log(f"ERREUR : {e!r}\n{traceback.format_exc()}")
finally:
    out = os.path.join(unreal.Paths.project_saved_dir(), "Logs", "setup_player_colors.txt")
    with open(out, "w", encoding="utf-8") as f:
        f.write("\n".join(report))
    unreal.SystemLibrary.quit_editor()
