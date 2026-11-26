import unreal,json
from pathlib import Path
out=Path(unreal.Paths.project_saved_dir())/'POICheckpoints/PushV15';out.mkdir(parents=True,exist_ok=True)
paths=['/AST_InteractionSystem/Blueprints/ActorComponent/BPC_Interaction','/AST_InteractionSystem/Character/AnimationNotify/AN_GrabPullPushObje']
for p in paths:
    if p.rsplit('/',1)[-1].split('.')[0] not in ['BPC_Interaction','AN_GrabPullPushObje']: continue
    bp=unreal.load_asset(p)
    analysis=unreal.BlueprintAnalyzerLibrary.analyze_blueprint(bp)
    (out/(bp.get_name()+'.json')).write_text(unreal.BlueprintAnalyzerLibrary.export_to_json(analysis),encoding='utf8')
unreal.log('PUSH_V15_INSPECTED')

