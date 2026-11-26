import unreal,json
from pathlib import Path
out={}
sub=unreal.get_engine_subsystem(unreal.SubobjectDataSubsystem)
for kind in ['Torch','Box','Barrel']:
 bp=unreal.load_asset('/GASPALS/Systems/EquipmentSystem/Actors/BP_Equipment_'+kind)
 out[kind]=[]
 for handle in sub.k2_gather_subobject_data_for_blueprint(bp):
  obj=unreal.SubobjectDataBlueprintFunctionLibrary.get_object(unreal.SubobjectDataBlueprintFunctionLibrary.get_data(handle))
  if isinstance(obj,unreal.StaticMeshComponent):
   out[kind].append(dict(name=obj.get_name(),location=str(obj.get_editor_property('relative_location')),rotation=str(obj.get_editor_property('relative_rotation')),scale=str(obj.get_editor_property('relative_scale3d'))))
(Path(unreal.Paths.project_saved_dir())/'CarryProps/SourceMeshTransforms.json').write_text(json.dumps(out,indent=2),encoding='utf8')
