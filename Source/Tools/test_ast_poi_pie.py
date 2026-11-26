import unreal,time,json,traceback
from pathlib import Path
# Also expose the third native AST spline actor in the palette.
if not unreal.EditorAssetLibrary.does_asset_exist('/Game/POIKit/BP_AST_NarrowPassage'):
 factory=unreal.BlueprintFactory()
 factory.set_editor_property('parent_class',unreal.load_class(None,'/AST_ParkourSystem/Blueprints/Actor/NarrowPassageActor.NarrowPassageActor_C'))
 preset=unreal.AssetToolsHelpers.get_asset_tools().create_asset('BP_AST_NarrowPassage','/Game/POIKit',unreal.Blueprint,factory)
 unreal.EditorAssetLibrary.save_loaded_asset(preset)
out=Path(unreal.Paths.project_saved_dir())/'POICheckpoints'/'AST_POI_PIE.json'
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
sub=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
state={'phase':0,'start':time.monotonic(),'result':{}}
def finish(error=None):
 if error:state['result']['error']=str(error)
 out.write_text(json.dumps(state['result'],indent=2),encoding='utf8')
 unreal.log('AST_POI_PIE_RESULT '+json.dumps(state['result']))
 sub.editor_request_end_play()
 unreal.unregister_slate_post_tick_callback(handle)
 unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def tick(delta):
 try:
  if time.monotonic()-state['start']>120:finish('Timed out waiting for PIE');return
  worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
  if not worlds:return
  world=worlds[0]
  gm=unreal.GameplayStatics.get_game_mode(world)
  pc=unreal.GameplayStatics.get_player_controller(world,0)
  pawn=unreal.GameplayStatics.get_player_pawn(world,0) if pc else None
  if not pawn or time.monotonic()-state['start']<8:return
  if state['phase']==0:
   mover=pawn.get_component_by_class(unreal.MoverComponent)
   parkour_class=unreal.load_class(None,'/AST_ParkourSystem/Blueprints/Actor/ParkourActor.ParkourActor_C')
   parkour_actor=unreal.GameplayStatics.get_all_actors_of_class(world,parkour_class)[0]
   state['result']['parkour_pawn_responses']=[str(c.get_collision_response_to_channel(unreal.CollisionChannel.ECC_PAWN)) for c in parkour_actor.get_components_by_class(unreal.SplineMeshComponent)]
   state['result'].update(game_mode=gm.get_class().get_path_name(),controller=pc.get_class().get_path_name(),pawn=pawn.get_class().get_path_name(),test_mode=gm.get_editor_property('bPOITestMode'),input_producer=mover.get_editor_property('input_producer').get_class().get_name())
   source_world=unreal.load_asset('/Game/Levels/L_SirrohGameplay')
   source_mode=source_world.get_world_settings().get_editor_property('default_game_mode')
   source_defaults=unreal.get_default_object(source_mode)
   state['result']['main_menu_world_game_mode']=source_mode.get_path_name()
   assert source_defaults.get_editor_property('player_pawn_class')==pawn.get_class()
   assert source_defaults.get_editor_property('player_controller_class')==pc.get_class()
   assert state['result']['test_mode']
   assert 'PWGPlayerController' in state['result']['controller']
   assert 'PWGSidescrollerInputComponent' in state['result']['input_producer']
   assert not unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PWGv2Generator)
   beamclass=unreal.load_class(None,'/AST_ParkourSystem/Blueprints/Actor/BeamActor.BeamActor_C')
   beams=unreal.GameplayStatics.get_all_actors_of_class(world,beamclass)
   beam=beams[0]
   spline=beam.get_component_by_class(unreal.SplineComponent)
   point=spline.get_location_at_distance_along_spline(spline.get_spline_length()*0.5,unreal.SplineCoordinateSpace.WORLD)
   point.z+=110
   for start in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PlayerStart):
    start.get_editor_property('root_component').set_mobility(unreal.ComponentMobility.MOVABLE)
    start.set_actor_location(point,False,True)
    assert abs(start.get_actor_location().x-point.x)<1
   state['result']['requested_beam_spawn']=str(point)
   assert gm.respawn_player(pc)
   state['phase']=1;state['respawn_time']=time.monotonic()
  elif state['phase']==1 and time.monotonic()-state['respawn_time']>5:
   values={}
   for comp in pawn.get_components_by_class(unreal.ActorComponent):
    if 'StateManager' in comp.get_class().get_name():
     for key in ['ParkourState','MovementState']:
      try:values[key]=str(comp.get_editor_property(key))
      except Exception:pass
   state['result']['after_beam_respawn']=values
   state['result']['pawn_location']=str(pawn.get_actor_location())
   state['result']['movement_mode']=str(pawn.get_component_by_class(unreal.MoverComponent).get_movement_mode_name())
   assert 'BEAM' in values.get('ParkourState',''), 'Character did not enter Beam state' 
   state['result']['success']=True
   finish()
 except Exception:finish(traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick)
sub.editor_request_begin_play()
