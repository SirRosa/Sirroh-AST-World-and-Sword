import unreal,time,json,traceback
exec(compile(open(unreal.Paths.project_dir()+"Source/Tools/fix_poi_directions_v05.py",encoding="utf-8-sig").read(),"fix_poi_directions_v05.py","exec"))
from pathlib import Path
out=Path(unreal.Paths.project_saved_dir())/'POICheckpoints'/'Parkour-controller-v05'
out.mkdir(parents=True,exist_ok=True)
for path in ['/AST_ParkourSystem/Blueprints/Actor/ParkourActor','/AST_ParkourSystem/Blueprints/ActorComponent/BPC_ParkourSystem','/Game/SirrohIntegration/BP_SirrohCharacter','/AST_StateManager/Blueprints/ActorComponent/BPC_StateManager']:
 bp=unreal.load_asset(path)
 (out/(bp.get_name()+'.json')).write_text(unreal.BlueprintAnalyzerLibrary.export_to_json(unreal.BlueprintAnalyzerLibrary.analyze_blueprint(bp)),encoding='utf8')
for name in ['SandboxCharacter_MoverPlayer_Ragdoll','SandboxCharacter_MoverPlayer','SandboxCharacter_MoverBase']:
 bp=unreal.load_asset('/AST_StateManager/Blueprints/Pawn/'+name)
 (out/(name+'.json')).write_text(unreal.BlueprintAnalyzerLibrary.export_to_json(unreal.BlueprintAnalyzerLibrary.analyze_blueprint(bp)),encoding='utf8')
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
sub=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
state={'phase':0,'clock':time.monotonic(),'results':[]}
def finish(error=None):
 if error:state['error']=str(error)
 (out/'probe.json').write_text(json.dumps(state,indent=2),encoding='utf8')
 unreal.log('PARKOUR_PROBE '+json.dumps(state))
 sub.editor_request_end_play()
 unreal.unregister_slate_post_tick_callback(handle)
 unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def snapshot(pawn,component):
 result={'location':str(pawn.get_actor_location()),'rotation':str(pawn.get_actor_rotation())}
 for prop in ['Parkour Result','DoingTraversalAction','Parkour State','CanAirClimb']:
  try:
   value=component.get_editor_property(prop)
   result[prop]=value.export_text() if isinstance(value,unreal.StructBase) else str(value)
  except Exception as e:result[prop]=str(e)
 movement=pawn.get_component_by_class(unreal.MoverComponent)
 result['mode']=str(movement.get_movement_mode_name())
 floor=movement.try_get_floor_check_hit_result()
 result['floor']=floor.export_text() if isinstance(floor,unreal.StructBase) else str(floor)
 return result
def tick(dt):
 try:
  worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
  if not worlds:return
  world=worlds[0];pc=unreal.GameplayStatics.get_player_controller(world,0)
  pawn=unreal.GameplayStatics.get_player_pawn(world,0)
  if not pawn or time.monotonic()-state['clock']<3:return
  component=next(c for c in pawn.get_components_by_class(unreal.ActorComponent) if c.get_class().get_name().startswith('BPC_ParkourSystem'))
  if state['phase'] in [0,3]:
   if state['phase']==0:
    for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor):
     pos=a.get_actor_location()
     if (abs(pos.x+180)<1 or abs(pos.x+60)<1) and abs(pos.z-150)<1:
      a.destroy_actor();continue
     if (abs(pos.x-180)<1 or abs(pos.x-60)<1) and abs(pos.z-300)<1:
      a.get_editor_property('root_component').set_mobility(unreal.ComponentMobility.MOVABLE)
      pos.z=150;a.set_actor_location(pos,False,True)
   if state['phase']==3:
    parkour=unreal.load_class(None,'/AST_ParkourSystem/Blueprints/Actor/ParkourActor.ParkourActor_C')
    for a in unreal.GameplayStatics.get_all_actors_of_class(world,parkour):
     pos=a.get_actor_location()
     if abs(pos.x-60)<1 and abs(pos.z-150)<1:
      a.get_editor_property('root_component').set_mobility(unreal.ComponentMobility.MOVABLE)
      pos.y=-pos.y;a.set_actor_location(pos,False,True)
      a.set_actor_rotation(unreal.Rotator(pitch=0,yaw=90,roll=0),False)
   for start in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PlayerStart):
    start.get_editor_property('root_component').set_mobility(unreal.ComponentMobility.MOVABLE)
    start.set_actor_location(unreal.Vector(0,0,100),False,True)
    start.set_actor_rotation(unreal.Rotator(pitch=0,yaw=0,roll=0),False)
   assert unreal.GameplayStatics.get_game_mode(world).respawn_player(pc)
   state['phase']+=1;state['clock']=time.monotonic()
  elif state['phase'] in [1,4]:
   result=snapshot(pawn,component)
   for name,args in [('Check Ground Jump Surface Exist',(unreal.Vector(60,0,150),unreal.Vector(1,0,0))),('Check Block Path',(unreal.Vector(60,0,150),))]:
    try:result[name]=str(component.call_method(name,args=args))
    except Exception as e:result[name]=str(e)
   for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor):
    pos=a.get_actor_location()
    if abs(pos.x-60)<1 and abs(pos.z-150)<1:
     try:result['direction']=str(a.call_method('CheckLedgeDirection',args=(unreal.Vector(60,0,150),unreal.Vector(1,0,0))))
     except Exception as e:result['direction']=str(e)
   result['find']=str(component.call_method('Find Parkour Location'))
   pc.sirroh_parkour(False)
   result['controller_action']='SirrohParkour'
   result['after_action']=snapshot(pawn,component)
   state['results'].append(result);state['phase']+=1;state['clock']=time.monotonic()
  elif state['phase'] in [2,5]:
   state['results'].append(snapshot(pawn,component))
   if state['phase']==2:
    assert pawn.get_actor_location().x>80 and pawn.get_actor_location().z>220, 'Controller mantle did not reach platform'
    state['passed']=True;finish();return
   state['phase']+=1;state['clock']=time.monotonic()
 except Exception:finish(traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick)
sub.editor_request_begin_play()
