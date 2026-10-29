"""Upgrade only rifle upper-body layering and its reload reference; preserve other asset settings."""
import unreal

bp = unreal.load_asset("/Game/Characters/Mannequins/Anims/Rifle/ABP_CombatRifle")
pose = unreal.load_asset("/Game/Characters/Mannequins/Anims/Rifle/MF_Rifle_Idle_ADS")
if not unreal.CombatAnimationAssetLibrary.configure_rifle_upper_body(bp, pose):
    raise RuntimeError("Could not configure the rifle upper-body layer")
unreal.BlueprintEditorLibrary.compile_blueprint(bp)
if not unreal.EditorAssetLibrary.save_loaded_asset(bp, only_if_is_dirty=False):
    raise RuntimeError("Could not save rifle upper-body graph")
weapon = unreal.load_asset("/Game/Data/DA_DefaultWeapon")
weapon.set_editor_property("character_reload_animation", unreal.load_asset("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload"))
if not unreal.EditorAssetLibrary.save_loaded_asset(weapon, only_if_is_dirty=False):
    raise RuntimeError("Could not save default reload reference")
unreal.log("[CombatAnimation] Saved locomotion base + spine_01 aim/fire/reload layer and reload data")
