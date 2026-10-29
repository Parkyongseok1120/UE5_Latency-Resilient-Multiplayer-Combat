"""Derive arm-only geometry from Manny while preserving its skeleton, fingers and materials."""
import unreal

path = "/Game/Characters/Mannequins/Meshes/SKM_CombatArms_Closed"
mesh = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.EditorAssetLibrary.duplicate_asset(
    "/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple", path
)
editor = unreal.get_editor_subsystem(unreal.SkeletalMeshEditorSubsystem)
lod_count = editor.get_lod_count(mesh)
# The close-up view uses the full-detail base LOD; discard generated whole-body LODs first.
if lod_count > 1 and not editor.remove_lods(mesh, list(range(1, lod_count))):
    raise RuntimeError("Could not remove generated world-body LODs from the view arms")
if not mesh or not unreal.CombatAnimationAssetLibrary.keep_first_person_arm_geometry(mesh):
    raise RuntimeError("Could not derive first-person arm geometry")
if not unreal.EditorAssetLibrary.save_loaded_asset(mesh, only_if_is_dirty=False):
    raise RuntimeError("Could not save first-person arms")
unreal.log("[FirstPersonArms] Saved SKM_CombatArms_Closed with skinned forearm end caps")
