# Deploy only the pinned semantic iteration, preserving all older backups.
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$p='D:\0 Sirro\Unreal Projects\AST_V1_18'
$e='D:\0 Sirro\UE 5.8.1\UE_5.8'
$u=Join-Path $p 'AST_V1_18.uproject'
$d=Join-Path $p 'Saved/ChatGPT/VisualFoundation_Semantic_20261126'
$stage=Join-Path $d 'staging_5712a39'
$backup=Join-Path $d 'backup_5712a39'
$commit='5712a39c9f10e6fbf5fb1ff9d75329315dce60d5'
$root='Content/Systems/Procedural_World_Generator/PWG_v2'
$legacy="$root/M_PWGv2_DepthAtmosphere.uasset"
$v1="$root/M_PWGv2_DepthAtmosphere_V2.uasset"
$semantic="$root/M_PWGv2_DepthAtmosphere_Semantic.uasset"
$save='Saved/SaveGames/PWGv2_asd.sav'
$copy='Saved/SaveGames/PWGv2_VF_SemanticProbe_20261126.sav'
$entries=@(
@{Path='Source/Sirroh_Reimagineg_v3/Private/Debug/SirrohDebugSubsystem.cpp';Old='954a2f6a6d868f1bf15ac015684bf2fc9ce1c1fb';New='9e3a5f3c551298813280fa37373000f7800d08af'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2.cpp';Old='58902fd1bd10e2826cf5f8e8705bf6cba49636b7';New='80ca62bb91b53c9225309fbb79a275daaf14d5be'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2SemanticVerification.cpp';Old='';New='8afc5c4d28134b6b3434b26f2ab7079eb9b3b332'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGVisualProfileTests.cpp';Old='1fdf6378c1e68c3118def1145cb57f80bffbcf58';New='ef91ae1cc0b801924ea30433a0a2c7b001212e77'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Debug.cpp';Old='d6770882481b283b5e1e4c1ead6911ba6b81f575';New='663a3014f43cf4fb2882777845e42388da6b1168'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Decorations.cpp';Old='bb871a837b50e76d7b0e189e0e976e264e0ce793';New='fc00f7b45a8c51d9df329f4a254ddd2f0d93ae7a'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Generator.cpp';Old='282729b313d0df80042b1efca02d76f6952f7cf6';New='7043038aced6ff3a3f7e8131ea38d5a03e1a41c2'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Underground.cpp';Old='b8765ef87cc51dfa4a12318dc4c34131d43f25a7';New='e7182c2ab690d3a719b8a9c3d1fead17feb5f496'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2VisualMasks.cpp';Old='';New='4019cee76ecae016aad938441542ee2c3022debf'},
@{Path='Source/Sirroh_Reimagineg_v3/Public/PWG/PWGEnvironmentV2.h';Old='a2c04e8bfce12481e3afc63e9069feb4798fba2b';New='d595f871efaf79f82f4f2015cdc2a95de0828205'},
@{Path='Source/Sirroh_Reimagineg_v3/Public/Procedural_World_Generator/PWGv2Generator.h';Old='f4351d72b326d725f6776c90c3a431176b4b5fd8';New='b5377a37ef5d1b9ba6765b18364bf3c8ea8f9e75'},
@{Path='Source/Tools/create_pwg_depth_atmosphere_semantic.py';Old='';New='f94e1835a0a0ac2bbc4640b621c89d026eedae96'}
)
New-Item -ItemType Directory -Path $d,$stage,$backup -Force | Out-Null
$result=Join-Path $d 'result.txt'
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
 if(Test-Path (Join-Path $p $semantic)){throw 'Semantic asset already exists'}
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
 & "$e\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $u -run=pythonscript "-script=$script" -unattended -nop4 -AllowCommandletRendering -nosplash -stdout -FullStdOutLogOutput 2>&1 | Out-File -Encoding utf8 (Join-Path $d 'author.log')
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop'
 Note "ASSET_AUTHOR_EXIT=$rc"
 if($rc -ne 0 -or !(Test-Path (Join-Path $p $semantic)) -or !(Select-String -Path (Join-Path $d 'author.log') -Pattern 'VISUAL_FOUNDATION_V2_SAVED' -Quiet)){throw 'Semantic asset authoring failed; see author.log'}
 if(Select-String -Path (Join-Path $d 'author.log') -Pattern 'Failed to compile Material.*DepthAtmosphere_Semantic|Error.*DepthAtmosphere_Semantic' -Quiet){throw 'Semantic material compile failure'}
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
 & "$e\Engine\Build\BatchFiles\Build.bat" AST_V1_18Editor Win64 Development "-Project=$u" -WaitMutex -NoHotReloadFromIDE 2>&1 | Out-File -Encoding utf8 (Join-Path $d 'build.log')
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop'
 Note "BUILD_EXIT=$rc"
 if($rc -ne 0){throw 'Build failed; see build.log'}
 CheckNoEditor
 Note 'TESTS=START'
 $ErrorActionPreference='Continue'
 & "$e\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $u '-ExecCmds=Automation RunTests Sirroh.PWG.VisualProfile+Sirroh.PWG.SpawnGeometryAudit+Sirroh.PWG.StreamingCost+Sirroh.POI.RoomChains' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$d/test_report" -unattended -nop4 -nullrhi -nosplash -stdout -FullStdOutLogOutput 2>&1 | Out-File -Encoding utf8 (Join-Path $d 'test.log')
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop'
 Note "TEST_EXIT=$rc"
 $testLines=Select-String -Path (Join-Path $d 'test.log') -Pattern 'Test Completed|Found [0-9]+ automation tests|No automation tests'
 foreach($line in $testLines){Note $line.Line}
 if($rc -ne 0 -or (Select-String -Path (Join-Path $d 'test.log') -Pattern 'Result=\{Fail\}' -Quiet)){throw 'Technical tests failed'}
 foreach($n in @('VisualProfile','SpawnGeometryAudit','StreamingCost','RoomChains')){
  if(!($testLines | Where-Object {$_.Line -match 'Result=\{Success\}' -and $_.Line -match $n})){throw "No verified success for $n"}
 }
 Note 'TECHNICAL_TESTS=PASS'
 CheckOriginals
 CheckNoEditor
 Copy-Item (Join-Path $p $save) (Join-Path $p $copy)
 if((Get-FileHash (Join-Path $p $copy) -Algorithm SHA256).Hash -ne $protected[$save]){throw 'Probe save copy mismatch'}
 Note 'RENDER=START'
 $renderLog=Join-Path $d 'render.log'
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
