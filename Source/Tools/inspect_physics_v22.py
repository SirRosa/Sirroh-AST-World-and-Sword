import unreal,json
from pathlib import Path
out=Path(unreal.Paths.project_saved_dir())/'POICheckpoints';out.mkdir(exist_ok=True,parents=True)

es=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
report=[]
for path in ['/Game/SirrohIntegration/BP_SirrohCharacter','/AST_InteractionSystem/Actors/PullPushObject','/Game/POIKit/Underground/Actors/BP_Pushable_U_Crate']:
 bp=unreal.load_asset(path)
 (out/(bp.get_name()+'-v22-graph.json')).write_text(unreal.BlueprintAnalyzerLibrary.export_to_json(unreal.BlueprintAnalyzerLibrary.analyze_blueprint(bp)),encoding='utf8')
 unreal.log('INSPECTING '+path)
 a=unreal.get_default_object(bp.generated_class())
 item={'asset':path,'components':[]}
 for c in a.get_components_by_class(unreal.ActorComponent):
  data={'name':c.get_name(),'class':c.get_class().get_path_name(),'properties':{}}
  for p in ['backend_class','movement_modes','shared_settings','body_instance','simulate_physics','mass','enable_physics_interaction','push_force_factor','initial_push_force_factor','touch_force_factor','replicate_physics_to_autonomous_proxy']:
   try:data['properties'][p]=str(c.get_editor_property(p))
   except Exception:pass
  if isinstance(c,unreal.PrimitiveComponent):
   data.update(collision=str(c.get_collision_enabled()),profile=str(c.get_collision_profile_name()),mass=c.get_mass(),simulate=c.is_simulating_physics())
  item['components'].append(data)
 report.append(item)
for name,path in [('Parkour','/AST_ParkourSystem/Blueprints/ActorComponent/BPC_ParkourSystem'),('MoverParent','/AST_StateManager/Blueprints/Pawn/SandboxCharacter_MoverPlayer_Ragdoll'),('MoverBase','/AST_StateManager/Blueprints/Pawn/SandboxCharacter_MoverBase'),('MoverPlayer','/AST_StateManager/Blueprints/Pawn/SandboxCharacter_MoverPlayer'),('SandboxMover','/Game/Blueprints/SandboxCharacter_Mover')]:
 bp=unreal.load_asset(path);(out/(name+'-v22-graph.json')).write_text(unreal.BlueprintAnalyzerLibrary.export_to_json(unreal.BlueprintAnalyzerLibrary.analyze_blueprint(bp)),encoding='utf8')
(out/'Physics-v22-inspection.json').write_text(json.dumps(report,indent=2),encoding='utf8')
unreal.log('PHYSICS_V22_INSPECTED')
