import unreal,json
from pathlib import Path
out={}
for kind,suffix in [('Torch','Stand_Relax_Idle'),('Barrel','Stand_Idle'),('Box','Stand_Idle')]:
 a=unreal.load_asset('/GASPALS/Systems/OverlaySystem/OverlayPose/'+kind+'/Pose_'+kind+'_'+suffix)
 names=unreal.AnimationLibrary.get_animation_curve_names(a,unreal.RawCurveTrackTypes.RCT_FLOAT)
 out[kind]={str(n):unreal.AnimationLibrary.get_float_value_at_time(a,n,0.0) for n in names}
(Path(unreal.Paths.project_saved_dir())/'CarryProps/SourcePoseCurves.json').write_text(json.dumps(out,indent=2),encoding='utf8')
