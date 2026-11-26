import unreal,json
from pathlib import Path
out={}
cls=unreal.load_class(None,'/AST_ParkourSystem/Blueprints/Actor/ParkourActor.ParkourActor_C');cdo=unreal.get_default_object(cls)
for prop in ['LedgeType','ClimbStyle']:
 value=cdo.get_editor_property(prop)
 out[prop]={'default':str(value),'values':[(str(v),v.value) for v in type(value)]}
for name in ['BPC_ParkourSystem','BPC_StateManager']:
 package='AST_ParkourSystem' if name=='BPC_ParkourSystem' else 'AST_StateManager'
 bp=unreal.load_asset('/'+package+'/Blueprints/ActorComponent/'+name)
 data=unreal.BlueprintAnalyzerLibrary.analyze_blueprint(bp)
 (Path(unreal.Paths.project_saved_dir())/'POICheckpoints'/('v07-'+name+'.json')).write_text(unreal.BlueprintAnalyzerLibrary.export_to_json(data),encoding='utf8')
mat=unreal.load_asset('/Game/POIKit/M_KitImageFit');out['material_errors']=str(unreal.MaterialEditingLibrary.get_material_default_vector_parameter_value(mat,'UVRect'))
(Path(unreal.Paths.project_saved_dir())/'POICheckpoints/AST-enums-v07.json').write_text(json.dumps(out,indent=2),encoding='utf8')
