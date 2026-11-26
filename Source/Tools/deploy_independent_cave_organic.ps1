# Independent cave reference only: deploy pinned sources, preserve prior assets and saves.
$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$p='D:\0 Sirro\Unreal Projects\AST_V1_18'
$e='D:\0 Sirro\UE 5.8.1\UE_5.8'
$u=Join-Path $p 'AST_V1_18.uproject'
$d=Join-Path $p 'Saved/ChatGPT/IndependentCaveReference_20261126'
$stage=Join-Path $d 'staging_organic_56fabc3'
$backup=Join-Path $d 'backup_organic_56fabc3'
$commit='56fabc3f674588efbf480f0f395ca79044e2433b'
$root='Content/Systems/Procedural_World_Generator/PWG_v2'
$newAsset="$root/M_PWGv2_DepthAtmosphere_CaveReference.uasset"
$newRock="$root/M_PWGv2_CaveReferenceRock.uasset"
$save='Saved/SaveGames/PWGv2_VF_POIProbe_20261126.sav'
$probe='Saved/SaveGames/PWGv2_VF_CaveReference_20261126.sav'
$entries=@(
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2IndependentCave.h';Old='00e9a79f26abfdf6688e3afbe360dc9de5c53c8b';New='e8e801b8f9a767668dc58109b2884f3e17278ebf'},
@{Path='Source/Sirroh_Reimagineg_v3/Public/PWG/PWGEnvironmentV2.h';Old='93827261ef91823d0d1e328cc0c412d2f6715e78';New='d1b2ded3b58ce5bfde7071d898f586169cf0b154'},
@{Path='Source/Sirroh_Reimagineg_v3/Public/Procedural_World_Generator/PWGv2Generator.h';Old='12129df4dc7a8b90ff471d862de63f88a9c34f2d';New='c444c40e640ef8eac3ec0f95a70a2d9d30c292ee'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2IndependentCave.cpp';Old='8d3cbb7e1b2c6d2a976601699ea6ea7c4e69c3d5';New='b60f9fdd0e72159311a06a16c3d1a0bb5043e801'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2SemanticVerification.cpp';Old='ff51079321352e04ce96ddb3e18490d08f5b4691';New='b0b5abecfe3e3fbf793a3da452923cb5a7892170'}
)
New-Item -ItemType Directory -Path $d,$stage,$backup -Force | Out-Null
$result=Join-Path $d 'result_organic.txt'
if(Test-Path $result){Copy-Item $result (Join-Path $d ('result_before_'+(Get-Date -Format 'yyyyMMdd_HHmmss')+'.txt'))}
"COMMIT=$commit" | Set-Content $result
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
 if(!(Select-String -Path (Join-Path $d 'result_resume.txt') -Pattern '^COMPLETE=PASS$' -Quiet)){throw 'First render not verified'}
 if((Get-FileHash (Join-Path $p $newAsset)).Hash -ne '287249DFBEE615AED918F5EF9EB1C5AB5D90064E4977AB12C9CA8083E64B2C6C'){throw 'Reference PP mismatch'}
 if((Get-FileHash (Join-Path $p $newRock)).Hash -ne 'C3F137208F657E2969D2127A594AE1BE3CF55691E06AE21F07A562EA3AD7EEA8'){throw 'Reference rock mismatch'}
 $protected[$newAsset]=(Get-FileHash (Join-Path $p $newAsset)).Hash
 $protected[$newRock]=(Get-FileHash (Join-Path $p $newRock)).Hash
 CheckSources
 foreach($x in $entries){
   $local=Join-Path $p $x.Path; $bak=Join-Path $backup $x.Path; $st=Join-Path $stage $x.Path
   New-Item -ItemType Directory -Path (Split-Path $bak),(Split-Path $st) -Force|Out-Null
   Copy-Item $local $bak
   if((GitHash $bak) -ne $x.Old){throw "Backup mismatch $($x.Path)"}
   Invoke-WebRequest -UseBasicParsing -Uri "https://raw.githubusercontent.com/SirRosa/Sirroh-AST-World-and-Sword/$commit/$($x.Path)" -OutFile $st
   if((GitHash $st) -ne $x.New){throw "Staging mismatch $($x.Path)"}
 }
 CheckNoEditor;CheckOriginals;CheckSources
 foreach($x in $entries){
   $local=Join-Path $p $x.Path;Copy-Item (Join-Path $stage $x.Path) $local -Force
   if((GitHash $local) -ne $x.New){throw "Deploy mismatch $($x.Path)"}
 }
 Note 'ORGANIC_DEPLOYED=PASS'
 Note 'BUILD=START'
 $ErrorActionPreference='Continue'
 & "$e\Engine\Build\BatchFiles\Build.bat" AST_V1_18Editor Win64 Development "-Project=$u" -WaitMutex -NoHotReloadFromIDE 2>&1 | Out-File -Encoding utf8 (Join-Path $d 'organic_build.log')
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop';Note "BUILD_EXIT=$rc";if($rc -ne 0){throw 'Build failed'}
 CheckOriginals;CheckNoEditor
 Note 'TESTS=START'
 $ErrorActionPreference='Continue'
 & "$e\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $u '-ExecCmds=Automation RunTests Sirroh.PWG.IndependentCave+Sirroh.PWG.DepthPrototype+Sirroh.PWG.VisualProfile+Sirroh.PWG.SpawnGeometryAudit+Sirroh.PWG.StreamingCost+Sirroh.POI.RoomChains' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$d/organic_test_report" -unattended -nop4 -nullrhi -nosplash -stdout -FullStdOutLogOutput 2>&1 | Out-File -Encoding utf8 (Join-Path $d 'organic_test.log')
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop';Note "TEST_EXIT=$rc"
 $testLines=Select-String -Path (Join-Path $d 'resume_test.log') -Pattern 'Test Completed|Found [0-9]+ automation tests|No automation tests'
 foreach($line in $testLines){Note $line.Line}
 if($rc -ne 0 -or (Select-String -Path (Join-Path $d 'resume_test.log') -Pattern 'Result=\{Fail\}' -Quiet)){throw 'Technical tests failed'}
 foreach($n in @('IndependentCave','DepthPrototype','VisualProfile','SpawnGeometryAudit','StreamingCost','RoomChains')){if(!($testLines | Where-Object {$_.Line -match 'Result=\{Success\}' -and $_.Line -match $n})){throw "No verified success for $n"}}
 Note 'TECHNICAL_TESTS=PASS'

 Render 'RenderOrganic' @() @('Tunnel') @('Stage1','Composition','DepthMasks','NearOnly','MidOnly','FarOnly','ClosureOnly','Dark')

 CheckOriginals
 Note 'ORIGINALS_UNCHANGED=PASS'
 Note 'COMPLETE=PASS'
}catch{Note "FAILED=$($_.Exception.Message)";exit 1}
