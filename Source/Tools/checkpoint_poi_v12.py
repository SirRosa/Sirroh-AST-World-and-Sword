"""Recoverable project-relative checkpoint including editor Python and compiled module."""
from pathlib import Path
import zipfile,json,hashlib
root=Path(__file__).resolve().parents[2]
out=root/'Saved/POICheckpoints'; archive=out/'POI-v12-shaft-terrain.zip'
files=[root/'AGENTS.md',root/'AST_V1_18.uproject']
for folder in ['Source','Content/POIKit','Content/Python','Config','Plugins/BlueprintAnalyzer/Source','Plugins/BlueprintAnalyzer/Binaries/Win64']:
    files += [p for p in (root/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix not in ['.pdb','.lib','.exp']]
files += list((root/'Binaries/Win64').glob('UnrealEditor-Sirroh_Reimagineg_v3.dll'))
files += list((root/'Binaries/Win64').glob('UnrealEditor.modules'))
for name in ['Build-v12.log','Tests-v12.log','Build-v11-fix.log','Roundtrip-v11-fix.log','Editor-export-v11-fix.log','Build-v11.log','Setup-v11.log','Roundtrip-v11.log','Authoring-v11.json','Authoring-roundtrip-v11.json','Tests-v11.log','Finalize-v11.log']:
    p=out/name
    if p.exists():files.append(p)
files += [p for p in (out/'POI-v12-tests').rglob('*') if p.is_file()]
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=3) as z:
    for p in sorted(set(files)):z.write(p,p.relative_to(root))
with zipfile.ZipFile(archive) as z:assert z.testzip() is None
manifest={'archive':str(archive),'files':len(set(files)),'bytes':archive.stat().st_size,'sha256':hashlib.sha256(archive.read_bytes()).hexdigest()}
(out/'POI-v12-manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
print(json.dumps(manifest,indent=2))
