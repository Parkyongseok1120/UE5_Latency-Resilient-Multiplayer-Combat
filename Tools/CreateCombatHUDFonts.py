"""Import embedded NanumSquare Neo faces for the native Canvas HUD. Run in an Editor session with Slate initialized."""
from pathlib import Path
import unreal

project = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir()))
source = project / "SourceAssets" / "Fonts" / "NanumSquareNeo"
destination = "/Game/UI/Fonts"
asset_tools = unreal.AssetToolsHelpers.get_asset_tools()

for weight in ("Regular", "Bold"):
    asset_name = f"FF_NanumSquareNeo_{weight}"
    face = unreal.load_asset(f"{destination}/{asset_name}") if unreal.EditorAssetLibrary.does_asset_exist(
        f"{destination}/{asset_name}"
    ) else None
    if face is None:
        task = unreal.AssetImportTask()
        task.filename = str(source / f"NanumSquareNeo-{weight}.ttf")
        task.destination_path = destination
        task.destination_name = asset_name
        task.factory = unreal.FontFileImportFactory()
        task.automated = True
        task.save = True
        asset_tools.import_asset_tasks([task])
        face = unreal.load_asset(f"{destination}/{asset_name}")
    if not isinstance(face, unreal.FontFace):
        raise RuntimeError(f"Expected embedded FontFace: {asset_name}, got {face}")
    face.set_editor_property("loading_policy", unreal.FontLoadingPolicy.INLINE)
    if not unreal.EditorAssetLibrary.save_loaded_asset(face, only_if_is_dirty=False):
        raise RuntimeError(f"Could not save font face: {asset_name}")
    unreal.log(f"[CombatHUDFont] Saved {destination}/{asset_name}: NanumSquare Neo {weight}, inline face")
