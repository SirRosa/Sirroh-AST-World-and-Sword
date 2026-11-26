import unreal,json
from pathlib import Path
world=unreal.EditorLoadingAndSavingUtils.load_map('/Game/POIKit/L_POI_Workshop_AST')
count=0
for a in unreal.get_editor_subsystem(unreal.EditorActorSubsystem).get_all_level_actors():
 if isinstance(a,unreal.POIKitBlock):a.refresh_block();count+=1
assert unreal.EditorLoadingAndSavingUtils.save_map(world,'/Game/POIKit/L_POI_Workshop_AST')
(Path(unreal.Paths.project_saved_dir())/'POICheckpoints/Refresh-v07.json').write_text(json.dumps({'refreshed_blocks':count}),encoding='utf8')
