import unreal,json,random
from pathlib import Path
root='/Game/POIKit'
tools=unreal.AssetToolsHelpers.get_asset_tools()
ea=unreal.EditorAssetLibrary
art={}
for group,prefix in [('Backgrounds','BG'),('Platforms','Platform')]:
 tasks=[]
 for i,f in enumerate(sorted((Path(unreal.Paths.project_saved_dir())/'POIImports'/group).glob('*.png')),1):
  t=unreal.AssetImportTask();t.filename=str(f);t.destination_path=root+'/Art/'+group;t.destination_name='T_'+prefix+'_'+str(i).zfill(2);t.automated=True;t.save=True;t.replace_existing=False;tasks.append(t)
 tools.import_asset_tasks(tasks)
 art[group]=[unreal.load_asset(t.imported_object_paths[0]) for t in tasks]
 assert len(art[group])==10
randomizer=random.Random(51703)
for values in art.values():randomizer.shuffle(values)
counts={'Backgrounds':0,'Platforms':0}
def choose(group):
 result=art[group][counts[group]%len(art[group])];counts[group]+=1
 return result
material=unreal.load_asset(root+'/M_KitImage')
world=unreal.EditorLoadingAndSavingUtils.load_map(root+'/L_POI_Workshop_AST')
actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors()
for a in actors:
 if isinstance(a,unreal.POIKitBlock):
  s=a.get_editor_property('settings')
  group='Backgrounds' if s.kind==unreal.POIKitKind.BACKGROUND or s.size.z>s.size.x else 'Platforms'
  s.set_editor_property('image',choose(group));s.set_editor_property('material',material);s.set_editor_property('tint',unreal.LinearColor(1,1,1,1))
  a.set_editor_property('settings',s);a.refresh_block()
assert unreal.EditorLoadingAndSavingUtils.save_map(unreal.EditorLevelLibrary.get_editor_world(),root+'/L_POI_Workshop_AST')
for path in [root+'/DT_ShaftModules',root+'/DT_ShaftVariants']:
 table=unreal.load_asset(path)
 if not table:continue
 rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
 for row in rows:
  row['PlatformImage']=choose('Platforms').get_path_name()
  row['BackImage']=choose('Backgrounds').get_path_name()
  row['SideImage']=choose('Backgrounds').get_path_name()
  for piece in row.get('Pieces',[]):
   block=piece['Block'];size=block['Size']
   group='Backgrounds' if block.get('Kind')=='Background' or size['Z']>size['X'] else 'Platforms'
   block['Image']=choose(group).get_path_name();block['Material']=material.get_path_name()
   block['Tint']={'R':1,'G':1,'B':1,'A':1}
 assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(rows))
 ea.save_loaded_asset(table)
report={'imported':{k:[t.get_path_name() for t in v] for k,v in art.items()},'assignments':counts}
(Path(unreal.Paths.project_saved_dir())/'POICheckpoints/Art-v05.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('POI_ART_ASSIGNED '+json.dumps(counts))
