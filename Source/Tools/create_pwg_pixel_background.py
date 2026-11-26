"""Author a new masked/default-lit data material. No hand-painted or existing rock assets."""
import unreal, struct, zlib
from pathlib import Path
root='/Game/Systems/Procedural_World_Generator/PWG_v2'
ea=unreal.EditorAssetLibrary
me=unreal.MaterialEditingLibrary
mp=unreal.MaterialProperty
name='M_PWGv2_PixelBackground'
texname='T_PWGv2_PixelDataDefault'
assert not ea.does_asset_exist(root+'/'+name),'Existing pixel material; inspect, do not overwrite'
assert not ea.does_asset_exist(root+'/'+texname),'Existing pixel data default; inspect first'
out=Path(unreal.Paths.project_saved_dir())/'ChatGPT'/'ProceduralPixel2D_20261126'
out.mkdir(parents=True,exist_ok=True)
def chunk(tag,data):
    return struct.pack('>I',len(data))+tag+data+struct.pack('>I',zlib.crc32(tag+data)&0xffffffff)
png=b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',2,2,8,6,0,0,0))+chunk(b'IDAT',zlib.compress((b'\x00'+bytes([108,0,0,255])*2)*2))+chunk(b'IEND',b'')
path=out/(texname+'.png');path.write_bytes(png)
task=unreal.AssetImportTask()
task.set_editor_property('filename',str(path));task.set_editor_property('destination_path',root)
task.set_editor_property('destination_name',texname);task.set_editor_property('automated',True);task.set_editor_property('save',False)
unreal.AssetToolsHelpers.get_asset_tools().import_asset_tasks([task])
tex=unreal.load_asset(root+'/'+texname)
assert tex
tex.set_editor_property('srgb',False)
tex.set_editor_property('compression_settings',unreal.TextureCompressionSettings.TC_VECTOR_DISPLACEMENTMAP)
tex.set_editor_property('filter',unreal.TextureFilter.TF_NEAREST)
tex.set_editor_property('mip_gen_settings',unreal.TextureMipGenSettings.TMGS_NO_MIPMAPS)
assert ea.save_loaded_asset(tex,only_if_is_dirty=False)
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
palette=scalar('PaletteOverride',0);plan=scalar('Plan',0);grade=scalar('GradeStrength',1)
color=custom(r'''
// R = byte index (0..7); G = family; A = actual mask. No colour noise here.
// All palette values are authored numeric linear RGB, not source images.
int i=clamp((int)round(Data.r*255.0/36.0),0,7);
float3 shallow[8]={float3(.025,.027,.020),float3(.056,.061,.041),float3(.105,.128,.081),float3(.165,.185,.115),float3(.255,.238,.142),float3(.085,.166,.146),float3(.30,.175,.067),float3(.092,.061,.10)};
float3 deep[8]={float3(.015,.020,.034),float3(.034,.046,.078),float3(.066,.092,.146),float3(.117,.148,.22),float3(.19,.235,.32),float3(.053,.151,.164),float3(.23,.105,.13),float3(.082,.051,.118)};
bool d=Choice>1.5 || (Choice<.5 && Data.g>.5);
float3 c=d?deep[i]:shallow[i];
float3 base=d?deep[2]:shallow[2];
float q=saturate(Plan/3);
c=lerp(c,base,q*.28*saturate(Grade));
return c*lerp(1.0,.76,q*saturate(Grade));
''',[('Data',data,'RGB'),('Choice',palette,''),('Plan',plan,''),('Grade',grade,'')],unreal.CustomMaterialOutputType.CMOT_FLOAT3)
output(color,'',mp.MP_BASE_COLOR)
output(data,'A',mp.MP_OPACITY_MASK)
output(const(0),'',mp.MP_EMISSIVE_COLOR)
output(const(0),'',mp.MP_SPECULAR);output(const(0),'',mp.MP_METALLIC);output(const(1),'',mp.MP_ROUGHNESS)
me.layout_material_expressions(mat);me.recompile_material(mat)
assert ea.save_loaded_asset(mat,only_if_is_dirty=False)
unreal.log('PIXEL_BACKGROUND_SAVED '+mat.get_path_name())
