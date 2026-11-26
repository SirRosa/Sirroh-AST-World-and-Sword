# Stage 2 prototype: deploy pinned sources, preserve prior assets and saves.
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$p='D:\0 Sirro\Unreal Projects\AST_V1_18'
$e='D:\0 Sirro\UE 5.8.1\UE_5.8'
$u=Join-Path $p 'AST_V1_18.uproject'
$d=Join-Path $p 'Saved/ChatGPT/DepthLayersPrototype_20261126'
$stage=Join-Path $d 'staging_9ed8af8'
$backup=Join-Path $d 'backup_9ed8af8'
$commit='9ed8af86aceb9fa0a6dc9ccdb59e8141f6b0ae58'
$root='Content/Systems/Procedural_World_Generator/PWG_v2'
$newAsset="$root/M_PWGv2_DepthAtmosphere_DepthLayersPrototype.uasset"
$save='Saved/SaveGames/PWGv2_VF_POIProbe_20261126.sav'
$probe='Saved/SaveGames/PWGv2_VF_DepthProbe_20261126.sav'
$entries=@(
@{Path='Source/Sirroh_Reimagineg_v3/Public/PWG/PWGEnvironmentV2.h';Old='239cd90b543a4924b3412744709a9ec41996b57f';New='3751e28fb4109fb65837add129239e9c294f3a19'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2.cpp';Old='d8ca53b341dfe3f604d481a3ab0634288c17e3d2';New='8b2f140f223be89e100806e66a5d8d253ec5303e'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Debug/SirrohDebugSubsystem.cpp';Old='1a2c8ad42c85aa7cee201c5e76ec6ab37c744d01';New='2f23302e49ffb9b72c583030229cd0b272ba2234'},
@{Path='Source/Sirroh_Reimagineg_v3/Public/Procedural_World_Generator/PWGv2Generator.h';Old='56e3b39adfface403ad101983a9e6e785d050707';New='8b687ff4b2ab926a3b6e1d650420337617b12dfd'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Decorations.cpp';Old='d4e0de89466fed8c7a9741f72a916886167d6a15';New='31ba2e6451f72a7a8d45115dadbf8554b673c877'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Underground.cpp';Old='6dd15301a5e6d2589f91b61f4d37c39df7afd4e6';New='7c31c1dc50821744ffed646743685884cb605c48'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2DepthPrototype.h';Old='';New='60aa9362571bfa957944c076b27d05e2211b534a'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2DepthPrototypeTests.cpp';Old='';New='2a29d8fc8a082b41104438006b2d3140384cee71'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2SemanticVerification.cpp';Old='5b057acf62252f876feb9858bda95bd6a9809062';New='e7450628f1950f3b5095e1bcfb5762ffc6ea9222'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGVisualProfileTests.cpp';Old='e474d52571e3c5bcd5d7e8744e4999958851f454';New='af7eb8161780f1c656104cd4200aaeeea5febab1'},
@{Path='Source/Tools/create_pwg_depth_atmosphere_depth_layers_prototype.py';Old='';New='50487d34af9f1944c5382682acf63f58922a1aa8'}
)
New-Item -ItemType Directory -Path $d,$stage,$backup -Force | Out-Null
$result=Join-Path $d 'resume_result.txt'
if(Test-Path $result){Copy-Item $result (Join-Path $d ('result_before_'+(Get-Date -Format 'yyyyMMdd_HHmmss')+'.txt'))}
"COMMIT=$commit; AUDIT_COMMIT=517fe101eaa6d9e67af247a07efcc10ae1090d20" | Set-Content $result
function Note([string]$s){$s | Add-Content $result; Write-Output $s}
function GitHash([string]$f){$h=& git hash-object --no-filters $f;if($LASTEXITCODE -ne 0){throw "git hash failed: $f"};return $h.Trim()}
function CheckNoEditor {if(Get-Process UnrealEditor,UnrealEditor-Cmd,AST_V1_18 -ErrorAction SilentlyContinue){throw 'Editor/game open; stopping without closing it'}}
$protected=@{}
function CheckSources {foreach($x in $entries){$local=Join-Path $p $x.Path;if($x.Old){if(!(Test-Path $local) -or (GitHash $local) -ne $x.Old){throw "Local mismatch: $($x.Path)"}}elseif(Test-Path $local){throw "New path already exists: $($x.Path)"}}}
function CheckOriginals {foreach($item in $protected.GetEnumerator()){if((Get-FileHash (Join-Path $p $item.Key) -Algorithm SHA256).Hash -ne $item.Value){throw "Protected file changed: $($item.Key)"}}}
function Render([string]$name,[string[]]$extra,[string[]]$scenes,[string[]]$suffixes) {
 CheckNoEditor;CheckOriginals
 Copy-Item (Join-Path $p $save) (Join-Path $p $probe) -Force
 if((Get-FileHash (Join-Path $p $probe)).Hash -ne $protected[$save]){throw 'Probe save copy mismatch'}
 Note "RENDER_START=$name"
 $log=Join-Path $d ("render_"+$name+".log")
 $ErrorActionPreference='Continue'
 & "$e\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $u '/Game/Levels/L_SirrohGameplay' -game -SirrohSkipMainMenu -PWGWorld=VF_DepthProbe_20261126 -PWGVisualPOIQA -PWGDepthLayersQA -windowed -ResX=1600 -ResY=900 -unattended -nop4 -nosound -nosplash -stdout -FullStdOutLogOutput '-ExecCmds=t.MaxFPS 60' @extra 2>&1 | Out-File -Encoding utf8 $log
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop'
 Note "RENDER_EXIT=$name $rc"
 foreach($line in (Select-String -Path $log -Pattern 'VF_RENDER_|VF_DEPTH_GPU|VF_DEPTH_CHUNK|VF_LANE_|VF_MASK_COUNTS|VF_POI_MASK|VF_SCENE|UBG_ISOLATION')){Note $line.Line}
 CheckOriginals
 if($rc -ne 0 -or !(Select-String -Path $log -Pattern 'VF_RENDER_COMPLETE' -Quiet) -or (Select-String -Path $log -Pattern 'VF_RENDER_FAIL|VF_RENDER_SCENE_UNAVAILABLE' -Quiet)){throw "Real render sequence failed $name"}
 foreach($scene in $scenes){foreach($suffix in $suffixes){
   $f=Join-Path $d "$name/${scene}_${suffix}.png"
   if(!(Test-Path $f) -or (Get-Item $f).Length -lt 10000){throw "Missing render $f"}
 }}
 Note "RENDER_CAPTURES_PASS=$name"
}
try{
 CheckNoEditor
 foreach($rel in @("$root/M_PWGv2_DepthAtmosphere.uasset","$root/M_PWGv2_DepthAtmosphere_V2.uasset","$root/M_PWGv2_DepthAtmosphere_Semantic.uasset","$root/M_PWGv2_DepthAtmosphere_SemanticPOI.uasset",$save,'Saved/SaveGames/PWGv2_Serrania.sav','Saved/SaveGames/SirrohCharacter.sav')){
  if(Test-Path (Join-Path $p $rel)){ $protected[$rel]=(Get-FileHash (Join-Path $p $rel) -Algorithm SHA256).Hash }
 }
 if(!$protected.ContainsKey($save)){throw 'Missing original world'}
 $checkpoint=@{
  "$root/M_PWGv2_DepthAtmosphere.uasset"='565DB3F82136F15033C3F6B4652C8D26B1F1BF214AF1D85B64AAE956F64F3F62'
  "$root/M_PWGv2_DepthAtmosphere_V2.uasset"='D9AFA66C19389C4D69A2BA6121FDCF40C0A1F4A2F740EC3021DE0741EF2E5820'
  "$root/M_PWGv2_DepthAtmosphere_Semantic.uasset"='D736F40A523F9DB74F43F5B12C3C6AC8B4332B0A38E6433922C1698064DC8F34'
  "$root/M_PWGv2_DepthAtmosphere_SemanticPOI.uasset"='5ECFB7DB0CF8BF1F9F059B4369D8EE0B31025DE033CFC6F6628F5AB69C90807E'
 }
 foreach($item in $checkpoint.GetEnumerator()){if($protected[$item.Key] -ne $item.Value){throw "Stage 1 asset mismatch: $($item.Key)"}}
 if(!(Test-Path (Join-Path $d 'result.txt')) -or !(Select-String -Path (Join-Path $d 'result.txt') -Pattern '^DEPLOYED_HASHES=PASS$' -Quiet)){throw 'Initial deployment not verified'}
 if(!(Test-Path (Join-Path $d 'result.txt')) -or !(Select-String -Path (Join-Path $d 'result.txt') -Pattern '^(FAILED=|COMPLETE=PASS)' -Quiet)){throw 'Initial job has not reached terminal state'}
 if(!(Test-Path (Join-Path $p $newAsset)) -or (Get-FileHash (Join-Path $p $newAsset)).Hash -ne '0CCAB2019FD82284B0AC2E4115F2D5BEB9FBE45F1970902818A3AD94377DB31A'){throw 'Prototype asset mismatch'}
 foreach($x in $entries){if((GitHash (Join-Path $p $x.Path)) -ne $x.New){throw "Deployed source mismatch: $($x.Path)"};if($x.Old -and (GitHash (Join-Path $backup $x.Path)) -ne $x.Old){throw "Original backup mismatch: $($x.Path)"}}
 foreach($item in $protected.GetEnumerator()){if((Get-FileHash (Join-Path $backup $item.Key)).Hash -ne $item.Value){throw "Protected file backup mismatch: $($item.Key)"}}
 $rel='Source/Sirroh_Reimagineg_v3/Private/POI/PWGSpawnGeometryAudit.cpp'
 $local=Join-Path $p $rel
 if((GitHash $local) -ne 'ea122b988ddc78571c13e7c73a5641d966d29b29'){throw 'Audit source mismatch'}
 $auditBackup=Join-Path $backup $rel
 New-Item -ItemType Directory -Path (Split-Path $auditBackup) -Force|Out-Null
 if(Test-Path $auditBackup){throw 'Audit backup already exists'}
 Copy-Item $local $auditBackup
 if((GitHash $auditBackup) -ne 'ea122b988ddc78571c13e7c73a5641d966d29b29'){throw 'Audit backup mismatch'}
 $auditStage=Join-Path $stage $rel
 New-Item -ItemType Directory -Path (Split-Path $auditStage) -Force|Out-Null
 Invoke-WebRequest -UseBasicParsing -Uri "https://raw.githubusercontent.com/SirRosa/Sirroh-AST-World-and-Sword/517fe101eaa6d9e67af247a07efcc10ae1090d20/$rel" -OutFile $auditStage
 if((GitHash $auditStage) -ne '43e351661d6e2dd17bb65c88b5745e6a0bbab6df'){throw 'Audit staging mismatch'}
 CheckNoEditor;CheckOriginals
 Copy-Item $auditStage $local
 if((GitHash $local) -ne '43e351661d6e2dd17bb65c88b5745e6a0bbab6df'){throw 'Audit deployment mismatch'}
 Note 'AUDIT_PATCH_DEPLOYED=PASS'
 Note 'BUILD=START'
 $ErrorActionPreference='Continue'
 & "$e\Engine\Build\BatchFiles\Build.bat" AST_V1_18Editor Win64 Development "-Project=$u" -WaitMutex -NoHotReloadFromIDE 2>&1 | Out-File -Encoding utf8 (Join-Path $d 'resume_build.log')
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop';Note "BUILD_EXIT=$rc";if($rc -ne 0){throw 'Build failed'}
 CheckOriginals;CheckNoEditor
 Note 'TESTS=START'
 $ErrorActionPreference='Continue'
 & "$e\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $u '-ExecCmds=Automation RunTests Sirroh.PWG.DepthPrototype+Sirroh.PWG.VisualProfile+Sirroh.PWG.SpawnGeometryAudit+Sirroh.PWG.StreamingCost+Sirroh.POI.RoomChains' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$d/resume_test_report" -unattended -nop4 -nullrhi -nosplash -stdout -FullStdOutLogOutput 2>&1 | Out-File -Encoding utf8 (Join-Path $d 'resume_test.log')
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop';Note "TEST_EXIT=$rc"
 $testLines=Select-String -Path (Join-Path $d 'resume_test.log') -Pattern 'Test Completed|Found [0-9]+ automation tests|No automation tests'
 foreach($line in $testLines){Note $line.Line}
 if($rc -ne 0 -or (Select-String -Path (Join-Path $d 'resume_test.log') -Pattern 'Result=\{Fail\}' -Quiet)){throw 'Technical tests failed'}
 foreach($n in @('DepthPrototype','VisualProfile','SpawnGeometryAudit','StreamingCost','RoomChains')){if(!($testLines | Where-Object {$_.Line -match 'Result=\{Success\}' -and $_.Line -match $n})){throw "No verified success for $n"}}
 Note 'TECHNICAL_TESTS=PASS'

 Render 'Render' @() @('Tunnel','Shaft','POI','Transition','Shaft2','ChunkSeam','POI2') @('Stage1','Stage2','DepthMasks')
 Render 'Enclosed' @('-PWGBackgroundAuditEnclosed') @('Tunnel','Shaft','POI','Transition') @('Stage1','Stage2','DepthMasks')
 CheckOriginals
 Note 'ORIGINALS_UNCHANGED=PASS'
 Note 'COMPLETE=PASS'
}catch{Note "FAILED=$($_.Exception.Message)";exit 1}
