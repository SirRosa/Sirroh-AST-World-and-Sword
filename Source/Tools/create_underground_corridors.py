import unreal,json,sys
from pathlib import Path
ROOT='/Game/POIKit'; BASE=ROOT+'/Underground'
ea=unreal.EditorAssetLibrary; tools=unreal.AssetToolsHelpers.get_asset_tools()
es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report={'variants':{},'maps':{},'textures':[]}
def child(name,parent):
    path=BASE+'/Actors/'+name
    bp=unreal.load_asset(path)
    if not bp:
        f=unreal.BlueprintFactory();f.set_editor_property('parent_class',parent)
        bp=tools.create_asset(name,BASE+'/Actors',unreal.Blueprint,f)
    assert bp;ea.save_loaded_asset(bp,False);return bp.generated_class()
def spawn(cls,pos,label):
    a=es.spawn_actor_from_class(cls,unreal.Vector(*pos));assert a;a.set_actor_label(label);return a
def data_asset(name,folder,cls):
    a=unreal.load_asset(folder+'/'+name)
    if not a:
        f=unreal.DataAssetFactory();f.set_editor_property('data_asset_class',cls)
        a=tools.create_asset(name,folder,cls,f)
    assert a;return a

unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
push={}
sm_editor=unreal.get_editor_subsystem(unreal.StaticMeshEditorSubsystem)
for key in ['barrel','crate','bench']:
    source='/Game/ZL_retroCaves/Meshes/decos/m_deco_cave_'+key
    mesh=unreal.load_asset(source);assert mesh
    if sm_editor.get_simple_collision_count(mesh)==0:
        path=BASE+'/Meshes/SM_Pushable_'+key.title()
        mesh=unreal.load_asset(path) or ea.duplicate_asset(source,path)
        if sm_editor.get_simple_collision_count(mesh)==0:
            assert sm_editor.add_simple_collisions(mesh,unreal.ScriptingCollisionShapeType.BOX)>=0
        ea.save_loaded_asset(mesh,False)
    bp=unreal.POICorridorTools.create_pushable_variant(BASE+'/Actors/BP_Pushable_U_'+key.title(),mesh);assert bp
    ea.save_loaded_asset(bp,False);push[key]=bp.generated_class()
    a=spawn(push[key],(0,0,0),'Inspect_'+key)
    components={c.get_name():c for c in a.get_components_by_class(unreal.SceneComponent)}
    body=components['PullPushObject'];assert body.static_mesh==mesh
    required=['FrontActor','BackActor','LeftActor','RightActor','FrontWarpPoint','BackWarpPoint','LeftWarpPoint','RightWarpPoint','ObjeFrontTrigger','ObjeBackTrigger','ObjeLeftTrigger','ObjeRightTrigger']
    assert all(n in components for n in required)
    assert sum(n.startswith('ParkourActor') for n in components)==4
    lo,hi=body.get_local_bounds()
    for name in ['FrontWarpPoint','BackWarpPoint','LeftWarpPoint','RightWarpPoint']:
        assert abs(components[name].get_world_location().z-lo.z)<.1,(key,name,components[name].get_world_location(),lo)
    metadata=json.loads(unreal.BlueprintAnalyzerLibrary.export_to_json(unreal.BlueprintAnalyzerLibrary.analyze_blueprint(bp)))['Metadata']
    assert 'PullPushObject' in metadata['ParentClass']
    report['variants'][key]={'blueprint':bp.get_path_name(),'mesh':mesh.get_path_name(),'bounds':[str(lo),str(hi)],'components':list(components),'original_parent':metadata['ParentClass']}
    es.destroy_actor(a)
door=child('BP_Door_U',unreal.load_class(None,'/AST_InteractionSystem/Actors/DoorActor.DoorActor_C'))
torch=child('BP_Torch_U',unreal.POITorch)
tasks=[]
for i,file in enumerate(sorted((Path(unreal.Paths.project_saved_dir())/'POIImports/Dirt').glob('*.png')),1):
    path=ROOT+'/Art/Dirt/T_Dirt_'+str(i).zfill(2)
    if not ea.does_asset_exist(path):
        task=unreal.AssetImportTask();task.filename=str(file);task.destination_path=ROOT+'/Art/Dirt';task.destination_name='T_Dirt_'+str(i).zfill(2);task.automated=True;task.save=True;tasks.append(task)
tools.import_asset_tasks(tasks)
textures=[unreal.load_asset(ROOT+'/Art/Dirt/T_Dirt_'+str(i).zfill(2)) for i in range(1,11)]
assert all(textures)
for t in textures:
    t.set_editor_property('srgb',True);t.set_editor_property('compression_no_alpha',False)
    t.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_DEFAULT);ea.save_loaded_asset(t,False)
pool=data_asset('BG_Dirt',ROOT+'/Pools',unreal.POIArtPool)
if not pool.images:
    pairs=[]
    for t in textures:
        p=unreal.POIImagePair();p.set_editor_property('side',t);pairs.append(p)
    pool.set_editor_property('images',pairs)
pool.bake_alpha_bounds();ea.save_loaded_asset(pool,False)
theme=data_asset('DA_Underground',ROOT+'/Themes',unreal.POIArtTheme)
material=unreal.load_asset(ROOT+'/M_KitImageLit')
if not material:
    material=ea.duplicate_asset(ROOT+'/M_KitImageFit',ROOT+'/M_KitImageLit');assert material
    me=unreal.MaterialEditingLibrary
    material.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    color=me.get_material_property_input_node(material,unreal.MaterialProperty.MP_EMISSIVE_COLOR);assert color
    assert me.connect_material_property(color,'',unreal.MaterialProperty.MP_BASE_COLOR)
    ambient=me.create_material_expression(material,unreal.MaterialExpressionScalarParameter)
    ambient.set_editor_property('parameter_name','Ambient');ambient.set_editor_property('default_value',.12)
    mul=me.create_material_expression(material,unreal.MaterialExpressionMultiply)
    assert me.connect_material_expressions(color,'',mul,'A')
    assert me.connect_material_expressions(ambient,'',mul,'B')
    assert me.connect_material_property(mul,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    me.recompile_material(material);ea.save_loaded_asset(material,False)
theme.set_editor_property('backgrounds',pool);theme.set_editor_property('platforms',unreal.load_asset(ROOT+'/Pools/PF_Planks'))
theme.set_editor_property('material',material);ea.save_loaded_asset(theme,False)
report['textures']=[t.get_path_name() for t in textures]
gm=unreal.load_class(None,ROOT+'/BP_POITest_PWG.BP_POITest_PWG_C');assert gm
def block(pos,label,size,kind=unreal.POIKitKind.SIDE_WALL):
    a=spawn(unreal.POIKitBlock,pos,label);s=unreal.POIKitSettings()
    s.set_editor_property('kind',kind);s.set_editor_property('size',unreal.Vector(*size))
    s.set_editor_property('material',material);s.set_editor_property('image',textures[0])
    s.set_editor_property('collision',unreal.POIKitCollision.SOLID)
    if kind==unreal.POIKitKind.AST:
        s.set_editor_property('collision',unreal.POIKitCollision.ONE_WAY);s.set_editor_property('bBothASTSides',True)
        s.set_editor_property('image',unreal.load_asset(ROOT+'/Art/Platforms/T_Platform_01'))
        s.set_editor_property('top_image',unreal.load_asset(ROOT+'/Art/Tops/T_Top_01'))
    a.set_editor_property('settings',s);a.refresh_block();return a
for category,count in [('Pushable_U',5),('Entrance_U',5),('Light_U',5),('Empty_U',1),('Parkour_U',1)]:
    path=BASE+'/Templates/L_'+category
    if ea.does_asset_exist(path):
        report['maps'][category]={'path':path,'preserved_existing':True};continue
    world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    world.get_world_settings().set_editor_property('default_game_mode',gm)
    floor=spawn(unreal.StaticMeshActor,((count-1)*650,0,-27),'Floor')
    floor.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
    floor.set_actor_scale3d(unreal.Vector((count*1300)/100,4,.5))
    floor.set_editor_property('tags',['POIExclude'])
    spawn(unreal.PlayerStart,(-400,0,100),'PlayerStart')
    sun=spawn(unreal.DirectionalLight,(0,0,1000),'WorkshopLight')
    sun.set_actor_rotation(unreal.Rotator(pitch=-45,yaw=-30,roll=0),False)
    sun.light_component.set_editor_property('intensity',.5 if category=='Light_U' else 2)
    for i in range(count):
        x=i*1300;prefix=category+'_'+str(i+1).zfill(2)
        bg=block((x,130,400),prefix+'_Background',(1000,1,400),unreal.POIKitKind.BACKGROUND)
        s=bg.settings;s.set_editor_property('image',textures[i%10]);bg.set_editor_property('settings',s);bg.refresh_block()
        block((x,0,0),prefix+'_Ground',(1000,250,40))
        label=spawn(unreal.TextRenderActor,(x,-160,455),prefix+'_LABEL')
        label.text_render.set_text(prefix);label.text_render.set_world_size(40)
        label.set_actor_rotation(unreal.Rotator(pitch=0,yaw=-90,roll=0),False)
        if category=='Pushable_U':
            keys=[['barrel'],['crate'],['bench'],['barrel','crate'],['crate','bench']][i]
            for j,key in enumerate(keys):
                a=spawn(push[key],(x-180+j*280,0,0),prefix+'_'+key)
                body=next(c for c in a.get_components_by_class(unreal.StaticMeshComponent) if c.get_name()=='PullPushObject')
                lo,hi=body.get_local_bounds();a.set_actor_location(unreal.Vector(x-180+j*280,0,-lo.z),False,True)
            if i in (1,3):block((x+350,0,120),prefix+'_ClimbTarget',(200,100,120),unreal.POIKitKind.AST)
        elif category=='Entrance_U':
            dx=[-250,-100,0,150,300][i]
            spawn(door,(x+dx,0,0),prefix+'_Door')
        elif category=='Light_U':
            a=spawn(torch,(x+[-300,-150,0,150,300][i],95,[100,150,190,130,170][i]),prefix+'_Torch')
            assert a.light.intensity>0 and a.light.attenuation_radius>0
        elif category=='Parkour_U':
            for j,(dx,z) in enumerate([(-300,100),(-80,200),(180,300)]):
                block((x+dx,0,z),prefix+'_Platform_'+str(j),(180,100,40),unreal.POIKitKind.AST)
    assert unreal.EditorLoadingAndSavingUtils.save_map(world,path)
    report['maps'][category]={'path':path,'variants':count}
sys.path.insert(0,str(Path(unreal.Paths.project_dir())/'Content/Python'))
import poi_authoring
ea.save_loaded_asset(poi_authoring.get_catalog('Underground'),False)
poi_authoring.install_menu()
(Path(unreal.Paths.project_saved_dir())/'POICheckpoints/Corridors-v13.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('CORRIDORS_V13_READY')
