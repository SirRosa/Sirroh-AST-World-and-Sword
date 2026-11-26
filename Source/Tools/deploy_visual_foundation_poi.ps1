# Deploy only the pinned semantic iteration, preserving all older backups.
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$p='D:\0 Sirro\Unreal Projects\AST_V1_18'
$e='D:\0 Sirro\UE 5.8.1\UE_5.8'
$u=Join-Path $p 'AST_V1_18.uproject'
$d=Join-Path $p 'Saved/ChatGPT/VisualFoundation_POI_20261126'
$stage=Join-Path $d 'staging_61dd75b'
$backup=Join-Path $d 'backup_61dd75b'
$commit='61dd75b1e944c457b1cf6b1e2fce2730f2c40ed2'
$root='Content/Systems/Procedural_World_Generator/PWG_v2'
$legacy="$root/M_PWGv2_DepthAtmosphere.uasset"
$v1="$root/M_PWGv2_DepthAtmosphere_V2.uasset"
$priorSemantic="$root/M_PWGv2_DepthAtmosphere_Semantic.uasset"
$semantic="$root/M_PWGv2_DepthAtmosphere_SemanticPOI.uasset"
$save='Saved/SaveGames/PWGv2_asd.sav'
$copy='Saved/SaveGames/PWGv2_VF_POIProbe_20261126.sav'
$entries=@(
@{Path='Source/Sirroh_Reimagineg_v3/Private/Debug/SirrohDebugSubsystem.cpp';Old='9e3a5f3c551298813280fa37373000f7800d08af';New='1a2c8ad42c85aa7cee201c5e76ec6ab37c744d01'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/POI/POIKitBlock.cpp';Old='7b728cd287f40bb790752fc5ddc7a58bccabf962';New='fc20bd97f42d0fa93ec6fce2102c80d02409f63a'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/POI/POIShaft.cpp';Old='fab257f028c66d96888e0c5692db5b5876cfc976';New='bd2eed5169181d44ac3b484c5086367f8d600885'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2.cpp';Old='80ca62bb91b53c9225309fbb79a275daaf14d5be';New='d8ca53b341dfe3f604d481a3ab0634288c17e3d2'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2SemanticVerification.cpp';Old='12db938527af288f7cbbedf2d01f3e0229f67280';New='b3c24c21f14b07fa321658ac359085215fe44a36'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGVisualProfileTests.cpp';Old='ef91ae1cc0b801924ea30433a0a2c7b001212e77';New='e474d52571e3c5bcd5d7e8744e4999958851f454'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2AuthoredPOI.cpp';Old='15bebdd424d3e6af029cef0fd65ed16efb7f2c9f';New='934502f0fbcaf1e8ddd8dd329e35bbce956fa323'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Decorations.cpp';Old='fc00f7b45a8c51d9df329f4a254ddd2f0d93ae7a';New='628f3adf7e2ad64cf30740a49511a9327f8036c8'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Underground.cpp';Old='e7182c2ab690d3a719b8a9c3d1fead17feb5f496';New='23408fa00710545e81225c10c5689a0375d4e97a'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2VisualMasks.cpp';Old='46e8fda3329ef2924f2cf9fcc038360af8980f48';New='3bb1ade3214f974d1f1aec291a4be73a52af3117'},
@{Path='Source/Sirroh_Reimagineg_v3/Public/POI/POIKitBlock.h';Old='f3c7f793a36f5ab17f22b48c92157182569ee0fb';New='2cd22e5d48fd39e736030e8f3d87e6c85f33013f'},
@{Path='Source/Sirroh_Reimagineg_v3/Public/POI/POIShaft.h';Old='c1539312f66edc9ff3c67b4726a6ff7dd1856bcc';New='130b25f2c1b3ca21f9257ae29927cc8a5daa9e7f'},
@{Path='Source/Sirroh_Reimagineg_v3/Public/PWG/PWGEnvironmentV2.h';Old='d595f871efaf79f82f4f2015cdc2a95de0828205';New='239cd90b543a4924b3412744709a9ec41996b57f'},
@{Path='Source/Sirroh_Reimagineg_v3/Public/Procedural_World_Generator/PWGv2Generator.h';Old='1db4217e10717011ad41b37e521e7ddd3cef7900';New='11d6d44f01148e9740988ef958124f9a45116e19'},
@{Path='Source/Sirroh_Reimagineg_v3/Sirroh_Reimagineg_v3.Build.cs';Old='afbe5dfdb44a08d0b284fb1296af86bcad4ea44f';New='6f3091ec9a1692e2b00ab9e8ac476e55977de0f4'},
@{Path='Source/Tools/create_pwg_depth_atmosphere_semantic_poi.py';Old='';New='a9e54eedd0dcbe236cf5940638a44756c25bf7fc'}
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
 foreach($rel in @($legacy,$v1,$priorSemantic,$save,'Saved/SaveGames/SirrohCharacter.sav')){
   if(Test-Path (Join-Path $p $rel)){ $protected[$rel]=(Get-FileHash (Join-Path $p $rel) -Algorithm SHA256).Hash }
 }
 if($protected[$legacy] -ne '565DB3F82136F15033C3F6B4652C8D26B1F1BF214AF1D85B64AAE956F64F3F62'){throw 'Legacy asset differs'}
 if($protected[$v1] -ne 'D9AFA66C19389C4D69A2BA6121FDCF40C0A1F4A2F740EC3021DE0741EF2E5820'){throw 'V1 asset differs'}
 if($protected[$priorSemantic] -ne 'D736F40A523F9DB74F43F5B12C3C6AC8B4332B0A38E6433922C1698064DC8F34'){throw 'Previous semantic asset differs'}
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
 $script=(Join-Path $stage 'Source/Tools/create_pwg_depth_atmosphere_semantic_poi.py').Replace('\','/')
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
 & "$e\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $u '/Game/Levels/L_SirrohGameplay' -game -PWGWorld=VF_POIProbe_20261126 -PWGVisualPOIQA -windowed -ResX=1600 -ResY=900 -unattended -nop4 -nosound -nosplash -stdout -FullStdOutLogOutput '-ExecCmds=t.MaxFPS 60' 2>&1 | Out-File -Encoding utf8 $renderLog
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop'
 Note "RENDER_EXIT=$rc"
 $renderLines=Select-String -Path $renderLog -Pattern 'VF_RENDER_|VF_GPU|VF_LANE_|VF_COVERAGE|VF_MASK_COUNTS|VF_POI_MASK|VF_SCENE'
 foreach($line in $renderLines){Note $line.Line}
 CheckOriginals
 Note 'LEGACY_V1_V2_AND_ORIGINAL_SAVES=UNCHANGED'
 if($rc -ne 0 -or !(Select-String -Path $renderLog -Pattern 'VF_RENDER_COMPLETE' -Quiet) -or (Select-String -Path $renderLog -Pattern 'VF_RENDER_FAIL|VF_RENDER_SCENE_UNAVAILABLE' -Quiet)){throw 'Real render sequence failed'}
 foreach($n in @('Tunnel_Legacy','Tunnel_Atmospheric','Tunnel_Masks','Tunnel_Dark','Tunnel_Warm','Tunnel_TorchDark','Tunnel_TorchAtmospheric','Tunnel_LaneNear','Shaft_Legacy','Shaft_Atmospheric','Shaft_Masks','POI_Legacy','POI_Atmospheric','POI_Masks','POI_Dark','POI_Warm','Transition_Legacy','Transition_Atmospheric','Transition_Masks')){
  $f=Join-Path $d "Render/$n.png"
  if(!(Test-Path $f) -or (Get-Item $f).Length -lt 10000){throw "Missing render $n"}
  Note "SCREENSHOT=$n $((Get-Item $f).Length) bytes"
 }
 Note 'RENDER_CAPTURES=PASS'
 Note 'VISUAL_ACCEPTANCE=PENDING_REVIEW'
}catch{Note "FAILED=$($_.Exception.Message)";exit 1}
