"""Run with UE -run=pythonscript -script=<absolute path> -EnablePlugins=PythonScriptPlugin.
Creates isolated /Game/POIPrototype assets. Never overwrites existing assets.
"""
import unreal
import json
import math
from pathlib import Path

ROOT = '/Game/POIPrototype'
tools = unreal.AssetToolsHelpers.get_asset_tools()
editor = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

def load(path):
    return unreal.EditorAssetLibrary.load_asset(path)

def prop(actor, name, value):
    actor.set_editor_property(name, value)

def spawn(cls, xyz, label):
    actor = editor.spawn_actor_from_class(cls, unreal.Vector(*xyz))
    actor.set_actor_label(label)
    return actor

unreal.EditorAssetLibrary.make_directory(ROOT + '/Modules')
material = load(ROOT + '/M_POIImage')
if not material:
    material = tools.create_asset('M_POIImage', ROOT, unreal.Material, unreal.MaterialFactoryNew())
    prop(material, 'blend_mode', unreal.BlendMode.BLEND_MASKED)
    prop(material, 'two_sided', True)
    sample = unreal.MaterialEditingLibrary.create_material_expression(material, unreal.MaterialExpressionTextureSampleParameter2D, -300, 0)
    prop(sample, 'parameter_name', 'ImageTexture')
    prop(sample, 'texture', load('/Engine/EngineResources/DefaultTexture'))
    unreal.MaterialEditingLibrary.connect_material_property(sample, 'RGB', unreal.MaterialProperty.MP_BASE_COLOR)
    unreal.MaterialEditingLibrary.connect_material_property(sample, 'A', unreal.MaterialProperty.MP_OPACITY_MASK)
    unreal.MaterialEditingLibrary.recompile_material(material)
    unreal.EditorAssetLibrary.save_loaded_asset(material)

catalog = load(ROOT + '/DT_POIExamples')
new_catalog = catalog is None
if new_catalog:
    factory = unreal.DataTableFactory()
    prop(factory, 'struct', unreal.load_object(None, '/Script/Sirroh_Reimagineg_v3.POIModuleRow'))
    catalog = tools.create_asset('DT_POIExamples', ROOT, unreal.DataTable, factory)

cube = load('/Engine/BasicShapes/Cube')
rows = []
for name, width, height, entry, exit_floor, weight in [
    ('Corridor', 2, 1, 0, 0, 3), ('Up', 3, 2, 0, 1, 2), ('Down', 3, 2, 1, 0, 2)
]:
    path = ROOT + '/Modules/L_POI_' + name
    row = {'Name': name + '_Demo', 'ModuleId': name + '_Demo', 'Environment': 'Dungeon',
           'Category': 'Corridor' if name == 'Corridor' else 'Stairs', 'WidthCells': width,
           'HeightCells': height, 'EntryFloor': entry, 'ExitFloor': exit_floor, 'Weight': weight,
           'CellWidth': 400, 'FloorHeight': 300, 'LaneWidth': 300, 'EntryType': 'Walk', 'ExitType': 'Walk',
           'MinimumBaseFloor': 0, 'Level': path + '.L_POI_' + name}
    rows.append(row)
    if unreal.EditorAssetLibrary.does_asset_exist(path):
        unreal.log('POI: preserving existing map ' + path)
        continue
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    length = width * 400
    rise = (exit_floor-entry) * 300
    steps = max(1, math.ceil(abs(rise)/15))
    for i in range(steps):
        alpha = 0 if steps == 1 else i/(steps-1)
        floor = spawn(unreal.StaticMeshActor, ((i+0.5)*length/steps, 0, entry*300+alpha*rise-10), 'Floor_%02d' % i)
        floor.static_mesh_component.set_static_mesh(cube)
        floor.set_actor_scale3d(unreal.Vector(length/steps/100, 3, 0.2))
    author = spawn(unreal.POIModuleAuthor, (0,0,0), 'ModuleDefinition')
    definition = unreal.POIModuleRow()
    for field, value in [('module_id', row['ModuleId']), ('width_cells', width), ('height_cells',height),
                         ('entry_floor',entry), ('exit_floor',exit_floor), ('weight',weight)]:
        prop(definition, field, value)
    prop(author, 'definition', definition)
    prop(author, 'target_catalog', catalog)
    if name == 'Corridor':
        door = spawn(unreal.POIInteractiveProp, (620,0,100), 'Door_1')
        prop(door, 'collision_half_extent', unreal.Vector(20,140,100))
        prop(door, 'visual_transform', unreal.Transform(scale=unreal.Vector(.4,2.8,2)))
        prop(door.persistent_state, 'local_id', 'Door_1')
        lever = spawn(unreal.POIInteractiveProp, (380,-90,40), 'Lever_1')
        prop(lever, 'kind', unreal.POIInteractionKind.LEVER)
        prop(lever, 'targets', [door])
        prop(lever, 'collision_half_extent', unreal.Vector(20,20,40))
        prop(lever, 'visual_transform', unreal.Transform(scale=unreal.Vector(.4,.4,.8)))
        prop(lever.persistent_state, 'local_id', 'Lever_1')
        chest = spawn(unreal.POIInteractiveProp, (160,100,35), 'Chest_1')
        prop(chest, 'kind', unreal.POIInteractionKind.CONTAINER)
        prop(chest, 'initial_loot', ['Torch','Brick'])
        prop(chest, 'collision_half_extent', unreal.Vector(35,25,35))
        prop(chest, 'visual_transform', unreal.Transform(scale=unreal.Vector(.7,.5,.7)))
        prop(chest.persistent_state, 'local_id', 'Chest_1')
        lamp = spawn(unreal.POIProp, (400,120,200), 'Light_Source_Template')
        prop(lamp, 'bLightEnabled', True)
        prop(lamp, 'collision_mode', unreal.POIPropCollision.DECORATION)
        prop(lamp, 'visual_transform', unreal.Transform(scale=unreal.Vector(.2,.2,.2)))
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, path):
        raise RuntimeError('Could not save ' + path)
    definition = author.get_editor_property('definition')
    prop(definition, 'level', world)
    prop(author, 'definition', definition)
    unreal.EditorLoadingAndSavingUtils.save_map(world, path)

if new_catalog:
    if not unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(catalog, json.dumps(rows)):
        raise RuntimeError('Could not fill POI catalog')
    unreal.EditorAssetLibrary.save_loaded_asset(catalog)

demo_path = ROOT + '/L_POI_Demo'
if not unreal.EditorAssetLibrary.does_asset_exist(demo_path):
    world = unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
    generator = spawn(unreal.POIGenerator, (0,0,0), 'POI_Generator_E_Interact_F5_Save_F9_Load')
    prop(generator, 'bUseExampleCatalog', False)
    prop(generator, 'catalog', catalog)
    prop(generator, 'bEnableDemoInput', True)
    prop(generator, 'layout_slot', 'POI_Demo_Layout')
    generator.generate_preview()
    spawn(unreal.PlayerStart, (80,0,120), 'PlayerStart')
    light = spawn(unreal.DirectionalLight, (0,0,1000), 'Sun')
    light.set_actor_rotation(unreal.Rotator(-45,-30,0), False)
    settings = unreal.GameplayStatics.get_all_actors_of_class(world, unreal.WorldSettings)[0]
    prop(settings, 'default_game_mode', unreal.GameModeBase)
    if not unreal.EditorLoadingAndSavingUtils.save_map(world, demo_path):
        raise RuntimeError('Could not save demo')

# Read-only inspection of existing AST interfaces for a later adapter.
output = Path(unreal.Paths.project_saved_dir()) / 'POICheckpoints' / 'ASTInspection'
output.mkdir(parents=True, exist_ok=True)
if hasattr(unreal, 'BlueprintAnalyzerLibrary'):
    for name in ['BPI_InteractionActor', 'BPI_DoorActor', 'BPI_Cupboard', 'BPI_PullpushObje']:
        bp = load('/AST_InteractionSystem/Blueprints/Interfaces/' + name)
        if bp:
            analysis = unreal.BlueprintAnalyzerLibrary.analyze_blueprint(bp)
            (output / (name + '.json')).write_text(unreal.BlueprintAnalyzerLibrary.export_to_json(analysis), encoding='utf-8')
unreal.log('POI_DEMO_SETUP_SUCCESS')
