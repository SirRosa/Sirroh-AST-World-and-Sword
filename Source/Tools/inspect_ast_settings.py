import unreal,json
from pathlib import Path
out=Path(unreal.Paths.project_saved_dir())/'POICheckpoints'/'AST_POI_Research'
gm=unreal.load_class(None,'/Game/Systems/Sprite2Bone/Biomes/BP_PWGGameMode.BP_PWGGameMode_C')
cdo=unreal.get_default_object(gm)
data={'game_mode':gm.get_path_name(),'controller':str(cdo.get_editor_property('player_controller_class')),'pawn':str(cdo.get_editor_property('player_pawn_class'))}
for path in ['/AST_ParkourSystem/Blueprints/ActorComponent/BPC_ParkourSystem','/AST_ParkourSystem/Blueprints/Actor/NarrowPassageActor']:
 bp=unreal.load_asset(path)
 (out/(bp.get_name()+'.json')).write_text(unreal.BlueprintAnalyzerLibrary.export_to_json(unreal.BlueprintAnalyzerLibrary.analyze_blueprint(bp)),encoding='utf8')
unreal.EditorLoadingAndSavingUtils.load_map('/AST_StateManager/Maps/ShowcaseMap')
es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
data['examples']=[]
for a in es.get_all_level_actors():
 if a.get_class().get_name() not in ['ParkourActor_C','BeamActor_C']:continue
 item={'label':a.get_actor_label(),'class':a.get_class().get_name(),'properties':{},'components':[]}
 for prop in ['SplineLength','LedgeType','LedgeDepth','ClimbStyle','Length','Height']:
  try:item['properties'][prop]=str(a.get_editor_property(prop))
  except Exception:pass
 for c in a.get_components_by_class(unreal.PrimitiveComponent):
  item['components'].append({'name':c.get_name(),'class':c.get_class().get_name(),'profile':str(c.get_collision_profile_name()),'collision':str(c.get_collision_enabled()),'object_type':str(c.get_collision_object_type())})
 data['examples'].append(item)
(out/'settings.json').write_text(json.dumps(data,indent=2),encoding='utf8')
unreal.log('AST_SETTINGS_DONE')
