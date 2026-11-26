import unreal,json,zipfile
from pathlib import Path
root=Path(unreal.Paths.project_dir());out=root/'Saved/POICheckpoints'
paths=['/AST_InteractionSystem/Blueprints/ActorComponent/BPC_Interaction','/AST_InteractionSystem/Actors/PullPushObject']+['/Game/POIKit/Underground/Actors/BP_Pushable_U_'+k for k in ['Barrel','Crate','Bench']]
backup=out/'Before-v20-push.zip'
if not backup.exists():
    with zipfile.ZipFile(backup,'w',zipfile.ZIP_DEFLATED) as z:
        files=[root/'Plugins/AST_InteractionSystem/Content/Blueprints/ActorComponent/BPC_Interaction.uasset',root/'Plugins/AST_InteractionSystem/Content/Actors/PullPushObject.uasset']+list((root/'Content/POIKit/Underground/Actors').glob('BP_Pushable_U_*.uasset'))
        for p in files:z.write(p,p.relative_to(root))
assert unreal.POICorridorTools.patch_ast_push_geometry()
for key in ['Barrel','Crate','Bench']:
    mesh=unreal.load_asset('/Game/POIKit/Underground/Meshes/SM_Pushable_'+key)
    assert unreal.POICorridorTools.create_pushable_variant('/Game/POIKit/Underground/Actors/BP_Pushable_U_'+key,mesh)
for path in paths: assert unreal.EditorAssetLibrary.save_asset(path,False)
bp=unreal.load_asset(paths[0]); report=json.loads(unreal.BlueprintAnalyzerLibrary.export_to_json(unreal.BlueprintAnalyzerLibrary.analyze_blueprint(bp)))
thresholds=[n for n in report['Nodes'] if n['GraphName']=='Update PushPullCollision TransformWhileMove' and n['FunctionName']=='Greater_DoubleDouble']
assert len(thresholds)==2
assert all(any(v.startswith('B=') and float(v[2:])==225 for v in n['LiteralValues']) for n in thresholds)
(out/'Push-v20-patched.json').write_text(json.dumps(report),encoding='utf8')
assert any(c['FromNodeGuid'].startswith('F887BB65') and c['FromPinName']=='else' and c['ToNodeGuid'].startswith('C3F22E94') for c in report['Connections'])
assert any(c['FromNodeGuid'].startswith('7752C505') and c['ToNodeGuid'].startswith('C3F22E94') and c['ToPinName']=='execute' for c in report['Connections'])
assert any(n['FunctionName']=='PushSupportEnd' and n['GraphName']=='CalculateBoxTransform' for n in report['Nodes'])
nodes={n['NodeGuid']:n for n in report['Nodes']}
assert any(nodes[c['FromNodeGuid']]['GraphName']=='Checking Interaction PullPushObje' and nodes[c['FromNodeGuid']]['FunctionName']=='Set  Interaction  IK' and nodes[c['ToNodeGuid']]['FunctionName']=='Play MoveToInteractionMontage' and c['ToPinName']=='execute' for c in report['Connections'])
push=json.loads(unreal.BlueprintAnalyzerLibrary.export_to_json(unreal.BlueprintAnalyzerLibrary.analyze_blueprint(unreal.load_asset(paths[1]))))
(out/'Push-v20-physics.json').write_text(json.dumps(push),encoding='utf8')
links=push['Connections']
assert any(c['FromNodeGuid'].startswith('4A4B774D') and c['FromPinName']=='NewEnumerator0' and c['ToNodeGuid'].startswith('F9646B39') for c in links)
assert any(c['FromNodeGuid'].startswith('4CB690A0') and c['FromPinName']=='then' and c['ToNodeGuid'].startswith('EC47A057') for c in links)
assert not any(c['ToNodeGuid'].startswith('F9646B39') and c['ToPinName']=='execute' and not c['FromNodeGuid'].startswith('4A4B774D') for c in links)
unreal.log('PUSH_V20_ASSETS_READY')
