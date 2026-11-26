import unreal,json
from pathlib import Path
out=Path(unreal.Paths.project_saved_dir())/'POICheckpoints/CorridorResearch';out.mkdir(parents=True,exist_ok=True)
es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
unreal.EditorLoadingAndSavingUtils.new_blank_map(False)
for name in ['PullPushObject','DoorActor']:
    path='/AST_InteractionSystem/Actors/'+name
    bp=unreal.load_asset(path);assert bp
    analysis=unreal.BlueprintAnalyzerLibrary.analyze_blueprint(bp)
    (out/(name+'.json')).write_text(unreal.BlueprintAnalyzerLibrary.export_to_json(analysis),encoding='utf8')
    actor=es.spawn_actor_from_class(bp.generated_class(),unreal.Vector())
    data={'class':actor.get_class().get_path_name(),'components':[]}
    for c in actor.get_components_by_class(unreal.SceneComponent):
        d={'name':c.get_name(),'class':c.get_class().get_name(),'transform':str(c.get_relative_transform()),'parent':c.get_attach_parent().get_name() if c.get_attach_parent() else None}
        if isinstance(c,unreal.StaticMeshComponent):d.update(mesh=c.static_mesh.get_path_name() if c.static_mesh else None,bounds=str(c.get_local_bounds()))
        data['components'].append(d)
    (out/(name+'-components.json')).write_text(json.dumps(data,indent=2),encoding='utf8')
    es.destroy_actor(actor)
unreal.log('CORRIDOR_RESEARCH_OK')
