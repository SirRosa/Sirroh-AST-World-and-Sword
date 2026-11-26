"""Create a recoverable project-relative checkpoint after build and verification."""
from pathlib import Path
import zipfile,json,hashlib
root=Path(__file__).resolve().parents[2]
out=root/'Saved/POICheckpoints'
archive=out/'POI-v05-parkour.zip'
files=[]
for folder in ['Source','Content/POIKit','Plugins/BlueprintAnalyzer/Source','Plugins/BlueprintAnalyzer/Binaries/Win64']:
 files += [p for p in (root/folder).rglob('*') if p.is_file() and '__pycache__' not in p.parts and p.suffix not in ['.pdb','.lib','.exp']]
files += list((root/'Binaries/Win64').glob('UnrealEditor-Sirroh_Reimagineg_v3.dll'))
files += list((root/'Binaries/Win64').glob('UnrealEditor.modules'))
for name in ['Build-v05.log','Art-v05.json','Direction-v05.json','Parkour-hang-v05.json']:
 p=out/name
 if p.exists():files.append(p)
for folder in ['Parkour-controller-v05','POI-v05-tests']:
 if (out/folder).exists():files += [p for p in (out/folder).rglob('*') if p.is_file()]
with zipfile.ZipFile(archive,'w',zipfile.ZIP_DEFLATED,compresslevel=3) as z:
 for p in sorted(set(files)):z.write(p,p.relative_to(root))
with zipfile.ZipFile(archive) as z:assert z.testzip() is None
manifest={'archive':str(archive),'files':len(set(files)),'bytes':archive.stat().st_size,'sha256':hashlib.sha256(archive.read_bytes()).hexdigest()}
(out/'POI-v05-manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf8')
print(json.dumps(manifest,indent=2))
