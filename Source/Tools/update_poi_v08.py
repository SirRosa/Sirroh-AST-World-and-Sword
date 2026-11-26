import unreal,json
from pathlib import Path
root='/Game/POIKit';me=unreal.MaterialEditingLibrary;ea=unreal.EditorAssetLibrary
mat=unreal.load_asset(root+'/M_KitImageFit')
marker='POI_PlayerForeground_v08'
if not any(e.get_editor_property('desc')==marker for e in me.get_material_expressions(mat)):
 alpha=me.get_material_property_input_node(mat,unreal.MaterialProperty.MP_OPACITY_MASK)
 output=me.get_material_property_input_node_output_name(mat,unreal.MaterialProperty.MP_OPACITY_MASK)
 def node(cls):return me.create_material_expression(mat,cls)
 def link(a,p,b,q):assert me.connect_material_expressions(a,p,b,q),(a.get_name(),p,b.get_name(),q)
 scene=node(unreal.MaterialExpressionSceneTexture)
 enum=type(scene.get_editor_property('scene_texture_id'))
 scene.set_editor_property('scene_texture_id',next(v for v in enum if 'CUSTOM_STENCIL' in str(v)))
 channel=node(unreal.MaterialExpressionComponentMask)
 for c in 'rgba':channel.set_editor_property(c,c=='r')
 link(scene,'Color',channel,'')
 value=node(unreal.MaterialExpressionConstant);value.set_editor_property('r',254)
 difference=node(unreal.MaterialExpressionSubtract);link(channel,'',difference,'A');link(value,'',difference,'B')
 absolute=node(unreal.MaterialExpressionAbs);link(difference,'',absolute,'')
 saturate=node(unreal.MaterialExpressionSaturate);link(absolute,'',saturate,'')
 multiply=node(unreal.MaterialExpressionMultiply);multiply.set_editor_property('desc',marker)
 link(alpha,output,multiply,'A');link(saturate,'',multiply,'B')
 assert me.connect_material_property(multiply,'',unreal.MaterialProperty.MP_OPACITY_MASK)
 me.recompile_material(mat);assert ea.save_loaded_asset(mat)
world=unreal.EditorLoadingAndSavingUtils.load_map(root+'/L_POI_Workshop_AST')
count=0
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 if isinstance(a,unreal.POIKitBlock):
  s=a.get_editor_property('settings')
  if s.kind==unreal.POIKitKind.BACKGROUND:s.set_editor_property('collision',unreal.POIKitCollision.SOLID);a.set_editor_property('settings',s)
  a.refresh_block();count+=1
assert unreal.EditorLoadingAndSavingUtils.save_map(world,root+'/L_POI_Workshop_AST')
for name in ['DT_ShaftModules','DT_ShaftVariants']:
 table=unreal.load_asset(root+'/'+name);rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
 for row in rows:
  for piece in row.get('Pieces',[]):
   if piece['Block'].get('Kind')=='Background':piece['Block']['Collision']='Solid'
 assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(rows));assert ea.save_loaded_asset(table)
bp=unreal.load_asset(root+'/BP_BackWall');cdo=unreal.get_default_object(bp.generated_class());s=cdo.get_editor_property('settings');s.set_editor_property('collision',unreal.POIKitCollision.SOLID);cdo.set_editor_property('settings',s);assert ea.save_loaded_asset(bp)
(Path(unreal.Paths.project_saved_dir())/'POICheckpoints/Assets-v08.json').write_text(json.dumps({'blocks':count,'foreground_stencil':254,'background_collision':'Solid'}),encoding='utf8')
unreal.log('POI_V08_ASSETS_DONE')
