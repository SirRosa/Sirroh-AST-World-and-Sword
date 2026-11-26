"""Create two isolated, non-emissive background materials from the original graphs.
The existing atlas/UV fitting, alpha mask and texture parameters are preserved.
Never modify the original materials or the solid cutaway material.
"""
import json
from pathlib import Path
import unreal

ea = unreal.EditorAssetLibrary
me = unreal.MaterialEditingLibrary
mp = unreal.MaterialProperty
out = Path(unreal.Paths.project_saved_dir()) / 'ChatGPT' / 'LitUndergroundBackgrounds'
out.mkdir(parents=True, exist_ok=True)
jobs = [
    ('/Game/POIKit/M_KitImageFit', '/Game/POIKit/M_KitImageFit_LitBackground'),
    ('/Game/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_Tiled',
     '/Game/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_BackgroundLit'),
]
# Preflight both assets before creating either. Refuse to overwrite any preexisting output.
for src, dst in jobs:
    source = unreal.load_asset(src)
    assert source, 'Missing ' + src
    assert not ea.does_asset_exist(dst), 'Existing target requires backup: ' + dst
    assert me.get_material_property_input_node(source, mp.MP_EMISSIVE_COLOR), 'No original color output: ' + src

reports = []
for src, dst in jobs:
    source = unreal.load_asset(src)
    original_mask = me.get_material_property_input_node(source, mp.MP_OPACITY_MASK)
    mat = ea.duplicate_asset(src, dst)
    assert mat, dst
    color = me.get_material_property_input_node(mat, mp.MP_EMISSIVE_COLOR)
    pin = me.get_material_property_input_node_output_name(mat, mp.MP_EMISSIVE_COLOR) or ''
    assert color
    def constant(value, y):
        n = me.create_material_expression(mat, unreal.MaterialExpressionConstant, 200, y)
        assert n
        n.set_editor_property('r', value)
        return n
    def connect(n, p, prop):
        assert me.connect_material_property(n, p, prop), str(prop)
        assert me.get_material_property_input_node(mat, prop) == n, str(prop)
    mat.set_editor_property('shading_model', unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
    mat.set_editor_property('two_sided', True)
    connect(color, pin, mp.MP_BASE_COLOR)
    black = constant(0.0, 200)
    connect(black, '', mp.MP_EMISSIVE_COLOR)
    connect(black, '', mp.MP_METALLIC)
    connect(black, '', mp.MP_SPECULAR)
    connect(constant(1.0, 360), '', mp.MP_ROUGHNESS)
    if original_mask:
        assert me.get_material_property_input_node(mat, mp.MP_OPACITY_MASK), 'Alpha mask lost'
    me.recompile_material(mat)
    assert ea.save_loaded_asset(mat, only_if_is_dirty=False), dst
    assert me.get_material_property_input_node(mat, mp.MP_BASE_COLOR) == color
    assert me.get_material_property_input_node(mat, mp.MP_EMISSIVE_COLOR) == black
    assert black.get_editor_property('r') == 0.0
    assert mat.get_editor_property('shading_model') == unreal.MaterialShadingModel.MSM_DEFAULT_LIT
    reports.append(dict(material=mat.get_path_name(), source=source.get_path_name(),
                        shading='Default Lit', emissive=0, metallic=0, specular=0,
                        roughness=1, alpha_mask_preserved=bool(original_mask)))
(out / 'lit-background-graph.json').write_text(json.dumps(dict(status='PASS', materials=reports), indent=2), encoding='utf8')
unreal.log('PWG_LIT_BACKGROUNDS_GRAPH_PASS ' + json.dumps(reports))
