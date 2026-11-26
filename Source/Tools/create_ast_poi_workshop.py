import unreal,json
from pathlib import Path
ROOT='/Game/POIKit'
ea=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
def cls(path): return unreal.load_class(None,path+'.'+path.rsplit('/',1)[-1]+'_C')
def spawn(c,pos,label):
 a=es.spawn_actor_from_class(c,unreal.Vector(*pos));a.set_actor_label(label);return a
def child(name,parent):
 existing=unreal.load_asset(ROOT+'/'+name)
 if existing:return existing
 f=unreal.BlueprintFactory();f.set_editor_property('parent_class',parent)
 return tools.create_asset(name,ROOT,unreal.Blueprint,f)
gm=cls('/Game/Systems/Sprite2Bone/Biomes/BP_PWGGameMode')
bp=child('BP_POITest_PWG',gm)
cdo=unreal.get_default_object(bp.generated_class())
cdo.set_editor_property('bPOITestMode',True)
cdo.set_editor_property('bStartAutomatically',True)
cdo.set_editor_property('bSpawnCombatTrainingNPC',False)
cdo.set_editor_property('bSpawnASTIntegrationTestObjects',False)
ea.save_loaded_asset(bp)
parkour=cls('/AST_ParkourSystem/Blueprints/Actor/ParkourActor')
beam=cls('/AST_ParkourSystem/Blueprints/Actor/BeamActor')
ledge_type=type(unreal.get_default_object(parkour).get_editor_property('LedgeType'))
for name,typ in [('BP_AST_Ledge',ledge_type.LEDGE),('BP_AST_Mantle',ledge_type.MANTLE),('BP_AST_Vault',ledge_type.VAULT)]:
 b=child(name,parkour);d=unreal.get_default_object(b.generated_class());d.set_editor_property('LedgeType',typ);ea.save_loaded_asset(b)
ea.save_loaded_asset(child('BP_AST_BalanceBeam',beam))
ea.save_loaded_asset(child('BP_AST_NarrowPassage',cls('/AST_ParkourSystem/Blueprints/Actor/NarrowPassageActor')))
# Preserve the showcase geometry and original actor settings, with the real PWG game mode parent.
if not ea.does_asset_exist(ROOT+'/L_AST_Showcase_PWG'):
 unreal.EditorLoadingAndSavingUtils.load_map('/AST_StateManager/Maps/ShowcaseMap')
 world=unreal.EditorLevelLibrary.get_editor_world()
 world.get_world_settings().set_editor_property('default_game_mode',bp.generated_class())
 assert unreal.EditorLoadingAndSavingUtils.save_map(world,ROOT+'/L_AST_Showcase_PWG')
if not ea.does_asset_exist(ROOT+'/DT_ShaftModules'):
 f=unreal.DataTableFactory();f.set_editor_property('struct',unreal.POIShaftRow.static_struct())
 table=tools.create_asset('DT_ShaftModules',ROOT,unreal.DataTable,f)
else:table=unreal.load_asset(ROOT+'/DT_ShaftModules')
if not ea.does_asset_exist(ROOT+'/L_POI_Workshop_AST'):
 world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
 world.get_world_settings().set_editor_property('default_game_mode',bp.generated_class())
 floor=spawn(unreal.StaticMeshActor,(2600,0,-25),'Floor')
 floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
 floor.set_actor_scale3d(unreal.Vector(95,8,.5))
 spawn(unreal.PlayerStart,(-500,0,100),'PlayerStart')
 sun=spawn(unreal.DirectionalLight,(0,0,2000),'Sun');sun.set_actor_rotation(unreal.Rotator(pitch=-45,yaw=-30,roll=0),False)
 spawn(unreal.SkyLight,(0,0,1800),'SkyLight')
 modules=[]
 for i,(name,role) in enumerate([('ENTRANCE_01',unreal.POIShaftRole.ENTRANCE),('MIDDLE_01',unreal.POIShaftRole.MIDDLE),('MIDDLE_02',unreal.POIShaftRole.MIDDLE),('MIDDLE_03',unreal.POIShaftRole.MIDDLE),('EXIT_01',unreal.POIShaftRole.EXIT)]):
  x=i*1000
  a=spawn(unreal.POIShaft,(x,0,0),name)
  a.set_editor_property('height',300)
  r=unreal.POIShaftRow();r.set_editor_property('role',role)
  a.set_editor_property('recipe',r)
  a.set_editor_property('target_table',table)
  a.set_editor_property('row_name',name+'_Layer0')
  # Physical platform and original editable AST edge are separate actors.
  for j,z in enumerate([150,300]):
   left=j==0
   offset=(-180 if left else 180)
   if i==2:offset=-offset
   if i==3 and j==0:z=100
   block=spawn(unreal.POIKitBlock,(x+offset,0,z),name+'_Platform'+str(j))
   settings=unreal.POIKitSettings();settings.set_editor_property('kind',unreal.POIKitKind.SIDE_WALL);settings.set_editor_property('collision',unreal.POIKitCollision.ONE_WAY);settings.set_editor_property('size',unreal.Vector(240,100,40))
   block.set_editor_property('settings',settings);block.refresh_block()
   block.attach_to_actor(a,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
   edge=spawn(parkour,(x+offset+(120 if offset<0 else -120),50 if offset<0 else -50,z),name+'_OriginalASTEdge'+str(j))
   edge.set_actor_rotation(unreal.Rotator(pitch=0,yaw=-90 if offset<0 else 90,roll=0),False)
   edge.set_editor_property('LedgeType',ledge_type.MANTLE)
   edge.attach_to_actor(a,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
  # Visible role-specific decoration without obstructing the route.
  decoration=spawn(unreal.POIKitBlock,(x,45,300),name+'_Background')
  settings=unreal.POIKitSettings();settings.set_editor_property('kind',unreal.POIKitKind.BACKGROUND);settings.set_editor_property('material',unreal.load_asset(ROOT+'/M_KitImage'));settings.set_editor_property('tint',unreal.LinearColor(0.15,0.2,0.25,1));settings.set_editor_property('size',unreal.Vector(600,1,300))
  decoration.set_editor_property('settings',settings);decoration.refresh_block()
  decoration.attach_to_actor(a,'',unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,unreal.AttachmentRule.KEEP_WORLD,False)
  # Background extends behind the playable Y range, but its actor stays within the section.
  for layer in range(4):
   r.set_editor_property('upper_layer',layer);a.set_editor_property('recipe',r);a.set_editor_property('row_name',name+'_Layer'+str(layer));a.call_method('ExportSection')
   assert 'exported' in a.get_editor_property('status'),a.get_editor_property('status')
  r.set_editor_property('upper_layer',0);a.set_editor_property('recipe',r);a.set_editor_property('row_name',name+'_Layer0')
  text=spawn(unreal.TextRenderActor,(x,-110,400),name+'_LABEL')
  text.set_actor_rotation(unreal.Rotator(pitch=0,yaw=-90,roll=0),False)
  text.text_render.set_text(name)
  text.text_render.set_world_size(50)
  modules.append(a)
 preview=spawn(unreal.POIShaft,(6000,0,0),'PREVIEW_Entrance_RandomMiddles_Exit')
 preview.set_editor_property('height',1500);preview.set_editor_property('module_table',table);preview.set_editor_property('bAssembleModules',True);preview.set_editor_property('bBuildBoundaryWalls',False);preview.build_shaft()
 assert len(preview.get_editor_property('built_roles'))==5,preview.get_editor_property('status')
 # Dedicated editable original beam at ground level, reached from the floor.
 spawn(beam,(-1200,0,10),'ORIGINAL_BeamActor_EditSpline')
 ea.save_loaded_asset(table)
 assert unreal.EditorLoadingAndSavingUtils.save_map(world,ROOT+'/L_POI_Workshop_AST')
unreal.log('AST_POI_WORKSHOP_READY')
