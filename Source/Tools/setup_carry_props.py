"""Create AST carry assets; never modify authored maps or the Lyra source project."""
import unreal, json
from pathlib import Path

ROOT='/Game/POIKit/CarryProps'
EA=unreal.EditorAssetLibrary
tools=unreal.AssetToolsHelpers.get_asset_tools()
report={}
for kind in ['Torch','Barrel','Box']:
    mesh=unreal.load_asset('/GASPALS/Systems/EquipmentSystem/Assets/Meshes/'+kind)
    assert mesh, kind
    assert unreal.SirrohCarryPoseComponent.prepare_imported_mesh(mesh)
    assert EA.save_loaded_asset(mesh,False)
    name='BP_AST_'+kind
    path=ROOT+'/'+name
    bp=unreal.load_asset(path) if EA.does_asset_exist(path) else None
    if not bp:
        factory=unreal.BlueprintFactory()
        factory.set_editor_property('parent_class',unreal.SirrohTorch if kind=='Torch' else unreal.SirrohCarryProp)
        bp=tools.create_asset(name,ROOT,unreal.Blueprint,factory)
    assert bp
    cdo=unreal.get_default_object(bp.generated_class())
    pose=cdo.get_editor_property('carry_pose')
    assert pose.configure_imported_pose(kind), kind+' overlay/socket'
    if kind!='Torch':
        cdo.get_editor_property('prop_mesh').set_static_mesh(mesh)
        cdo.set_editor_property('mass_kg',18.0 if kind=='Barrel' else 8.0)
    else:
        cdo.set_editor_property('flame_tip',unreal.Vector(0,0,47))
        fire=cdo.get_editor_property('fire')
        fire.set_editor_property('PreferredMeshComponentName','FlameHead')
        fire.set_editor_property('PointOffset',unreal.Vector(0,0,0))
        fire.set_editor_property('bEnableProceduralFlames',True)
        fire.set_editor_property('bEnableNiagara',True)
    unreal.BlueprintEditorLibrary.compile_blueprint(bp)
    assert EA.save_loaded_asset(bp,False)
    pose=unreal.get_default_object(bp.generated_class()).get_editor_property('carry_pose')
    report[kind]={'asset':path,'mesh':mesh.get_path_name(),'grip_bone':str(pose.get_editor_property('GripBone')),
                  'grip_offset':str(pose.get_editor_property('GripOffset')),'standing_pose':pose.get_editor_property('StandingIdle').get_path_name(),
                  'moving_pose':pose.get_editor_property('StandingMove').get_path_name(),'crouching_pose':pose.get_editor_property('Crouching').get_path_name(),
                  'mirror_pose':pose.get_editor_property('bMirrorPose'),'two_handed':pose.get_editor_property('bTwoHanded')}
# Named light template is the same lootable weapon, without a second light actor.
path=ROOT+'/BP_POI_Light_Template'
if not EA.does_asset_exist(path):
    factory=unreal.BlueprintFactory()
    factory.set_editor_property('parent_class',unreal.load_class(None,ROOT+'/BP_AST_Torch.BP_AST_Torch_C'))
    template=tools.create_asset('BP_POI_Light_Template',ROOT,unreal.Blueprint,factory)
    assert template and EA.save_loaded_asset(template,False)
template=unreal.load_asset(path)
cdo=unreal.get_default_object(template.generated_class())
fire=cdo.get_editor_property('fire')
fire.set_editor_property('PreferredMeshComponentName','FlameHead')
fire.set_editor_property('PointOffset',unreal.Vector(0,0,0))
fire.set_editor_property('bEnableProceduralFlames',True)
fire.set_editor_property('bEnableNiagara',True)
unreal.BlueprintEditorLibrary.compile_blueprint(template)
assert EA.save_loaded_asset(template,False)
out=Path(unreal.Paths.project_saved_dir())/'CarryProps'
(out/'integration-assets.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('CARRY_ASSETS_READY')

world=unreal.load_asset('/Game/POIKit/Underground/Templates/L_Light_U')
if world:
 for actor in unreal.GameplayStatics.get_all_actors_of_class(world,unreal.SirrohTorch):
  unreal.log('TORCH_INSTANCE '+actor.get_path_name()+' SCALE '+str(actor.get_actor_scale3d()))
  unreal.log('TORCH_BOUNDS '+str(actor.get_editor_property('torch_mesh').get_local_bounds()))
