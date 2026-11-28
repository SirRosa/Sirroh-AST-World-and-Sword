$ErrorActionPreference='Stop'
$ProgressPreference='SilentlyContinue'
$p='D:\0 Sirro\Unreal Projects\AST_V1_18'
$e='D:\0 Sirro\UE 5.8.1\UE_5.8'
$u=Join-Path $p 'AST_V1_18.uproject'
$d=Join-Path $p 'Saved\ChatGPT\PixelVisibility_20261128'
$stage=Join-Path $d 'stage'
$backup=Join-Path $d 'backup'
New-Item -ItemType Directory -Force $d,$stage,$backup|Out-Null
$result=Join-Path $d 'result.txt'
function Note($s){$s|Tee-Object -FilePath $result -Append}
function Hash($f){(& git hash-object --no-filters $f).Trim()}
function Native($exe,$a,$log){& $exe @a *> $log;return $LASTEXITCODE}
function NoEditor {if(Get-Process UnrealEditor,UnrealEditor-Cmd,AST_V1_18 -ErrorAction SilentlyContinue){throw 'Editor/game running; deployment stopped'}}
$commit='36aa0101fd26b6c064e88e3d452ea7c5ef7a4db5'
$files=@(
@{Path='Source/Sirroh_Reimagineg_v3/Private/Debug/SirrohDebugSubsystem.cpp';Old='1531f8e790f037559edd4b41743b35264afa5a74';New='cfca55aba56ae834f89203f67043ae1e2c67b75a'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2.cpp';Old='30fc136e6a1d6bbec02fb22ccde632a3617fe595';New='677fa6e67e7fc947b7278e764021e1c2eba8b562'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGEnvironmentV2SemanticVerification.cpp';Old='2c8fe1cf320f9a4eac0031a0a2fc158d40a13ed6';New='a44ee708da606c43d8a5bcfd9eb41a6ae33af040'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/PWG/PWGVisualProfileTests.cpp';Old='af7eb8161780f1c656104cd4200aaeeea5febab1';New='f63151bf0dc6eb81f9972ad19bfa5503421260fa'},
@{Path='Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2PixelBackground.cpp';Old='f2d57d0d0719532bf5b6ff9071bce21b9ebed994';New='dce5bd9697277a309b371ac97e2e4f3a8de51b1a'},
@{Path='Source/Sirroh_Reimagineg_v3/Public/PWG/PWGEnvironmentV2.h';Old='63280343ba72d3170ecd682735d542e9b1485e2b';New='641b2b9a06e2e39602a003435c9a5d45d323fe0a'},
@{Path='Source/Sirroh_Reimagineg_v3/Public/Procedural_World_Generator/PWGv2Generator.h';Old='c4fa46342d9a07c40bee4830153da261e13b854e';New='5e9109d5457761c6e0624581a465740f97ecfc25'},
@{Path='Source/Tools/create_pwg_pixel_raw_palette.py';Old='';New='e670cb67a86833cc189b02e4b6af4ad1823931ba'}
)
try {
 Note "SOURCE_COMMIT=$commit"
 NoEditor
 if(!(Test-Path $u)){throw 'Project missing'}
 foreach($f in $files){
  $local=Join-Path $p $f.Path
  if($f.Old){if(!(Test-Path $local) -or (Hash $local) -ne $f.Old){throw "BASELINE_MISMATCH $($f.Path)"}}
  elseif(Test-Path $local){throw "NEW_PATH_EXISTS $($f.Path)"}
  $target=Join-Path $stage $f.Path
  New-Item -ItemType Directory -Force (Split-Path $target)|Out-Null
  Invoke-WebRequest -UseBasicParsing -Uri "https://raw.githubusercontent.com/SirRosa/Sirroh-AST-World-and-Sword/$commit/$($f.Path)" -OutFile $target
  if((Hash $target) -ne $f.New){throw "STAGED_HASH_MISMATCH $($f.Path)"}
 }
 $unchanged=@{
 'Source/Sirroh_Reimagineg_v3/Private/Procedural_World_Generator/PWGv2PixelField.h'='2bf7e842d514a2b2943a98fd70a12cf11d647143'
 'Source/Tools/create_pwg_pixel_background.py'='2adb35e586761cd2d6bcf50345b083cf5a6959c6'
 'Source/Tools/create_pwg_pixel_depth_pp.py'='adbe58cedeaf343cc40eb11de0b8fb1b29cc3607'
 }
 foreach($k in $unchanged.Keys){if((Hash (Join-Path $p $k)) -ne $unchanged[$k]){throw "UNEXPECTED_GENERATOR_OR_PALETTE_CHANGE $k"}}
 Note 'STAGED_HASHES_OK; FIELD_AND_PALETTE_UNCHANGED'
 $protected=@{}
 Get-ChildItem (Join-Path $p 'Saved\SaveGames') -File -Filter '*.sav'|ForEach-Object {$protected[$_.FullName]=(Get-FileHash $_.FullName).Hash}
 Get-ChildItem (Join-Path $p 'Content') -Recurse -File -Filter 'M_PWGv2*.uasset'|ForEach-Object {$protected[$_.FullName]=(Get-FileHash $_.FullName).Hash}
 $copySource=Join-Path $p 'Saved\SaveGames\PWGv2_VF_POIProbe_20261126.sav'
 $copyDest=Join-Path $p 'Saved\SaveGames\PWGv2_VF_PixelVisibility_20261128.sav'
 if(!(Test-Path $copySource)){throw 'QA source slot missing'}
 if(Test-Path $copyDest){throw 'QA destination already exists; preserve for inspection'}
 if(Test-Path (Join-Path $d 'Render')){throw 'Previous renders exist; preserve for inspection'}
 foreach($f in $files){if($f.Old){$b=Join-Path $backup $f.Path;New-Item -ItemType Directory -Force (Split-Path $b)|Out-Null;Copy-Item (Join-Path $p $f.Path) $b;if((Hash $b) -ne $f.Old){throw 'BACKUP_HASH_MISMATCH'}}}
 Note 'LOCAL_SOURCE_BACKUP_OK'
 $editor=Join-Path $e 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
 $author=(Join-Path $stage 'Source/Tools/create_pwg_pixel_raw_palette.py').Replace('\','/')
 $rc=Native $editor @($u,'-run=pythonscript',("-script="+$author),'-unattended','-nop4','-AllowCommandletRendering','-nosplash','-stdout','-FullStdOutLogOutput') (Join-Path $d 'author.log')
 Note "AUTHOR_EXIT=$rc"
 $al=Get-Content (Join-Path $d 'author.log') -Raw
 if($rc -ne 0 -or $al -notmatch 'PIXEL_RAW_PALETTE_VERIFIED' -or $al -match 'Traceback \(most recent call last\)|Failed to compile Material.*PixelBackground_RawPalette'){throw 'Raw material authoring not verified; see author.log'}
 $raw=Get-ChildItem (Join-Path $p 'Content') -Recurse -File -Filter 'M_PWGv2_PixelBackground_RawPalette.uasset'|Select-Object -First 1
 if(!$raw){throw 'Raw asset missing'}
 Note "RAW_ASSET_SHA256=$((Get-FileHash $raw.FullName).Hash)"
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
 $rc=Native $editor @($u,'/Game/Levels/L_SirrohGameplay','-game','-PWGVisualPOIQA','-PWGPixelVisibilityQA','-SirrohSkipMainMenu','-PWGWorld=VF_PixelVisibility_20261128','-windowed','-ResX=1600','-ResY=900','-unattended','-nop4','-nosound','-nosplash','-stdout','-FullStdOutLogOutput','-ExecCmds=t.MaxFPS 60') (Join-Path $d 'render.log')
 Note "RENDER_EXIT=$rc"
 $rl=Get-Content (Join-Path $d 'render.log') -Raw
 foreach($k in $protected.Keys){if(!(Test-Path $k) -or (Get-FileHash $k).Hash -ne $protected[$k]){throw "ORIGINAL_CHANGED $k"}}
 Note 'ORIGINALS_UNCHANGED=1'
 if($rc -ne 0 -or $rl -notmatch 'VF_RENDER_COMPLETE' -or $rl -match 'VF_RENDER_FAIL|VF_RENDER_SCENE_UNAVAILABLE|PWG_PIXEL2D_CHUNK.*ready=0|PWG_PIXEL2D_RAW_SWAP_FAIL'){throw 'RHI capture failed or incomplete'}
 $names=@('Tunnel_RawPalette','Tunnel_Atmospheric','Tunnel_Atmospheric_8lux','Tunnel_Atmospheric_32lux','Tunnel_Atmospheric_128lux','Tunnel_Lit128_NoDepthPP','Tunnel_Lit128_ManualExposure','Tunnel_Lit128_ReverseFill','Tunnel_Atmospheric_512lux','Tunnel_Dark','Tunnel_TorchDark','DeepCave_RawPalette')
 $audit=@(Select-String -Path (Join-Path $d 'render.log') -Pattern 'PIXEL_AUDIT ' | ForEach-Object {$_.Line})
 if(!($audit|Where-Object {$_ -match 'visible=1.*material=M_PWGv2_PixelBackground_RawPalette .*data=1'})){throw 'Raw material not visible in audit'}
 if($audit|Where-Object {$_ -match 'invalid=[1-9]\d*|data=0|plan=3 .*transparent=[1-9]\d*'}){throw 'Data audit failed'}
 $audit|Set-Content -Encoding UTF8 (Join-Path $d 'pixel_sections.txt')
 Select-String -Path (Join-Path $d 'render.log') -Pattern 'PIXEL_FILL |PIXEL_CAMERA_PP |PIXEL_OCCLUSION |VF_RENDER_CAPTURE ' | ForEach-Object {$_.Line}|Set-Content -Encoding UTF8 (Join-Path $d 'lighting_audit.txt')
 Add-Type -AssemblyName System.Drawing
 foreach($n in $names){
  $f=Join-Path $d "Render\$n.png"
  if(!(Test-Path $f) -or (Get-Item $f).Length -lt 10000){throw "CAPTURE_MISSING $n"}
  $img=New-Object System.Drawing.Bitmap($f)
  try {
   if($img.Width -ne 1600 -or $img.Height -ne 900){throw "UNEXPECTED_CAPTURE_SIZE $n"}
   $values=@()
   for($y=250;$y -lt 610;$y+=15){for($x=100;$x -lt 1500;$x+=20){$c=$img.GetPixel($x,$y);$values+=([double]$c.R*.2126+$c.G*.7152+$c.B*.0722)}}
   $sorted=@($values|Sort-Object)
   $p90=$sorted[[int][Math]::Floor(($sorted.Count-1)*.9)]
   $mean=($values|Measure-Object -Average).Average
   Note ("CAPTURE={0} mean_display_luma={1:F2} p90={2:F2}" -f $n,$mean,$p90)
  }finally{$img.Dispose()}
 }
 Note 'RENDER_CAPTURES=12'
 Note 'CAPTURE_PIPELINE_COMPLETE=1; VISUAL_AND_LIGHTING_ACCEPTANCE_PENDING_INSPECTION'
}catch{Note "FAIL=$($_.Exception.Message)";exit 1}
