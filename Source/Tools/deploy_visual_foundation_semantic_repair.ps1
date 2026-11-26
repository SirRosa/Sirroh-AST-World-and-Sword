# Deploy only the pinned semantic iteration, preserving all older backups.
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$p='D:\0 Sirro\Unreal Projects\AST_V1_18'
$e='D:\0 Sirro\UE 5.8.1\UE_5.8'
$u=Join-Path $p 'AST_V1_18.uproject'
$d=Join-Path $p 'Saved/ChatGPT/VisualFoundation_Semantic_20261126'
$stage=Join-Path $d 'staging_c696188'
$backup=Join-Path $d 'backup_c696188'
$commit='c696188a624426779a2370a7b166dd91aa1d60ac'
$root='Content/Systems/Procedural_World_Generator/PWG_v2'
$legacy="$root/M_PWGv2_DepthAtmosphere.uasset"
$v1="$root/M_PWGv2_DepthAtmosphere_V2.uasset"
$semantic="$root/M_PWGv2_DepthAtmosphere_Semantic.uasset"
$save='Saved/SaveGames/PWGv2_asd.sav'
$copy='Saved/SaveGames/PWGv2_VF_SemanticProbe_20261126.sav'
$entries=@(
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2SemanticVerification.cpp';Old='8afc5c4d28134b6b3434b26f2ab7079eb9b3b332';New='12db938527af288f7cbbedf2d01f3e0229f67280'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2VisualMasks.cpp';Old='4019cee76ecae016aad938441542ee2c3022debf';New='46e8fda3329ef2924f2cf9fcc038360af8980f48'},
@{Path='Source/Sirroh_Reimagineg_v3/Public/Procedural_World_Generator/PWGv2Generator.h';Old='b5377a37ef5d1b9ba6765b18364bf3c8ea8f9e75';New='1db4217e10717011ad41b37e521e7ddd3cef7900'},
@{Path='Source/Tools/create_pwg_depth_atmosphere_semantic.py';Old='f94e1835a0a0ac2bbc4640b621c89d026eedae96';New='36f7738ba6a16132bdb9c4d11368b5bd6bf66916'}
)
New-Item -ItemType Directory -Path $d,$stage,$backup -Force | Out-Null
$result=Join-Path $d 'result_repair.txt'
"COMMIT=$commit" | Set-Content $result
function Note([string]$s){$s | Add-Content $result; Write-Output $s}
function GitHash([string]$f){$h=(& git hash-object --no-filters $f);if($LASTEXITCODE -ne 0){throw "git hash failed: $f"};return $h.Trim()}
function CheckNoEditor {if(Get-Process UnrealEditor,UnrealEditor-Cmd,AST_V1_18 -ErrorAction SilentlyContinue){throw 'Editor/game open; stopping without closing it'}}
function CheckOriginals {
 foreach($item in $protected.GetEnumerator()){
  if((Get-FileHash (Join-Path $p $item.Key) -Algorithm SHA256).Hash -ne $item.Value){throw "Protected file changed: $($item.Key)"}
 }
}
try{
 CheckNoEditor
 $protected=@{}
 foreach($rel in @($legacy,$v1,$save,'Saved/SaveGames/SirrohCharacter.sav')){
   if(Test-Path (Join-Path $p $rel)){ $protected[$rel]=(Get-FileHash (Join-Path $p $rel) -Algorithm SHA256).Hash }
 }
 if($protected[$legacy] -ne '565DB3F82136F15033C3F6B4652C8D26B1F1BF214AF1D85B64AAE956F64F3F62'){throw 'Legacy asset differs'}
 if($protected[$v1] -ne 'D9AFA66C19389C4D69A2BA6121FDCF40C0A1F4A2F740EC3021DE0741EF2E5820'){throw 'V1 asset differs'}
 if(!$protected.ContainsKey($save)){throw 'Missing source save'}
 foreach($x in $entries){
  $local=Join-Path $p $x.Path
  if($x.Old){if(!(Test-Path $local) -or (GitHash $local) -ne $x.Old){throw "Local mismatch $($x.Path)"}}
  elseif(Test-Path $local){throw "New path already exists $($x.Path)"}
 }
 if(!(Test-Path (Join-Path $p $semantic)) -or (Get-FileHash (Join-Path $p $semantic) -Algorithm SHA256).Hash -ne 'BB01CF848212F151FF78845A6C0C30959D327137C5ACBC25F867BDE5A93AE1B6'){throw 'Previous semantic asset differs'}
 $semanticBackup=Join-Path $backup $semantic
 New-Item -ItemType Directory -Path (Split-Path $semanticBackup) -Force|Out-Null
 Copy-Item (Join-Path $p $semantic) $semanticBackup
 if((Get-FileHash $semanticBackup -Algorithm SHA256).Hash -ne 'BB01CF848212F151FF78845A6C0C30959D327137C5ACBC25F867BDE5A93AE1B6'){throw 'Semantic backup mismatch'}
 if(Test-Path (Join-Path $p $copy)){throw 'Probe save already exists; inspect before retry'}
 Note 'LOCAL_PREFLIGHT=PASS'
 foreach($x in $entries){
  $dst=Join-Path $stage $x.Path
  New-Item -ItemType Directory -Path (Split-Path $dst) -Force | Out-Null
  Invoke-WebRequest -UseBasicParsing -Uri "https://raw.githubusercontent.com/SirRosa/Sirroh-AST-World-and-Sword/$commit/$($x.Path)" -OutFile $dst
  if((GitHash $dst) -ne $x.New){throw "Staging hash mismatch $($x.Path)"}
 }
 Note 'STAGING_HASHES=PASS'
 foreach($x in $entries){
  if(!$x.Old){continue}
  $dst=Join-Path $backup $x.Path
  New-Item -ItemType Directory -Path (Split-Path $dst) -Force | Out-Null
  Copy-Item (Join-Path $p $x.Path) $dst
  if((GitHash $dst) -ne $x.Old){throw "Backup hash mismatch $($x.Path)"}
 }
 foreach($item in $protected.GetEnumerator()){
  $dst=Join-Path $backup $item.Key
  New-Item -ItemType Directory -Path (Split-Path $dst) -Force | Out-Null
  Copy-Item (Join-Path $p $item.Key) $dst
  if((Get-FileHash $dst -Algorithm SHA256).Hash -ne $item.Value){throw "Backup hash mismatch $($item.Key)"}
 }
 Note 'BACKUPS=PASS'
 CheckNoEditor
 $script=(Join-Path $stage 'Source/Tools/create_pwg_depth_atmosphere_semantic.py').Replace('\','/')
 Note 'ASSET_AUTHOR=START'
 $ErrorActionPreference='Continue'
 & "$e\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $u -run=pythonscript "-script=$script" -VFRepairPriorSemantic -unattended -nop4 -AllowCommandletRendering -nosplash -stdout -FullStdOutLogOutput 2>&1 | Out-File -Encoding utf8 (Join-Path $d 'author_repair.log')
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop'
 Note "ASSET_AUTHOR_EXIT=$rc"
 if($rc -ne 0 -or !(Test-Path (Join-Path $p $semantic)) -or !(Select-String -Path (Join-Path $d 'author_repair.log') -Pattern 'VISUAL_FOUNDATION_V2_SAVED' -Quiet)){throw 'Semantic asset authoring failed; see author.log'}
 if(Select-String -Path (Join-Path $d 'author_repair.log') -Pattern 'Failed to compile Material.*DepthAtmosphere_Semantic|Error.*DepthAtmosphere_Semantic' -Quiet){throw 'Semantic material compile failure'}
 CheckOriginals
 Note "SEMANTIC_SHA256=$((Get-FileHash (Join-Path $p $semantic) -Algorithm SHA256).Hash)"
 CheckNoEditor
 foreach($x in $entries){
  $local=Join-Path $p $x.Path
  if($x.Old){if((GitHash $local) -ne $x.Old){throw "Changed during authoring: $($x.Path)"}}
  elseif(Test-Path $local){throw "New path appeared: $($x.Path)"}
 }
 foreach($x in $entries){
  $local=Join-Path $p $x.Path
  New-Item -ItemType Directory -Path (Split-Path $local) -Force | Out-Null
  Copy-Item (Join-Path $stage $x.Path) $local
  if((GitHash $local) -ne $x.New){throw "Deploy hash mismatch $($x.Path)"}
 }
 Note 'DEPLOYED_HASHES=PASS'
 CheckNoEditor
 Note 'BUILD=START'
 $ErrorActionPreference='Continue'
 & "$e\Engine\Build\BatchFiles\Build.bat" AST_V1_18Editor Win64 Development "-Project=$u" -WaitMutex -NoHotReloadFromIDE 2>&1 | Out-File -Encoding utf8 (Join-Path $d 'build_repair.log')
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop'
 Note "BUILD_EXIT=$rc"
 if($rc -ne 0){throw 'Build failed; see build.log'}
 CheckNoEditor
 Note 'TESTS=START'
 $ErrorActionPreference='Continue'
 & "$e\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $u '-ExecCmds=Automation RunTests Sirroh.PWG.VisualProfile+Sirroh.PWG.SpawnGeometryAudit+Sirroh.PWG.StreamingCost+Sirroh.POI.RoomChains' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$d/test_report" -unattended -nop4 -nullrhi -nosplash -stdout -FullStdOutLogOutput 2>&1 | Out-File -Encoding utf8 (Join-Path $d 'test_repair.log')
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop'
 Note "TEST_EXIT=$rc"
 $testLines=Select-String -Path (Join-Path $d 'test_repair.log') -Pattern 'Test Completed|Found [0-9]+ automation tests|No automation tests'
 foreach($line in $testLines){Note $line.Line}
 if($rc -ne 0 -or (Select-String -Path (Join-Path $d 'test_repair.log') -Pattern 'Result=\{Fail\}' -Quiet)){throw 'Technical tests failed'}
 foreach($n in @('VisualProfile','SpawnGeometryAudit','StreamingCost','RoomChains')){
  if(!($testLines | Where-Object {$_.Line -match 'Result=\{Success\}' -and $_.Line -match $n})){throw "No verified success for $n"}
 }
 Note 'TECHNICAL_TESTS=PASS'
 CheckOriginals
 CheckNoEditor
 Copy-Item (Join-Path $p $save) (Join-Path $p $copy)
 if((Get-FileHash (Join-Path $p $copy) -Algorithm SHA256).Hash -ne $protected[$save]){throw 'Probe save copy mismatch'}
 Note 'RENDER=START'
 $renderLog=Join-Path $d 'render_repair.log'
 $ErrorActionPreference='Continue'
 & "$e\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $u '/Game/Levels/L_SirrohGameplay' -game -PWGWorld=VF_SemanticProbe_20261126 -PWGVisualSemanticQA -windowed -ResX=1600 -ResY=900 -unattended -nop4 -nosound -nosplash -stdout -FullStdOutLogOutput '-ExecCmds=t.MaxFPS 60' 2>&1 | Out-File -Encoding utf8 $renderLog
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop'
 Note "RENDER_EXIT=$rc"
 $renderLines=Select-String -Path $renderLog -Pattern 'VF_RENDER_'
 foreach($line in $renderLines){Note $line.Line}
 CheckOriginals
 Note 'LEGACY_V1_AND_ORIGINAL_SAVES=UNCHANGED'
 if($rc -ne 0 -or !(Select-String -Path $renderLog -Pattern 'VF_RENDER_COMPLETE' -Quiet) -or (Select-String -Path $renderLog -Pattern 'VF_RENDER_FAIL' -Quiet)){throw 'Real render sequence failed'}
 foreach($n in @('01_legacy_dark','02_semantic_dark','03_masks_dark','04_no_ambient_dark','05_legacy_torch','06_semantic_torch')){
  $f=Join-Path $d "Render/$n.png"
  if(!(Test-Path $f) -or (Get-Item $f).Length -lt 10000){throw "Missing render $n"}
  Note "SCREENSHOT=$n $((Get-Item $f).Length) bytes"
 }
 Note 'RENDER_CAPTURES=PASS'
 Note 'VISUAL_ACCEPTANCE=PENDING_REVIEW'
}catch{Note "FAILED=$($_.Exception.Message)";exit 1}
