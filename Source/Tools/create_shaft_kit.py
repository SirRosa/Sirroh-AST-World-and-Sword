"""Create /Game/POIKit presets, four transition rows and an actual-character workshop.
Run only through Unreal PythonScriptPlugin. Existing assets are preserved.
"""
import unreal,json
ROOT='/Game/POIKit'
tools=unreal.AssetToolsHelpers.get_asset_tools()
editor=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
unreal.EditorAssetLibrary.make_directory(ROOT)
def setp(obj,key,value): obj.set_editor_property(key,value)
def load(path): return unreal.load_asset(path)
def exists(path): return unreal.EditorAssetLibrary.does_asset_exist(path)
def spawn(cls,pos,label):
    a=editor.spawn_actor_from_class(cls,unreal.Vector(*pos));a.set_actor_label(label);return a

mat=load(ROOT+'/M_KitImage') if exists(ROOT+'/M_KitImage') else None
if not mat:
    mat=tools.create_asset('M_KitImage',ROOT,unreal.Material,unreal.MaterialFactoryNew())
    setp(mat,'blend_mode',unreal.BlendMode.BLEND_MASKED);setp(mat,'two_sided',True)
    setp(mat,'shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    sample=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionTextureSampleParameter2D,-500,0)
    setp(sample,'parameter_name','ImageTexture');setp(sample,'texture',load('/Engine/EngineResources/DefaultTexture'))
    tint=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionVectorParameter,-500,200)
    setp(tint,'parameter_name','Tint');setp(tint,'default_value',unreal.LinearColor(1,1,1,1))
    multiply=unreal.MaterialEditingLibrary.create_material_expression(mat,unreal.MaterialExpressionMultiply,-200,0)
    unreal.MaterialEditingLibrary.connect_material_expressions(sample,'RGB',multiply,'A')
    unreal.MaterialEditingLibrary.connect_material_expressions(tint,'RGB',multiply,'B')
    unreal.MaterialEditingLibrary.connect_material_property(multiply,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(sample,'A',unreal.MaterialProperty.MP_OPACITY_MASK)
    unreal.MaterialEditingLibrary.recompile_material(mat);unreal.EditorAssetLibrary.save_loaded_asset(mat)

presets=[('GASP_Solid','GASP','SOLID'),('GASP_NoCollision','GASP','NO_PHYSICAL_COLLISION'),('GASP_OneWay','GASP','ONE_WAY'),
         ('AST_Solid','AST','SOLID'),('AST_NoCollision','AST','NO_PHYSICAL_COLLISION'),('AST_OneWay','AST','ONE_WAY'),
         ('Both_OneWay','BOTH','ONE_WAY'),('SideWall','SIDE_WALL','SOLID'),('BackWall','BACKGROUND','NO_PHYSICAL_COLLISION')]
for name,kind,collision in presets:
    if exists(ROOT+'/BP_'+name): continue
    factory=unreal.BlueprintFactory();setp(factory,'parent_class',unreal.POIKitBlock)
    bp=tools.create_asset('BP_'+name,ROOT,unreal.Blueprint,factory)
    cdo=unreal.get_default_object(bp.generated_class());s=unreal.POIKitSettings()
    setp(s,'kind',getattr(unreal.POIKitKind,kind));setp(s,'collision',getattr(unreal.POIKitCollision,collision));setp(s,'material',mat)
    if name=='BackWall':setp(s,'size',unreal.Vector(400,1,400))
    if name=='SideWall':setp(s,'size',unreal.Vector(50,100,400))
    setp(cdo,'settings',s);unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp)

table=load(ROOT+'/DT_ShaftVariants') if exists(ROOT+'/DT_ShaftVariants') else None
if not table:
    factory=unreal.DataTableFactory();setp(factory,'struct',unreal.load_object(None,'/Script/Sirroh_Reimagineg_v3.POIShaftRow'))
    table=tools.create_asset('DT_ShaftVariants',ROOT,unreal.DataTable,factory)
    rows=[{'Name':name,'UpperLayer':i,'Weight':1,'Width':600,'StepRise':150,'Climbing':'Both','Collision':'OneWay','SectionHeight':300}
          for i,name in enumerate(['Surface_Underground','Underground_Caves','Caves_Dungeon','Dungeon_Catacombs'])]
    assert unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,json.dumps(rows))
    unreal.EditorAssetLibrary.save_loaded_asset(table)

if not exists(ROOT+'/L_ShaftWorkshop'):
    world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    cube=load('/Engine/BasicShapes/Cube')
    floor=spawn(unreal.StaticMeshActor,(1800,0,-20),'WorkshopFloor')
    floor.static_mesh_component.set_static_mesh(cube);floor.set_actor_scale3d(unreal.Vector(65,8,.4))
    for i,name in enumerate(['Surface_Underground','Underground_Caves','Caves_Dungeon','Dungeon_Catacombs']):
        shaft=spawn(unreal.POIShaft,(i*1200,0,0),name)
        recipe=unreal.POIShaftRow();setp(recipe,'upper_layer',i)
        setp(shaft,'recipe',recipe);setp(shaft,'height',900);setp(shaft,'target_table',table);setp(shaft,'row_name',name)
        shaft.build_shaft()
    # Independent 300cm section to edit and export. Its table row does not overwrite the standard four.
    author=spawn(unreal.POIShaft,(-1200,0,0),'EDIT_ME_300cm_Section')
    setp(author,'height',300);setp(author,'target_table',table);setp(author,'row_name','MyCustomSurfaceShaft');author.build_shaft()
    spawn(unreal.PlayerStart,(-550,0,100),'PlayerStart')
    sun=spawn(unreal.DirectionalLight,(0,0,1800),'Sun');sun.set_actor_rotation(unreal.Rotator(-45,-30,0),False)
    settings=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.WorldSettings)[0]
    setp(settings,'default_game_mode',unreal.POIWorkshopGameMode)
    assert unreal.EditorLoadingAndSavingUtils.save_map(world,ROOT+'/L_ShaftWorkshop')

# A clean PWG entry map using the existing configured game mode. New worlds pick DT_ShaftVariants.
if not exists(ROOT+'/L_PWG_Shafts'):
    world=unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    settings=unreal.GameplayStatics.get_all_actors_of_class(world,unreal.WorldSettings)[0]
    gm=unreal.load_class(None,'/Game/Systems/Sprite2Bone/Biomes/BP_PWGGameMode.BP_PWGGameMode_C')
    # Isolated child preset to avoid modifying the user's normal game mode or save slot.
    factory=unreal.BlueprintFactory();setp(factory,'parent_class',gm)
    bp=load(ROOT+'/BP_ShaftWorldGameMode') if exists(ROOT+'/BP_ShaftWorldGameMode') else tools.create_asset('BP_ShaftWorldGameMode',ROOT,unreal.Blueprint,factory)
    cdo=unreal.get_default_object(bp.generated_class());setp(cdo,'PWGv2WorldName','ShaftKit_FirstWorld');setp(cdo,'bStartAutomatically',True)
    setp(cdo,'bSpawnCombatTrainingNPC',False);setp(cdo,'bSpawnASTIntegrationTestObjects',False)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp);unreal.EditorAssetLibrary.save_loaded_asset(bp)
    setp(settings,'default_game_mode',bp.generated_class())
    assert unreal.EditorLoadingAndSavingUtils.save_map(world,ROOT+'/L_PWG_Shafts')
unreal.log('SHAFT_KIT_ASSETS_SUCCESS')
