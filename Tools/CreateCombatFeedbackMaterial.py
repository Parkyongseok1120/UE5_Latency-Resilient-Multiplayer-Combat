"""Build the camera-only damage/glitch material from editable graph and HLSL source."""
import unreal

path = "/Game/UI/Materials/M_CombatDamage"
library = unreal.MaterialEditingLibrary
material = unreal.load_asset(path) if unreal.EditorAssetLibrary.does_asset_exist(path) else unreal.AssetToolsHelpers.get_asset_tools().create_asset(
    "M_CombatDamage", "/Game/UI/Materials", unreal.Material, unreal.MaterialFactoryNew()
)
material.set_editor_property("material_domain", unreal.MaterialDomain.MD_POST_PROCESS)
material.set_editor_property("blendable_location", unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING)
# Native constructor references may root the material's expressions. Update known nodes in place.
existing_nodes = [item for item in unreal.ObjectIterator(unreal.MaterialExpression)
                  if item.get_outer() == material]

def node(cls, x, y):
    for existing in existing_nodes:
        if isinstance(existing, cls) and existing.get_editor_property("material_expression_editor_x") == x and existing.get_editor_property("material_expression_editor_y") == y:
            return existing
    return library.create_material_expression(material, cls, x, y)

def scalar(name, y):
    parameter = node(unreal.MaterialExpressionScalarParameter, -1000, y)
    parameter.set_editor_property("parameter_name", name)
    parameter.set_editor_property("default_value", 0.0)
    return parameter

def custom(description, code, names, x, y, output):
    expression = node(unreal.MaterialExpressionCustom, x, y)
    expression.set_editor_property("description", description)
    expression.set_editor_property("code", code)
    expression.set_editor_property("output_type", output)
    inputs = []
    for name in names:
        entry = unreal.CustomInput()
        entry.set_editor_property("input_name", name)
        inputs.append(entry)
    expression.set_editor_property("inputs", inputs)
    return expression

uv = node(unreal.MaterialExpressionScreenPosition, -1000, -400)
hit = scalar("HitFlash", -100)
low = scalar("LowHealth", 80)
time = scalar("EffectTime", 240)
warp = custom("Low-health staggered edge blocks", """
float strength = saturate(LowHealth * 1.4);
float frame = floor(EffectTime * (18.0 + strength * 16.0));
float borderDistance = min(min(UV.x, 1.0 - UV.x), min(UV.y, 1.0 - UV.y));
float edge = 1.0 - smoothstep(0.015, 0.16 + strength * 0.07, borderDistance);
float row = floor(UV.y * 28.0);
float stagger = frac(sin(row * 91.7 + frame * 3.1) * 28751.13);
float2 cell = floor(UV * float2(24.0, 28.0) + float2(stagger, 0));
float2 randomBlock = frac(sin(float2(dot(cell, float2(127.1, 311.7)),
    dot(cell, float2(269.5, 183.3))) + frame * float2(13.7, 7.3)) * 43758.5453);
float active = step(0.62 - strength * 0.25, randomBlock.x);
float2 shift = (randomBlock - 0.5) * float2(0.13, 0.055) * active;
float fineRow = floor(UV.y * 120.0);
float fineNoise = frac(sin(fineRow * 71.3 + frame * 41.7) * 15317.23);
shift.x += (fineNoise - 0.5) * 0.025 * step(0.67, fineNoise);
return clamp(UV + shift * edge * strength, float2(0.001, 0.001), float2(0.999, 0.999));
""", ["UV", "LowHealth", "EffectTime"], -650, -350, unreal.CustomMaterialOutputType.CMOT_FLOAT2)
library.connect_material_expressions(uv, "ViewportUV", warp, "UV")
library.connect_material_expressions(low, "", warp, "LowHealth")
library.connect_material_expressions(time, "", warp, "EffectTime")

samples = []
for index, sign in enumerate((0, 1, -1)):
    offset = custom("Edge RGB sample UV", f"""
float borderDistance = min(min(UV.x, 1.0 - UV.x), min(UV.y, 1.0 - UV.y));
float edge = 1.0 - smoothstep(0.015, 0.23, borderDistance);
return clamp(UV + float2({sign}.0 * LowHealth * 0.018 * edge, 0), 0.001, 0.999);
""",
                    ["UV", "LowHealth"], -350, index * 160 - 350, unreal.CustomMaterialOutputType.CMOT_FLOAT2)
    library.connect_material_expressions(warp, "", offset, "UV")
    library.connect_material_expressions(low, "", offset, "LowHealth")
    sample = node(unreal.MaterialExpressionSceneTexture, -100, index * 160 - 350)
    sample.set_editor_property("scene_texture_id", unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    library.connect_material_expressions(offset, "", sample, "Coordinates")
    samples.append(sample)

compose = custom("Spectrum rectangles and edge static", """
float3 color = float3(Red.r, Base.g, Blue.b);
float2 centered = abs(UV * 2.0 - 1.0);
float damageEdge = pow(saturate(max(centered.x, centered.y)), 3.5);
float alpha = damageEdge * saturate(HitFlash * 0.62 + LowHealth * 0.10);
color = lerp(color, float3(0.72, 0.008, 0.018), alpha);
float strength = saturate(LowHealth * 1.4);
float enabled = smoothstep(0.0, 0.07, LowHealth);
float frame = floor(EffectTime * (18.0 + strength * 16.0));
float borderDistance = min(min(UV.x, 1.0 - UV.x), min(UV.y, 1.0 - UV.y));
float edge = 1.0 - smoothstep(0.015, 0.16 + strength * 0.07, borderDistance);
// Offset rows and differently sized grids make the rectangles overlap and break apart.
[unroll] for (int layer = 0; layer < 3; ++layer)
{
    float2 grid = layer == 0 ? float2(18, 24) : (layer == 1 ? float2(42, 48) : float2(96, 104));
    float row = floor(UV.y * grid.y);
    float stagger = frac(sin(row * 71.3 + frame * 2.1 + layer * 17.7) * 13758.31);
    float2 blockUV = UV * grid + float2(stagger, layer * 0.37);
    float2 cell = floor(blockUV);
    float2 tile = frac(blockUV);
    float hash = dot(cell, float2(127.1, 311.7)) + frame * 53.7 + layer * 83.1;
    float randomBlock = frac(sin(hash) * 43758.5453);
    float active = step(0.68 - strength * 0.30, randomBlock);
    float2 inset = float2(0.045 + 0.12 * frac(randomBlock * 7.1), 0.12);
    float2 rectangle = step(inset, tile) * step(tile, 1.0 - inset);
    float patch = rectangle.x * rectangle.y * active;
    float hue = frac(randomBlock * 3.7 + frame * 0.071 + layer * 0.29);
    float3 spectrum = saturate(abs(frac(hue + float3(0, 0.6666667, 0.3333333)) * 6.0 - 3.0) - 1.0);
    float3 blockColor = 0.035 + spectrum * 1.12;
    float blockAlpha = edge * enabled * patch * (0.30 + strength * 0.48) * (1.0 - layer * 0.13);
    color = lerp(color, blockColor, saturate(blockAlpha));
}
float grain = frac(sin(dot(floor(UV * float2(960, 540)), float2(12.9898, 78.233)) + frame) * 43758.5453) - 0.5;
float scan = sin(UV.y * 1400.0 + frame * 1.7) * 0.08;
color += (scan + grain * 0.24) * edge * strength;
return saturate(color);
""", ["Base", "Red", "Blue", "UV", "HitFlash", "LowHealth", "EffectTime"], 200, -100,
unreal.CustomMaterialOutputType.CMOT_FLOAT3)
for sample, name in zip(samples, ("Base", "Red", "Blue")):
    library.connect_material_expressions(sample, "Color", compose, name)
for expression, output, name in ((uv, "ViewportUV", "UV"), (hit, "", "HitFlash"), (low, "", "LowHealth"), (time, "", "EffectTime")):
    library.connect_material_expressions(expression, output, compose, name)
library.connect_material_property(compose, "", unreal.MaterialProperty.MP_EMISSIVE_COLOR)
library.recompile_material(material)
if not unreal.EditorAssetLibrary.save_loaded_asset(material, only_if_is_dirty=False):
    raise RuntimeError("Could not save combat feedback material")
unreal.log("[CombatFeedback] Saved camera postprocess material with HitFlash, LowHealth and EffectTime")
