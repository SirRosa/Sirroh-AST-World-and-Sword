"""Recoverable project-relative checkpoint including editor Python and compiled module."""
from pathlib import Path
import zipfile,json,hashlib
root=Path(__file__).resolve().parents[2]
out=root/'Saved/POICheckpoints'; archive=out/'POI-v28-grip-and-fist-sweeps.zip'
files=[root/'Plugins/AST_InventorySystem/Content/UI/WeaponHUD.uasset',root/'Plugins/AST_InteractionSystem/Content/Actors/PullPushObject.uasset',root/'AGENTS.md',root/'AST_V1_18.uproject',root/'Plugins/AST_InteractionSystem/Content/Blueprints/ActorComponent/BPC_Interaction.uasset']
for folder in ['Source','Content/POIKit','Content/Python','Config','Plugins/BlueprintAnalyzer/Source','Plugins/BlueprintAnalyzer/Binaries/Win64']:
    files += [p for p in (root/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix not in ['.pdb','.lib','.exp']]
files += list((root/'Binaries/Win64').glob('UnrealEditor-Sirroh_Reimagineg_v3.dll'))
files += list((root/'Binaries/Win64').glob('UnrealEditor.modules'))
for name in ['Melee-v28.json','Inspect-v28.log','Before-v28-hud.zip','Pendulum-v28.log','HUD-v28.json','Build-v23.log','Assets-v23.log','Tests-v23.log','Push-v23-patched.json','Push-v23-physics.json','Before-v28-push.zip','Build-v22.log','Assets-v22.log','Tests-v22.log','Build-v28.log','Assets-v28.log','Tests-v28.log','Push-v28-patched.json','Push-v28-physics.json','Build-v20.log','Assets-v20.log','Tests-v20.log','Push-v20-patched.json','Push-v20-physics.json','Build-v19.log','Assets-v19.log','Push-v19-patched.json','Push-v19-IK.json','Build-v18.log','Assets-v18.log','Tests-v18.log','Push-v18-patched.json','Build-v17.log','Assets-v17.log','Tests-v17.log','Push-v17-patched.json','Build-v16-final.log','Assets-v16.log','Tests-v16.log','Push-v16-patched.json','Build-v15-final.log','Build-v15-resume.log','Tests-v15.log','Assets-v15-resume.log','Assets-v15.json','Pivot-v15.json','Push-v15-patched.json','Build-v14.log','Build-v14-final.log','Corridors-v14.log','Corridors-v14.json','Tests-v14.log','Build-v13.log','Corridors-v13.log','Corridors-v13.json','Corridors-v13-verify.log','Corridors-v13-verified.json','Build-v12.log','Tests-v12.log','Build-v11-fix.log','Roundtrip-v11-fix.log','Editor-export-v11-fix.log','Build-v11.log','Setup-v11.log','Roundtrip-v11.log','Authoring-v11.json','Authoring-roundtrip-v11.json','Tests-v11.log','Finalize-v11.log']:
    p=out/name
    if p.exists():files.append(p)
files += [p for p in (out/'POI-v28-tests').rglob('*') if p.is_file()]
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=3) as z:
    for p in sorted(set(files)):z.write(p,p.relative_to(root))
with zipfile.ZipFile(archive) as z:assert z.testzip() is None
manifest={'archive':str(archive),'files':len(set(files)),'bytes':archive.stat().st_size,'sha256':hashlib.sha256(archive.read_bytes()).hexdigest()}
(out/'POI-v28-manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
print(json.dumps(manifest,indent=2))















