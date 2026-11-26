"""Preserve existing authored fields, append to ten, then publish the five sets."""
import unreal,sys,json,gc,zipfile
from pathlib import Path
root=Path(unreal.Paths.project_dir())
sys.path.insert(0,str(root/'Content/Python'))
import poi_authoring as pa
es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
base='/Game/POIKit/Underground/Templates/'
categories=['Pushable_U','Entrance_U','Light_U','Empty_U','Parkour_U']
checkpoint=root/'Saved/POICheckpoints'
backup=checkpoint/'Templates-before-v14.zip'
if not backup.exists():
    with zipfile.ZipFile(backup,'w',zipfile.ZIP_DEFLATED) as z:
        for p in (root/'Content/POIKit/Underground/Templates').glob('*.umap'): z.write(p,p.relative_to(root))
report={}
for category in categories:
    path=base+'L_'+category
    assert unreal.EditorLoadingAndSavingUtils.load_map(path)
    def expand():
        actors=list(es.get_all_level_actors())
        backgrounds=sorted([a for a in actors if pa.background(a)],key=lambda a:a.get_actor_location().x)
        assert backgrounds
        initial=len(backgrounds)
        last_x=backgrounds[-1].get_actor_location().x
        for index in range(initial,10):
            source=backgrounds[(index-initial)%initial]
            region=pa.describe_region(source,actors)
            group=[a for a in actors if a.get_name() in region['names']]
            dx=last_x+(index-initial+1)*1300-source.get_actor_location().x
            copies=es.duplicate_actors(group,None,unreal.Vector(dx,0,0))
            assert len(copies)==len(group),(category,index,len(copies),len(group))
            prefix=category+'_'+str(index+1).zfill(2)
            source_prefix=source.get_actor_label().removesuffix('_Background')
            for a in copies:
                old=a.get_actor_label()
                suffix=old[len(source_prefix):] if old.startswith(source_prefix) else '_'+old
                a.set_actor_label(prefix+suffix)
                if pa.background(a): a.set_actor_label(prefix+'_Background')
        all_actors=list(es.get_all_level_actors())
        fields=sorted([a for a in all_actors if pa.background(a)],key=lambda a:a.get_actor_location().x)
        assert len(fields)>=10
        for i,bg in enumerate(fields):
            # Number every field prominently without changing its user-authored name.
            name='POI_FIELD_LABEL_'+str(i+1)
            label=next((a for a in all_actors if a.get_actor_label()==name),None)
            if not label:
                p=bg.get_actor_location()
                label=es.spawn_actor_from_class(unreal.TextRenderActor,unreal.Vector(p.x,-160,p.z+100))
                label.set_actor_label(name)
                label.set_actor_rotation(unreal.Rotator(pitch=0,yaw=-90,roll=0),False)
            label.text_render.set_text(category+' '+str(i+1).zfill(2)+' / '+str(len(fields)))
            label.text_render.set_world_size(60)
        floor=next((a for a in all_actors if a.get_actor_label()=='Floor'),None)
        if floor:
            left=fields[0].get_actor_location().x-650; right=fields[-1].get_actor_location().x+650
            floor.set_actor_location(unreal.Vector((left+right)/2,0,-27),False,True)
            floor.set_actor_scale3d(unreal.Vector((right-left)/100,4,.5))
        # Select all boundaries so pressing F immediately shows the whole set.
        es.set_selected_level_actors(fields)
        assert unreal.EditorLoadingAndSavingUtils.save_current_level()
        return {'before':initial,'after':len(fields)}
    report[category]=expand()
    gc.collect()
    report[category]['published']=len(pa.publish(1,kind='Underground',quiet=True))
    unreal.log('CORRIDORS_V14_SET_READY '+category)
(checkpoint/'Corridors-v14.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('CORRIDORS_V14_50_READY')
