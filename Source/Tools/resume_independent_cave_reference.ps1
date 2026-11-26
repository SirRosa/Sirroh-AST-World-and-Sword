# Independent cave reference only: deploy pinned sources, preserve prior assets and saves.
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$p='D:\0 Sirro\Unreal Projects\AST_V1_18'
$e='D:\0 Sirro\UE 5.8.1\UE_5.8'
$u=Join-Path $p 'AST_V1_18.uproject'
$d=Join-Path $p 'Saved/ChatGPT/IndependentCaveReference_20261126'
$stage=Join-Path $d 'staging_927db8b'
$backup=Join-Path $d 'backup_927db8b'
$commit='927db8b24371335754571048609179bac22ca3de'
$root='Content/Systems/Procedural_World_Generator/PWG_v2'
$newAsset="$root/M_PWGv2_DepthAtmosphere_CaveReference.uasset"
$newRock="$root/M_PWGv2_CaveReferenceRock.uasset"
$save='Saved/SaveGames/PWGv2_VF_POIProbe_20261126.sav'
$probe='Saved/SaveGames/PWGv2_VF_CaveReference_20261126.sav'
$entries=@(
@{Path='Source/Sirroh_Reimagineg_v3/Public/PWG/PWGEnvironmentV2.h';Old='3751e28fb4109fb65837add129239e9c294f3a19';New='93827261ef91823d0d1e328cc0c412d2f6715e78'},
@{Path='Source/Sirroh_Reimagineg_v3/Public/Procedural_World_Generator/PWGv2Generator.h';Old='8b687ff4b2ab926a3b6e1d650420337617b12dfd';New='12129df4dc7a8b90ff471d862de63f88a9c34f2d'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Decorations.cpp';Old='31ba2e6451f72a7a8d45115dadbf8554b673c877';New='a3237272a0e43537daa7be26257892bac7131fcc'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Generator.cpp';Old='7043038aced6ff3a3f7e8131ea38d5a03e1a41c2';New='d58f29a0467fdfae9004209bf80fcb741f991b6e'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2.cpp';Old='8b2f140f223be89e100806e66a5d8d253ec5303e';New='52df6fb8bc6f5f582a8a47de7a1e1451b1a33b06'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Debug/SirrohDebugSubsystem.cpp';Old='2f23302e49ffb9b72c583030229cd0b272ba2234';New='f47e91480376e92eafbe4125eb431cd4dc21f916'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2SemanticVerification.cpp';Old='e7450628f1950f3b5095e1bcfb5762ffc6ea9222';New='ff51079321352e04ce96ddb3e18490d08f5b4691'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2IndependentCave.h';Old='';New='00e9a79f26abfdf6688e3afbe360dc9de5c53c8b'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2IndependentCave.cpp';Old='';New='290c6142438f77caa937437a26f3315fa059b026'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2IndependentCaveTests.cpp';Old='';New='872f71a7608a327c3412c012dcd4ebc312d3b4ee'},
@{Path='Source/Tools/create_pwg_cave_reference_pp.py';Old='';New='db5684947a11349d41b0406bbd3ba8f8afff3bb3'},
@{Path='Source/Tools/create_pwg_cave_reference_rock.py';Old='';New='ac276cab40c2032a4b82ca5895678d8eb553b1d5'}
)
New-Item -ItemType Directory -Path $d,$stage,$backup -Force | Out-Null
$result=Join-Path $d 'result_resume.txt'
if(Test-Path $result){Copy-Item $result (Join-Path $d ('result_before_'+(Get-Date -Format 'yyyyMMdd_HHmmss')+'.txt'))}
"COMMIT=$commit; IWYU_FIX=e1654556004ca0c3c8ae9f358ae579cc2f85d84c" | Set-Content $result
function Note([string]$s){for($i=0;$i -lt 30;$i++){try{$s | Add-Content -Path $result -ErrorAction Stop; Write-Output $s; return}catch{if($i -eq 29){throw};Start-Sleep -Milliseconds 300}}}
function GitHash([string]$f){$h=& git hash-object --no-filters $f;if($LASTEXITCODE -ne 0){throw "git hash failed: $f"};return $h.Trim()}
function CheckNoEditor {
 if(Get-Process UnrealEditor,UnrealEditor-Cmd,AST_V1_18 -ErrorAction SilentlyContinue){
  throw 'Editor/game open; stopping without closing it'
 }
}
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
 & "$e\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $u '/Game/Levels/L_SirrohGameplay' -game -SirrohSkipMainMenu -PWGWorld=VF_CaveReference_20261126 -PWGVisualPOIQA -PWGDepthLayersQA -PWGCaveReferenceQA -windowed -ResX=1600 -ResY=900 -unattended -nop4 -nosound -nosplash -stdout -FullStdOutLogOutput '-ExecCmds=t.MaxFPS 60' @extra 2>&1 | Out-File -Encoding utf8 $log
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop'
 Note "RENDER_EXIT=$name $rc"
 foreach($line in (Select-String -Path $log -Pattern 'VF_RENDER_|VF_DEPTH_GPU|VF_DEPTH_CHUNK|VF_CAVE_REFERENCE|VF_LANE_|VF_MASK_COUNTS|VF_POI_MASK|VF_SCENE|UBG_ISOLATION')){Note $line.Line}
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
 foreach($rel in @("$root/M_PWGv2_DepthAtmosphere.uasset","$root/M_PWGv2_DepthAtmosphere_V2.uasset","$root/M_PWGv2_DepthAtmosphere_Semantic.uasset","$root/M_PWGv2_DepthAtmosphere_SemanticPOI.uasset","$root/M_PWGv2_DepthAtmosphere_DepthLayersPrototype.uasset","$root/M_PWGv2_BackgroundLit.uasset",$save,'Saved/SaveGames/PWGv2_Serrania.sav','Saved/SaveGames/SirrohCharacter.sav')){
  if(Test-Path (Join-Path $p $rel)){ $protected[$rel]=(Get-FileHash (Join-Path $p $rel) -Algorithm SHA256).Hash }
 }
 if(!$protected.ContainsKey($save)){throw 'Missing original world'}
 $checkpoint=@{
  "$root/M_PWGv2_DepthAtmosphere.uasset"='565DB3F82136F15033C3F6B4652C8D26B1F1BF214AF1D85B64AAE956F64F3F62'
  "$root/M_PWGv2_DepthAtmosphere_V2.uasset"='D9AFA66C19389C4D69A2BA6121FDCF40C0A1F4A2F740EC3021DE0741EF2E5820'
  "$root/M_PWGv2_DepthAtmosphere_Semantic.uasset"='D736F40A523F9DB74F43F5B12C3C6AC8B4332B0A38E6433922C1698064DC8F34'
  "$root/M_PWGv2_DepthAtmosphere_SemanticPOI.uasset"='5ECFB7DB0CF8BF1F9F059B4369D8EE0B31025DE033CFC6F6628F5AB69C90807E'
  "$root/M_PWGv2_DepthAtmosphere_DepthLayersPrototype.uasset"='0CCAB2019FD82284B0AC2E4115F2D5BEB9FBE45F1970902818A3AD94377DB31A'
 }
 foreach($item in $checkpoint.GetEnumerator()){if($protected[$item.Key] -ne $item.Value){throw "Stage 1 asset mismatch: $($item.Key)"}}
 if(!(Select-String -Path (Join-Path $d 'result_retry.txt') -Pattern '^DEPLOYED_HASHES=PASS$' -Quiet) -or !(Select-String -Path (Join-Path $d 'result_retry.txt') -Pattern '^FAILED=Build failed$' -Quiet)){throw 'Prior deployment not in expected state'}
 if(!(Test-Path (Join-Path $p $newAsset)) -or (Get-FileHash (Join-Path $p $newAsset)).Hash -ne '287249DFBEE615AED918F5EF9EB1C5AB5D90064E4977AB12C9CA8083E64B2C6C'){throw 'Reference PP mismatch'}
 if(!(Test-Path (Join-Path $p $newRock)) -or (Get-FileHash (Join-Path $p $newRock)).Hash -ne 'C3F137208F657E2969D2127A594AE1BE3CF55691E06AE21F07A562EA3AD7EEA8'){throw 'Reference rock mismatch'}
 foreach($x in $entries){if((GitHash (Join-Path $p $x.Path)) -ne $x.New){throw "Deployed source mismatch: $($x.Path)"};if($x.Old -and (GitHash (Join-Path $backup $x.Path)) -ne $x.Old){throw "Original backup mismatch: $($x.Path)"}}
 foreach($item in $protected.GetEnumerator()){if((Get-FileHash (Join-Path $backup $item.Key)).Hash -ne $item.Value){throw "Protected backup mismatch: $($item.Key)"}}
 $rel='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2IndependentCave.cpp'
 $local=Join-Path $p $rel
 $oldCopy=Join-Path $d 'before_iwyu_fix.cpp'
 if(Test-Path $oldCopy){throw 'IWYU backup already exists'}
 Copy-Item $local $oldCopy
 if((GitHash $oldCopy) -ne '290c6142438f77caa937437a26f3315fa059b026'){throw 'IWYU backup mismatch'}
 $next=Join-Path $d 'iwyu_fixed.cpp'
 Invoke-WebRequest -UseBasicParsing -Uri "https://raw.githubusercontent.com/SirRosa/Sirroh-AST-World-and-Sword/e1654556004ca0c3c8ae9f358ae579cc2f85d84c/$rel" -OutFile $next
 if((GitHash $next) -ne '8d3cbb7e1b2c6d2a976601699ea6ea7c4e69c3d5'){throw 'IWYU staging mismatch'}
 CheckNoEditor;CheckOriginals
 Copy-Item $next $local
 if((GitHash $local) -ne '8d3cbb7e1b2c6d2a976601699ea6ea7c4e69c3d5'){throw 'IWYU deployment mismatch'}
 Note 'IWYU_FIX_DEPLOYED=PASS'
 Note 'BUILD=START'
 $ErrorActionPreference='Continue'
 & "$e\Engine\Build\BatchFiles\Build.bat" AST_V1_18Editor Win64 Development "-Project=$u" -WaitMutex -NoHotReloadFromIDE 2>&1 | Out-File -Encoding utf8 (Join-Path $d 'resume_build.log')
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop';Note "BUILD_EXIT=$rc";if($rc -ne 0){throw 'Build failed'}
 CheckOriginals;CheckNoEditor
 Note 'TESTS=START'
 $ErrorActionPreference='Continue'
 & "$e\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $u '-ExecCmds=Automation RunTests Sirroh.PWG.IndependentCave+Sirroh.PWG.DepthPrototype+Sirroh.PWG.VisualProfile+Sirroh.PWG.SpawnGeometryAudit+Sirroh.PWG.StreamingCost+Sirroh.POI.RoomChains' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$d/resume_test_report" -unattended -nop4 -nullrhi -nosplash -stdout -FullStdOutLogOutput 2>&1 | Out-File -Encoding utf8 (Join-Path $d 'resume_test.log')
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop';Note "TEST_EXIT=$rc"
 $testLines=Select-String -Path (Join-Path $d 'resume_test.log') -Pattern 'Test Completed|Found [0-9]+ automation tests|No automation tests'
 foreach($line in $testLines){Note $line.Line}
 if($rc -ne 0 -or (Select-String -Path (Join-Path $d 'resume_test.log') -Pattern 'Result=\{Fail\}' -Quiet)){throw 'Technical tests failed'}
 foreach($n in @('IndependentCave','DepthPrototype','VisualProfile','SpawnGeometryAudit','StreamingCost','RoomChains')){if(!($testLines | Where-Object {$_.Line -match 'Result=\{Success\}' -and $_.Line -match $n})){throw "No verified success for $n"}}
 Note 'TECHNICAL_TESTS=PASS'

 Render 'Render' @() @('Tunnel') @('Stage1','Composition','DepthMasks','NearOnly','MidOnly','FarOnly','ClosureOnly','Dark')

 CheckOriginals
 Note 'ORIGINALS_UNCHANGED=PASS'
 Note 'COMPLETE=PASS'
}catch{Note "FAILED=$($_.Exception.Message)";exit 1}
