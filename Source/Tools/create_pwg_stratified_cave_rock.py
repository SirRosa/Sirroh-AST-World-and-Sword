"""Audit original rock assets and author a separate physically lit rock material.
Never overwrite existing assets. A material-only diagnostic is disabled by default.
"""
import json
from pathlib import Path
import unreal
ea=unreal.EditorAssetLibrary
me=unreal.MaterialEditingLibrary
mp=unreal.MaterialProperty
reg=unreal.AssetRegistryHelpers.get_asset_registry()
out=Path(unreal.Paths.project_saved_dir())/'ChatGPT'/'StratifiedCave_20261126'
out.mkdir(parents=True,exist_ok=True)
reg.search_all_assets(True)
names={'S_Rock_shopk','Debris_Chunk1','Debris_Chunk2','Debris_Chunk3','ultkedqjw_LOD7','ultkedqjw_4K_Albedo','ultkedqjw_4K_Normal_LOD0','ultkedqjw_4K_Roughness','T_Rock_shopk_2K_D','T_Rock_shopk_2K_N','T_Rock_shopk_2K_DpR','MI_Rock_shopk_2K','M_PWGv2_BackgroundLit','M_PWGv2_CaveReferenceRock'}
found={}
for d in reg.get_assets_by_paths(['/Game','/AST_InventorySystem'],recursive=True):
    name=str(d.asset_name)
    if name in names:
        found.setdefault(name,[]).append(d)
report={}
for name,items in sorted(found.items()):
    report[name]=[]
    for d in items:
        o=d.get_asset()
        row=dict(path=o.get_path_name(),cls=o.get_class().get_name())
        if isinstance(o,unreal.Texture2D):
            row.update(width=o.blueprint_get_size_x(),height=o.blueprint_get_size_y(),srgb=o.get_editor_property('srgb'),compression=str(o.get_editor_property('compression_settings')))
        if isinstance(o,unreal.StaticMesh):
            row['slots']=[str(m.get_editor_property('material_interface').get_path_name()) if m.get_editor_property('material_interface') else None for m in o.get_editor_property('static_materials')]
            try: row['triangles_lod0']=o.get_num_triangles(0)
            except Exception as e: row['triangles_unavailable']=str(e)
        if isinstance(o,unreal.Material):
            row['inputs']={}
            for k in ['MP_BASE_COLOR','MP_NORMAL','MP_ROUGHNESS','MP_EMISSIVE_COLOR','MP_OPACITY_MASK']:
                n=me.get_material_property_input_node(o,getattr(mp,k))
                row['inputs'][k]=n.get_class().get_name() if n else None
            row['scalars']=[str(n) for n in me.get_scalar_parameter_names(o)]
            row['textures']=[str(n) for n in me.get_texture_parameter_names(o)]
        report[name].append(row)
(out/'asset_audit.json').write_text(json.dumps(report,indent=2),encoding='utf8')
def unique(name,cls):
    assert len(found.get(name,[]))==1,'Missing/ambiguous '+name
    o=found[name][0].get_asset()
    assert isinstance(o,cls),name
    return o
albedo=unique('T_Rock_shopk_2K_D',unreal.Texture2D)
normal=unique('T_Rock_shopk_2K_N',unreal.Texture2D)
assert normal.get_editor_property('compression_settings')==unreal.TextureCompressionSettings.TC_NORMALMAP
root='/Game/Systems/Procedural_World_Generator/PWG_v2'
name='M_PWGv2_StratifiedCaveRock'
assert not ea.does_asset_exist(root+'/'+name),'Existing target: inspect first'
mat=unreal.AssetToolsHelpers.get_asset_tools().create_asset(name,root,unreal.Material,unreal.MaterialFactoryNew())
assert mat
mat.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_DEFAULT_LIT)
mat.set_editor_property('two_sided',True)
def node(cls,**props):
    n=me.create_material_expression(mat,cls,0,0)
    assert n
    for k,v in props.items(): n.set_editor_property(k,v)
    return n
def scalar(n,v):
    return node(unreal.MaterialExpressionScalarParameter,parameter_name=n,default_value=v)
def const(v): return node(unreal.MaterialExpressionConstant,r=v)
def link(a,p,b,q): assert me.connect_material_expressions(a,p,b,q),(p,q)
def output(a,p,q): assert me.connect_material_property(a,p,q),str(q)
def custom(code,inputs,kind=unreal.CustomMaterialOutputType.CMOT_FLOAT3):
    n=node(unreal.MaterialExpressionCustom,code=code,output_type=kind)
    arr=[]
    for nm,_,_ in inputs:
        x=unreal.CustomInput();x.set_editor_property('input_name',nm);arr.append(x)
    n.set_editor_property('inputs',arr)
    for nm,a,p in inputs:link(a,p,n,nm)
    return n
uv=node(unreal.MaterialExpressionTextureCoordinate)
vc=node(unreal.MaterialExpressionVertexColor)
detail=scalar('CaveTextureIntensity',1.0)
debug=scalar('CaveDebugMode',0.0)
flat=node(unreal.MaterialExpressionVectorParameter,parameter_name='CaveFlatAlbedo',default_value=unreal.LinearColor(.16,.16,.17,1))
tex=node(unreal.MaterialExpressionTextureSampleParameter2D,parameter_name='RockAlbedo',texture=albedo,sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_COLOR)
norm=node(unreal.MaterialExpressionTextureSampleParameter2D,parameter_name='RockNormal',texture=normal,sampler_type=unreal.MaterialSamplerType.SAMPLERTYPE_NORMAL)
link(uv,'',tex,'UVs');link(uv,'',norm,'UVs')
color=custom(r'''
float2 p=UV*2.1;
float2 c=floor(p), f=frac(p);
float a=1e3,b=1e3;
[unroll] for(int j=-1;j<=1;j++) [unroll] for(int i=-1;i<=1;i++){
 float2 g=float2(i,j);
 float2 h=frac(sin(float2(dot(c+g,float2(127.1,311.7)),dot(c+g,float2(269.5,183.3))))*43758.5453);
 float d=length(g+h-f);
 if(d<a){b=a;a=d;}else b=min(b,d);
}
float crack=1-smoothstep(.018,.055,b-a);
float strata=0.5+0.5*sin(UV.y*37+sin(UV.x*9)*1.4);
float erosion=lerp(.84,1.12,strata);
float3 rock=lerp(dot(Tex,float3(.2126,.7152,.0722)).xxx,Tex,.35)*float3(.96,.94,1);
rock*=lerp(.80,1.13,Vertex.r)*erosion*lerp(1,.46,crack);
return saturate(lerp(Flat,rock,saturate(Detail)));
''',[('UV',uv,''),('Tex',tex,'RGB'),('Vertex',vc,''),('Flat',flat,''),('Detail',detail,'')])
nrm=custom('return normalize(lerp(float3(0,0,1),Tex,saturate(Detail)*.8));',[('Tex',norm,'RGB'),('Detail',detail,'')])
base=custom('return Color*(Mode<.5?1:0);',[('Color',color,''),('Mode',debug,'')])
wn=node(unreal.MaterialExpressionPixelNormalWS)
em=custom('if(Mode<.5)return 0; if(Mode<1.5)return Color; if(Mode<2.5)return NormalWS*.5+.5; return float3(frac(UV),0.15);',[('Color',color,''),('Mode',debug,''),('NormalWS',wn,''),('UV',uv,'')])
output(base,'',mp.MP_BASE_COLOR);output(nrm,'',mp.MP_NORMAL);output(em,'',mp.MP_EMISSIVE_COLOR)
output(const(.92),'',mp.MP_ROUGHNESS);output(const(0),'',mp.MP_METALLIC);output(const(.18),'',mp.MP_SPECULAR)
me.layout_material_expressions(mat);me.recompile_material(mat)
assert ea.save_loaded_asset(mat,only_if_is_dirty=False)
unreal.log('STRATIFIED_ROCK_SAVED '+mat.get_path_name())
