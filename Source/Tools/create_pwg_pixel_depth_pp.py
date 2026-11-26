"""Create the separate V2 postprocess using the audited Legacy graph contract.

Never modify the original material. An existing V2 is only accepted if its
marker and parameter contract match; otherwise stop rather than overwrite it.
"""
import unreal

ROOT = '/Game/Systems/Procedural_World_Generator/PWG_v2'
LEGACY = ROOT + '/M_PWGv2_DepthAtmosphere'
NAME = 'M_PWGv2_DepthAtmosphere_Pixel2D'
DEST = ROOT + '/' + NAME
LIB = unreal.MaterialEditingLibrary
MARKER = '// SIRROH_PIXEL_DEPTH_V1'
LEGACY_SCALARS = ['NeutralWidth','BackRange','FrontRange','FogStrength','Desaturation',
                  'ForegroundDarkness','GroundZ','MistHeight','MistStrength','PixelBands',
                  'WindTime','SkyStart','SkyEnd']
LEGACY_VECTORS = ['PlayerOrigin','DepthAxis','FogColor']
NEW_SCALARS = {
    'ProfileNeutralWidth':160.0,'ProfileBackRange':150.0,'ProfileFrontRange':130.0,
    'ProfileStrength':1.0,'ProfileFogStrength':0.65,'ProfileDesaturation':0.55,
    'ProfileForegroundDarkness':0.35,'ProfileMix':0.0,
    'ProfileBackgroundBrightness':4.5,'ProfileBackgroundContrast':0.85,'ProfileBackgroundAmbient':0.075,
    'ProfileGameplayBrightness':1.25,'ProfileGameplayContrast':1.0,'ProfileVisualizeMasks':0.0,
    'DepthGradeEnabled':1.0,'DepthGradeStart':100.0,'DepthGradeRange':450.0,
    'DepthGradeBrightness':1.25,'DepthGradeContrast':.85,'DepthGradeDesaturation':.5,
    'DepthGradeTintStrength':.45,'DepthGradeFog':.75,'DepthLayerDebug':0.0,
    'DepthLayerSpan':290.0,'DepthLayerActive':0.0,
    'CaveReferenceActive':0.0,'CaveReferenceStart':165.0,'CaveReferenceDepth':2200.0,
    'CaveTextureAmbient':.42,
    'PixelMode':0.0,'PixelStart':260.0,'PixelSpan':280.0,'PixelGradeStrength':1.0,
    'CaveHaze0':.24,'CaveHaze1':.56,'CaveHaze2':.83,'CaveHaze3':.98,
}
NEW_VECTORS = {
    'ProfileOrigin':(0,0,0,0),
    'ProfileBackgroundTint':(0.90,0.86,1.0,1.0),
    'ProfileForegroundTint':(0.78,0.66,0.90,1.0),
    'ProfileAtmosphereColor':(0.48,0.43,0.58,1.0),
    'ProfileGameplayTint':(0.96,0.91,1.0,1.0),
    'DepthGradeTint':(.78,.84,1.0,1.0),
    'CaveGrade0':(.53,1.32,.72,.24),'CaveGrade1':(.85,1.08,.55,.56),
    'CaveGrade2':(1.14,.88,.30,.83),'CaveGrade3':(1.37,.80,.12,.98),
    'CaveTint0':(.72,.83,.92,1),'CaveTint1':(.84,.91,1,1),
    'CaveTint2':(.93,.98,1,1),'CaveTint3':(1,1,1,1),
}
OUT = unreal.MaterialProperty.MP_EMISSIVE_COLOR

# Stencil role + visible depth match, artistic rear-only ambient. Do not treat
# post-tonemap display values as physical light energy.
STYLE = r'''
// SIRROH_PIXEL_DEPTH_V1
float3 src=max(Scene.rgb,0.0);
float z=SceneZ.r, cz=MaskZ.r;
// Both scene-texture depths are linear world units. Occluded rear proxies cannot
// classify the foreground player, equipment, props, or the solid cutaway.
float match=abs(z-cz)<=max(0.5,abs(z)*0.0001)?1.0:0.0;
float stencil=MaskId.r;
float rear=match*(abs(stencil-201.0)<0.5?1.0:0.0);
float cut=match*(abs(stencil-202.0)<0.5?1.0:0.0);
float gameplay=match*(abs(stencil-203.0)<0.5?1.0:0.0);
float other=1.0-max(max(rear,cut),gameplay);
if (ProfileVisualizeMasks>0.5)
    return rear*float3(0.05,0.9,1.0)+cut*float3(1.0,0.05,0.65)+gameplay*float3(1.0,0.85,0.03)+other*float3(0.08,0.65,0.18);
float3 weights=float3(0.2126,0.7152,0.0722);
float lum=dot(src,weights);
float peak=max(src.r,max(src.g,src.b));
float protect=smoothstep(0.32,0.85,peak);
float d=dot(World.xyz-ProfileOrigin.xyz,DepthAxis.xyz);
float caveQ=(d-CaveReferenceStart)/max(CaveReferenceDepth,500.0);
float isCave=(CaveReferenceActive>0.5 && caveQ>=0.0 && caveQ<1.03)?1.0:0.0;
float3 planGrade=caveQ<.23?CaveGrade0.rgb:caveQ<.51?CaveGrade1.rgb:caveQ<.80?CaveGrade2.rgb:CaveGrade3.rgb;
float planHaze=caveQ<.23?CaveHaze0:caveQ<.51?CaveHaze1:caveQ<.80?CaveHaze2:CaveHaze3;
float3 planTint=caveQ<.23?CaveTint0.rgb:caveQ<.51?CaveTint1.rgb:caveQ<.80?CaveTint2.rgb:CaveTint3.rgb;
if (DepthLayerDebug>0.5)
{
    float q=(d-250.0)/max(DepthLayerSpan,1.0);
    float3 depthColor=float3(0.05,0.9,1.0);
    if (DepthLayerActive>0.5 && DepthLayerSpan>=120.0 && q>=0.0 && q<1.0)
        depthColor=q<.30?float3(.96,.18,.12):q<.72?float3(.20,.55,1.0):float3(.72,.30,1.0);
    if (PixelMode>0.5 && d>=PixelStart-2 && d<=PixelStart+PixelSpan+2)
    {
        float t=saturate((d-PixelStart)/max(PixelSpan,1.0));
        depthColor=t<.16?float3(.96,.18,.12):t<.50?float3(.20,.55,1.0):t<.83?float3(.72,.30,1.0):float3(.96,.84,.25);
    }
    if (isCave>0.5)
        depthColor=caveQ<.23?float3(.96,.18,.12):caveQ<.51?float3(.20,.55,1.0):caveQ<.80?float3(.72,.30,1.0):float3(.96,.84,.25);
    return rear*depthColor+cut*float3(1.0,.05,.65)+gameplay*float3(1.0,.85,.03)+other*float3(.08,.65,.18);
}
// Physical depth is only a continuous secondary atmospheric cue, not a gate.
float distanceFog=saturate(max(d,0.0)/max(ProfileBackRange,20.0));
float profileFog=saturate(ProfileFogStrength)*(0.65+0.35*distanceFog);
float gain=max(ProfileBackgroundBrightness,0.0);
// Monotonic shoulder: gain affects shadows, with no clipping of bright firelit detail.
float3 curved=gain<=0.0?float3(0,0,0):1.0-pow(1.0-saturate(src),max(gain,0.001));
curved=pow(max(curved,0.0),max(ProfileBackgroundContrast,0.4));
float bgLum=dot(curved,weights);
float3 bg=lerp(curved,bgLum.xxx,saturate(ProfileDesaturation))*max(ProfileBackgroundTint.rgb,0.0);
bg=lerp(bg,bgLum.xxx*max(ProfileAtmosphereColor.rgb,0.0),profileFog*0.35);
float ambient=saturate(ProfileBackgroundAmbient)*(1.0-protect);
// Additive term applies solely to visible tagged rear wall pixels. No lift elsewhere.
bg=bg+ambient*max(ProfileAtmosphereColor.rgb,0.0)*(1.0-saturate(bgLum));
// Rear-only continuous grading. Off is the exact accepted Stage 1 formula.
if (DepthGradeEnabled>0.5)
{
    float t=smoothstep(0.0,1.0,(d-DepthGradeStart)/max(DepthGradeRange,20.0));
    bg=pow(max(bg,0.0),lerp(1.0,max(.4,DepthGradeContrast),t));
    bg*=lerp(1.0,max(DepthGradeBrightness,0.0),t);
    float depthLum=dot(bg,weights);
    bg=lerp(bg,depthLum.xxx,t*saturate(DepthGradeDesaturation));
    bg*=lerp(float3(1,1,1),saturate(DepthGradeTint.rgb),t*saturate(DepthGradeTintStrength));
    // Zero ambient keeps the haze target black; Dark Cave does not lift black.
    float3 hazeTarget=ProfileAtmosphereColor.rgb*saturate(ProfileBackgroundAmbient*2.8);
    bg=lerp(bg,hazeTarget,t*saturate(DepthGradeFog)*saturate(ProfileFogStrength));
}
if (isCave>0.5)
{
    // Explicit per-plan display values. Far silhouettes approach atmosphere;
    // real illumination remains in the lit source, and Dark Cave has no haze floor.
    float3 rock=pow(max(src,0.0),max(planGrade.y,.4))*max(planGrade.x,0.0);
    rock=lerp(dot(rock,weights).xxx,rock,saturate(planGrade.z))*max(planTint,0.0);
    float haze=saturate(planHaze)*saturate(ProfileBackgroundAmbient/.075)*saturate(ProfileFogStrength/.4);
    float3 target=max(ProfileAtmosphereColor.rgb,0.0)*max(planTint,0.0)*max(planGrade.x,0.0);
    // Rear-only textured artistic ambient, separate from actual lighting.
    // Read the deferred GBuffer, not a new unlit/emissive rock material.
    // Dark Cave sets ProfileBackgroundAmbient to zero.
    float3 albedo=pow(saturate(RockAlbedo.rgb),0.454545);
    float3 nn=normalize(RockNormal.rgb+float3(0.0001,0,0));
    float directional=.57+.43*saturate(dot(nn,normalize(float3(-.22,-.85,.48))));
    float detail=1.0-saturate(planHaze)*.68;
    float strength=saturate(CaveTextureAmbient)*saturate(ProfileBackgroundAmbient/.085)*detail;
    float3 fill=lerp(dot(albedo,weights).xxx,albedo,saturate(planGrade.z))*planTint*directional*strength;
    rock=rock+fill*(1.0-saturate(rock))*(1.0-protect);
    bg=lerp(rock,target,haze*(1.0-protect));
}
// The new global 2D mode uses only actual scene illumination. It does not
// add the old artistic ambient floor or blend black toward a bright haze.
// All operations map zero to zero, including the closure.
if(PixelMode>0.5)
{
    float inPlanes=(d>=PixelStart-2 && d<=PixelStart+PixelSpan+2)?1.0:0.0;
    float q=saturate((d-PixelStart)/max(PixelSpan,1.0))*saturate(PixelGradeStrength)*inPlanes;
    float3 lit=pow(max(src,0.0),lerp(1.0,.90,q));
    float l=dot(lit,weights);
    bg=lerp(lit,l.xxx,q*.28);
    bg*=lerp(float3(1,1,1),float3(.84,.90,1),q*.40);
}
bg=lerp(bg,src,protect);
float3 fg=src*lerp(float3(1,1,1),saturate(ProfileForegroundTint.rgb)*(1.0-saturate(ProfileForegroundDarkness)),1.0-protect);
float3 play=pow(max(src,0.0),max(ProfileGameplayContrast,0.4))*max(ProfileGameplayBrightness,0.0)*saturate(ProfileGameplayTint.rgb);
play=lerp(play,src,protect);
float3 styled=rear*bg+cut*fg+gameplay*play+other*src;
float3 stylized=lerp(src,styled,saturate(ProfileStrength));
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
            raise RuntimeError('Existing semantic POI differs; refuse overwrite')
        unreal.log('PIXEL_DEPTH_PP_VERIFIED '+existing.get_path_name())
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
    scene_z=node(unreal.MaterialExpressionSceneTexture,scene_texture_id=unreal.SceneTextureId.PPI_SCENE_DEPTH)
    mask_z=node(unreal.MaterialExpressionSceneTexture,scene_texture_id=unreal.SceneTextureId.PPI_CUSTOM_DEPTH)
    mask_id=node(unreal.MaterialExpressionSceneTexture,scene_texture_id=unreal.SceneTextureId.PPI_CUSTOM_STENCIL)
    rock_albedo=node(unreal.MaterialExpressionSceneTexture,scene_texture_id=unreal.SceneTextureId.PPI_BASE_COLOR)
    rock_normal=node(unreal.MaterialExpressionSceneTexture,scene_texture_id=unreal.SceneTextureId.PPI_WORLD_NORMAL)
    inputs=[('RockAlbedo',rock_albedo,'Color'),('RockNormal',rock_normal,'Color'),('Scene',scene,'Color'),('World',world,''),('SceneZ',scene_z,'Color'),('MaskZ',mask_z,'Color'),('MaskId',mask_id,'Color')]
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
    unreal.log('PIXEL_DEPTH_PP_SAVED '+asset.get_path_name())

main()
