import unreal,json,random
from pathlib import Path
root='/Game/POIKit';out=Path(unreal.Paths.project_saved_dir())/'POICheckpoints'
ea=unreal.EditorAssetLibrary;tools=unreal.AssetToolsHelpers.get_asset_tools();me=unreal.MaterialEditingLibrary
path=root+'/M_KitImageFit'
mat=unreal.load_asset(path) if ea.does_asset_exist(path) else tools.create_asset('M_KitImageFit',root,unreal.Material,unreal.MaterialFactoryNew())
me.delete_all_material_expressions(mat)
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
tasks=[]
for i,p in enumerate(sorted((Path(unreal.Paths.project_saved_dir())/'POIImports/Tops').glob('*.png')),1):
 t=unreal.AssetImportTask();t.filename=str(p);t.destination_path=root+'/Art/Tops';t.destination_name=f'T_Top_{i:02}';t.automated=True;t.save=True;tasks.append(t)
tools.import_asset_tasks(tasks);tops=[unreal.load_asset(t.imported_object_paths[0]) for t in tasks];assert len(tops)==10
random.Random(611).shuffle(tops);counter=0
world=unreal.EditorLoadingAndSavingUtils.load_map(root+'/L_POI_Workshop_AST')
report={'blocks':[],'edges':[],'textures':{}}
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 if isinstance(a,unreal.POIKitBlock):
  s=a.get_editor_property('settings');s.set_editor_property('material',mat)
  if '/Platforms/' in str(s.image):s.set_editor_property('top_image',tops[counter%10]);counter+=1
  a.set_editor_property('settings',s);a.refresh_block();s=a.get_editor_property('settings')
  report['blocks'].append({'label':a.get_actor_label(),'transform':a.get_actor_transform().export_text(),'settings':s.export_text()})
 if a.get_class().get_name()=='ParkourActor_C':
  report['edges'].append({'label':a.get_actor_label(),'transform':a.get_actor_transform().export_text(),'ledge':str(a.get_editor_property('LedgeType')),'style':str(a.get_editor_property('ClimbStyle')),'spline':str(a.get_component_by_class(unreal.SplineComponent).get_spline_length())})
assert unreal.EditorLoadingAndSavingUtils.save_map(world,root+'/L_POI_Workshop_AST')
def bounds(path):
 tex=unreal.load_asset(path) if path and path!='None' else None
 b=unreal.POIKitBlock.get_image_alpha_bounds(tex)
 report['textures'][str(path)]=[b.x,b.y,b.z,b.w]
 return dict(X=b.x,Y=b.y,Z=b.z,W=b.w)
for name in ['DT_ShaftModules','DT_ShaftVariants']:
 table=unreal.load_asset(root+'/'+name);rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
 for row in rows:
  for piece in row.get('Pieces',[]):
   s=piece['Block'];s['Material']=mat.get_path_name();s['ImageUVRect']=bounds(s.get('Image'));s['bFitImageToAlpha']=True
   if '/Platforms/' in s.get('Image',''):
    s['TopImage']=tops[counter%10].get_path_name();counter+=1;s['TopImageUVRect']=bounds(s['TopImage'])
 assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(rows));assert ea.save_loaded_asset(table)
(out/'Art-and-layout-v06.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('POI_V06_ART_DONE')


