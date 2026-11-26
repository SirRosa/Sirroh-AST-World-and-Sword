"""Create the separate V2 postprocess using the audited Legacy graph contract.

Never modify the original material. An existing V2 is only accepted if its
marker and parameter contract match; otherwise stop rather than overwrite it.
"""
import unreal

ROOT = '/Game/Systems/Procedural_World_Generator/PWG_v2'
LEGACY = ROOT + '/M_PWGv2_DepthAtmosphere'
NAME = 'M_PWGv2_DepthAtmosphere_V2'
DEST = ROOT + '/' + NAME
LIB = unreal.MaterialEditingLibrary
MARKER = '// SIRROH_VISUAL_FOUNDATION_V1'
LEGACY_SCALARS = ['NeutralWidth','BackRange','FrontRange','FogStrength','Desaturation',
                  'ForegroundDarkness','GroundZ','MistHeight','MistStrength','PixelBands',
                  'WindTime','SkyStart','SkyEnd']
LEGACY_VECTORS = ['PlayerOrigin','DepthAxis','FogColor']
NEW_SCALARS = {
    'ProfileNeutralWidth':160.0,'ProfileBackRange':150.0,'ProfileFrontRange':130.0,
    'ProfileStrength':1.0,'ProfileFogStrength':0.65,'ProfileDesaturation':0.55,
    'ProfileForegroundDarkness':0.55,'ProfileMix':0.0,
}
NEW_VECTORS = {
    'ProfileOrigin':(0,0,0,0),
    'ProfileBackgroundTint':(0.76,0.84,1.0,1.0),
    'ProfileForegroundTint':(0.78,0.66,0.90,1.0),
}
OUT = unreal.MaterialProperty.MP_EMISSIVE_COLOR

# Multiplicative, black-preserving grading. World position rather than Pawn Y
# keeps all lanes invariant. Do not treat a post-tonemap value as light energy.
STYLE = r'''
// SIRROH_VISUAL_FOUNDATION_V1
float3 src=Scene.rgb;
float d=dot(World.xyz-ProfileOrigin.xyz,DepthAxis.xyz);
float n=max(ProfileNeutralWidth,50.0);
float b=max(d-n,0.0);
float f=max(-d-n,0.0);
float skyKeep=1.0-smoothstep(SkyStart,max(SkyStart+1.0,SkyEnd),d);
float3 weights=float3(0.2126,0.7152,0.0722);
float lum=dot(src,weights);
float highlights=1.0-smoothstep(0.65,1.0,max(src.r,max(src.g,src.b)));
float bg=smoothstep(0.0,max(ProfileBackRange,20.0),b)
    *saturate(ProfileFogStrength)*saturate(ProfileStrength)*skyKeep*highlights;
float fg=smoothstep(0.0,max(ProfileFrontRange,20.0),f)
    *saturate(ProfileStrength)*highlights;
float3 tint=max(ProfileBackgroundTint.rgb,0.0);
float tintLum=dot(tint,weights);
tint=tintLum>0.05?tint/tintLum:float3(1.0,1.0,1.0);
float3 target=lerp(src,lum.xxx*tint,saturate(ProfileDesaturation));
// Modest contrast compression through multiplication, never additive fill.
target*=1.0+0.12*saturate(lum);
float3 stylized=lerp(src,target,bg);
stylized*=lerp(float3(1.0,1.0,1.0),
    saturate(ProfileForegroundTint.rgb)*(1.0-saturate(ProfileForegroundDarkness)),fg);
[branch] if (ProfileMix>=0.99999) return stylized;
'''

def input_node(material):
    node = LIB.get_material_property_input_node(material, OUT)
    if not isinstance(node, unreal.MaterialExpressionCustom):
        raise RuntimeError('Unexpected emissive input, inspect graph before authoring')
    return node

def names(material, vector=False):
    fn = LIB.get_vector_parameter_names if vector else LIB.get_scalar_parameter_names
    return {str(n) for n in fn(material)}

def verify(material):
    assert material.get_editor_property('material_domain') == unreal.MaterialDomain.MD_POST_PROCESS
    assert material.get_editor_property('blendable_location') == unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING
    assert material.get_editor_property('blendable_priority') == -10
    assert names(material) == set(LEGACY_SCALARS) | set(NEW_SCALARS)
    assert names(material,True) == set(LEGACY_VECTORS) | set(NEW_VECTORS)
    assert MARKER in input_node(material).get_editor_property('code')

def main():
    original=unreal.load_asset(LEGACY)
    if original is None:
        raise RuntimeError('Missing Legacy asset')
    assert original.get_editor_property('material_domain') == unreal.MaterialDomain.MD_POST_PROCESS
    assert original.get_editor_property('blendable_location') == unreal.BlendableLocation.BL_SCENE_COLOR_AFTER_TONEMAPPING
    assert original.get_editor_property('blendable_priority') == -10
    assert names(original) == set(LEGACY_SCALARS)
    assert names(original,True) == set(LEGACY_VECTORS)
    legacy_code=input_node(original).get_editor_property('code')
    assert legacy_code.count('return color;') == 1
    assert 'dot(World.xyz-PlayerOrigin.xyz,DepthAxis.xyz)' in legacy_code
    # Keep the original Legacy formula for a short, single-pass blend-in.
    code=STYLE+legacy_code.replace('return color;', 'return lerp(color,stylized,saturate(ProfileMix));')
    existing=unreal.load_asset(DEST)
    if existing is not None:
        verify(existing)
        if input_node(existing).get_editor_property('code').replace('\r\n','\n') != code.replace('\r\n','\n'):
            raise RuntimeError('Existing V2 differs; back up and inspect before updating')
        unreal.log('VISUAL_FOUNDATION_V2_VERIFIED '+existing.get_path_name())
        return
    asset=unreal.AssetToolsHelpers.get_asset_tools().create_asset(NAME,ROOT,unreal.Material,unreal.MaterialFactoryNew())
    if asset is None:
        raise RuntimeError('Cannot create V2 asset')
    asset.set_editor_property('material_domain',original.get_editor_property('material_domain'))
    asset.set_editor_property('blendable_location',original.get_editor_property('blendable_location'))
    asset.set_editor_property('blendable_priority',original.get_editor_property('blendable_priority'))

    def node(cls,**props):
        n=LIB.create_material_expression(asset,cls,0,0)
        if n is None:
            raise RuntimeError('Cannot create expression '+str(cls))
        for k,v in props.items():
            n.set_editor_property(k,v)
        return n

    def scalar(name,value):
        return node(unreal.MaterialExpressionScalarParameter,parameter_name=name,default_value=value)

    def vector(name,value):
        return node(unreal.MaterialExpressionVectorParameter,parameter_name=name,default_value=value)

    scene=node(unreal.MaterialExpressionSceneTexture,scene_texture_id=unreal.SceneTextureId.PPI_POST_PROCESS_INPUT0)
    world=node(unreal.MaterialExpressionWorldPosition)
    inputs=[('Scene',scene,'Color'),('World',world,'')]
    for n in LEGACY_SCALARS:
        inputs.append((n,scalar(n,LIB.get_material_default_scalar_parameter_value(original,n)),''))
    for n in LEGACY_VECTORS:
        inputs.append((n,vector(n,LIB.get_material_default_vector_parameter_value(original,n)),''))
    for n,v in NEW_SCALARS.items():
        inputs.append((n,scalar(n,v),''))
    for n,v in NEW_VECTORS.items():
        inputs.append((n,vector(n,unreal.LinearColor(*v)),''))
    custom=node(unreal.MaterialExpressionCustom,output_type=unreal.CustomMaterialOutputType.CMOT_FLOAT3,code=code)
    custom_inputs=[]
    for n,_,_ in inputs:
        entry=unreal.CustomInput()
        entry.set_editor_property('input_name',n)
        custom_inputs.append(entry)
    custom.set_editor_property('inputs',custom_inputs)
    for n,source,pin in inputs:
        if not LIB.connect_material_expressions(source,pin,custom,n):
            raise RuntimeError('Cannot connect '+n)
    if not LIB.connect_material_property(custom,'',OUT):
        raise RuntimeError('Cannot connect output')
    LIB.layout_material_expressions(asset)
    LIB.recompile_material(asset)
    verify(asset)
    if not unreal.EditorAssetLibrary.save_loaded_asset(asset,only_if_is_dirty=False):
        raise RuntimeError('Cannot save V2 asset')
    unreal.log('VISUAL_FOUNDATION_V2_SAVED '+asset.get_path_name())

main()
