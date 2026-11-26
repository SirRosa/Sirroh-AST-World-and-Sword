import unreal,json,zipfile,gc
from pathlib import Path
root=Path(unreal.Paths.project_dir()); out=root/'Saved/POICheckpoints'; out.mkdir(parents=True,exist_ok=True)
ea=unreal.EditorAssetLibrary
backup=out/'Before-v15-assets.zip'
if not backup.exists():
    with zipfile.ZipFile(backup,'w',zipfile.ZIP_DEFLATED) as z:
        paths=list((root/'Content/POIKit').rglob('*.uasset'))+list((root/'Content/POIKit').rglob('*.umap'))
        paths+=[root/'Plugins/AST_InteractionSystem/Content/Blueprints/ActorComponent/BPC_Interaction.uasset']
        for p in paths: z.write(p,p.relative_to(root))
tools=unreal.AssetToolsHelpers.get_asset_tools()
material=unreal.load_asset('/Game/POIKit/M_POISight')
if not material:
    material=tools.create_asset('M_POISight','/Game/POIKit',unreal.Material,unreal.MaterialFactoryNew())
    material.set_editor_property('shading_model',unreal.MaterialShadingModel.MSM_UNLIT)
    material.set_editor_property('two_sided',True)
    color=unreal.MaterialEditingLibrary.create_material_expression(material,unreal.MaterialExpressionConstant3Vector)
    color.set_editor_property('constant',unreal.LinearColor(.006,.011,.007,1))
    unreal.MaterialEditingLibrary.connect_material_property(color,'',unreal.MaterialProperty.MP_EMISSIVE_COLOR)
    unreal.MaterialEditingLibrary.recompile_material(material)
    ea.save_loaded_asset(material,False)
centers={}
state=out/'Pivot-v15.json'
if state.exists(): centers=json.loads(state.read_text())
for key in ['Barrel','Crate','Bench']:
    mesh=unreal.load_asset('/Game/POIKit/Underground/Meshes/SM_Pushable_'+key)
    shift=unreal.POICorridorTools.center_push_mesh(mesh)
    if abs(shift.x)+abs(shift.y)+abs(shift.z)>.01: centers[key]=[shift.x,shift.y,shift.z]
    ea.save_loaded_asset(mesh,False)
    bp=unreal.POICorridorTools.create_pushable_variant('/Game/POIKit/Underground/Actors/BP_Pushable_U_'+key,mesh)
    assert bp; ea.save_loaded_asset(bp,False)
state.write_text(json.dumps(centers,indent=2))
assert unreal.POICorridorTools.patch_ast_push_geometry()
bp=unreal.load_asset('/AST_InteractionSystem/Blueprints/ActorComponent/BPC_Interaction')
ea.save_loaded_asset(bp,False)
analysis=unreal.BlueprintAnalyzerLibrary.analyze_blueprint(bp)
(out/'Push-v15-patched.json').write_text(unreal.BlueprintAnalyzerLibrary.export_to_json(analysis),encoding='utf8')
bp=analysis=None
es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
maps=list((root/'Content/POIKit').rglob('*.umap'))
changed=[]
for file in maps:
    if b'BP_Pushable_U_' not in file.read_bytes(): continue
    # Touch only maps that contain the custom pushable variants; never republish or discard authored layouts.
    path='/Game/'+file.relative_to(root/'Content').with_suffix('').as_posix()
    assert unreal.EditorLoadingAndSavingUtils.load_map(path)
    def migrate():
        dirty=False
        for a in es.get_all_level_actors():
            name=a.get_class().get_name()
            for key,offset in centers.items():
                if name=='BP_Pushable_U_'+key+'_C' and 'POIPivotV15' not in [str(t) for t in a.tags]:
                    shift=unreal.MathLibrary.transform_location(a.get_actor_transform(),unreal.Vector(*offset))-a.get_actor_location()
                    a.set_actor_location(a.get_actor_location()+shift,False,True)
                    a.set_editor_property('tags',list(a.tags)+['POIPivotV15']); dirty=True
        if dirty: assert unreal.EditorLoadingAndSavingUtils.save_current_level()
        return dirty
    if migrate(): changed.append(path)
    gc.collect()
(out/'Assets-v15.json').write_text(json.dumps({'pivot_offsets':centers,'updated_maps':changed},indent=2))
unreal.log('POI_V15_ASSETS_READY')
