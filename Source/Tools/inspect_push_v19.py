import unreal
from pathlib import Path
out=Path(unreal.Paths.project_saved_dir())/'POICheckpoints'
for name,path in [('Mover','/Game/Blueprints/SandboxCharacter_Mover_ABP'),('Hands','/AST_StateManager/QuickSetup/HandLocationControl_ABP')]:
 bp=unreal.load_asset(path)
 (out/('Push-v19-'+name+'.json')).write_text(unreal.BlueprintAnalyzerLibrary.export_to_json(unreal.BlueprintAnalyzerLibrary.analyze_blueprint(bp)),encoding='utf8')
unreal.log('PUSH_V19_INSPECTED')
