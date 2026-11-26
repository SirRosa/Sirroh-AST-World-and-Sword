import unreal
from pathlib import Path
s=unreal.load_asset('/GASPALS/Characters/UEFN_Mannequin/Meshes/SK_UEFN_Mannequin')
t=unreal.AssetExportTask();t.object=s;t.filename=str(Path(unreal.Paths.project_saved_dir())/'CarryProps/Skeleton.t3d');t.automated=True;t.prompt=False;t.exporter=unreal.ObjectExporterT3D();unreal.Exporter.run_asset_export_task(t)
