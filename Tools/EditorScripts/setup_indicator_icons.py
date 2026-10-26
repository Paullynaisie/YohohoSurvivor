"""Icônes des bulles d'indication : Brooker (t_i_BrookerIcon) et coffre (t_i_ChestIcon, hérité par le coffre au trésor).

Lancer : UnrealEditor.exe <uproject> -ExecutePythonScript="Tools/EditorScripts/setup_indicator_icons.py"
"""
import unreal

ICONS = {
    "/Game/Placeable/Character/NPC/BP_NPC_Brooker": "/Game/Art/Textures/Icones/t_i_BrookerIcon",
    "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_Chest": "/Game/Art/Textures/Icones/t_i_ChestIcon",
}
# enfants à vérifier : s'ils ont une icône à eux (override), on la remplace aussi
CHILDREN = {
    "/Game/Placeable/InteractiveObjects/BP_InteractiveObject_TreasureChest": "/Game/Art/Textures/Icones/t_i_ChestIcon",
}

log = []


def apply(bp_path, icon_path, only_if_empty_or_different_parent=False):
    bp = unreal.load_asset(bp_path)
    icon = unreal.load_asset(icon_path)
    if not bp or not icon:
        log.append("ERREUR : %s ou %s introuvable" % (bp_path, icon_path))
        return
    cdo = unreal.get_default_object(bp.generated_class())
    old = cdo.get_editor_property("indicator_icon")
    cdo.set_editor_property("indicator_icon", icon)
    unreal.EditorAssetLibrary.save_asset(bp_path, only_if_is_dirty=False)
    log.append("%s : IndicatorIcon %s -> %s" % (bp_path, old.get_name() if old else "aucune", icon.get_name()))


for bp_path, icon_path in ICONS.items():
    apply(bp_path, icon_path)
for bp_path, icon_path in CHILDREN.items():
    apply(bp_path, icon_path)

with open(unreal.Paths.project_saved_dir() + "Logs/setup_indicator_icons.txt", "w", encoding="utf-8") as f:
    f.write("\n".join(log))
unreal.log("\n".join(log))
unreal.SystemLibrary.quit_editor()
