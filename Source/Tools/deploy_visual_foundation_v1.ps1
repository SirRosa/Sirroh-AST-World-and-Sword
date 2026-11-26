# Deploy the exact Visual Foundation implementation commit. No main reset or save changes.
$ErrorActionPreference='Stop'
$p='D:\0 Sirro\Unreal Projects\AST_V1_18'
$e='D:\0 Sirro\UE 5.8.1\UE_5.8'
$u=Join-Path $p 'AST_V1_18.uproject'
$d=Join-Path $p 'Saved/ChatGPT/VisualFoundation_20261126'
$stage=Join-Path $d 'staging_202c3a6'
$backup=Join-Path $d 'backup_202c3a6'
$commit='202c3a6c0cb4b60b11e578925eafe8c25ff2ea61'
$legacy='Content/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_DepthAtmosphere.uasset'
$v2='Content/Systems/Procedural_World_Generator/PWG_v2/M_PWGv2_DepthAtmosphere_V2.uasset'
$save='Saved/SaveGames/PWGv2_asd.sav'
$originalAsset='565DB3F82136F15033C3F6B4652C8D26B1F1BF214AF1D85B64AAE956F64F3F62'
$originalSave='834A48D3BDA5D8D0D081C590E50C14A38B050B1D2BADB6A5AE15AA4F9A0B022A'
$entries=@(
@{Path='Source/Sirroh_Reimagineg_v3/Public/PWG/PWGEnvironmentV2.h';Old='23024f79f5d0868cb4f7f2d65446a1e06527e054';New='a2c04e8bfce12481e3afc63e9069feb4798fba2b'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2.cpp';Old='abdfca5365cdab68315a3eccddf66f5ba8edd2b0';New='58902fd1bd10e2826cf5f8e8705bf6cba49636b7'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Debug/SirrohDebugSubsystem.cpp';Old='e03d12e042cad4452d1f288430fa9ff3ab5bb35c';New='954a2f6a6d868f1bf15ac015684bf2fc9ce1c1fb'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2Generator.cpp';Old='673ea12741a8887a3b18cccd39696e0ecbe0056e';New='282729b313d0df80042b1efca02d76f6952f7cf6'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGVisualProfileTests.cpp';Old='';New='1fdf6378c1e68c3118def1145cb57f80bffbcf58'},
@{Path='Source/Tools/create_pwg_depth_atmosphere_v2.py';Old='';New='69529d2572069da7a3b27614aa6db202f9967feb'}
)
New-Item -ItemType Directory -Path $d,$stage,$backup -Force | Out-Null
$result=Join-Path $d 'result.txt'
"COMMIT=$commit" | Set-Content $result
function Note([string]$s){$s | Add-Content $result; Write-Output $s}
function GitHash([string]$f){$h=(& git hash-object --no-filters $f);if($LASTEXITCODE -ne 0){throw "git hash failed: $f"};return $h.Trim()}
function CheckNoEditor {if(Get-Process UnrealEditor,UnrealEditor-Cmd -ErrorAction SilentlyContinue){throw 'Editor is open; stopping without closing it'}}
function CheckOriginals {
 if((Get-FileHash (Join-Path $p $legacy) -Algorithm SHA256).Hash -ne $originalAsset){throw 'Legacy asset changed'}
 if((Get-FileHash (Join-Path $p $save) -Algorithm SHA256).Hash -ne $originalSave){throw 'asd save changed'}
}
try{
 CheckNoEditor
 CheckOriginals
 foreach($x in $entries){
  $local=Join-Path $p $x.Path
  if($x.Old){if(!(Test-Path $local) -or (GitHash $local) -ne $x.Old){throw "Local mismatch $($x.Path)"}}
  elseif(Test-Path $local){throw "New path already exists $($x.Path)"}
 }
 if(Test-Path (Join-Path $p $v2)){throw 'V2 asset already exists; inspect and backup before changing'}
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
 $assetBackup=Join-Path $backup $legacy
 New-Item -ItemType Directory -Path (Split-Path $assetBackup) -Force | Out-Null
 Copy-Item (Join-Path $p $legacy) $assetBackup
 if((Get-FileHash $assetBackup -Algorithm SHA256).Hash -ne $originalAsset){throw 'Asset backup mismatch'}
 Note 'BACKUPS=PASS'
 CheckNoEditor
 $script=(Join-Path $stage 'Source/Tools/create_pwg_depth_atmosphere_v2.py').Replace('\','/')
 Note 'ASSET_AUTHOR=START'
 $ErrorActionPreference='Continue'
 & "$e\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $u -run=pythonscript "-script=$script" -unattended -nop4 -AllowCommandletRendering -nosplash -stdout -FullStdOutLogOutput 2>&1 | Out-File (Join-Path $d 'author.log')
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop'
 Note "ASSET_AUTHOR_EXIT=$rc"
 if($rc -ne 0 -or !(Test-Path (Join-Path $p $v2)) -or !(Select-String -Path (Join-Path $d 'author.log') -Pattern 'VISUAL_FOUNDATION_V2_SAVED' -Quiet)){throw 'Asset authoring not verified; see author.log'}
 CheckOriginals
 Note "ASSET_V2_SHA256=$((Get-FileHash (Join-Path $p $v2) -Algorithm SHA256).Hash)"
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
 & "$e\Engine\Build\BatchFiles\Build.bat" AST_V1_18Editor Win64 Development "-Project=$u" -WaitMutex -NoHotReloadFromIDE 2>&1 | Out-File (Join-Path $d 'build.log')
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop'
 Note "BUILD_EXIT=$rc"
 if($rc -ne 0){throw 'Build failed; see build.log'}
 CheckNoEditor
 Note 'TESTS=START'
 $ErrorActionPreference='Continue'
 & "$e\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" $u '-ExecCmds=Automation RunTests Sirroh.PWG.VisualProfile+Sirroh.PWG.SpawnGeometryAudit+Sirroh.PWG.StreamingCost+Sirroh.POI.RoomChains' '-TestExit=Automation Test Queue Empty' "-ReportExportPath=$d/test_report" -unattended -nop4 -nullrhi -nosplash -stdout -FullStdOutLogOutput 2>&1 | Out-File (Join-Path $d 'test.log')
 $rc=$LASTEXITCODE
 $ErrorActionPreference='Stop'
 Note "TEST_EXIT=$rc"
 $log=Get-Content (Join-Path $d 'test.log') -Raw
 $testLines=Select-String -Path (Join-Path $d 'test.log') -Pattern 'Test Completed|Result=\{|Found [0-9]+ automation tests|No automation tests'
 foreach($m in $testLines){Note $m.Line}
 CheckOriginals
 Note 'LEGACY_AND_ASD_SHA256=UNCHANGED'
 if($rc -ne 0 -or $log -match 'Result=\{Fail\}'){throw 'Technical tests failed; see test.log'}
 foreach($n in @('VisualProfile','SpawnGeometryAudit','StreamingCost','RoomChains')){
  if(!($testLines | Where-Object {$_.Line -match 'Result=\{Success\}' -and $_.Line -match $n})){throw "No verified success for $n"}
 }
 Note 'TECHNICAL_TESTS=PASS'
 Note 'VISUAL_BASELINE_AND_QA=PENDING'
}catch{Note "FAILED=$($_.Exception.Message)";exit 1}
