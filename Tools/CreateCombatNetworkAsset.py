"""Create the default network tuning asset once; existing tuning is preserved."""
import unreal

asset_path = "/Game/Data/DA_CombatNetwork"
asset = unreal.load_asset(asset_path) if unreal.EditorAssetLibrary.does_asset_exist(asset_path) else None
if asset is None:
    asset_class = unreal.load_class(None, "/Script/Latency_MultiCombat.CombatNetworkDataAsset")
    if asset_class is None:
        raise RuntimeError("Build the project before creating its network data asset.")
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "DA_CombatNetwork", "/Game/Data", asset_class, factory
    )
    if asset is None or not unreal.EditorAssetLibrary.save_loaded_asset(asset):
        raise RuntimeError("Could not save DA_CombatNetwork.")
    unreal.log("[Week6] Created /Game/Data/DA_CombatNetwork")
else:
    unreal.log("[Week6] Existing DA_CombatNetwork preserved")
