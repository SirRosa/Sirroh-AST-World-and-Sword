import unreal,json
from pathlib import Path
out=Path(unreal.Paths.project_saved_dir())/'POICheckpoints'/'AST_POI_Research'
out.mkdir(parents=True,exist_ok=True)
for path in ['/AST_ParkourSystem/Blueprints/Actor/BeamActor','/AST_ParkourSystem/Blueprints/Actor/ParkourActor','/Game/Systems/Sprite2Bone/Biomes/BP_PWGGameMode']:
 bp=unreal.load_asset(path)
 if bp:
  result=unreal.BlueprintAnalyzerLibrary.analyze_blueprint(bp)
  (out/(bp.get_name()+'.json')).write_text(unreal.BlueprintAnalyzerLibrary.export_to_json(result),encoding='utf8')
world=unreal.EditorLoadingAndSavingUtils.load_map('/AST_StateManager/Maps/ShowcaseMap')
es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors=[]
for a in es.get_all_level_actors():
 if any(s in a.get_class().get_name() for s in ['Parkour','Beam','Ladder','Rope']):
  actors.append({'label':a.get_actor_label(),'class':a.get_class().get_path_name(),'transform':str(a.get_actor_transform()),'splines':[{'name':s.get_name(),'points':[str(s.get_location_at_spline_point(i,unreal.SplineCoordinateSpace.LOCAL)) for i in range(s.get_number_of_spline_points())]} for s in a.get_components_by_class(unreal.SplineComponent)]})
(out/'showcase.json').write_text(json.dumps(actors,indent=2),encoding='utf8')
unreal.log('AST_POI_RESEARCH_DONE')
