import unreal,time,json,traceback
from pathlib import Path
out=Path(unreal.Paths.project_saved_dir())/'POICheckpoints/Parkour-hang-v05.json'
unreal.EditorPythonScripting.set_keep_python_script_alive(True)
sub=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
state={'phase':0,'clock':time.monotonic(),'results':[]}
def finish(error=None):
 if error:state['error']=error
 out.write_text(json.dumps(state,indent=2),encoding='utf8')
 sub.editor_request_end_play();unreal.unregister_slate_post_tick_callback(handle)
 unreal.EditorPythonScripting.set_keep_python_script_alive(False)
def snap(pawn,component):
 return {'location':str(pawn.get_actor_location()),'state':str(component.get_editor_property('Parkour State')),'doing':str(component.get_editor_property('DoingTraversalAction')),'result':component.get_editor_property('Parkour Result').export_text(),'mode':str(pawn.get_component_by_class(unreal.MoverComponent).get_movement_mode_name())}
def tick(dt):
 try:
  worlds=unreal.EditorLevelLibrary.get_pie_worlds(False)
  if not worlds:return
  world=worlds[0];pc=unreal.GameplayStatics.get_player_controller(world,0);pawn=unreal.GameplayStatics.get_player_pawn(world,0)
  if not pawn or time.monotonic()-state['clock']<4:return
  component=next(c for c in pawn.get_components_by_class(unreal.ActorComponent) if c.get_class().get_name().startswith('BPC_ParkourSystem'))
  if state['phase']==0:
   for a in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.Actor):
    pos=a.get_actor_location()
    if (abs(pos.x+180)<1 or abs(pos.x+60)<1) and abs(pos.z-150)<1:a.destroy_actor();continue
    if (abs(pos.x-180)<1 or abs(pos.x-60)<1) and abs(pos.z-300)<1:
     a.get_editor_property('root_component').set_mobility(unreal.ComponentMobility.MOVABLE)
     pos.z=220;a.set_actor_location(pos,False,True)
     if isinstance(a,unreal.POIKitBlock):
      s=a.get_editor_property('settings');s.set_editor_property('collision',unreal.POIKitCollision.SOLID);s.set_editor_property('size',unreal.Vector(240,100,220));a.set_editor_property('settings',s);a.refresh_block()
     elif a.get_class().get_name()=='ParkourActor_C':
      enum=type(a.get_editor_property('LedgeType'));a.set_editor_property('LedgeType',enum.LEDGE)
   for start in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.PlayerStart):
    start.get_editor_property('root_component').set_mobility(unreal.ComponentMobility.MOVABLE);start.set_actor_location(unreal.Vector(0,0,100),False,True)
   assert unreal.GameplayStatics.get_game_mode(world).respawn_player(pc)
  elif state['phase']==1:
   state['results'].append(snap(pawn,component));pc.sirroh_parkour(False)
  elif state['phase']==2:
   state['results'].append(snap(pawn,component));pc.sirroh_parkour(True)
  elif state['phase']==3:
   state['results'].append(snap(pawn,component));finish();return
  state['phase']+=1;state['clock']=time.monotonic()
 except Exception:finish(traceback.format_exc())
handle=unreal.register_slate_post_tick_callback(tick);sub.editor_request_begin_play()
