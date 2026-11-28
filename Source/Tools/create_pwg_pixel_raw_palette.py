"""Duplicate the exact lit material graph, preserving palette custom code, alpha and UV.
Raw is a separate diagnostic asset, never a modification of the lit checkpoint."""
import unreal
ea=unreal.EditorAssetLibrary
me=unreal.MaterialEditingLibrary
mp=unreal.MaterialProperty
root='/Game/Systems/Procedural_World_Generator/PWG_v2/'
src=root+'M_PWGv2_PixelBackground'
dst=root+'M_PWGv2_PixelBackground_RawPalette'
original=unreal.load_asset(src)
assert original and original.get_editor_property('blend_mode')==unreal.BlendMode.BLEND_MASKED
original_color=me.get_material_property_input_node(original,mp.MP_BASE_COLOR)
assert isinstance(original_color,unreal.MaterialExpressionCustom)
original_mask=me.get_material_property_input_node(original,mp.MP_OPACITY_MASK)
assert original_mask
raw=unreal.load_asset(dst) if ea.does_asset_exist(dst) else None
if not raw:
    raw=ea.duplicate_asset(src,dst)
    assert raw
    raw.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    color=me.get_material_property_input_node(raw,mp.MP_BASE_COLOR)
    assert isinstance(color,unreal.MaterialExpressionCustom)
    assert color.get_editor_property('code')==original_color.get_editor_property('code')
    assert me.connect_material_property(color,'',mp.MP_EMISSIVE_COLOR)
    me.recompile_material(raw)
    assert ea.save_loaded_asset(raw,only_if_is_dirty=False)
assert raw.get_editor_property('blend_mode')==unreal.BlendMode.BLEND_MASKED
assert raw.get_editor_property('shading_model')==unreal.MaterialShadingModel.MSM_UNLIT
color=me.get_material_property_input_node(raw,mp.MP_EMISSIVE_COLOR)
assert isinstance(color,unreal.MaterialExpressionCustom)
assert color.get_editor_property('code')==original_color.get_editor_property('code')
mask=me.get_material_property_input_node(raw,mp.MP_OPACITY_MASK)
assert mask and mask.get_class()==original_mask.get_class()
for fn in (me.get_scalar_parameter_names,me.get_vector_parameter_names,me.get_texture_parameter_names):
    assert set(map(str,fn(raw)))==set(map(str,fn(original)))
unreal.log('PIXEL_RAW_PALETTE_VERIFIED '+raw.get_path_name())
