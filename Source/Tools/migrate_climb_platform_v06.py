import unreal,json
from pathlib import Path
root='/Game/POIKit';ea=unreal.EditorAssetLibrary;editor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
world=unreal.EditorLoadingAndSavingUtils.load_map(root+'/L_POI_Workshop_AST')
report={'platforms':[],'replaced_edges':[]}
for a in list(editor.get_all_level_actors()):
 if isinstance(a,unreal.POIKitBlock):
  s=a.get_editor_property('settings')
  if '/Platforms/' not in str(s.image):continue
  s.set_editor_property('kind',unreal.POIKitKind.AST);s.set_editor_property('both_ast_sides',True);s.set_editor_property('ast_ledge',unreal.POIASTLedge.LEDGE);s.set_editor_property('ast_hang',unreal.POIASTHang.AUTOMATIC)
  a.set_editor_property('settings',s);a.refresh_block();report['platforms'].append(a.get_actor_label())
 elif a.get_class().get_name()=='ParkourActor_C' and isinstance(a.get_attach_parent_actor(),unreal.POIShaft):
  # These are the old independent sample edges. Preserve the user's platform transforms.
  report['replaced_edges'].append(a.get_actor_label());editor.destroy_actor(a)
assert unreal.EditorLoadingAndSavingUtils.save_map(world,root+'/L_POI_Workshop_AST')
for name in ['DT_ShaftModules','DT_ShaftVariants']:
 table=unreal.load_asset(root+'/'+name);rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
 for row in rows:
  for piece in row.get('Pieces',[]):
   s=piece['Block']
   if '/Platforms/' in s.get('Image',''):s.update(Kind='AST',bBothASTSides=True,ASTLedge='Ledge',ASTHang='Automatic')
  row['NativePieces']=[p for p in row.get('NativePieces',[]) if not ('/Actor/ParkourActor.' in p.get('ActorClass','') and abs(abs(p['Transform']['Translation']['X'])-60)<1)]
 assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(rows));assert ea.save_loaded_asset(table)
path=root+'/BP_ClimbPlatform'
if not ea.does_asset_exist(path):
 f=unreal.BlueprintFactory();f.set_editor_property('parent_class',unreal.POIKitBlock)
 bp=unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_ClimbPlatform',root,unreal.Blueprint,f)
else:bp=unreal.load_asset(path)
cdo=unreal.get_default_object(bp.generated_class());s=cdo.get_editor_property('settings')
s.set_editor_property('kind',unreal.POIKitKind.AST);s.set_editor_property('both_ast_sides',True);s.set_editor_property('collision',unreal.POIKitCollision.ONE_WAY);s.set_editor_property('size',unreal.Vector(240,100,40))
s.set_editor_property('material',unreal.load_asset(root+'/M_KitImageFit'));s.set_editor_property('image',unreal.load_asset(root+'/Art/Platforms/T_Platform_01'));s.set_editor_property('top_image',unreal.load_asset(root+'/Art/Tops/T_Top_01'))
temporary=editor.spawn_actor_from_class(unreal.POIKitBlock,unreal.Vector(0,0,-10000));temporary.set_editor_property('settings',s);temporary.refresh_block();s=temporary.get_editor_property('settings');editor.destroy_actor(temporary)
cdo.set_editor_property('settings',s);assert ea.save_loaded_asset(bp)
(Path(unreal.Paths.project_saved_dir())/'POICheckpoints/Climb-platform-v06.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('POI_CLIMB_PLATFORMS_UPDATED')
