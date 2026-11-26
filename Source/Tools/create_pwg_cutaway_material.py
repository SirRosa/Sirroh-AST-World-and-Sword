"""Create a dedicated cutaway shader without modifying the ordinary PWG tiled asset.
Run in the Unreal Python commandlet. A nonempty result JSON is required by deployment.
"""
import json
from pathlib import Path
import unreal

root = '/Game/Systems/Procedural_World_Generator/PWG_v2'
source_path = root + '/M_PWGv2_Tiled'
target_name = 'M_PWGv2_SolidCutaway'
target_path = root + '/' + target_name
out = Path(unreal.Paths.project_saved_dir()) / 'ChatGPT' / 'CutawayMaterialFix'
out.mkdir(parents=True, exist_ok=True)
ea = unreal.EditorAssetLibrary
me = unreal.MaterialEditingLibrary
tools = unreal.AssetToolsHelpers.get_asset_tools()
mp = unreal.MaterialProperty
source = unreal.load_asset(source_path)
assert source, 'Missing source tiled material'
source_exprs = list(me.get_material_expressions(source))
source_vertex_nodes = sum(isinstance(e, unreal.MaterialExpressionVertexColor) for e in source_exprs)
original_outputs = {}
for prop in (mp.MP_EMISSIVE_COLOR, mp.MP_BASE_COLOR):
    n = me.get_material_property_input_node(source, prop)
    if n:
        original_outputs[str(prop)] = n.get_class().get_name()
if ea.does_asset_exist(target_path):
    # Retry is permitted only after the deployment script has backed up the new asset
    # from this task's failed authoring attempt. Never touch the source material.
    assert (out / 'M_PWGv2_SolidCutaway.before-retry.uasset').exists(), 'Existing target is not backed up'
    assert ea.delete_asset(target_path), 'Could not remove the incomplete generated target'
# Preserve every existing atlas/tiling expression if it is independent of vertex color.
# Otherwise explicitly reconstruct the established tiled shader, avoiding double shading.
clone = bool(original_outputs) and source_vertex_nodes == 0
mat = ea.duplicate_asset(source_path, target_path) if clone else tools.create_asset(
    target_name, root, unreal.Material, unreal.MaterialFactoryNew())
assert mat, 'Failed to create independent cutaway material'

def node(cls, x=0, y=0):
    n = me.create_material_expression(mat, cls, x, y)
    assert n, str(cls)
    return n

def connect(a, pin, b, dest):
    assert me.connect_material_expressions(a, pin, b, dest), (a.get_name(), pin, b.get_name(), dest)

def output(n, pin, prop):
    assert me.connect_material_property(n, pin, prop), str(prop)
    assert me.get_material_property_input_node(mat, prop) == n, 'Output connection not retained'

mat.set_editor_property('blend_mode', unreal.BlendMode.BLEND_OPAQUE)
mat.set_editor_property('two_sided', True)
vertex = node(unreal.MaterialExpressionVertexColor, -180, 360)
shaded_outputs = []
if clone:
    for prop in (mp.MP_EMISSIVE_COLOR, mp.MP_BASE_COLOR):
        original = me.get_material_property_input_node(mat, prop)
        if not original:
            continue
        pin = me.get_material_property_input_node_output_name(mat, prop) or ''
        shaded = node(unreal.MaterialExpressionMultiply, 100, 150 if prop == mp.MP_EMISSIVE_COLOR else -150)
        connect(original, pin, shaded, 'A')
        connect(vertex, '', shaded, 'B')
        output(shaded, '', prop)
        shaded_outputs.append((prop, shaded))
else:
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_UNLIT)
    uv = node(unreal.MaterialExpressionTextureCoordinate, -1000, -200)
    frac = node(unreal.MaterialExpressionFrac, -800, -200)
    connect(uv, '', frac, '')
    def vector(name, value, x, y):
        n = node(unreal.MaterialExpressionVectorParameter, x, y)
        n.set_editor_property('parameter_name', name)
        n.set_editor_property('default_value', unreal.LinearColor(*value))
        return n
    def rg(n, x, y):
        m = node(unreal.MaterialExpressionComponentMask, x, y)
        for c in 'rgba':
            m.set_editor_property(c, c in 'rg')
        connect(n, 'RGBA', m, '')
        return m
    scale = rg(vector('UVScale', (1,1,0,0), -1000, 0), -800, 0)
    offset = rg(vector('UVOffset', (0,0,0,0), -1000, 200), -800, 200)
    mul = node(unreal.MaterialExpressionMultiply, -600, -150)
    add = node(unreal.MaterialExpressionAdd, -400, -150)
    connect(frac, '', mul, 'A')
    connect(scale, '', mul, 'B')
    connect(mul, '', add, 'A')
    connect(offset, '', add, 'B')
    sample = node(unreal.MaterialExpressionTextureSampleParameter2D, -180, -200)
    sample.set_editor_property('parameter_name', 'Atlas')
    sample.set_editor_property('texture', unreal.load_asset('/Engine/EngineResources/DefaultTexture'))
    connect(add, '', sample, '')
    tint = vector('Tint', (1,1,1,1), -400, 300)
    color = node(unreal.MaterialExpressionMultiply, 80, -100)
    connect(sample, 'RGB', color, 'A')
    connect(tint, 'RGB', color, 'B')
    shaded = node(unreal.MaterialExpressionMultiply, 300, -100)
    connect(color, '', shaded, 'A')
    connect(vertex, '', shaded, 'B')
    output(shaded, '', mp.MP_EMISSIVE_COLOR)
    shaded_outputs.append((mp.MP_EMISSIVE_COLOR, shaded))
assert shaded_outputs
me.recompile_material(mat)
assert ea.save_loaded_asset(mat, only_if_is_dirty=False), 'Material save failed'
for prop, shaded in shaded_outputs:
    assert me.get_material_property_input_node(mat, prop) == shaded
    assert vertex in me.get_inputs_for_material_expression(mat, shaded), 'Vertex RGB is not connected'
assert not source.get_path_name().startswith(target_path)
report = dict(status='PASS', material=mat.get_path_name(), source=source.get_path_name(),
              source_vertex_nodes=source_vertex_nodes, source_outputs=original_outputs,
              preserved_source_graph=clone, shaded_outputs=[str(p) for p,_ in shaded_outputs])
(out / 'cutaway-material-graph.json').write_text(json.dumps(report, indent=2), encoding='utf8')
unreal.log('PWG_CUTAWAY_MATERIAL_GRAPH_PASS ' + json.dumps(report))
