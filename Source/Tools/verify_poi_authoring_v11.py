"""Headless asset roundtrip, not a gameplay test. Restores catalog after verification."""
import unreal,json,sys,uuid
from pathlib import Path
root=Path(unreal.Paths.project_dir());sys.path.insert(0,str(root/'Content/Python'))
import poi_authoring as author
ea=unreal.EditorAssetLibrary
source=author.ROOT+'/L_POI_Workshop_AST'
template=author.ROOT+'/L_POI_Template'
test=author.ROOT+'/Tests/L_ExportRoundtrip_'+uuid.uuid4().hex[:8]
if not ea.does_asset_exist(template):
    assert ea.duplicate_asset(source,template)
    world=unreal.EditorLoadingAndSavingUtils.load_map(template)
    es=author.actors()
    # Preserve editable blocks but remove old exporter actors and generated preview.
    for a in list(es.get_all_level_actors()):
        if isinstance(a,unreal.POIShaft):
            if a.get_editor_property('bAssembleModules'):
                a.clear_generated()
            else:
                for child in a.get_attached_actors():
                    child.detach_from_actor(unreal.DetachmentRule.KEEP_WORLD,unreal.DetachmentRule.KEEP_WORLD,unreal.DetachmentRule.KEEP_WORLD)
            es.destroy_actor(a)
    assert unreal.EditorLoadingAndSavingUtils.save_map(world,template)
assert unreal.EditorLoadingAndSavingUtils.load_map(template)
author.duplicate_map_unloaded(template,test)
assert unreal.EditorLoadingAndSavingUtils.load_map(test)
es=author.actors()
backgrounds=[a for a in es.get_all_level_actors() if author.background(a) and author.eligible(a)]
bg=next(a for a in backgrounds if 'MIDDLE_02' in a.get_actor_label())
# Arbitrary non-AST actor with authored transform and a reference to another retained actor.
prop=es.spawn_actor_from_class(unreal.StaticMeshActor,bg.get_actor_location()+unreal.Vector(-70,-20,-80))
prop.set_actor_label('Roundtrip_ArbitraryActor')
prop.static_mesh_component.set_static_mesh(unreal.load_asset('/Engine/BasicShapes/Cube'))
prop.set_actor_rotation(unreal.Rotator(pitch=12,yaw=27,roll=4),False)
prop.set_actor_scale3d(unreal.Vector(.4,.7,1.2))
prop.set_owner(bg)
es.set_selected_level_actors([bg])
table=author.get_catalog();previous=unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table)
published=[]
try:
    published=author.publish(0,False,quiet=True)
    assert len(published)==5
    data=json.loads(unreal.DataTableFunctionLibrary.export_data_table_to_json_string(table))
    assert any(published[0] in row.get('SectionLevel','') for row in data)
    assert unreal.EditorLoadingAndSavingUtils.load_map(next(p for p in published if 'MIDDLE_02' in p))
    stored=list(es.get_all_level_actors())
    saved=next(a for a in stored if a.get_actor_label()=='Roundtrip_ArbitraryActor')
    assert abs(saved.get_actor_rotation().yaw-27)<.01
    assert abs(saved.get_actor_scale3d().z-1.2)<.01
    assert abs(saved.get_actor_location().x+70)<.01
    assert abs(saved.get_actor_location().z-220)<.01
    assert saved.get_owner() and author.background(saved.get_owner())
    assert saved.get_owner().get_outer()==saved.get_outer(), 'Actor reference escaped the exported level'
    assert not any(isinstance(a,(unreal.POIShaft,unreal.PlayerStart,unreal.TextRenderActor)) for a in stored)
    assert not any(a.get_actor_label()=='Floor' for a in stored)
    report={'roundtrip':'PASS','arbitrary_actor_transform':'PASS','internal_reference':'PASS','test_world_helpers_excluded':'PASS',
            'exported_actor_labels':[a.get_actor_label() for a in stored if author.eligible(a)]}
    (root/'Saved/POICheckpoints/Authoring-roundtrip-v11.json').write_text(json.dumps(report,indent=2),encoding='utf8')
finally:
    unreal.DataTableFunctionLibrary.fill_data_table_from_json_string(table,previous);ea.save_loaded_asset(table,False)
    unreal.EditorLoadingAndSavingUtils.load_map(template)
    for path in published+[test]:
        if ea.does_asset_exist(path):ea.delete_asset(path)
unreal.log('POI_V11_ROUNDTRIP_OK')
