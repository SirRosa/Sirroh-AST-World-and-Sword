$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$p='D:\0 Sirro\Unreal Projects\AST_V1_18'
$e='D:\0 Sirro\UE 5.8.1\UE_5.8'
$u=Join-Path $p 'AST_V1_18.uproject'
$d=Join-Path $p 'Saved\ChatGPT\ProceduralPixel2D_20261126'
$stage=Join-Path $d 'stage_resume'
$backup=Join-Path $d 'backup_resume'
New-Item -ItemType Directory -Force $d,$stage,$backup|Out-Null
$result=Join-Path $d 'result_resume.txt'
function Note($s){$s|Tee-Object -FilePath $result -Append}
function Hash($f){(& git hash-object --no-filters $f).Trim()}
function Native($exe,$a,$log){& $exe @a *> $log;return $LASTEXITCODE}
function NoEditor {if(Get-Process UnrealEditor,UnrealEditor-Cmd,AST_V1_18 -ErrorAction SilentlyContinue){throw 'Editor/game running; deployment stopped'}}
$commit='ba63f29accfc0a9869e8f32b3b0d5e9a8302a155'
$files=@(
@{Path='Source/Sirroh_Reimagineg_v3/Private/POI/PWGSpawnGeometryAudit.cpp';Old='43e351661d6e2dd17bb65c88b5745e6a0bbab6df';New='b3391105907d36c361537e7aeed6f701051ba126'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2PixelBackground.cpp';Old='0a57d7329778c7a33a9f402552ab19fb399da561';New='f2d57d0d0719532bf5b6ff9071bce21b9ebed994'}
)
try {
 Note "SOURCE_COMMIT=$commit"
 NoEditor
 foreach($f in $files){
  $local=Join-Path $p $f.Path
  if($f.Old){if(!(Test-Path $local) -or (Hash $local) -ne $f.Old){throw "BASELINE_MISMATCH $($f.Path)"}}
  elseif(Test-Path $local){throw "NEW_PATH_EXISTS $($f.Path)"}
  $target=Join-Path $stage $f.Path
  New-Item -ItemType Directory -Force (Split-Path $target)|Out-Null
  Invoke-WebRequest -UseBasicParsing -Uri "https://raw.githubusercontent.com/SirRosa/Sirroh-AST-World-and-Sword/$commit/$($f.Path)" -OutFile $target
  if((Hash $target) -ne $f.New){throw "STAGED_HASH_MISMATCH $($f.Path)"}
 }
 Note 'STAGED_HASHES_OK'
 $protected=@{}
 Get-ChildItem (Join-Path $p 'Saved\SaveGames') -File -Filter '*.sav' -ErrorAction SilentlyContinue|Where-Object {$_.Name -notlike '*Test*' -and $_.Name -notlike '*Audit*'}|ForEach-Object {$protected[$_.FullName]=(Get-FileHash $_.FullName).Hash}
 Get-ChildItem (Join-Path $p 'Content') -Recurse -File -Filter 'M_PWGv2*.uasset'|ForEach-Object {$protected[$_.FullName]=(Get-FileHash $_.FullName).Hash}
 $copySource=Join-Path $p 'Saved\SaveGames\PWGv2_VF_POIProbe_20261126.sav'
 $copyDest=Join-Path $p 'Saved\SaveGames\PWGv2_VF_Pixel2D_20261126.sav'
 if(!(Test-Path $copySource)){throw 'QA source slot missing'}
 if(Test-Path $copyDest){throw 'QA destination already exists'}
 foreach($f in $files){if($f.Old){$b=Join-Path $backup $f.Path;New-Item -ItemType Directory -Force (Split-Path $b)|Out-Null;Copy-Item (Join-Path $p $f.Path) $b;if((Hash $b) -ne $f.Old){throw 'BACKUP_HASH_MISMATCH'}}}
 Note 'LOCAL_SOURCE_BACKUP_OK'
 $editor=Join-Path $e 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
 Note 'AUTHOR_PREVIOUSLY_VERIFIED=1'
 foreach($k in $protected.Keys){if(!(Test-Path $k) -or (Get-FileHash $k).Hash -ne $protected[$k]){throw "ORIGINAL_CHANGED $k"}}
 NoEditor
 foreach($f in $files){$local=Join-Path $p $f.Path;New-Item -ItemType Directory -Force (Split-Path $local)|Out-Null;Copy-Item (Join-Path $stage $f.Path) $local -Force;if((Hash $local) -ne $f.New){throw "DEPLOY_HASH_MISMATCH $($f.Path)"}}
 Note 'DEPLOYED=1'
 $rc=Native (Join-Path $e 'Engine\Build\BatchFiles\Build.bat') @('AST_V1_18Editor','Win64','Development',"-Project=$u",'-WaitMutex','-NoHotReloadFromIDE') (Join-Path $d 'build.log')
 Note "BUILD_EXIT=$rc"
 if($rc -ne 0){Get-Content (Join-Path $d 'build.log') -Tail 35|ForEach-Object {Note $_};throw 'Build failed'}
 $rc=Native $editor @($u,'-ExecCmds=Automation RunTests Sirroh.PWG.PixelField+Sirroh.PWG.VisualProfile+Sirroh.PWG.SpawnGeometryAudit+Sirroh.PWG.StreamingCost+Sirroh.POI.RoomChains','-TestExit=Automation Test Queue Empty','-unattended','-nop4','-nullrhi','-nosplash','-stdout','-FullStdOutLogOutput') (Join-Path $d 'tests.log')
 Note "TEST_EXIT=$rc"
 $tests=Select-String -Path (Join-Path $d 'tests.log') -Pattern 'Test Completed|Automation Test Queue Empty'
 $tests|ForEach-Object {Note $_.Line}
 if($rc -ne 0 -or @($tests|Where-Object {$_.Line -match 'Result=\{Success\}'}).Count -lt 5 -or @($tests|Where-Object {$_.Line -match 'Result=\{Fail'}).Count -gt 0){throw 'Technical tests not verified'}
 Note 'TECHNICAL_TESTS=PASS'
 Copy-Item $copySource $copyDest
 $rc=Native $editor @($u,'-game','-PWGVisualPOIQA','-PWGPixel2DQA','-SirrohSkipMainMenu','-PWGWorld=VF_Pixel2D_20261126','-windowed','-ResX=1600','-ResY=900','-unattended','-nop4','-nosound','-nosplash','-stdout','-FullStdOutLogOutput','-ExecCmds=t.MaxFPS 60') (Join-Path $d 'render.log')
 Note "RENDER_EXIT=$rc"
 $rl=Get-Content (Join-Path $d 'render.log') -Raw
 if($rc -ne 0 -or $rl -notmatch 'VF_RENDER_COMPLETE' -or $rl -match 'VF_RENDER_FAIL|VF_RENDER_SCENE_UNAVAILABLE|PWG_PIXEL2D_CHUNK.*ready=0'){throw 'Render QA failed or incomplete'}
 foreach($n in @('Tunnel_Composition','Tunnel_Dark','Tunnel_TorchDark','Shaft_Composition','POI_Composition','DeepCave_Composition')){$f=Join-Path $d "Render\$n.png";if(!(Test-Path $f) -or (Get-Item $f).Length -lt 10000){throw "CAPTURE_MISSING $n"}}
 Note 'RENDER_CAPTURES=6'
 foreach($k in $protected.Keys){if(!(Test-Path $k) -or (Get-FileHash $k).Hash -ne $protected[$k]){throw "ORIGINAL_CHANGED $k"}}
 Note 'ORIGINALS_UNCHANGED=1'
 Note 'COMPLETE=1'
}catch{Note "FAIL=$($_.Exception.Message)";exit 1}
