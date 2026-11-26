# Local visual-only shaft material correction; preserve tested source and original saves.
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$p='D:\0 Sirro\Unreal Projects\AST_V1_18'
$e='D:\0 Sirro\UE 5.8.1\UE_5.8'
$d=Join-Path $p 'Saved/ChatGPT/VisualFoundation_POI_20261126'
$b=Join-Path $d 'backup_c6573cc'
$u=Join-Path $p 'AST_V1_18.uproject'
$r=Join-Path $d 'result_seam.txt'
$rel='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Underground.cpp'
$commit='876ec0fc15ff95b6bb3afc4a2202f86d25de08fb'
"COMMIT=$commit" | Set-Content $r
function Note($s){$s | Add-Content $r; Write-Output $s}
function CheckNoEditor{if(Get-Process UnrealEditor,UnrealEditor-Cmd,AST_V1_18 -ErrorAction SilentlyContinue){throw 'Other editor/game open; stop without closing it'}}
function GitHash($f){$h=& git hash-object --no-filters $f;if($LASTEXITCODE -ne 0){throw "git hash failed $f"};return $h.Trim()}
$protected=@('Content/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_DepthAtmosphere.uasset','Content/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_DepthAtmosphere_V2.uasset','Content/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_DepthAtmosphere_Semantic.uasset','Content/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_DepthAtmosphere_SemanticPOI.uasset','Saved/SaveGames/PWGv2_asd.sav','Saved/SaveGames/SirrohCharacter.sav')
function CheckOriginals{foreach($f in $protected){if(Test-Path (Join-Path $b $f)){if((Get-FileHash (Join-Path $p $f)).Hash -ne (Get-FileHash (Join-Path $b $f)).Hash){throw "Protected changed: $f"}}}}
try{
CheckNoEditor;CheckOriginals
$local=Join-Path $p $rel
if((GitHash $local) -ne '23408fa00710545e81225c10c5689a0375d4e97a'){throw 'Prior tested source differs'}
$stage=Join-Path $d 'shaft_seam_stage.cpp'
Invoke-WebRequest -UseBasicParsing -Uri "https://raw.githubusercontent.com/SirRosa/Sirroh-AST-World-and-Sword/$commit/$rel" -OutFile $stage
if((GitHash $stage) -ne 'df0d654e963191731fc04daf6bfd74f45ab61a6b'){throw 'New source hash mismatch'}
$prev=Join-Path $d 'backup_876ec0f'
$dest=Join-Path $prev $rel
New-Item -ItemType Directory -Path (Split-Path $dest) -Force | Out-Null
Copy-Item $local $dest
if((GitHash $dest) -ne '23408fa00710545e81225c10c5689a0375d4e97a'){throw 'Backup mismatch'}
CheckNoEditor
Copy-Item $stage $local
if((GitHash $local) -ne 'df0d654e963191731fc04daf6bfd74f45ab61a6b'){throw 'Deploy mismatch'}
Note 'DEPLOY_HASH_BACKUP=PASS'
$ErrorActionPreference='Continue'
& "$e\Engine\Build\BatchFiles\Build.bat" AST_V1_18Editor Win64 Development "-Project=$u" -WaitMutex -NoHotReloadFromIDE 2>&1 | Out-File -Encoding utf8 (Join-Path $d 'build_seam.log')
$rc=$LASTEXITCODE
$ErrorActionPreference='Stop';Note "BUILD_EXIT=$rc";if($rc -ne 0){throw 'Build failed'}
CheckOriginals;CheckNoEditor
$old=Join-Path $d 'Render'
$archive=Join-Path $d 'Render_before_shaft_material'
if(Test-Path $archive){throw 'Render archive exists'}
if(!(Test-Path $old)){throw 'Prior render directory missing'}
Move-Item $old $archive
$src=Join-Path $p 'Saved/SaveGames/PWGv2_asd.sav'
$copy=Join-Path $p 'Saved/SaveGames/PWGv2_VF_POIProbe_20261126.sav'
if(Test-Path $copy){Copy-Item $copy (Join-Path $d 'probe_before_shaft_material.sav')}
Copy-Item $src $copy -Force
if((Get-FileHash $copy).Hash -ne (Get-FileHash $src).Hash){throw 'Probe copy mismatch'}
Note 'RENDER=START'
$log=Join-Path $d 'render_seam.log'
$ErrorActionPreference='Continue'
& "$e\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $u '/Game/Levels/L_SirrohGameplay' -game -SirrohSkipMainMenu -PWGWorld=VF_POIProbe_20261126 -PWGVisualPOIQA -windowed -ResX=1600 -ResY=900 -unattended -nop4 -nosound -nosplash -stdout -FullStdOutLogOutput '-ExecCmds=t.MaxFPS 60' 2>&1 | Out-File -Encoding utf8 $log
$rc=$LASTEXITCODE
$ErrorActionPreference='Stop';Note "RENDER_EXIT=$rc"
foreach($line in (Select-String -Path $log -Pattern 'VF_RENDER_|VF_GPU|VF_LANE_|VF_COVERAGE|VF_MASK_COUNTS|VF_POI_MASK|VF_SCENE')){Note $line.Line}
CheckOriginals
Note 'LEGACY_V1_V2_AND_ORIGINAL_SAVES=UNCHANGED'
if($rc -ne 0 -or !(Select-String -Path $log -Pattern 'VF_RENDER_COMPLETE' -Quiet) -or (Select-String -Path $log -Pattern 'VF_RENDER_FAIL|VF_RENDER_SCENE_UNAVAILABLE' -Quiet)){throw 'Real render sequence failed'}
foreach($n in @('Tunnel_Legacy','Tunnel_Atmospheric','Tunnel_Masks','Tunnel_Dark','Tunnel_Warm','Tunnel_TorchDark','Tunnel_TorchAtmospheric','Tunnel_LaneNear','Shaft_Legacy','Shaft_Atmospheric','Shaft_Masks','POI_Legacy','POI_Atmospheric','POI_Masks','POI_Dark','POI_Warm','Transition_Legacy','Transition_Atmospheric','Transition_Masks')){ $f=Join-Path $d "Render/$n.png"; if(!(Test-Path $f)){throw "Missing render $n"}; Note "SCREENSHOT=$n $((Get-Item $f).Length) bytes" }
Note 'RENDER_CAPTURES=PASS'
}catch{Note "FAILED=$($_.Exception.Message)";exit 1}
