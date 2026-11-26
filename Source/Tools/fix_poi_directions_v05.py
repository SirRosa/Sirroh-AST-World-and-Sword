import unreal,json,math
from pathlib import Path
root='/Game/POIKit'
out=Path(unreal.Paths.project_saved_dir())/'POICheckpoints'
report={'actors':[],'rows':0}
world=unreal.EditorLoadingAndSavingUtils.load_map(root+'/L_POI_Workshop_AST')
parkour=unreal.load_class(None,'/AST_ParkourSystem/Blueprints/Actor/ParkourActor.ParkourActor_C')
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 if isinstance(a,unreal.POIKitBlock):a.refresh_block()
 if a.get_class()!=parkour:continue
 parent=a.get_attach_parent_actor()
 if not isinstance(parent,unreal.POIShaft):continue
 p=a.get_actor_location(); origin=parent.get_actor_location(); x=p.x-origin.x
 if abs(abs(x)-60)>1:continue
 yaw=-90 if x<0 else 90
 p.y=origin.y+(50 if x<0 else -50)
 a.set_actor_location(p,False,True)
 a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=yaw,roll=0),False)
 report['actors'].append(a.get_actor_label())
assert unreal.EditorLoadingAndSavingUtils.save_map(world,root+'/L_POI_Workshop_AST')
table=unreal.load_asset(root+'/DT_ShaftModules')
rows=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
(out/'Modules-before-direction-v05.json').write_text(json.dumps(rows,indent=2),encoding='utf8')
for row in rows:
 for piece in row.get('NativePieces',[]):
  t=piece['Transform']; x=t['Translation']['X']
  if abs(abs(x)-60)>1:continue
  t['Translation']['Y']=50 if x<0 else -50
  t['Rotation']={'X':0,'Y':0,'Z':(-1 if x<0 else 1)*math.sqrt(.5),'W':math.sqrt(.5)}
  report['rows']+=1
assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(rows))
assert unreal.EditorAssetLibrary.save_loaded_asset(table)
(out/'Direction-v05.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('POI_DIRECTION_FIXED '+json.dumps(report))

