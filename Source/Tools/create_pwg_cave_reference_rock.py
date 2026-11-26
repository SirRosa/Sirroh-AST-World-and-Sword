"""New isolated DefaultLit rock parent. Existing atlas and alpha graph are unchanged."""
import unreal
ea=unreal.EditorAssetLibrary
me=unreal.MaterialEditingLibrary
mp=unreal.MaterialProperty
root='/Game/Systems/Procedural_World_Generator/PWG_v2/'
src=root+'M_PWGv2_BackgroundLit'
dst=root+'M_PWGv2_CaveReferenceRock'
source=unreal.load_asset(src)
assert source and source.get_editor_property('shading_model')==unreal.MaterialShadingModel.MSM_DEFAULT_LIT
assert not ea.does_asset_exist(dst), 'Reference rock already exists; inspect before overwriting'
mat=ea.duplicate_asset(src,dst)
assert mat
base=me.get_material_property_input_node(mat,mp.MP_BASE_COLOR)
pin=me.get_material_property_input_node_output_name(mat,mp.MP_BASE_COLOR) or ''
assert base
flat=me.create_material_expression(mat,unreal.MaterialExpressionVectorParameter,350,250)
flat.set_editor_property('parameter_name','CaveFlatAlbedo')
flat.set_editor_property('default_value',unreal.LinearColor(.30,.32,.34,1))
strength=me.create_material_expression(mat,unreal.MaterialExpressionScalarParameter,350,400)
strength.set_editor_property('parameter_name','CaveTextureIntensity')
strength.set_editor_property('default_value',1.0)
mix=me.create_material_expression(mat,unreal.MaterialExpressionLinearInterpolate,650,250)
assert me.connect_material_expressions(flat,'',mix,'A')
assert me.connect_material_expressions(base,pin,mix,'B')
assert me.connect_material_expressions(strength,'',mix,'Alpha')
assert me.connect_material_property(mix,'',mp.MP_BASE_COLOR)
assert me.get_material_property_input_node(mat,mp.MP_BASE_COLOR)==mix
assert mat.get_editor_property('shading_model')==unreal.MaterialShadingModel.MSM_DEFAULT_LIT
me.layout_material_expressions(mat)
me.recompile_material(mat)
assert ea.save_loaded_asset(mat,only_if_is_dirty=False)
unreal.log('CAVE_REFERENCE_ROCK_SAVED '+mat.get_path_name())
