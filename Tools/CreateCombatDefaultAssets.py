"""Create or update default character and weapon data assets wired to UE 5.7 default meshes."""
import unreal
from pathlib import Path

arms_script = Path(unreal.Paths.convert_relative_path_to_full(unreal.Paths.project_dir())) / "Tools/CreateCombatFirstPersonArms.py"
exec(compile(arms_script.read_text(encoding="utf-8"), str(arms_script), "exec"))

asset_tools = unreal.AssetToolsHelpers.get_asset_tools()


def require_asset(path):
    asset = unreal.load_asset(path)
    if asset is None:
        raise RuntimeError(f"Missing required asset: {path}")
    return asset


def duplicate_if_missing(source_path, target_path):
    if unreal.EditorAssetLibrary.does_asset_exist(target_path):
        return require_asset(target_path)
    asset = unreal.EditorAssetLibrary.duplicate_asset(source_path, target_path)
    if asset is None:
        raise RuntimeError(f"Could not duplicate {source_path} to {target_path}")
    return asset


def create_rifle_anim_blueprint():
    root = "/Game/Characters/Mannequins/Anims"
    locomotion = duplicate_if_missing(
        f"{root}/Unarmed/BS_Idle_Walk_Run", f"{root}/Rifle/BS_CombatRifle"
    )
    samples = locomotion.get_editor_property("sample_data")
    for index, sample in enumerate(samples):
        animation = sample.get_editor_property("animation")
        if animation is None:
            raise RuntimeError("Locomotion blend space contains an empty animation sample")
        source = animation.get_path_name().split(".")[0]
        if "/Unarmed/" in source:
            target = source.replace("/Unarmed/", "/Rifle/").replace("MF_Unarmed_", "MF_Rifle_")
            if source.endswith("/MM_Idle"):
                target = f"{root}/Rifle/MF_Rifle_Idle_ADS"
            sample.set_editor_property("animation", require_asset(target))
            samples[index] = sample
    locomotion.set_editor_property("sample_data", samples)
    for sample in locomotion.get_editor_property("sample_data"):
        if "/Rifle/" not in sample.get_editor_property("animation").get_path_name():
            raise RuntimeError("Rifle locomotion still references an unarmed animation")
    if not unreal.EditorAssetLibrary.save_loaded_asset(locomotion, only_if_is_dirty=False):
        raise RuntimeError("Could not save rifle locomotion blend space")

    blueprint = duplicate_if_missing(
        f"{root}/Unarmed/ABP_Unarmed", f"{root}/Rifle/ABP_CombatRifle"
    )
    parent = unreal.load_class(None, "/Script/Latency_MultiCombat.CombatAnimInstance")
    if parent is None:
        raise RuntimeError("Build the Editor target with CombatAnimInstance before creating assets")
    if blueprint.get_editor_property("parent_class") != parent:
        unreal.BlueprintEditorLibrary.reparent_blueprint(blueprint, parent)
    replacements = {
        f"{root}/Unarmed/MM_Idle": f"{root}/Rifle/MF_Rifle_Idle_ADS",
        f"{root}/Unarmed/Jump/MM_Jump": f"{root}/Rifle/Jump/MM_Rifle_Jump_Start",
        f"{root}/Unarmed/Jump/MM_Fall_Loop": f"{root}/Rifle/Jump/MM_Rifle_Jump_Fall_Loop",
        f"{root}/Unarmed/Jump/MM_Land": f"{root}/Rifle/Jump/MM_Rifle_Jump_Fall_Land",
    }
    # The template already supplies locomotion, foot IK and an output DefaultSlot.
    # Edit its duplicated source nodes; compilation rebuilds the generated nodes.
    prefix = f"{blueprint.get_path_name()}:AnimGraph."
    for node in unreal.ObjectIterator(unreal.AnimGraphNode_SequencePlayer):
        if not node.get_path_name().startswith(prefix):
            continue
        data = node.get_editor_property("node")
        animation = data.get_editor_property("sequence")
        source = animation.get_path_name().split(".")[0] if animation else ""
        if source in replacements:
            data.set_editor_property("sequence", require_asset(replacements[source]))
            node.set_editor_property("node", data)
    for node in unreal.ObjectIterator(unreal.AnimGraphNode_BlendSpacePlayer):
        if node.get_path_name().startswith(prefix):
            data = node.get_editor_property("node")
            data.set_editor_property("blend_space", locomotion)
            node.set_editor_property("node", data)
    if not unreal.CombatAnimationAssetLibrary.configure_rifle_upper_body(
        blueprint, require_asset(f"{root}/Rifle/MF_Rifle_Idle_ADS")
    ):
        raise RuntimeError("Could not configure the rifle upper-body layer")
    unreal.BlueprintEditorLibrary.compile_blueprint(blueprint)
    if blueprint.generated_class() is None or not unreal.EditorAssetLibrary.save_loaded_asset(
        blueprint, only_if_is_dirty=False
    ):
        raise RuntimeError("Could not compile/save rifle animation blueprint")
    return blueprint


def configure_left_hand_grip(weapon_mesh, character_mesh):
    weapon_sockets = {
        str(weapon_mesh.get_socket_by_index(i).socket_name): weapon_mesh.get_socket_by_index(i)
        for i in range(weapon_mesh.num_sockets())
    }
    character_sockets = {
        str(character_mesh.get_socket_by_index(i).socket_name): character_mesh.get_socket_by_index(i)
        for i in range(character_mesh.num_sockets())
    }
    support = weapon_sockets.get("GripPoint_002")
    palm = character_sockets.get("HandGrip_L")
    if support is None or palm is None:
        raise RuntimeError("Rifle support grip and character left palm sockets are required")

    def local_transform(socket):
        return unreal.Transform(
            location=socket.relative_location, rotation=socket.relative_rotation, scale=socket.relative_scale
        )

    # Keep the authored foregrip socket. Offset hand_l so its palm-contact socket
    # HandGrip_L coincides with GripPoint_002, rather than placing the wrist pivot there.
    offset = local_transform(palm).inverse()
    unreal.log(f"[DefaultAssets] Support grip={support.socket_name} palm offset={offset}")
    return offset


def get_or_create_data_asset(asset_name: str, package_path: str, class_path: str):
    full_path = f"{package_path}/{asset_name}"
    if unreal.EditorAssetLibrary.does_asset_exist(full_path):
        loaded = unreal.load_asset(full_path)
        if loaded is not None:
            return loaded
    asset_class = unreal.load_class(None, class_path)
    if asset_class is None:
        raise RuntimeError(f"Missing native class {class_path}. Build Editor target first.")
    factory = unreal.DataAssetFactory()
    factory.set_editor_property("data_asset_class", asset_class)
    created = asset_tools.create_asset(asset_name, package_path, asset_class, factory)
    if created is None:
        raise RuntimeError(f"Failed to create {full_path}")
    return created


weapon_skel = unreal.load_asset("/Game/Weapons/Rifle/Meshes/SKM_Rifle")
weapon_static = unreal.load_asset("/Game/Weapons/Rifle/Meshes/SM_Rifle")
fire_sound = unreal.load_asset("/Game/Weapons/GrenadeLauncher/Audio/FirstPersonTemplateWeaponFire02")
char_skel = unreal.load_asset("/Game/Characters/Mannequins/Meshes/SKM_Manny_Simple")
anim_bp = create_rifle_anim_blueprint()
fire_animation = require_asset("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Fire")

if char_skel is None or weapon_skel is None:
    raise RuntimeError("Expected UE 5.7 Characters/Weapons assets under /Game/Characters and /Game/Weapons.")
left_hand_grip_offset = configure_left_hand_grip(weapon_skel, char_skel)

char_sockets = [str(char_skel.get_socket_by_index(i).socket_name) for i in range(char_skel.num_sockets())]
weapon_sockets = [str(weapon_skel.get_socket_by_index(i).socket_name) for i in range(weapon_skel.num_sockets())]
weapon_bounds = weapon_skel.get_bounds()
unreal.log(f"[DefaultAssets] SKM_Manny_Simple sockets={char_sockets} bounds={char_skel.get_bounds().box_extent}")
unreal.log(
    f"[DefaultAssets] SKM_Rifle sockets={weapon_sockets} origin={weapon_bounds.origin} extent={weapon_bounds.box_extent}"
)

grip_socket = "HandGrip_R" if "HandGrip_R" in char_sockets else "hand_r"
muzzle_socket = "Muzzle" if "Muzzle" in weapon_sockets else ("MuzzleFlash" if "MuzzleFlash" in weapon_sockets else "Muzzle")
fp_yaw = -90.0 if weapon_bounds.box_extent.y > weapon_bounds.box_extent.x else 0.0

weapon_asset = get_or_create_data_asset(
    "DA_DefaultWeapon", "/Game/Data", "/Script/Latency_MultiCombat.WeaponDataAsset"
)
weapon_asset.set_editor_property("weapon_name", unreal.Text("Assault Rifle"))
weapon_stats = weapon_asset.get_editor_property("stats")
weapon_stats.set_editor_property("magazine_size", 30)
weapon_stats.set_editor_property("reserve_ammo", 330)
weapon_asset.set_editor_property("stats", weapon_stats)
weapon_asset.set_editor_property("weapon_skeletal_mesh", weapon_skel)
if weapon_static is not None:
    weapon_asset.set_editor_property("weapon_mesh", weapon_static)
weapon_asset.set_editor_property("muzzle_socket_name", unreal.Name(muzzle_socket))
weapon_asset.set_editor_property("third_person_attach_socket_name", unreal.Name(grip_socket))
weapon_asset.set_editor_property("first_person_location_offset", unreal.Vector(38.0, 16.0, -18.0))
weapon_asset.set_editor_property("first_person_rotation_offset", unreal.Rotator(pitch=0.0, yaw=fp_yaw, roll=0.0))
weapon_asset.set_editor_property("third_person_location_offset", unreal.Vector(-2.0, 4.0, -2.0))
weapon_asset.set_editor_property("third_person_rotation_offset", unreal.Rotator(pitch=0.0, yaw=0.0, roll=0.0))
weapon_asset.set_editor_property("character_fire_animation", fire_animation)
weapon_asset.set_editor_property("character_reload_animation", require_asset("/Game/Characters/Mannequins/Anims/Rifle/MM_Rifle_Reload"))
weapon_asset.set_editor_property("character_animation_slot", unreal.Name("DefaultSlot"))
weapon_asset.set_editor_property("left_hand_grip_socket_name", unreal.Name("GripPoint_002"))
weapon_asset.set_editor_property("left_hand_grip_offset", left_hand_grip_offset)
if fire_sound is not None:
    weapon_asset.set_editor_property("fire_sound", fire_sound)

if not unreal.EditorAssetLibrary.save_loaded_asset(weapon_asset, only_if_is_dirty=False):
    raise RuntimeError("Could not save /Game/Data/DA_DefaultWeapon")

char_asset = get_or_create_data_asset(
    "DA_DefaultCharacter", "/Game/Data", "/Script/Latency_MultiCombat.CharacterDataAsset"
)
char_asset.set_editor_property("character_name", unreal.Text("Combat Trooper"))
char_asset.set_editor_property("default_weapon", weapon_asset)
char_asset.set_editor_property("character_skel_mesh", char_skel)
if anim_bp is not None and anim_bp.generated_class() is not None:
    char_asset.set_editor_property("character_anim_class", anim_bp.generated_class())
char_asset.set_editor_property("mesh_location_offset", unreal.Vector(0.0, 0.0, -90.0))
# Unreal's Python positional Rotator arguments differ from C++ FRotator: name the axes.
char_asset.set_editor_property("mesh_rotation_offset", unreal.Rotator(pitch=0.0, yaw=-90.0, roll=0.0))
char_asset.set_editor_property("team_color_parameter_name", unreal.Name("Paint Tint"))
char_asset.set_editor_property("weapon_attach_socket_name", unreal.Name(grip_socket))

if not unreal.EditorAssetLibrary.save_loaded_asset(char_asset, only_if_is_dirty=False):
    raise RuntimeError("Could not save /Game/Data/DA_DefaultCharacter")

unreal.log(
    f"[DefaultAssets] Saved DA_DefaultCharacter (grip={grip_socket}) and DA_DefaultWeapon (muzzle={muzzle_socket}, fp_yaw={fp_yaw})"
)
