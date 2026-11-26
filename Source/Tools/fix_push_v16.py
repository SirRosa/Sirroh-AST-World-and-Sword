import unreal,json,zipfile
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/POICheckpoints'
paths=['/AST_InteractionSystem/Blueprints/ActorComponent/BPC_Interaction']+['/Game/POIKit/Underground/Actors/BP_Pushable_U_'+k for k in ['Barrel','Crate','Bench']]
backup=out/'Before-v16-push.zip'
if not backup.exists():
    with zipfile.ZipFile(backup,'w',zipfile.ZIP_DEFLATED) as z:
        files=[root/'Plugins/AST_InteractionSystem/Content/Blueprints/ActorComponent/BPC_Interaction.uasset']+list((root/'Content/POIKit/Underground/Actors').glob('BP_Pushable_U_*.uasset'))
        for p in files:z.write(p,p.relative_to(root))
assert unreal.POICorridorTools.patch_ast_push_geometry()
for key in ['Barrel','Crate','Bench']:
    mesh=unreal.load_asset('/Game/POIKit/Underground/Meshes/SM_Pushable_'+key)
    assert unreal.POICorridorTools.create_pushable_variant('/Game/POIKit/Underground/Actors/BP_Pushable_U_'+key,mesh)
for path in paths: assert unreal.EditorAssetLibrary.save_asset(path,False)
bp=unreal.load_asset(paths[0]); report=json.loads(unreal.BlueprintAnalyzerLibrary.export_to_json(unreal.BlueprintAnalyzerLibrary.analyze_blueprint(bp)))
thresholds=[n for n in report['Nodes'] if n['GraphName']=='Update PushPullCollision TransformWhileMove' and n['FunctionName']=='Greater_DoubleDouble']
assert len(thresholds)==2
assert sum(any(v.startswith('B=') and float(v[2:])<0 for v in n['LiteralValues']) for n in thresholds)==1
(out/'Push-v16-patched.json').write_text(json.dumps(report),encoding='utf8')
unreal.log('PUSH_V16_ASSETS_READY')
