"""Author a new masked/default-lit data material. No hand-painted or existing rock assets."""
import unreal
from pathlib import Path
root='/Game/Systems/Procedural_World_Generator/PWG_v2'
ea=unreal.EditorAssetLibrary
me=unreal.MaterialEditingLibrary
mp=unreal.MaterialProperty
name='M_PWGv2_PixelBackground_Readability'
texname='T_PWGv2_PixelDataDefault'
assert not ea.does_asset_exist(root+'/'+name),'Existing readability material; preserve and inspect'
tex=unreal.load_asset(root+'/'+texname)
assert tex, 'Original PixelData default is required; do not overwrite it'
mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,root,unreal.Material,unreal.MaterialFactoryNew())
assert mat
mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
mat.set_editor_property('blend_mode',unreal.BlendMode.BLEND_MASKED)
mat.set_editor_property('two_sided',True)
def node(cls,**props):
    n=me.create_material_expression(mat,cls,0,0);assert n
    for k,v in props.items():n.set_editor_property(k,v)
    return n
def scalar(n,v):return node(unreal.MaterialExpressionScalarParameter,parameter_name=n,default_value=v)
def vector(n,v):return node(unreal.MaterialExpressionVectorParameter,parameter_name=n,default_value=unreal.LinearColor(*v))
def const(v):return node(unreal.MaterialExpressionConstant,r=v)
def link(a,p,b,q):assert me.connect_material_expressions(a,p,b,q),(p,q)
def output(a,p,q):assert me.connect_material_property(a,p,q),str(q)
def custom(code,inputs,kind):
    n=node(unreal.MaterialExpressionCustom,code=code,output_type=kind)
    arr=[]
    for nm,_,_ in inputs:
        x=unreal.CustomInput();x.set_editor_property('input_name',nm);arr.append(x)
    n.set_editor_property('inputs',arr)
    for nm,a,p in inputs:link(a,p,n,nm)
    return n
uv=node(unreal.MaterialExpressionTextureCoordinate)
scale=vector('DataScale',(1,1,0,1));offset=vector('DataOffset',(0,0,0,1))
uv2=custom('return UV*Scale.xy+Offset.xy;',[('UV',uv,''),('Scale',scale,''),('Offset',offset,'')],unreal.CustomMaterialOutputType.CMOT_FLOAT2)
data=node(unreal.MaterialExpressionTextureSampleParameter2D,parameter_name='PixelData',texture=tex,sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_LINEAR_COLOR)
link(uv2,'',data,'UVs')
palette=scalar('PaletteOverride',0);plan=scalar('Plan',0);grade=scalar('GradeStrength',1);readability=scalar('Readability',1)
color=custom(r'''
// R = byte index (0..7); G = family; A = actual mask. No colour noise here.
// All palette values are authored numeric linear RGB, not source images.
int i=clamp((int)round(Data.r*255.0/36.0),0,7);
if(Readability>0.5)
{
    // Material family is hue; plan is an independent luminance budget.
    // All values are finite physical Base Color, never emissive light.
    bool isDeep=Choice>1.5 || (Choice<.5 && Data.g>.5);
    float3 family=isDeep?float3(.43,.78,1.58):float3(1.08,1.18,.48);
    float b=Data.b*2.0-1.0;
    family*=float3(1.0+.15*b,1.0-.045*b,1.0-.20*b);
    float3 hue=family;
    // Keep a material's body recognizable; shadowed material is cooler and
    // raised material warmer, but both retain the same luminance band.
    if(i==2)hue*=float3(.89,.99,1.12);
    if(i==4)hue*=float3(1.12,1.02,.86);
    if(i==5)hue=isDeep?float3(.56,1.10,1.12):float3(.70,1.13,.91);
    if(i==6)hue=isDeep?float3(1.22,.84,.83):float3(1.32,.93,.58);
    if(i==7)hue=isDeep?float3(1.06,.83,1.29):float3(1.12,.84,1.18);
    // Non-overlapping plan tonal bands at Grade=1, with narrowing contrast.
    float nearT[8]={.015,.026,.038,.053,.070,.050,.060,.047};
    float midT[8]={.074,.078,.082,.093,.106,.091,.096,.090};
    float farT[8]={.112,.117,.123,.131,.142,.131,.131,.131};
    float endT[8]={.151,.151,.151,.161,.176,.161,.161,.161};
    int p=clamp((int)round(Plan),0,3);
    float T=p==0?nearT[i]:p==1?midT[i]:p==2?farT[i]:endT[i];
    float q=saturate(Grade);
    T=lerp(nearT[i],T,q)*(isDeep?.88:1.);
    float3 weights=float3(.2126,.7152,.0722);
    hue/=max(dot(hue,weights),.01);
    float sat=p==0?1.:p==1?.83:p==2?.65:.47;
    hue=lerp(float3(1,1,1),hue,lerp(1.,sat,q));
    return saturate(hue*T);
}
float3 shallow[8]={float3(.025,.027,.020),float3(.056,.061,.041),float3(.105,.128,.081),float3(.165,.185,.115),float3(.255,.238,.142),float3(.085,.166,.146),float3(.30,.175,.067),float3(.092,.061,.10)};
float3 deep[8]={float3(.015,.020,.034),float3(.034,.046,.078),float3(.066,.092,.146),float3(.117,.148,.22),float3(.19,.235,.32),float3(.053,.151,.164),float3(.23,.105,.13),float3(.082,.051,.118)};
bool d=Choice>1.5 || (Choice<.5 && Data.g>.5);
float3 c=d?deep[i]:shallow[i];
float3 base=d?deep[2]:shallow[2];
float q=saturate(Plan/3);
c=lerp(c,base,q*.28*saturate(Grade));
return c*lerp(1.0,.76,q*saturate(Grade));
''',[('Data',data,'RGB'),('Choice',palette,''),('Plan',plan,''),('Grade',grade,''),('Readability',readability,'')],unreal.CustomMaterialOutputType.CMOT_FLOAT3)
output(color,'',mp.MP_BASE_COLOR)
output(data,'A',mp.MP_OPACITY_MASK)
output(const(0),'',mp.MP_EMISSIVE_COLOR)
output(const(0),'',mp.MP_SPECULAR);output(const(0),'',mp.MP_METALLIC);output(const(1),'',mp.MP_ROUGHNESS)
me.layout_material_expressions(mat);me.recompile_material(mat)
assert ea.save_loaded_asset(mat,only_if_is_dirty=False)
unreal.log('PIXEL_READABILITY_MATERIAL_SAVED '+mat.get_path_name())
