import unreal
path='/Game/POIKit/L_POI_Test_Pendulum'
if unreal.EditorAssetLibrary.does_asset_exist(path):
 unreal.log('V26_TEST_MAP_EXISTS_PRESERVED')
else:
 levels=unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
 assert levels.new_level(path)
 actors=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
 world=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
 world.get_world_settings().set_editor_property('default_game_mode',unreal.POIWorkshopGameMode)
 floor=actors.spawn_actor_from_class(unreal.StaticMeshActor,unreal.Vector(0,0,-25))
 floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
 floor.set_actor_scale3d(unreal.Vector(40,10,.5));floor.set_actor_label('Test_Floor')
 actors.spawn_actor_from_class(unreal.PlayerStart,unreal.Vector(-1200,0,120))
 actors.spawn_actor_from_class(unreal.DirectionalLight,unreal.Vector(0,0,800),unreal.Rotator(-45,25,0))
 actors.spawn_actor_from_class(unreal.SkyLight,unreal.Vector(0,0,800))
 for i,key in enumerate(['Bench','Barrel','Crate']):
  cls=unreal.load_class(None,'/Game/POIKit/Underground/Actors/BP_Pushable_U_'+key+'.BP_Pushable_U_'+key+'_C')
  a=actors.spawn_actor_from_class(cls,unreal.Vector(-800+i*350,0,200))
  body=a.root_component;box=body.static_mesh.get_bounding_box()
  a.set_actor_location(unreal.Vector(-800+i*350,0,-box.min.z*body.get_world_scale().z+3),False,False)
  a.set_actor_label('Contact_Test_'+key)
 cls=unreal.load_class(None,'/Game/POIKit/Underground/Actors/BP_Pendulum_Test.BP_Pendulum_Test_C')
 a=actors.spawn_actor_from_class(cls,unreal.Vector(600,0,550));a.set_actor_label('Continuous_Pendulum_Test')
 assert levels.save_current_level()
 unreal.log('V26_TEST_MAP_READY')
