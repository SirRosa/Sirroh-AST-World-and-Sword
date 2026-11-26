import unreal,json,sys
from pathlib import Path
sys.path.insert(0,str(Path(unreal.Paths.project_dir())/'Content/Python'))
import poi_authoring as author
es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
result={}
for category,count in [('Pushable_U',5),('Entrance_U',5),('Light_U',5),('Empty_U',1),('Parkour_U',1)]:
    assert unreal.EditorLoadingAndSavingUtils.load_map('/Game/POIKit/Underground/Templates/L_'+category)
    def inspect():
        aa=es.get_all_level_actors(); backgrounds=[a for a in aa if author.background(a)]
        assert len(backgrounds)==count,(category,len(backgrounds))
        regions=[author.describe_region(a,aa) for a in backgrounds]
        for r in regions:
            assert len(r['names'])>=2
        pushes=[a for a in aa if a.get_class().get_name().startswith('BP_Pushable_U_')]
        for a in pushes:
            cc={c.get_name():c for c in a.get_components_by_class(unreal.SceneComponent)}
            body=cc['PullPushObject']; assert 'SM_Pushable_' in body.static_mesh.get_name()
            for n in ['FrontWarpPoint','BackWarpPoint','LeftWarpPoint','RightWarpPoint']:
                assert n in cc
                assert abs(cc[n].get_world_location().z)<.1
            assert len([n for n in cc if n.startswith('ParkourActor')])==4
        torches=[a for a in aa if isinstance(a,unreal.POITorch)]
        if category=='Light_U':
            assert len(torches)==5
            for a in torches:
                assert a.light.intensity>0 and author.eligible(a)
                assert any(a.get_name() in r['names'] for r in regions)
        return {'regions':len(regions),'pushables':len(pushes),'torches':len(torches),'actors_per_region':[len(r['names']) for r in regions]}
    result[category]=inspect()
theme=unreal.load_asset('/Game/POIKit/Themes/DA_Underground')
assert len(theme.backgrounds.images)==10
assert theme.material.get_name()=='M_KitImageLit'
assert unreal.MaterialEditingLibrary.get_material_property_input_node(theme.material,unreal.MaterialProperty.MP_BASE_COLOR)
assert unreal.MaterialEditingLibrary.get_material_property_input_node(theme.material,unreal.MaterialProperty.MP_OPACITY_MASK)
(Path(unreal.Paths.project_saved_dir())/'POICheckpoints/Corridors-v13-verified.json').write_text(json.dumps(result,indent=2))
unreal.log('CORRIDORS_V13_RELOAD_VERIFIED')
