import unreal, pathlib, shutil, json
me=unreal.MaterialEditingLibrary
roots=[('GrimModular','M_Grim_Ink'),('GrimModularV7','M_GrimV7_Ink'),('GrimModularV10','M_GrimV10_Ink'),('SimpleMan','M_SimpleMan_Ink'),('GrimGirl','M_GrimGirl_Ink'),('UEFN_Mannequin','M_UEFN_Mannequin'),('UEFN_Mannequin','M_UEFN_Mannequin_Masked_Bicolor')]
report=[]
for folder,name in roots:
 path='/Game/Characters/'+folder+'/Materials/'+name
 mat=unreal.load_asset(path)
 if not mat: continue
 model=mat.get_editor_property('shading_model')
 emissive=me.get_material_property_input_node(mat,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
 row={'asset':path,'before':str(model),'emissive':str(emissive),'changed':False}
 if emissive and (not isinstance(emissive,unreal.MaterialExpressionConstant) or emissive.get_editor_property('r')!=0.0):
  original=pathlib.Path(unreal.Paths.project_content_dir())/('Characters/'+folder+'/Materials/'+name+'.uasset')
  backup=pathlib.Path(unreal.Paths.project_saved_dir())/'CarryProps/BeforeLightingFix'/folder/(name+'.uasset')
  backup.parent.mkdir(parents=True,exist_ok=True)
  if not backup.exists(): shutil.copy2(original,backup)
  output=me.get_material_property_input_node_output_name(mat,unreal.MaterialProperty.MP_EMISSIVE_COLOR)
  mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
  if model==unreal.MaterialShadingModel.MSM_UNLIT or not me.get_material_property_input_node(mat,unreal.MaterialProperty.MP_BASE_COLOR):
   assert me.connect_material_property(emissive,output,unreal.MaterialProperty.MP_BASE_COLOR)
  zero=me.create_material_expression(mat,unreal.MaterialExpressionConstant);zero.set_editor_property('r',0.0)
  assert me.connect_material_property(zero,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
  rough=me.create_material_expression(mat,unreal.MaterialExpressionConstant);rough.set_editor_property('r',0.8)
  me.connect_material_property(rough,'',unreal.MaterialProperty.MP_ROUGHNESS)
  me.recompile_material(mat)
  assert unreal.EditorAssetLibrary.save_loaded_asset(mat,False)
  row['changed']=True
 report.append(row)
pathlib.Path(unreal.Paths.project_saved_dir(),'CarryProps','lighting-fix.json').write_text(json.dumps(report,indent=2))
unreal.log('CHARACTER_LIGHTING '+json.dumps(report))
