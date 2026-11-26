import unreal, json
from pathlib import Path
out=Path(unreal.Paths.project_saved_dir())/'CarryProps'
def dump(obj, name):
    task=unreal.AssetExportTask()
    task.object=obj;task.filename=str(out/(name+'.t3d'));task.automated=True;task.prompt=False
    task.exporter=unreal.ObjectExporterT3D()
    unreal.Exporter.run_asset_export_task(task)
for kind in ['Torch','Barrel','Box']:
    obj=unreal.load_asset('/GASPALS/Systems/EquipmentSystem/Data/DA_Equipment_'+kind)
    if obj: dump(obj,'Equipment_'+kind)
    bp=unreal.load_asset('/GASPALS/Systems/OverlaySystem/OverlayPose/'+kind+'/ABP_Overlay_'+kind)
    if bp: dump(unreal.get_default_object(bp.generated_class()),'Overlay_'+kind)
base=unreal.load_asset('/GASPALS/Systems/OverlaySystem/OverlayPose/ABP_OverlayPose_Base')
(out/'OverlayBase.json').write_text(unreal.BlueprintAnalyzerLibrary.export_to_json(unreal.BlueprintAnalyzerLibrary.analyze_blueprint(base)),encoding='utf8')
unreal.log('CARRY_DEFAULTS_COMPLETE')
