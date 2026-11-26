"""Read-only source inspection. Never save packages through the temporary source mount."""
import unreal, json
from pathlib import Path
out = Path(unreal.Paths.project_saved_dir()) / 'CarryProps'
out.mkdir(parents=True, exist_ok=True)
r = unreal.AssetRegistryHelpers.get_asset_registry()
r.search_all_assets(True)
roots = []
for kind in ['Torch', 'Box', 'Barrel']:
    roots += ['/GASPALS/Systems/EquipmentSystem/Assets/Meshes/'+kind]
    roots += [str(a.package_name) for a in r.get_assets_by_path('/GASPALS/Systems/OverlaySystem/OverlayPose/'+kind, True)]
    for path in ['/GASPALS/Systems/EquipmentSystem/Actors/BP_Equipment_'+kind,
                 '/GASPALS/Systems/OverlaySystem/OverlayPose/'+kind+'/ABP_Overlay_'+kind]:
        bp = unreal.load_asset(path)
        if bp:
            (out/(bp.get_name()+'.json')).write_text(unreal.BlueprintAnalyzerLibrary.export_to_json(unreal.BlueprintAnalyzerLibrary.analyze_blueprint(bp)),encoding='utf8')
    mesh=unreal.load_asset(roots[-1]) if False else unreal.load_asset('/GASPALS/Systems/EquipmentSystem/Assets/Meshes/'+kind)
    if mesh:
        (out/(kind+'-mesh.txt')).write_text(str(mesh.get_bounds())+'\n'+str(mesh.get_editor_property('static_materials')),encoding='utf8')
seen=set(); pending=list(roots)
options=unreal.AssetRegistryDependencyOptions(include_soft_package_references=True,include_hard_package_references=True)
while pending:
    path=pending.pop()
    if path in seen: continue
    seen.add(path)
    if path.startswith('/GASPALS/'):
        pending += [str(p) for p in r.get_dependencies(path,options)]
(out/'lyra-dependencies.json').write_text(json.dumps(sorted(seen),indent=2),encoding='utf8')
for path in ['/AST_StateManager/Character/Animations/AnimationLayers/OverlayGraph',
 '/AST_StateManager/Blueprints/Components/StateActorComponent']:
    bp=unreal.load_asset(path)
    if bp:
        (out/(bp.get_name()+'.json')).write_text(unreal.BlueprintAnalyzerLibrary.export_to_json(unreal.BlueprintAnalyzerLibrary.analyze_blueprint(bp)),encoding='utf8')
unreal.log('LYRA_CARRY_INSPECTION_COMPLETE')
