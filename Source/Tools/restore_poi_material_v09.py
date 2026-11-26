import unreal,json,random
from pathlib import Path
root='/Game/POIKit';out=Path(unreal.Paths.project_saved_dir())/'POICheckpoints'
ea=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools();me=unreal.MaterialEditingLibrary
path=root+'/M_KitImageFit'
mat=unreal.load_asset(path) if ea.does_asset_exist(path) else tools.create_asset('M_KitImageFit',root,unreal.Material,unreal.MaterialFactoryNew())
for expression in list(me.get_material_expressions(mat)):
 me.delete_material_expression(mat,expression)
assert len(me.get_material_expressions(mat))==0
mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED);mat.set_editor_property('two_sided',True);mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
def node(cls):return me.create_material_expression(mat,cls)
def connect(a,p,b,q):assert me.connect_material_expressions(a,p,b,q)
sample=node(unreal.MaterialExpressionTextureSampleParameter2D);sample.set_editor_property('parameter_name','ImageTexture');sample.set_editor_property('texture',unreal.load_asset('/Engine/EngineResources/DefaultTexture'))
uv=node(unreal.MaterialExpressionTextureCoordinate)
rect=node(unreal.MaterialExpressionVectorParameter);rect.set_editor_property('parameter_name','UVRect');rect.set_editor_property('default_value',unreal.LinearColor(0,0,1,1))
offset=node(unreal.MaterialExpressionComponentMask);scale=node(unreal.MaterialExpressionComponentMask)
for n,channels in [(offset,'rg'),(scale,'ba')]:
 for c in 'rgba':n.set_editor_property(c,c in channels)
 connect(rect,'RGBA',n,'')
mul=node(unreal.MaterialExpressionMultiply);add=node(unreal.MaterialExpressionAdd)
connect(uv,'',mul,'A');connect(scale,'',mul,'B');connect(mul,'',add,'A');connect(offset,'',add,'B');connect(add,'',sample,'')
tint=node(unreal.MaterialExpressionVectorParameter);tint.set_editor_property('parameter_name','Tint');tint.set_editor_property('default_value',unreal.LinearColor(1,1,1,1))
color=node(unreal.MaterialExpressionMultiply);connect(sample,'RGB',color,'A');connect(tint,'RGB',color,'B')
me.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR);me.connect_material_property(sample,'A',unreal.MaterialProperty.MP_OPACITY_MASK)
me.recompile_material(mat);ea.save_loaded_asset(mat)

assert me.get_material_property_input_node(mat,unreal.MaterialProperty.MP_OPACITY_MASK)==sample
assert me.get_material_property_input_node_output_name(mat,unreal.MaterialProperty.MP_OPACITY_MASK) in ['A','Alpha']
assert not any(isinstance(e,unreal.MaterialExpressionSceneTexture) for e in me.get_material_expressions(mat))
(out/'Material-v09.json').write_text(json.dumps({'material':mat.get_path_name(),'opacity_source':'ImageTexture alpha','scene_texture_nodes':0,'uv_fit_preserved':True,'foreground_overlay':'disabled_pending_separate_fix'}),encoding='utf8')
unreal.log('POI_V09_MATERIAL_RESTORED')
