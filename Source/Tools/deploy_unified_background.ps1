# Unified Underground Background: deploy pinned sources, preserve prior assets and saves.
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$p='D:\0 Sirro\Unreal Projects\AST_V1_18'
$e='D:\0 Sirro\UE 5.8.1\UE_5.8'
$u=Join-Path $p 'AST_V1_18.uproject'
$d=Join-Path $p 'Saved/ChatGPT/UnifiedBackground_20261126'
$stage=Join-Path $d 'staging_78d4cac'
$backup=Join-Path $d 'backup_78d4cac'
$commit='78d4cac3e37ca9aade74d95e18d3967e5077cb1a'
$root='Content/Systems/Procedural_World_Generator/PWG_v2'
$save='Saved/SaveGames/PWGv2_asd.sav'
$probe='Saved/SaveGames/PWGv2_VF_POIProbe_20261126.sav'
$entries=@(
@{Path='Source/Sirroh_Reimagineg_v3/Private/POI/POIShaft.cpp';Old='dc654a6b8b1e705159f4b1e68ff62ee79e760318';New='12b427daef8e47d7e0090095a2905ef3a9fad4fe'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/POI/PWGSpawnGeometryAudit.cpp';Old='1537c273f59e099b68179556fba578d31d936755';New='ea122b988ddc78571c13e7c73a5641d966d29b29'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2SemanticVerification.cpp';Old='b3c24c21f14b07fa321658ac359085215fe44a36';New='42084526e293d6d69880996a1eb5500df5c5f9d5'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2AuthoredPOI.cpp';Old='934502f0fbcaf1e8ddd8dd329e35bbce956fa323';New='aac8aff5eddeb6114970a4af5ec3e9868e6313e6'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Decorations.cpp';Old='628f3adf7e2ad64cf30740a49511a9327f8036c8';New='d4e0de89466fed8c7a9741f72a916886167d6a15'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Underground.cpp';Old='df0d654e963191731fc04daf6bfd74f45ab61a6b';New='6dd15301a5e6d2589f91b61f4d37c39df7afd4e6'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2VisualMasks.cpp';Old='3bb1ade3214f974d1f1aec291a4be73a52af3117';New='47307a327f76e35a73e6b8849fa736bf97f126c8'},
@{Path='Source/Sirroh_Reimagineg_v3/Public/POI/POIShaft.h';Old='130b25f2c1b3ca21f9257ae29927cc8a5daa9e7f';New='f81a117c213c54c1f4a336e6764ea32b375994c1'},
@{Path='Source/Sirroh_Reimagineg_v3/Public/Procedural_World_Generator/PWGv2Generator.h';Old='11d6d44f01148e9740988ef958124f9a45116e19';New='56e3b39adfface403ad101983a9e6e785d050707'}
)
New-Item -ItemType Directory -Path $d,$stage,$backup -Force | Out-Null
$result=Join-Path $d 'result.txt'
"COMMIT=$commit" | Set-Content $result
function Note([string]$s){$s | Add-Content $result; Write-Output $s}
function GitHash([string]$f){$h=& git hash-object --no-filters $f;if($LASTEXITCODE -ne 0){throw "git hash failed: $f"};return $h.Trim()}
function CheckNoEditor {if(Get-Process UnrealEditor,UnrealEditor-Cmd,AST_V1_18 -ErrorAction SilentlyContinue){throw 'Editor/game open; stopping without closing it'}}
$protected=@{}
function CheckOriginals {foreach($item in $protected.GetEnumerator()){if((Get-FileHash (Join-Path $p $item.Key) -Algorithm SHA256).Hash -ne $item.Value){throw "Protected file changed: $($item.Key)"}}}
function Render([string]$name,[string[]]$extra,[string[]]$scenes,[string[]]$suffixes) {
 CheckNoEditor;CheckOriginals
 Copy-Item (Join-Path $p $save) (Join-Path $p $probe) -Force
 if((Get-FileHash (Join-Path $p $probe)).Hash -ne $protected[$save]){throw 'Probe save copy mismatch'}
 Note "RENDER_START=$name"
 $log=Join-Path $d ("render_"+$name+".log")
 $ErrorActionPreference='Continue'
 & "$e\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $u '/Game/Levels/L_SirrohGameplay' -game -SirrohSkipMainMenu -PWGWorld=VF_POIProbe_20261126 -PWGVisualPOIQA -windowed -ResX=1600 -ResY=900 -unattended -nop4 -nosound -nosplash -stdout -FullStdOutLogOutput '-ExecCmds=t.MaxFPS 60' @extra 2>&1 | Out-File -Encoding utf8 $log
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop'
 Note "RENDER_EXIT=$name $rc"
 foreach($line in (Select-String -Path $log -Pattern 'VF_RENDER_|VF_GPU|VF_LANE_|VF_MASK_COUNTS|VF_POI_MASK|VF_SCENE|UBG_ISOLATION')){Note $line.Line}
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
 foreach($rel in @("$root/M_PWGv2_DepthAtmosphere.uasset","$root/M_PWGv2_DepthAtmosphere_V2.uasset","$root/M_PWGv2_DepthAtmosphere_Semantic.uasset","$root/M_PWGv2_DepthAtmosphere_SemanticPOI.uasset",$save,'Saved/SaveGames/SirrohCharacter.sav')){
  if(Test-Path (Join-Path $p $rel)){ $protected[$rel]=(Get-FileHash (Join-Path $p $rel) -Algorithm SHA256).Hash }
 }
 if(!$protected.ContainsKey($save)){throw 'Missing original world'}
 if($protected["$root/M_PWGv2_DepthAtmosphere_SemanticPOI.uasset"] -ne '5ECFB7DB0CF8BF1F9F059B4369D8EE0B31025DE033CFC6F6628F5AB69C90807E'){throw 'Previous shader hash differs'}
 foreach($x in $entries){$local=Join-Path $p $x.Path;if(!(Test-Path $local) -or (GitHash $local) -ne $x.Old){throw "Local mismatch: $($x.Path)"}}
 Note 'LOCAL_PREFLIGHT=PASS'
 foreach($x in $entries){$dst=Join-Path $stage $x.Path;New-Item -ItemType Directory -Path (Split-Path $dst) -Force|Out-Null;Invoke-WebRequest -UseBasicParsing -Uri "https://raw.githubusercontent.com/SirRosa/Sirroh-AST-World-and-Sword/$commit/$($x.Path)" -OutFile $dst;if((GitHash $dst) -ne $x.New){throw "Staging mismatch $($x.Path)"}}
 Note 'STAGING_HASHES=PASS'
 foreach($x in $entries){$dst=Join-Path $backup $x.Path;New-Item -ItemType Directory -Path (Split-Path $dst) -Force|Out-Null;Copy-Item (Join-Path $p $x.Path) $dst;if((GitHash $dst) -ne $x.Old){throw "Backup mismatch $($x.Path)"}}
 foreach($item in $protected.GetEnumerator()){$dst=Join-Path $backup $item.Key;New-Item -ItemType Directory -Path (Split-Path $dst) -Force|Out-Null;Copy-Item (Join-Path $p $item.Key) $dst;if((Get-FileHash $dst).Hash -ne $item.Value){throw "Backup mismatch $($item.Key)"}}
 if(Test-Path (Join-Path $p $probe)){Copy-Item (Join-Path $p $probe) (Join-Path $d 'previous_probe.sav')}
 Note 'BACKUPS=PASS'
 CheckNoEditor;CheckOriginals
 foreach($x in $entries){if((GitHash (Join-Path $p $x.Path)) -ne $x.Old){throw "Local changed during staging $($x.Path)"}}
 foreach($x in $entries){$local=Join-Path $p $x.Path;Copy-Item (Join-Path $stage $x.Path) $local;if((GitHash $local) -ne $x.New){throw "Deploy mismatch $($x.Path)"}}
 Note 'DEPLOYED_HASHES=PASS'
 Note 'BUILD=START'
 $ErrorActionPreference='Continue'
 & "$e\Engine\Build\BatchFiles\Build.bat" AST_V1_18Editor Win64 Development "-Project=$u" -WaitMutex -NoHotReloadFromIDE 2>&1 | Out-File -Encoding utf8 (Join-Path $d 'build.log')
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop';Note "BUILD_EXIT=$rc";if($rc -ne 0){throw 'Build failed'}
 CheckOriginals;CheckNoEditor
 Note 'TESTS=START'
 $ErrorActionPreference='Continue'
 & "$e\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $u '-ExecCmds=Automation RunTests Sirroh.PWG.VisualProfile+Sirroh.PWG.SpawnGeometryAudit+Sirroh.PWG.StreamingCost+Sirroh.POI.RoomChains' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$d/test_report" -unattended -nop4 -nullrhi -nosplash -stdout -FullStdOutLogOutput 2>&1 | Out-File -Encoding utf8 (Join-Path $d 'test.log')
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop';Note "TEST_EXIT=$rc"
 $testLines=Select-String -Path (Join-Path $d 'test.log') -Pattern 'Test Completed|Found [0-9]+ automation tests|No automation tests'
 foreach($line in $testLines){Note $line.Line}
 if($rc -ne 0 -or (Select-String -Path (Join-Path $d 'test.log') -Pattern 'Result=\{Fail\}' -Quiet)){throw 'Technical tests failed'}
 foreach($n in @('VisualProfile','SpawnGeometryAudit','StreamingCost','RoomChains')){if(!($testLines | Where-Object {$_.Line -match 'Result=\{Success\}' -and $_.Line -match $n})){throw "No verified success for $n"}}
 Note 'TECHNICAL_TESTS=PASS'
 Render 'Audit' @('-PWGBackgroundAuditLegacy') @('Tunnel','Shaft','POI','Transition') @('Atmospheric','NoNativeCards','NoProjectedArt','NoShaftWalls','NoCaveBackdrop','NativeOnly')
 Render 'Render' @() @('Tunnel','Shaft','POI','Transition','Shaft2','ChunkSeam','POI2') @('Legacy','Atmospheric','Masks')
 Render 'Enclosed' @('-PWGBackgroundAuditEnclosed') @('Tunnel','Shaft','POI','Transition') @('Legacy','Atmospheric','Masks')
 CheckOriginals
 Note 'ORIGINALS_UNCHANGED=PASS'
 Note 'COMPLETE=PASS'
}catch{Note "FAILED=$($_.Exception.Message)";exit 1}
